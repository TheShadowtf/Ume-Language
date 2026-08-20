// evaluator.cpp — Tree-Walk Evaluator for the Ume Language

/*
    Is the code bad? Yes
    Does the code work? Also yes
    Will I refactor this? Maybe, but don't count on it.
    Will anyone else wants to refactor this? Nope, no one is crazy enough to refactor this code.
    If anyone wants to refactor this code, they are either brave or dumb.
    Good luck to them.
    Also, this is the largest file in the entire project sooo glhf.
*/
#include "../include/evaluator.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <random>
#include <cstdlib>
#include <condition_variable>

namespace Ume {

// Forward declaration — defined near end of file; used in callMethod()
static const FuncDecl* findEnumMethod(const EnumDecl& enm, const std::string& name);

double Value::toDouble() const {
    if (kind == Kind::Int)    return static_cast<double>(intVal);
    if (kind == Kind::Float)  return floatVal;
    if (kind == Kind::Bool)   return boolVal ? 1.0 : 0.0;
    if (kind == Kind::Char)   return static_cast<double>(charVal);
    if (kind == Kind::String) {
        try { return std::stod(strVal); } catch (...) { return 0.0; }
    }
    return 0.0;
}

bool Value::toBool() const {
    switch (kind) {
    case Kind::Null:   return false;
    case Kind::Bool:   return boolVal;
    case Kind::Int:    return intVal != 0;
    case Kind::Float:  return floatVal != 0.0;
    case Kind::String: return !strVal.empty();
    default:           return true;
    }
}

std::string Value::toString() const {
    switch (kind) {
    case Kind::Null:    return "null";
    case Kind::Bool:    return boolVal ? "true" : "false";
    case Kind::Int:     return std::to_string(intVal);
    case Kind::Float: {
        std::ostringstream oss; oss << floatVal; return oss.str();
    }
    case Kind::Char:    return std::string(1, charVal);
    case Kind::String:  return strVal;
    case Kind::Object: {
        if (!objVal) return "null";
        // Enum value — return the name
        auto en = objVal->fields.find("__enumName");
        if (en != objVal->fields.end() && en->second.isString()) return en->second.strVal;
        // Exception-like object — return the message
        auto msg = objVal->fields.find("message");
        if (msg != objVal->fields.end() && msg->second.isString()) return msg->second.strVal;
        return "[" + objVal->className + " instance]";
    }
    case Kind::Array:   return "[array]";
    case Kind::Function:return "[function]";
    }
    return "null";
}

bool Value::operator==(const Value& other) const {
    // Object vs int (enum compared to int literal)
    if (isObject() && other.isInt() && objVal) {
        auto ev = objVal->fields.find("__enumValue");
        if (ev != objVal->fields.end() && ev->second.isInt()) return ev->second.intVal == other.intVal;
    }
    if (isInt() && other.isObject() && other.objVal) {
        auto ev = other.objVal->fields.find("__enumValue");
        if (ev != other.objVal->fields.end() && ev->second.isInt()) return intVal == ev->second.intVal;
    }
    if (kind != other.kind) {
        // Cross-type int/float comparison
        if (isNumeric() && other.isNumeric()) return toDouble() == other.toDouble();
        return false;
    }
    switch (kind) {
    case Kind::Null:   return true;
    case Kind::Bool:   return boolVal  == other.boolVal;
    case Kind::Int:    return intVal   == other.intVal;
    case Kind::Float:  return floatVal == other.floatVal;
    case Kind::Char:   return charVal  == other.charVal;
    case Kind::String: return strVal   == other.strVal;
    case Kind::Object: {
        if (objVal.get() == other.objVal.get()) return true;
        if (objVal && other.objVal) {
            auto ev1 = objVal->fields.find("__enumValue");
            auto ev2 = other.objVal->fields.find("__enumValue");
            if (ev1 != objVal->fields.end() && ev2 != other.objVal->fields.end() && ev1->second.isInt() && ev2->second.isInt()) {
                return ev1->second.intVal == ev2->second.intVal;
            }
        }
        return false;
    }
    default:           return false;
    }
}

bool Value::operator<(const Value& other) const {
    if (isNumeric() && other.isNumeric()) return toDouble() < other.toDouble();
    if (kind == Kind::String && other.kind == Kind::String) return strVal < other.strVal;
    // Enum objects compared by __enumValue
    if (isObject() && other.isObject() && objVal && other.objVal) {
        auto a = objVal->fields.find("__enumValue");
        auto b = other.objVal->fields.find("__enumValue");
        if (a != objVal->fields.end() && b != other.objVal->fields.end())
            return a->second < b->second;
    }
    return false;
}

const char* UmeRuntimeException::what() const noexcept {
    static std::string msg;
    if (value.isObject() && value.objVal) {
        auto it = value.objVal->fields.find("message");
        if (it != value.objVal->fields.end())
            msg = it->second.toString();
        else
            msg = value.toString();
    } else if (value.isString()) {
        msg = value.strVal;
    } else {
        msg = value.toString();
    }
    return msg.c_str();
}

static void throwRuntimeError(const std::string& msg,
                               const ASTNode& node) {
    throw UmeRuntimeException(Value::makeString(msg), node.line, node.column, node.filename);
}

static void throwRuntimeError(const std::string& msg,
                               int line = 0, int column = 0, const std::string& filename = "") {
    throw UmeRuntimeException(Value::makeString(msg), line, column, filename);
}

Evaluator::Evaluator()
    : global_(std::make_shared<Environment>()) {}

void Evaluator::loadLibrary(const Program& prog) {
    collectDeclarations(prog, global_);
}

int Evaluator::run(const Program& program) {
    registerBuiltins(global_);
    collectDeclarations(program, global_);

    // Helper: call a FuncDecl as a static class method (with __class__ set)
    auto callStaticDecl = [&](const FuncDecl* fn, const std::string& className) -> int {
        auto staticEnv = global_->child();
        staticEnv->declare("__class__", Value::makeString(className));
        auto fi = std::make_shared<FunctionInstance>();
        fi->name    = fn->name;
        fi->params  = fn->params;
        fi->body    = fn->body.get();
        fi->closure = staticEnv;
        try {
            Value result = callFunction(*fi, {});
            if (result.isInt()) return static_cast<int>(result.intVal);
            return 0;
        } catch (const ReturnSignal& ret) {
            if (ret.value.isInt()) return static_cast<int>(ret.value.intVal);
            return 0;
        }
    };

    // 1. Global func main() — plain script style
    if (global_->has("main")) {
        Value& mainVal = global_->get("main");
        if (mainVal.isFunction()) {
            try {
                Value result = callFunction(*mainVal.funcVal, {});
                if (result.isInt()) return static_cast<int>(result.intVal);
                return 0;
            } catch (const ReturnSignal& ret) {
                if (ret.value.isInt()) return static_cast<int>(ret.value.intVal);
                return 0;
            }
        }
    }

    // 2. REPL wrapper: __repl__() — called by the REPL
    if (global_->has("__repl__")) {
        Value& fn = global_->get("__repl__");
        if (fn.isFunction()) callFunction(*fn.funcVal, {});
        return 0;
    }

    // 3. Class-based entry: look for class Main (or *.Main)
    for (auto& [clsName, cls] : classDefs_) {
        if (clsName == "Main" || (clsName.size() >= 5 && clsName.compare(clsName.size() - 5, 5, ".Main") == 0)) {
            for (auto& m : cls->methods) {
                if (m->name == "main") {
                    if (m->isStatic) {
                        return callStaticDecl(m.get(), clsName);
                    } else {
                        auto obj = std::make_shared<ObjectInstance>();
                        obj->className = cls->name;
                        obj->classDef = cls;
                        instantiateFields(*obj, *cls, global_);
                        Value thisVal = Value::makeObject(obj);
                        Value res = callMethod(thisVal, "main", {}, global_);
                        return res.isInt() ? static_cast<int>(res.intVal) : 0;
                    }
                }
            }
        }
    }

    // 4. Fallback: any class that has a main() method
    for (auto& [clsName, cls] : classDefs_) {
        for (auto& m : cls->methods) {
            if (m->name == "main") {
                if (m->isStatic) {
                    return callStaticDecl(m.get(), clsName);
                } else {
                    auto obj = std::make_shared<ObjectInstance>();
                    obj->className = cls->name;
                    obj->classDef = cls;
                    instantiateFields(*obj, *cls, global_);
                    Value thisVal = Value::makeObject(obj);
                    Value res = callMethod(thisVal, "main", {}, global_);
                    return res.isInt() ? static_cast<int>(res.intVal) : 0;
                }
            }
        }
    }

    return 0;
}

Value Evaluator::eval(const ASTNode& node, std::shared_ptr<Environment> env) {
    // ── Literals
    if (auto* n = dynamic_cast<const IntLiteralExpr*>(&node))    return Value::makeInt(n->value);
    if (auto* n = dynamic_cast<const FloatLiteralExpr*>(&node))  return Value::makeFloat(n->value);
    if (auto* n = dynamic_cast<const StringLiteralExpr*>(&node)) return Value::makeString(n->value);
    if (auto* n = dynamic_cast<const CharLiteralExpr*>(&node))   return Value::makeChar(n->value);
    if (auto* n = dynamic_cast<const BoolLiteralExpr*>(&node))   return Value::makeBool(n->value);
    if (dynamic_cast<const NullLiteralExpr*>(&node))             return Value::makeNull();

    // ── Identifier 
    if (auto* n = dynamic_cast<const IdentifierExpr*>(&node)) {
        if (n->name == "null")  return Value::makeNull();
        if (n->name == "true")  return Value::makeBool(true);
        if (n->name == "false") return Value::makeBool(false);
        if (env->has(n->name)) return env->get(n->name);
        
        // 1. Check instance fields/methods (this)
        if (env->has("__this")) {
            Value& tv = env->get("__this");
            if (tv.isObject() && tv.objVal) {
                auto fit = tv.objVal->fields.find(n->name);
                if (fit != tv.objVal->fields.end()) return fit->second;
                
                const ClassDecl* cls = findClass(tv.objVal->className);
                if (cls) {
                    const FuncDecl* m = findMethod(*cls, n->name);
                    if (m && m->body) {
                        auto fi = std::make_shared<FunctionInstance>();
                        fi->name   = m->name;
                        fi->params = m->params;
                        fi->body   = m->body.get();
                        fi->closure = env;
                        return Value::makeFunction(fi);
                    }
                }
                // Check class static fields via __this__ className
                std::string qk = tv.objVal->className + "." + n->name;
                if (global_->has(qk)) return global_->get(qk);
            }
        }
        
        // 2. Check static fields/methods (current class context)
        if (env->has("__class__")) {
            Value& cv = env->get("__class__");
            if (cv.isString()) {
                std::string qk = cv.strVal + "." + n->name;
                if (global_->has(qk)) return global_->get(qk);
                
                auto cit = classDefs_.find(cv.strVal);
                if (cit != classDefs_.end()) {
                    const FuncDecl* m = findMethod(*cit->second, n->name);
                    if (m && m->body) {
                        auto fi = std::make_shared<FunctionInstance>();
                        fi->name   = m->name;
                        fi->params = m->params;
                        fi->body   = m->body.get();
                        fi->closure = env;
                        return Value::makeFunction(fi);
                    }
                }
            }
        }
        
        if (global_->has(n->name)) return global_->get(n->name);
        
        throwRuntimeError("Undefined variable: " + n->name, *n);
    }

    // ── Expressions
    if (auto* n = dynamic_cast<const BinaryExpr*>(&node))            return evalBinary(*n, env);
    if (auto* n = dynamic_cast<const UnaryExpr*>(&node))             return evalUnary(*n, env);
    if (auto* n = dynamic_cast<const AssignExpr*>(&node))            return evalAssign(*n, env);
    if (auto* n = dynamic_cast<const CallExpr*>(&node))              return evalCall(*n, env);
    if (auto* n = dynamic_cast<const MemberAccessExpr*>(&node))      return evalMemberAccess(*n, env);
    if (auto* n = dynamic_cast<const IndexExpr*>(&node))             return evalIndex(*n, env);
    if (auto* n = dynamic_cast<const NewExpr*>(&node))               return evalNew(*n, env);
    if (auto* n = dynamic_cast<const CastExpr*>(&node))              return evalCast(*n, env);
    if (auto* n = dynamic_cast<const TernaryExpr*>(&node)) {
        Value cond = eval(*n->condition, env);
        return isTruthy(cond) ? eval(*n->thenExpr, env) : eval(*n->elseExpr, env);
    }
    if (auto* n = dynamic_cast<const LambdaExpr*>(&node))            return evalLambda(*n, env);
    if (auto* n = dynamic_cast<const InterpolatedStringExpr*>(&node)) return evalInterpolatedString(*n, env);
    if (auto* n = dynamic_cast<const NullCoalesceExpr*>(&node))      return evalNullCoalesce(*n, env);
    if (auto* n = dynamic_cast<const NullAssertExpr*>(&node))        return evalNullAssert(*n, env);
    if (auto* n = dynamic_cast<const ArrayLiteralExpr*>(&node))      return evalArrayLiteral(*n, env);
    if (auto* n = dynamic_cast<const StructInitExpr*>(&node))        return evalStructInit(*n, env);

    // unsafe ops
    if (auto* n = dynamic_cast<const AllocExpr*>(&node)) {
        Value cnt = eval(*n->count, env);
        std::function<Value(const std::vector<Value>&, size_t)> createMultiArray = [&](const std::vector<Value>& dims, size_t idx) -> Value {
            if (idx >= dims.size()) return Value::makeNull();
            int64_t sz = dims[idx].isInt() ? dims[idx].intVal : static_cast<int64_t>(dims[idx].toDouble());
            if (sz < 0) sz = 0;
            auto arr = std::make_shared<ArrayInstance>();
            arr->elements.resize(static_cast<size_t>(sz));
            if (idx + 1 < dims.size()) {
                for (size_t i = 0; i < arr->elements.size(); ++i) {
                    arr->elements[i] = createMultiArray(dims, idx + 1);
                }
            }
            return Value::makeArray(arr);
        };

        if (cnt.isArray()) {
            return createMultiArray(cnt.arrVal->elements, 0);
        } else {
            auto arr = std::make_shared<ArrayInstance>();
            int64_t sz = cnt.isInt() ? cnt.intVal : (cnt.isNull() ? 1 : static_cast<int64_t>(cnt.toDouble()));
            if (sz < 0) sz = 0;
            arr->elements.resize(static_cast<size_t>(sz));
            return Value::makeArray(arr);
        }
    }
    if (auto* n = dynamic_cast<const FreeExpr*>(&node)) { eval(*n->pointer, env); return Value::makeNull(); }
    if (auto* n = dynamic_cast<const DerefExpr*>(&node)) {
        // *ptr — if ptr is an array (from alloc), return the first element
        Value v = eval(*n->pointer, env);
        if (v.isArray() && !v.arrVal->elements.empty()) return v.arrVal->elements[0];
        return v;
    }
    if (auto* n = dynamic_cast<const AddressOfExpr*>(&node)) {
        // &value — return the value itself (aliased)
        return eval(*n->value, env);
    }

    // ── Statements
    if (auto* n = dynamic_cast<const BlockStmt*>(&node))    return evalBlock(*n, env);
    if (auto* n = dynamic_cast<const ExprStmt*>(&node))     { eval(*n->expr, env); return Value::makeNull(); }
    if (auto* n = dynamic_cast<const VarDeclStmt*>(&node))  return evalVarDecl(*n, env);
    if (auto* n = dynamic_cast<const ReturnStmt*>(&node)) {
        Value v = n->value ? eval(*n->value, env) : Value::makeNull();
        throw ReturnSignal(std::move(v));
    }
    if (auto* n = dynamic_cast<const IfStmt*>(&node))       return evalIf(*n, env);
    if (auto* n = dynamic_cast<const WhileStmt*>(&node))    return evalWhile(*n, env);
    if (auto* n = dynamic_cast<const DoWhileStmt*>(&node))  return evalDoWhile(*n, env);
    if (auto* n = dynamic_cast<const ForStmt*>(&node))      return evalFor(*n, env);
    if (auto* n = dynamic_cast<const ForEachStmt*>(&node))  return evalForEach(*n, env);
    if (auto* n = dynamic_cast<const SwitchStmt*>(&node))   return evalSwitch(*n, env);
    if (auto* n = dynamic_cast<const TryCatchStmt*>(&node)) return evalTryCatch(*n, env);
    if (auto* n = dynamic_cast<const ThrowStmt*>(&node)) {
        Value v = eval(*n->value, env);
        throw UmeRuntimeException(std::move(v), n->line);
    }
    if (dynamic_cast<const BreakStmt*>(&node))    throw BreakSignal{};
    if (dynamic_cast<const ContinueStmt*>(&node)) throw ContinueSignal{};
    if (auto* n = dynamic_cast<const UnsafeBlock*>(&node))  return evalUnsafe(*n, env);

    // ── Declarations (inside functions)
    if (auto* n = dynamic_cast<const FuncDecl*>(&node)) {
        auto fi      = std::make_shared<FunctionInstance>();
        fi->name     = n->name;
        fi->params   = n->params;
        fi->body     = n->body.get();
        fi->closure  = env;
        Value v      = Value::makeFunction(fi);
        env->declare(n->name, v);
        return Value::makeNull();
    }

    // Ignore top-level declarations that are already collected
    if (dynamic_cast<const ClassDecl*>(&node))     return Value::makeNull();
    if (dynamic_cast<const StructDecl*>(&node))    return Value::makeNull();
    if (dynamic_cast<const EnumDecl*>(&node))      return Value::makeNull();
    if (dynamic_cast<const InterfaceDecl*>(&node)) return Value::makeNull();
    if (auto* ns = dynamic_cast<const NamespaceDecl*>(&node)) {
        for (auto& d : ns->declarations) eval(*d, env);
        return Value::makeNull();
    }
    if (dynamic_cast<const PackageDecl*>(&node))   return Value::makeNull();
    if (dynamic_cast<const ImportDecl*>(&node))    return Value::makeNull();
    if (dynamic_cast<const IncludeDirective*>(&node)) return Value::makeNull();

    return Value::makeNull();
}

void Evaluator::collectDeclarations(const Program& program,
                                     std::shared_ptr<Environment> env) {
    std::function<void(const std::vector<ASTNodePtr>&)> collect = [&](const std::vector<ASTNodePtr>& decls) {
        // --- PASS 1: Register all types and functions first (Fixes forward references) ---
        for (auto& decl : decls) {
            if (!decl) continue;
            if (auto* ns = dynamic_cast<const NamespaceDecl*>(decl.get())) {
                collect(ns->declarations);
            } else if (auto* cls = dynamic_cast<const ClassDecl*>(decl.get())) {
                classDefs_[cls->name] = cls;
            } else if (auto* ifc = dynamic_cast<const InterfaceDecl*>(decl.get())) {
                ifaceDefs_[ifc->name] = ifc;
            } else if (auto* enm = dynamic_cast<const EnumDecl*>(decl.get())) {
                enumDefs_[enm->name] = enm;
                int64_t nextVal = 0;
                for (auto& ev : enm->values) {
                    int64_t val = nextVal;
                    if (ev.value) {
                        Value v = eval(*ev.value, env);
                        if (v.isInt()) val = v.intVal;
                    }
                    auto eo = std::make_shared<ObjectInstance>();
                    eo->className = enm->name;
                    eo->fields["__enumName"] = Value::makeString(ev.name);
                    eo->fields["__enumValue"] = Value::makeInt(val);
                    env->declare(enm->name + "_" + ev.name, Value::makeObject(eo));
                    env->declare(ev.name, Value::makeObject(eo));
                    nextVal = val + 1;
                }
            } else if (auto* str = dynamic_cast<const StructDecl*>(decl.get())) {
                structDefs_[str->name] = str;
            } else if (auto* func = dynamic_cast<const FuncDecl*>(decl.get())) {
                auto fi     = std::make_shared<FunctionInstance>();
                fi->name    = func->name;
                fi->params  = func->params;
                fi->body    = func->body.get();
                fi->closure = env;
                env->declare(func->name, Value::makeFunction(fi));
            }
        }

        // --- PASS 2: Now evaluate static fields and global variables ---
        for (auto& decl : decls) {
            if (!decl) continue;
            if (auto* ns = dynamic_cast<const NamespaceDecl*>(decl.get())) {
                continue; // Already processed in pass 1
            } else if (auto* cls = dynamic_cast<const ClassDecl*>(decl.get())) {
                for (auto& f : cls->fields) {
                    if (f->isStatic) {
                        Value v = f->initializer ? eval(*f->initializer, env) : Value::makeNull();
                        env->declare(cls->name + "." + f->name, v);
                    }
                }
            } else if (auto* ifc = dynamic_cast<const InterfaceDecl*>(decl.get())) {
                for (auto& c : ifc->constants) {
                    Value v = c->initializer ? eval(*c->initializer, env) : Value::makeNull();
                    env->declare(ifc->name + "." + c->name, v);
                }
            } else if (auto* var = dynamic_cast<const VarDeclStmt*>(decl.get())) {
                Value v = var->initializer ? eval(*var->initializer, env) : Value::makeNull();
                env->declare(var->name, v);
            }
        }
    };
    collect(program.declarations);
}

void Evaluator::registerBuiltins(std::shared_ptr<Environment> env) {
    registerConsole(env);
    registerMathExtended(env);
    registerSystem(env);
    registerFileSystem(env);
    registerStringUtils(env);
    registerGraphics(env);

    // Register Thread static methods (Thread.sleep, Thread.current)
    {
        auto threadObj = std::make_shared<ObjectInstance>();
        threadObj->className = "Thread";
        auto mkFn = [](const std::string& name, std::function<Value(std::vector<Value>)> fn) -> Value {
            auto fi = std::make_shared<FunctionInstance>();
            fi->name = name; fi->native = std::move(fn);
            return Value::makeFunction(fi);
        };
        threadObj->fields["sleep"] = mkFn("sleep", [](std::vector<Value> args) -> Value {
            int64_t ms = args.empty() ? 0 : args[0].intVal;
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            return Value::makeNull();
        });
        env->declare("Thread", Value::makeObject(threadObj));
    }

    // Register native StringBuilder (always available, even without prelude)
    {
        auto mkFn = [](const std::string& name, std::function<Value(std::vector<Value>)> fn) -> Value {
            auto fi = std::make_shared<FunctionInstance>();
            fi->name = name; fi->native = std::move(fn);
            return Value::makeFunction(fi);
        };
        auto sbObj = std::make_shared<ObjectInstance>();
        sbObj->className = "StringBuilder";
        sbObj->fields["append"] = mkFn("append", [](std::vector<Value> args) -> Value {
            return Value::makeNull();
        });
    }

    // Register native Pair (always available)
    {
        auto mkFn = [](const std::string& name, std::function<Value(std::vector<Value>)> fn) -> Value {
            auto fi = std::make_shared<FunctionInstance>();
            fi->name = name; fi->native = std::move(fn);
            return Value::makeFunction(fi);
        };
    }
}

void Evaluator::registerMathExtended(std::shared_ptr<Environment> env) {
    auto mathObj = std::make_shared<ObjectInstance>();
    mathObj->className = "Math";

    auto mkFn = [](const std::string& name, std::function<Value(std::vector<Value>)> fn) {
        auto fi = std::make_shared<FunctionInstance>();
        fi->name = name; fi->native = std::move(fn);
        return Value::makeFunction(fi);
    };

    // Basic
    mathObj->fields["sqrt"]      = mkFn("sqrt",      [](auto a){ return Value::makeFloat(std::sqrt(a[0].toDouble())); });
    mathObj->fields["cbrt"]      = mkFn("cbrt",      [](auto a){ return Value::makeFloat(std::cbrt(a[0].toDouble())); });
    mathObj->fields["pow"]       = mkFn("pow",       [](auto a){ return Value::makeFloat(std::pow(a[0].toDouble(), a[1].toDouble())); });
    mathObj->fields["exp"]       = mkFn("exp",       [](auto a){ return Value::makeFloat(std::exp(a[0].toDouble())); });
    mathObj->fields["abs"]       = mkFn("abs",       [](auto a){ return a[0].isInt() ? Value::makeInt(std::abs(a[0].intVal)) : Value::makeFloat(std::abs(a[0].toDouble())); });
    mathObj->fields["floor"]     = mkFn("floor",     [](auto a){ return Value::makeFloat(std::floor(a[0].toDouble())); });
    mathObj->fields["ceil"]      = mkFn("ceil",      [](auto a){ return Value::makeFloat(std::ceil(a[0].toDouble())); });
    mathObj->fields["round"]     = mkFn("round",     [](auto a){ return Value::makeFloat(std::round(a[0].toDouble())); });
    mathObj->fields["trunc"]     = mkFn("trunc",     [](auto a){ return Value::makeFloat(std::trunc(a[0].toDouble())); });
    // Logarithms
    mathObj->fields["log"]       = mkFn("log",       [](auto a){ return Value::makeFloat(std::log(a[0].toDouble())); });
    mathObj->fields["log2"]      = mkFn("log2",      [](auto a){ return Value::makeFloat(std::log2(a[0].toDouble())); });
    mathObj->fields["log10"]     = mkFn("log10",     [](auto a){ return Value::makeFloat(std::log10(a[0].toDouble())); });
    // Trig
    mathObj->fields["sin"]       = mkFn("sin",       [](auto a){ return Value::makeFloat(std::sin(a[0].toDouble())); });
    mathObj->fields["cos"]       = mkFn("cos",       [](auto a){ return Value::makeFloat(std::cos(a[0].toDouble())); });
    mathObj->fields["tan"]       = mkFn("tan",       [](auto a){ return Value::makeFloat(std::tan(a[0].toDouble())); });
    mathObj->fields["asin"]      = mkFn("asin",      [](auto a){ return Value::makeFloat(std::asin(a[0].toDouble())); });
    mathObj->fields["acos"]      = mkFn("acos",      [](auto a){ return Value::makeFloat(std::acos(a[0].toDouble())); });
    mathObj->fields["atan"]      = mkFn("atan",      [](auto a){ return Value::makeFloat(std::atan(a[0].toDouble())); });
    mathObj->fields["atan2"]     = mkFn("atan2",     [](auto a){ return Value::makeFloat(std::atan2(a[0].toDouble(), a[1].toDouble())); });
    // Min / max / clamp
    mathObj->fields["min"]       = mkFn("min",       [](auto a){
        return a[0].isInt() && a[1].isInt()
            ? Value::makeInt(std::min(a[0].intVal, a[1].intVal))
            : Value::makeFloat(std::min(a[0].toDouble(), a[1].toDouble()));
    });
    mathObj->fields["max"]       = mkFn("max",       [](auto a){
        return a[0].isInt() && a[1].isInt()
            ? Value::makeInt(std::max(a[0].intVal, a[1].intVal))
            : Value::makeFloat(std::max(a[0].toDouble(), a[1].toDouble()));
    });
    mathObj->fields["clamp"]     = mkFn("clamp",     [](auto a){
        double v = a[0].toDouble(), lo = a[1].toDouble(), hi = a[2].toDouble();
        return Value::makeFloat(std::max(lo, std::min(hi, v)));
    });
    mathObj->fields["lerp"]      = mkFn("lerp",      [](auto a){
        double al = a[0].toDouble(), bl = a[1].toDouble(), t = a[2].toDouble();
        return Value::makeFloat(al + t * (bl - al));
    });
    // Conversions & predicates
    mathObj->fields["toRadians"] = mkFn("toRadians", [](auto a){ return Value::makeFloat(a[0].toDouble() * 3.14159265358979323846 / 180.0); });
    mathObj->fields["toDegrees"] = mkFn("toDegrees", [](auto a){ return Value::makeFloat(a[0].toDouble() * 180.0 / 3.14159265358979323846); });
    mathObj->fields["isNaN"]     = mkFn("isNaN",     [](auto a){ return Value::makeBool(std::isnan(a[0].toDouble())); });
    mathObj->fields["isInfinite"]= mkFn("isInfinite",[](auto a){ return Value::makeBool(std::isinf(a[0].toDouble())); });
    // Constants
    mathObj->fields["PI"]  = Value::makeFloat(3.14159265358979323846);
    mathObj->fields["E"]   = Value::makeFloat(2.71828182845904523536);
    mathObj->fields["TAU"] = Value::makeFloat(6.28318530717958647692);
    mathObj->fields["absi"] = mathObj->fields["abs"];

    env->declare("Math", Value::makeObject(mathObj));
}

void Evaluator::registerSystem(std::shared_ptr<Environment> env) {
    auto sysObj = std::make_shared<ObjectInstance>();
    sysObj->className = "System";

    auto mkFn = [](const std::string& name, std::function<Value(std::vector<Value>)> fn) {
        auto fi = std::make_shared<FunctionInstance>();
        fi->name = name; fi->native = std::move(fn);
        return Value::makeFunction(fi);
    };

    // args() — returns program arguments as array
    std::vector<std::string>* argsPtr = &programArgs_;
    sysObj->fields["args"] = mkFn("args", [argsPtr](std::vector<Value>) -> Value {
        auto arr = std::make_shared<ArrayInstance>();
        for (auto& a : *argsPtr) arr->elements.push_back(Value::makeString(a));
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "List";
        obj->fields["_data"] = Value::makeArray(arr);
        return Value::makeObject(obj);
    });

    sysObj->fields["exit"] = mkFn("exit", [](std::vector<Value> args) -> Value {
        std::exit(args.empty() ? 0 : static_cast<int>(args[0].intVal));
        return Value::makeNull();
    });

    sysObj->fields["getEnv"] = mkFn("getEnv", [](std::vector<Value> args) -> Value {
        if (args.empty()) return Value::makeNull();
        const char* v = std::getenv(args[0].toString().c_str());
        return v ? Value::makeString(v) : Value::makeNull();
    });

    sysObj->fields["currentTimeMillis"] = mkFn("currentTimeMillis", [](std::vector<Value>) -> Value {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return Value::makeInt(static_cast<int64_t>(ms));
    });

    sysObj->fields["nanoTime"] = mkFn("nanoTime", [](std::vector<Value>) -> Value {
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        return Value::makeInt(static_cast<int64_t>(ns));
    });

    sysObj->fields["exec"] = mkFn("exec", [](std::vector<Value> args) -> Value {
        if (args.empty()) return Value::makeInt(-1);
        return Value::makeInt(static_cast<int64_t>(std::system(args[0].toString().c_str())));
    });

    sysObj->fields["cwd"] = mkFn("cwd", [](std::vector<Value>) -> Value {
        return Value::makeString(std::filesystem::current_path().string());
    });

    sysObj->fields["platform"] = mkFn("platform", [](std::vector<Value>) -> Value {
#ifdef _WIN32
        return Value::makeString("windows");
#elif defined(__APPLE__)
        return Value::makeString("macos");
#else
        return Value::makeString("linux");
#endif
    });

    env->declare("System", Value::makeObject(sysObj));
}

void Evaluator::registerFileSystem(std::shared_ptr<Environment> env) {
    auto fsObj = std::make_shared<ObjectInstance>();
    fsObj->className = "FileSystem";

    auto mkFn = [](const std::string& name, std::function<Value(std::vector<Value>)> fn) {
        auto fi = std::make_shared<FunctionInstance>();
        fi->name = name; fi->native = std::move(fn);
        return Value::makeFunction(fi);
    };

    fsObj->fields["exists"]      = mkFn("exists",      [](auto a){ return Value::makeBool(std::filesystem::exists(a[0].toString())); });
    fsObj->fields["isFile"]      = mkFn("isFile",      [](auto a){ return Value::makeBool(std::filesystem::is_regular_file(a[0].toString())); });
    fsObj->fields["isDirectory"] = mkFn("isDirectory", [](auto a){ return Value::makeBool(std::filesystem::is_directory(a[0].toString())); });
    fsObj->fields["createDir"]   = mkFn("createDir",   [](auto a){ std::filesystem::create_directory(a[0].toString()); return Value::makeNull(); });
    fsObj->fields["createDirs"]  = mkFn("createDirs",  [](auto a){ std::filesystem::create_directories(a[0].toString()); return Value::makeNull(); });
    fsObj->fields["deleteFile"]  = mkFn("deleteFile",  [](auto a){ std::filesystem::remove(a[0].toString()); return Value::makeNull(); });
    fsObj->fields["deleteDir"]   = mkFn("deleteDir",   [](auto a){ std::filesystem::remove_all(a[0].toString()); return Value::makeNull(); });
    fsObj->fields["copy"]        = mkFn("copy",        [](auto a){ std::filesystem::copy(a[0].toString(), a[1].toString()); return Value::makeNull(); });
    fsObj->fields["move"]        = mkFn("move",        [](auto a){ std::filesystem::rename(a[0].toString(), a[1].toString()); return Value::makeNull(); });
    fsObj->fields["readText"]    = mkFn("readText",    [](auto a) -> Value {
        std::ifstream f(a[0].toString());
        if (!f) return Value::makeString("");
        std::ostringstream o; o << f.rdbuf(); return Value::makeString(o.str());
    });
    fsObj->fields["listDir"]     = mkFn("listDir",     [](auto a) -> Value {
        auto arr = std::make_shared<ArrayInstance>();
        for (auto& e : std::filesystem::directory_iterator(a[0].toString()))
            arr->elements.push_back(Value::makeString(e.path().filename().string()));
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "List";
        obj->fields["_data"] = Value::makeArray(arr);
        return Value::makeObject(obj);
    });
    fsObj->fields["fileSize"]    = mkFn("fileSize",  [](auto a){ return Value::makeInt(static_cast<int64_t>(std::filesystem::file_size(a[0].toString()))); });
    fsObj->fields["extension"]   = mkFn("extension", [](auto a){ return Value::makeString(std::filesystem::path(a[0].toString()).extension().string()); });
    fsObj->fields["filename"]    = mkFn("filename",  [](auto a){ return Value::makeString(std::filesystem::path(a[0].toString()).filename().string()); });
    fsObj->fields["parentDir"]   = mkFn("parentDir", [](auto a){ return Value::makeString(std::filesystem::path(a[0].toString()).parent_path().string()); });
    fsObj->fields["join"]        = mkFn("join",      [](auto a){ return Value::makeString((std::filesystem::path(a[0].toString()) / a[1].toString()).string()); });
    
    // Add writeText implementation
    fsObj->fields["writeText"]   = mkFn("writeText", [](auto a) -> Value {
        if (a.size() < 2) return Value::makeNull();
        std::ofstream f(a[0].toString());
        if (f) f << a[1].toString();
        return Value::makeNull();
    });

    env->declare("FileSystem", Value::makeObject(fsObj));
}

void Evaluator::registerStringUtils(std::shared_ptr<Environment> env) {
    auto suObj = std::make_shared<ObjectInstance>();
    suObj->className = "StringUtils";

    auto mkFn = [](const std::string& name, std::function<Value(std::vector<Value>)> fn) {
        auto fi = std::make_shared<FunctionInstance>();
        fi->name = name; fi->native = std::move(fn);
        return Value::makeFunction(fi);
    };

    suObj->fields["parseInt"]    = mkFn("parseInt",    [](auto a) -> Value {
        try { return Value::makeInt(std::stoll(a[0].toString())); }
        catch (...) { throw UmeRuntimeException(Value::makeString("Cannot parse int: " + a[0].toString())); }
    });
    suObj->fields["parseDouble"] = mkFn("parseDouble", [](auto a) -> Value {
        try { return Value::makeFloat(std::stod(a[0].toString())); }
        catch (...) { throw UmeRuntimeException(Value::makeString("Cannot parse double: " + a[0].toString())); }
    });
    suObj->fields["parseBool"]   = mkFn("parseBool",   [](auto a) -> Value {
        std::string s = a[0].toString();
        if (s == "true" || s == "1" || s == "yes") return Value::makeBool(true);
        if (s == "false"|| s == "0" || s == "no")  return Value::makeBool(false);
        throw UmeRuntimeException(Value::makeString("Cannot parse bool: " + s));
    });
    suObj->fields["isInt"]       = mkFn("isInt",       [](auto a) -> Value {
        try { std::stoll(a[0].toString()); return Value::makeBool(true); } catch (...) { return Value::makeBool(false); }
    });
    suObj->fields["isNumber"]    = mkFn("isNumber",    [](auto a) -> Value {
        try { std::stod(a[0].toString()); return Value::makeBool(true); } catch (...) { return Value::makeBool(false); }
    });
    suObj->fields["repeat"]      = mkFn("repeat",      [](auto a) -> Value {
        std::string s = a[0].toString();
        int64_t n = a[1].intVal;
        std::string res; res.reserve(s.size() * static_cast<size_t>(n));
        for (int64_t i = 0; i < n; i++) res += s;
        return Value::makeString(res);
    });
    suObj->fields["padLeft"]     = mkFn("padLeft",     [](auto a) -> Value {
        std::string s = a[0].toString();
        int64_t width = a[1].intVal;
        char pad = a.size() > 2 && a[2].isChar() ? a[2].charVal : ' ';
        while ((int64_t)s.size() < width) s = pad + s;
        return Value::makeString(s);
    });
    suObj->fields["padRight"]    = mkFn("padRight",    [](auto a) -> Value {
        std::string s = a[0].toString();
        int64_t width = a[1].intVal;
        char pad = a.size() > 2 && a[2].isChar() ? a[2].charVal : ' ';
        while ((int64_t)s.size() < width) s += pad;
        return Value::makeString(s);
    });
    suObj->fields["join"]        = mkFn("join",        [](auto a) -> Value {
        std::string sep = a.size() > 1 ? a[1].toString() : "";
        std::string result;
        auto appendElems = [&](const std::vector<Value>& elems) {
            bool first = true;
            for (auto& e : elems) { if (!first) result += sep; result += e.toString(); first = false; }
        };
        if (a[0].isObject() && a[0].objVal) {
            auto it = a[0].objVal->fields.find("_data");
            if (it != a[0].objVal->fields.end() && it->second.isArray())
                appendElems(it->second.arrVal->elements);
        } else if (a[0].isArray()) {
            appendElems(a[0].arrVal->elements);
        }
        return Value::makeString(result);
    });

    suObj->fields["format"] = mkFn("format", [](std::vector<Value> a) -> Value {
        if (a.empty()) return Value::makeString("");
        std::string tmpl = a[0].toString();
        std::string result;
        size_t pos = 0;
        int argIdx = 1;
        while (pos < tmpl.size()) {
            if (tmpl[pos] == '{' && pos + 1 < tmpl.size() && tmpl[pos+1] == '}') {
                if (argIdx < static_cast<int>(a.size())) result += a[argIdx].toString();
                argIdx++;
                pos += 2;
            } else if (tmpl[pos] == '{' && pos + 2 < tmpl.size() && tmpl[pos+2] == '}') {
                // {0}, {1}, etc.
                int n = tmpl[pos+1] - '0';
                if (n + 1 < static_cast<int>(a.size())) result += a[n + 1].toString();
                argIdx++;
                pos += 3;
            } else {
                result += tmpl[pos];
                pos++;
            }
        }
        return Value::makeString(result);
    });

    env->declare("StringUtils", Value::makeObject(suObj));
}

void Evaluator::registerConsole(std::shared_ptr<Environment> env) {
    auto consoleObj = std::make_shared<ObjectInstance>();
    consoleObj->className = "Console";

    auto printlnFn = std::make_shared<FunctionInstance>();
    printlnFn->name = "println";
    printlnFn->native = [](std::vector<Value> args) -> Value {
        if (args.empty()) { std::cout << '\n' << std::flush; }
        else              { std::cout << args[0].toString() << '\n' << std::flush; }
        return Value::makeNull();
    };
    consoleObj->fields["println"] = Value::makeFunction(printlnFn);

    auto printFn = std::make_shared<FunctionInstance>();
    printFn->name = "print";
    printFn->native = [](std::vector<Value> args) -> Value {
        if (!args.empty()) std::cout << args[0].toString() << std::flush;
        return Value::makeNull();
    };
    consoleObj->fields["print"] = Value::makeFunction(printFn);
    consoleObj->fields["Write"] = Value::makeFunction(printFn);   // C# alias
    consoleObj->fields["WriteLine"] = Value::makeFunction(printlnFn); // C# alias

    auto readLineFn = std::make_shared<FunctionInstance>();
    readLineFn->name = "readLine";
    readLineFn->native = [](std::vector<Value> args) -> Value {
        if (!args.empty()) std::cout << args[0].toString();
        std::string line;
        std::getline(std::cin, line);
        return Value::makeString(line);
    };
    consoleObj->fields["readLine"] = Value::makeFunction(readLineFn);

    auto clearFn = std::make_shared<FunctionInstance>();
    clearFn->name = "clear";
    clearFn->native = [](std::vector<Value>) -> Value {
        std::cout << "\x1b[2J\x1b[H" << std::flush;
        return Value::makeNull();
    };
    consoleObj->fields["clear"] = Value::makeFunction(clearFn);

    env->declare("Console", Value::makeObject(consoleObj));
}

Value Evaluator::evalBlock(const BlockStmt& block, std::shared_ptr<Environment> env) {
    for (auto& stmt : block.stmts)
        eval(*stmt, env);
    return Value::makeNull();
}

Value Evaluator::evalVarDecl(const VarDeclStmt& stmt, std::shared_ptr<Environment> env) {
    Value val = stmt.initializer ? eval(*stmt.initializer, env) : Value::makeNull();
    
    // If the initializer is an anonymous struct literal, tag it with the declared type
    if (val.isObject() && val.objVal && val.objVal->className == "<struct>") {
        if (structDefs_.count(stmt.type.name))
            val.objVal->className = stmt.type.name;
    }

    if (env->has("__this")) {
        Value& tv = env->get("__this");
        if (tv.isObject() && tv.objVal) {
            auto& fields = tv.objVal->fields;
            if (fields.count(stmt.name)) { 
                fields[stmt.name] = val; 
                return Value::makeNull(); 
            }
        }
    }
    // Check if it's a static field
    if (env->has("__class__")) {
        Value& cv = env->get("__class__");
        if (cv.isString()) {
            std::string qk = cv.strVal + "." + stmt.name;
            if (global_->has(qk)) { 
                global_->assign(qk, val); 
                return Value::makeNull(); 
            }
        }
    }

    env->declare(stmt.name, val);
    return Value::makeNull();
}

Value Evaluator::evalIf(const IfStmt& stmt, std::shared_ptr<Environment> env) {
    Value cond = eval(*stmt.condition, env);
    if (isTruthy(cond))      eval(*stmt.thenBranch, env);
    else if (stmt.elseBranch) eval(*stmt.elseBranch, env);
    return Value::makeNull();
}

Value Evaluator::evalWhile(const WhileStmt& stmt, std::shared_ptr<Environment> env) {
    while (isTruthy(eval(*stmt.condition, env))) {
        try { eval(*stmt.body, env); }
        catch (const BreakSignal&)    { break; }
        catch (const ContinueSignal&) { continue; }
    }
    return Value::makeNull();
}

Value Evaluator::evalDoWhile(const DoWhileStmt& stmt, std::shared_ptr<Environment> env) {
    do {
        try { eval(*stmt.body, env); }
        catch (const BreakSignal&)    { break; }
        catch (const ContinueSignal&) {}
    } while (isTruthy(eval(*stmt.condition, env)));
    return Value::makeNull();
}

Value Evaluator::evalFor(const ForStmt& stmt, std::shared_ptr<Environment> env) {
    auto forEnv = env->child();
    if (stmt.init) eval(*stmt.init, forEnv);
    while (true) {
        if (stmt.condition && !isTruthy(eval(*stmt.condition, forEnv))) break;
        try { eval(*stmt.body, forEnv); }
        catch (const BreakSignal&)    { break; }
        catch (const ContinueSignal&) {}
        if (stmt.update) eval(*stmt.update, forEnv);
    }
    return Value::makeNull();
}

Value Evaluator::evalForEach(const ForEachStmt& stmt, std::shared_ptr<Environment> env) {
    Value iterable = eval(*stmt.iterable, env);

    auto doIteration = [&](auto& elements) {
        for (auto& elem : elements) {
            auto loopEnv = env->child();
            loopEnv->declare(stmt.varName, elem);
            try { eval(*stmt.body, loopEnv); }
            catch (const BreakSignal&)    { break; }
            catch (const ContinueSignal&) { continue; }
        }
    };

    if (iterable.isArray()) {
        doIteration(iterable.arrVal->elements);
    } else if (iterable.isObject() && iterable.objVal) {
        auto& fields = iterable.objVal->fields;
        // List<T> or Set<T> — access _data field
        auto it = fields.find("_data");
        if (it != fields.end() && it->second.isArray())
            doIteration(it->second.arrVal->elements);
        else if (iterable.objVal->className == "Map") {
            // Iterate over Map entries — bind varName to each key
            for (auto& [k, v] : fields) {
                if (k == "_data" || k.rfind("_", 0) == 0) continue;
                auto loopEnv = env->child();
                loopEnv->declare(stmt.varName, Value::makeString(k));
                try { eval(*stmt.body, loopEnv); }
                catch (const BreakSignal&) { break; }
                catch (const ContinueSignal&) { continue; }
            }
        }
    } else if (iterable.isString()) {
        for (char c : iterable.strVal) {
            auto loopEnv = env->child();
            loopEnv->declare(stmt.varName, Value::makeChar(c));
            try { eval(*stmt.body, loopEnv); }
            catch (const BreakSignal&)    { break; }
            catch (const ContinueSignal&) { continue; }
        }
    }
    return Value::makeNull();
}

Value Evaluator::evalSwitch(const SwitchStmt& stmt, std::shared_ptr<Environment> env) {
    Value switchVal = eval(*stmt.value, env);
    bool matched = false;
    for (auto& sc : stmt.cases) {
        if (!matched) {
            if (sc.isDefault) { matched = true; }
            else {
                Value caseVal = eval(*sc.value, env);
                if (switchVal == caseVal) matched = true;
            }
        }
        if (matched) {
            try {
                for (auto& s : sc.stmts) eval(*s, env);
            } catch (const BreakSignal&) { break; }
        }
    }
    return Value::makeNull();
}

Value Evaluator::evalTryCatch(const TryCatchStmt& stmt, std::shared_ptr<Environment> env) {
    auto runFinally = [&]() {
        if (stmt.finallyBody) eval(*stmt.finallyBody, env);
    };

    try {
        eval(*stmt.tryBody, env);
        runFinally();
    } catch (UmeRuntimeException& ex) {
        bool caught = false;
        for (auto& cc : stmt.catches) {
            bool matches = false;
            if (cc.type.name == "Exception" || cc.type.name == "any") {
                matches = true;
            } else if (ex.value.isObject() && ex.value.objVal) {
                std::string cn = ex.value.objVal->className;
                const ClassDecl* cls = findClass(cn);
                auto matchesName = [](const std::string& full, const std::string& shortName) {
                    if (full == shortName) return true;
                    if (full.size() > shortName.size() && full[full.size() - shortName.size() - 1] == '.' &&
                        full.compare(full.size() - shortName.size(), shortName.size(), shortName) == 0)
                        return true;
                    return false;
                };
                while (cls) {
                    if (matchesName(cls->name, cc.type.name)) { matches = true; break; }
                    if (cls->superClass) cls = findClass(cls->superClass->name);
                    else break;
                }
                if (!matches && matchesName(cn, cc.type.name)) matches = true;
            }
            if (matches) {
                auto catchEnv = env->child();
                catchEnv->declare(cc.varName, ex.value);
                try { eval(*cc.body, catchEnv); }
                catch (...) { runFinally(); throw; }
                caught = true;
                break;
            }
        }
        runFinally();
        if (!caught) throw;
    } catch (...) {
        runFinally();
        throw;
    }
    return Value::makeNull();
}

Value Evaluator::evalUnsafe(const UnsafeBlock& block, std::shared_ptr<Environment> env) {
    return eval(*block.body, env);
}

Value Evaluator::evalBinary(const BinaryExpr& expr, std::shared_ptr<Environment> env) {
    // Short-circuit logical
    if (expr.op == "&&") {
        Value l = eval(*expr.left, env);
        if (!isTruthy(l)) return Value::makeBool(false);
        return Value::makeBool(isTruthy(eval(*expr.right, env)));
    }
    if (expr.op == "||") {
        Value l = eval(*expr.left, env);
        if (isTruthy(l)) return Value::makeBool(true);
        return Value::makeBool(isTruthy(eval(*expr.right, env)));
    }

    Value l = eval(*expr.left,  env);
    Value r = eval(*expr.right, env);

    if (l.isObject() && l.objVal) {
        const ClassDecl* cls = findClass(l.objVal->className);
        if (cls) {
            for (auto& opDecl : cls->operators) {
                if (opDecl->op == expr.op && opDecl->body) {
                    auto opEnv = env->child();
                    opEnv->declare("this",   l);
                    opEnv->declare("__this", l);
                    opEnv->declare("__class__", Value::makeString(cls->name));
                    if (!opDecl->params.empty()) opEnv->declare(opDecl->params[0].name, r);
                    try { eval(*opDecl->body, opEnv); }
                    catch (const ReturnSignal& ret) { return ret.value; }
                    return Value::makeNull();
                }
            }
        }
    }

    return arith(expr.op, l, r);
}

Value Evaluator::evalUnary(const UnaryExpr& expr, std::shared_ptr<Environment> env) {
    if (expr.prefix) {
        if (expr.op == "!") {
            Value v = eval(*expr.operand, env);
            return Value::makeBool(!isTruthy(v));
        }
        if (expr.op == "-") {
            Value v = eval(*expr.operand, env);
            if (v.isInt())   return Value::makeInt(-v.intVal);
            if (v.isFloat()) return Value::makeFloat(-v.floatVal);
        }
        if (expr.op == "~") {
            Value v = eval(*expr.operand, env);
            if (v.isInt()) return Value::makeInt(~v.intVal);
        }
        if (expr.op == "++" || expr.op == "--") {
            Value& ref = resolveAssignTarget(*expr.operand, env);
            int64_t delta = (expr.op == "++") ? 1 : -1;
            if (ref.isInt())   { ref.intVal   += delta; return ref; }
            if (ref.isFloat()) { ref.floatVal += delta; return ref; }
        }
    } else {
        // Postfix ++ / --
        if (expr.op == "++" || expr.op == "--") {
            Value& ref = resolveAssignTarget(*expr.operand, env);
            Value old  = ref;
            int64_t delta = (expr.op == "++") ? 1 : -1;
            if (ref.isInt())   ref.intVal   += delta;
            if (ref.isFloat()) ref.floatVal += delta;
            return old;
        }
    }
    return eval(*expr.operand, env);
}

Value& Evaluator::resolveAssignTarget(const ASTNode& node, std::shared_ptr<Environment> env) {
    if (auto* id = dynamic_cast<const IdentifierExpr*>(&node)) {
        if (env->has(id->name)) return env->get(id->name);
        if (global_->has(id->name)) return global_->get(id->name);
        // Implicit __this field access
        if (env->has("__this")) {
            Value& tv = env->get("__this");
            if (tv.isObject() && tv.objVal) {
                auto& fields = tv.objVal->fields;
                if (fields.count(id->name)) return fields[id->name];
                // Class static field
                std::string qk = tv.objVal->className + "." + id->name;
                if (global_->has(qk)) return global_->get(qk);
            }
        }
        // Static field via __class__
        if (env->has("__class__")) {
            Value& cv = env->get("__class__");
            if (cv.isString()) {
                std::string qk = cv.strVal + "." + id->name;
                if (global_->has(qk)) return global_->get(qk);
            }
        }
        throwRuntimeError("Cannot assign to undefined: " + id->name, node);
    }
    if (auto* ma = dynamic_cast<const MemberAccessExpr*>(&node)) {
        // Static class field (ClassName.field)
        if (auto* objId = dynamic_cast<const IdentifierExpr*>(ma->object.get())) {
            auto cit = classDefs_.find(objId->name);
            if (cit != classDefs_.end()) {
                std::string key = objId->name + "." + ma->member;
                if (!global_->has(key)) global_->declare(key, Value::makeNull());
                return global_->get(key);
            }
        }
        Value& obj = resolveAssignTarget(*ma->object, env);
        if (obj.isObject() && obj.objVal) {
            // Check for property setter
            const ClassDecl* cls = obj.objVal->classDef ? obj.objVal->classDef : findClass(obj.objVal->className);
            if (cls) {
                for (auto& prop : cls->properties) {
                    if (prop->name == ma->member) {
                        if (prop->hasSet && prop->setDecl) {
                            // Call the setter
                            // (handled by evalAssign which calls this for the target)
                        }
                        if (!prop->hasSet) {
                            throwRuntimeError("Property '" + ma->member + "' is read-only", node);
                        }
                        break;
                    }
                }
            }
            // Auto-property or field — direct field access
            if (obj.objVal->fields.find(ma->member) == obj.objVal->fields.end())
                obj.objVal->fields[ma->member] = Value::makeNull();
            return obj.objVal->fields[ma->member];
        }
    }
    if (auto* ix = dynamic_cast<const IndexExpr*>(&node)) {
        Value& obj = resolveAssignTarget(*ix->object, env);
        Value idx  = eval(*ix->index, env);
        if (obj.isArray() && idx.isInt())
            return obj.arrVal->elements[static_cast<size_t>(idx.intVal)];
    }
    // Dereference assignment: *ptr = value
    if (auto* de = dynamic_cast<const DerefExpr*>(&node)) {
        Value ptr = eval(*de->pointer, env);
        if (ptr.isArray() && !ptr.arrVal->elements.empty())
            return ptr.arrVal->elements[0];
    }
    throwRuntimeError("Cannot assign to expression", node);
}

Value Evaluator::evalAssign(const AssignExpr& expr, std::shared_ptr<Environment> env) {
    Value newVal = eval(*expr.value, env);

    // For compound assignment, we need the old value
    if (expr.op != "=") {
        Value old = eval(*expr.target, env);
        std::string binOp = expr.op.substr(0, expr.op.size() - 1); // += -> +
        newVal = arith(binOp, old, newVal);
    }

    // Assign
    if (auto* id = dynamic_cast<const IdentifierExpr*>(expr.target.get())) {
        // 1. Check instance fields (this) FIRST
        if (env->has("__this")) {
            Value& tv = env->get("__this");
            if (tv.isObject() && tv.objVal) {
                auto& fields = tv.objVal->fields;
                if (fields.count(id->name)) { 
                    fields[id->name] = newVal; 
                    return newVal; 
                }
            }
        }
        // 2. Check static fields (__class__)
        if (env->has("__class__")) {
            Value& cv = env->get("__class__");
            if (cv.isString()) {
                std::string qk = cv.strVal + "." + id->name;
                if (global_->has(qk)) { 
                    global_->assign(qk, newVal); 
                    return newVal; 
                }
            }
        }
        // 3. Check local variables
        if (env->has(id->name)) { 
            env->assign(id->name, newVal); 
            return newVal; 
        }
        // 4. Check global variables
        if (global_->has(id->name)) { 
            global_->assign(id->name, newVal); 
            return newVal; 
        }
        // 5. Fallback: declare as local
        env->declare(id->name, newVal);
        return newVal;
    } else if (auto* ma = dynamic_cast<const MemberAccessExpr*>(expr.target.get())) {
        // Static class field assignment: ClassName.field = val
        if (auto* objId = dynamic_cast<const IdentifierExpr*>(ma->object.get())) {
            const ClassDecl* sCls = findClass(objId->name);
            if (sCls) {
                std::string key = sCls->name + "." + ma->member;
                if (global_->has(key)) { global_->assign(key, newVal); return newVal; }
                else { global_->declare(key, newVal); return newVal; }
            }
        }
        Value obj = eval(*ma->object, env);
        if (obj.isObject() && obj.objVal) {
            const ClassDecl* cls = findClass(obj.objVal->className);
            bool handledProp = false;
            if (cls) {
                for (auto& prop : cls->properties) {
                    if (prop->name == ma->member && prop->hasSet && prop->setDecl) {
                        auto propEnv = env->child();
                        propEnv->declare("this", obj);
                        propEnv->declare("__this", obj);
                        propEnv->declare("value", newVal);
                        eval(*prop->setDecl, propEnv);
                        handledProp = true;
                        break;
                    }
                }
            }
            if (!handledProp)
                obj.objVal->fields[ma->member] = newVal;
        }
        // also update in env if it's a named variable
        if (auto* objId = dynamic_cast<const IdentifierExpr*>(ma->object.get())) {
            if (env->has(objId->name)) env->assign(objId->name, obj);
            else if (global_->has(objId->name)) global_->assign(objId->name, obj);
        }
    } else if (auto* ix = dynamic_cast<const IndexExpr*>(expr.target.get())) {
        Value obj = eval(*ix->object, env);
        if (obj.isArray()) {
            Value idx = eval(*ix->index, env);
            if (idx.isInt())
                obj.arrVal->elements[static_cast<size_t>(idx.intVal)] = newVal;
        } else if (obj.isObject() && obj.objVal) {
            const ClassDecl* cls = findClass(obj.objVal->className);
            if (cls) {
                for (auto& indexer : cls->indexers) {
                    if (indexer->hasSet && indexer->setDecl) {
                        auto idxEnv = env->child();
                        idxEnv->declare("this", obj);
                        idxEnv->declare("__this", obj);
                        // Multi-index: if the index is an array literal, bind each element
                        if (auto* arrLit = dynamic_cast<const ArrayLiteralExpr*>(ix->index.get())) {
                            for (size_t i = 0; i < indexer->params.size() && i < arrLit->elements.size(); i++) {
                                Value v = eval(*arrLit->elements[i], env);
                                idxEnv->declare(indexer->params[i].name, v);
                            }
                        } else {
                            if (!indexer->params.empty()) idxEnv->declare(indexer->params[0].name, eval(*ix->index, env));
                        }
                        idxEnv->declare("value", newVal);
                        eval(*indexer->setDecl, idxEnv);
                        break;
                    }
                }
            }
        }
    }
    return newVal;
}

Value Evaluator::evalCall(const CallExpr& expr, std::shared_ptr<Environment> env) {
    // Member call: obj.method(args)
    if (auto* ma = dynamic_cast<const MemberAccessExpr*>(expr.callee.get())) {
        if (auto* objId = dynamic_cast<const IdentifierExpr*>(ma->object.get())) {
            // super.method(args) call
            if (objId->name == "super" && env->has("__this")) {
                Value thisObj = env->get("__this");
                if (thisObj.isObject() && thisObj.objVal) {
                    std::string curClass = thisObj.objVal->className;
                    if (env->has("__class__")) curClass = env->get("__class__").strVal;
                    auto cit = classDefs_.find(curClass);
                    if (cit != classDefs_.end() && cit->second->superClass) {
                        auto sit = classDefs_.find(cit->second->superClass->name);
                        if (sit != classDefs_.end()) {
                            const FuncDecl* m = findMethod(*sit->second, ma->member);
                            if (m && m->body) {
                                std::vector<Value> args;
                                for (auto& a : expr.args) args.push_back(eval(*a, env));
                                auto superEnv = env->child();
                                superEnv->declare("this",     thisObj);
                                superEnv->declare("__this",   thisObj);
                                superEnv->declare("__class__", Value::makeString(sit->second->name));
                                for (size_t i = 0; i < m->params.size(); i++) {
                                    auto& p = m->params[i];
                                    if (i < args.size()) superEnv->declare(p.name, args[i]);
                                    else if (p.defaultValue) superEnv->declare(p.name, eval(*p.defaultValue, superEnv));
                                    else superEnv->declare(p.name, Value::makeNull());
                                }
                                try { eval(*m->body, superEnv); }
                                catch (const ReturnSignal& r) { return r.value; }
                                return Value::makeNull();
                            }
                        }
                    }
                }
            }
            // Static class method call (ClassName.method(args))
            const ClassDecl* clsPtr = findClass(objId->name);
            if (clsPtr) {
                if (ma->member == "fromJsonString" && !expr.args.empty()) {
                    std::string jsonStr = eval(*expr.args[0], env).toString();
                    auto newObj = std::make_shared<ObjectInstance>();
                    newObj->className = clsPtr->name;
                    newObj->classDef = clsPtr;
                    instantiateFields(*newObj, *clsPtr, env);
                    for (auto& f : clsPtr->fields) {
                        size_t p = jsonStr.find("\"" + f->name + "\":");
                        if (p == std::string::npos) {
                            for (auto& attr : f->attributes) {
                                if (attr.name == "JsonProperty" && !attr.args.empty()) {
                                    if (auto* strLit = dynamic_cast<const StringLiteralExpr*>(attr.args[0].get())) {
                                        std::string key = strLit->value;
                                        p = jsonStr.find("\"" + key + "\":");
                                        if (p != std::string::npos) break;
                                    }
                                }
                            }
                        }
                        if (p != std::string::npos) {
                            size_t valStart = jsonStr.find(':', p) + 1;
                            while (valStart < jsonStr.size() && (jsonStr[valStart] == ' ' || jsonStr[valStart] == '"')) valStart++;
                            size_t valEnd = jsonStr.find_first_of(",}\"", valStart);
                            std::string rawVal = jsonStr.substr(valStart, valEnd - valStart);
                            if (f->type.name == "int" || f->type.name == "long") newObj->fields[f->name] = Value::makeInt(std::stoll(rawVal));
                            else if (f->type.name == "double" || f->type.name == "float") newObj->fields[f->name] = Value::makeFloat(std::stod(rawVal));
                            else newObj->fields[f->name] = Value::makeString(rawVal);
                        }
                    }
                    return Value::makeObject(newObj);
                }
                const FuncDecl* m = findMethod(*clsPtr, ma->member);
                if (m && m->body) {
                    std::vector<Value> args;
                    for (auto& a : expr.args) args.push_back(eval(*a, env));
                    auto staticEnv = global_->child();
                    staticEnv->declare("__class__", Value::makeString(objId->name));
                    for (size_t i = 0; i < m->params.size(); i++) {
                        auto& p = m->params[i];
                        if (p.variadic) {
                            auto arr = std::make_shared<ArrayInstance>();
                            for (size_t j = i; j < args.size(); j++) arr->elements.push_back(args[j]);
                            staticEnv->declare(p.name, Value::makeArray(arr));
                            break;
                        }
                        if (i < args.size()) staticEnv->declare(p.name, args[i]);
                        else if (p.defaultValue) staticEnv->declare(p.name, eval(*p.defaultValue, staticEnv));
                        else staticEnv->declare(p.name, Value::makeNull());
                    }
                    try { eval(*m->body, staticEnv); }
                    catch (const ReturnSignal& r) { return r.value; }
                    return Value::makeNull();
                }
            }
        }
        Value obj = eval(*ma->object, env);
        std::vector<Value> args;
        for (auto& a : expr.args) args.push_back(eval(*a, env));
        Value res = callMethod(obj, ma->member, std::move(args), env);
        if (auto* id = dynamic_cast<const IdentifierExpr*>(ma->object.get())) {
            if (env->has(id->name)) {
                env->assign(id->name, obj);
            } else if (env->has("__this")) {
                Value& tv = env->get("__this");
                if (tv.isObject() && tv.objVal) {
                    tv.objVal->fields[id->name] = obj;
                }
            }
        }
        return res;
    }

    // super(args) in constructor
    if (auto* id = dynamic_cast<const IdentifierExpr*>(expr.callee.get())) {
        if (id->name == "super") {
            if (env->has("__this")) {
                Value& thisVal = env->get("__this");
                if (thisVal.isObject() && thisVal.objVal) {
                    auto& obj = *thisVal.objVal;
                    // Use __class__ (current ctor class) for super lookup, not obj.className
                    std::string currentClass = obj.className;
                    if (env->has("__class__")) {
                        Value& cv = env->get("__class__");
                        if (cv.isString()) currentClass = cv.strVal;
                    }
                    auto cit = classDefs_.find(currentClass);
                    if (cit != classDefs_.end() && cit->second->superClass) {
                        auto& superName = cit->second->superClass->name;
                        std::vector<Value> args;
                        for (auto& a : expr.args) args.push_back(eval(*a, env));
                        auto sit = classDefs_.find(superName);
                        if (sit != classDefs_.end()) {
                            runConstructor(obj, *sit->second, std::move(args), env);
                        } else if (!args.empty()) {
                            // Unknown base class (e.g. Exception): store first arg as message
                            obj.fields["message"] = args[0];
                        }
                    }
                }
            }
            return Value::makeNull();
        }
        // this(args) — constructor delegation to another constructor of the same class
        if (id->name == "this" && env->has("__this")) {
            Value& thisVal = env->get("__this");
            if (thisVal.isObject() && thisVal.objVal) {
                auto& obj = *thisVal.objVal;
                std::string currentClass = obj.className;
                if (env->has("__class__")) {
                    Value& cv = env->get("__class__");
                    if (cv.isString()) currentClass = cv.strVal;
                }
                auto cit = classDefs_.find(currentClass);
                if (cit != classDefs_.end()) {
                    std::vector<Value> args;
                    for (auto& a : expr.args) args.push_back(eval(*a, env));
                    runConstructor(obj, *cit->second, std::move(args), env);
                }
            }
            return Value::makeNull();
        }
    }

    if (auto* cid = dynamic_cast<const IdentifierExpr*>(expr.callee.get())) {
        bool isLocalOrGlobal = env->has(cid->name) || global_->has(cid->name);
        bool isClassField = false;
        
        if (env->has("__class__")) {
            Value& cv = env->get("__class__");
            if (cv.isString()) {
                std::string qk = cv.strVal + "." + cid->name;
                if (global_->has(qk)) isClassField = true;
            }
        }
        
        if (!isLocalOrGlobal || isClassField) {
            // Unqualified call in static context via __class__
            if (env->has("__class__")) {
                Value& cv = env->get("__class__");
                if (cv.isString()) {
                    auto cit = classDefs_.find(cv.strVal);
                    if (cit != classDefs_.end()) {
                        const FuncDecl* m = findMethod(*cit->second, cid->name);
                        if (m && m->body) {
                            std::vector<Value> args;
                            for (auto& a : expr.args) args.push_back(eval(*a, env));
                            auto sEnv = env->child();
                            for (size_t i = 0; i < m->params.size(); i++) {
                                auto& p = m->params[i];
                                if (p.variadic) {
                                    auto arr = std::make_shared<ArrayInstance>();
                                    for (size_t j = i; j < args.size(); j++) arr->elements.push_back(args[j]);
                                    sEnv->declare(p.name, Value::makeArray(arr));
                                    break;
                                }
                                if (i < args.size()) sEnv->declare(p.name, args[i]);
                                else if (p.defaultValue) sEnv->declare(p.name, eval(*p.defaultValue, sEnv));
                                else sEnv->declare(p.name, Value::makeNull());
                            }
                            try { eval(*m->body, sEnv); }
                            catch (const ReturnSignal& r) { return r.value; }
                            return Value::makeNull();
                        }
                    }
                }
            }
            // Unqualified call in instance context via __this
            if (env->has("__this")) {
                Value& tv = env->get("__this");
                if (tv.isObject() && tv.objVal) {
                    const ClassDecl* cls = findClass(tv.objVal->className);
                    if (cls && findMethod(*cls, cid->name)) {
                        std::vector<Value> args;
                        for (auto& a : expr.args) args.push_back(eval(*a, env));
                        return callMethod(tv, cid->name, std::move(args), env);
                    }
                }
            }
        }
    }

    // Static Class.fromJsonString(json)
    if (auto* ma = dynamic_cast<const MemberAccessExpr*>(expr.callee.get())) {
        if (auto* objId = dynamic_cast<const IdentifierExpr*>(ma->object.get())) {
            const ClassDecl* cls = findClass(objId->name);
            if (ma->member == "fromJsonString" && cls) {
                std::string jsonStr = eval(*expr.args[0], env).toString();
                auto newObj = std::make_shared<ObjectInstance>();
                newObj->className = cls->name;
                newObj->classDef = cls;
                instantiateFields(*newObj, *cls, env);
                for (auto& f : cls->fields) {
                    size_t p = jsonStr.find("\"" + f->name + "\":");
                    if (p == std::string::npos) {
                        for (auto& attr : f->attributes) {
                            if (attr.name == "JsonProperty" && !attr.args.empty()) {
                                if (auto* strLit = dynamic_cast<const StringLiteralExpr*>(attr.args[0].get())) {
                                    std::string key = strLit->value;
                                    p = jsonStr.find("\"" + key + "\":");
                                    if (p != std::string::npos) break;
                                }
                            }
                        }
                    }
                    if (p != std::string::npos) {
                        size_t valStart = jsonStr.find(':', p) + 1;
                        while (valStart < jsonStr.size() && (jsonStr[valStart] == ' ' || jsonStr[valStart] == '"')) valStart++;
                        size_t valEnd = jsonStr.find_first_of(",}\"", valStart);
                        std::string rawVal = jsonStr.substr(valStart, valEnd - valStart);
                        if (f->type.name == "int" || f->type.name == "long") newObj->fields[f->name] = Value::makeInt(std::stoll(rawVal));
                        else if (f->type.name == "double" || f->type.name == "float") newObj->fields[f->name] = Value::makeFloat(std::stod(rawVal));
                        else newObj->fields[f->name] = Value::makeString(rawVal);
                    }
                }
                return Value::makeObject(newObj);
            }
        }
    }

    Value callee = eval(*expr.callee, env);
    std::vector<Value> args;
    for (auto& a : expr.args) args.push_back(eval(*a, env));

    if (callee.isFunction()) {
        return callFunction(*callee.funcVal, std::move(args));
    }

    // Constructor call (class name used as function)
    if (auto* id = dynamic_cast<const IdentifierExpr*>(expr.callee.get())) {
        auto cit = classDefs_.find(id->name);
        if (cit != classDefs_.end()) {
            return evalNew(NewExpr(TypeAnnotation(id->name), {}), env);
        }
    }

    throwRuntimeError("Not callable: " + callee.toString(), expr);
}

Value Evaluator::evalMemberAccess(const MemberAccessExpr& expr, std::shared_ptr<Environment> env) {
    if (auto* id = dynamic_cast<const IdentifierExpr*>(expr.object.get())) {
        if (id->name == "super" && env->has("__this")) {
            Value thisObj = env->get("__this");
            if (thisObj.isObject() && thisObj.objVal) {
                std::string curClass = thisObj.objVal->className;
                if (env->has("__class__")) curClass = env->get("__class__").strVal;
                auto cit = classDefs_.find(curClass);
                if (cit != classDefs_.end() && cit->second->superClass) {
                    auto sit = classDefs_.find(cit->second->superClass->name);
                    if (sit != classDefs_.end()) {
                        const FuncDecl* method = findMethod(*sit->second, expr.member);
                        if (method) {
                            auto fi = std::make_shared<FunctionInstance>();
                            fi->name   = method->name;
                            fi->params = method->params;
                            fi->body   = method->body.get();
                            auto superEnv = env->child();
                            superEnv->declare("this",   thisObj);
                            superEnv->declare("__this", thisObj);
                            superEnv->declare("__class__", Value::makeString(sit->second->name));
                            fi->closure = superEnv;
                            return Value::makeFunction(fi);
                        }
                    }
                }
            }
        }
        // Enum value
        const EnumDecl* eit = findEnum(id->name);
        if (eit) {
            int64_t idx = 0;
            for (auto& ev : eit->values) {
                if (ev.name == expr.member) {
                    int64_t val = idx;
                    if (ev.value) {
                        Value v = eval(*ev.value, env);
                        if (v.isInt()) val = v.intVal;
                    }
                    auto eo = std::make_shared<ObjectInstance>();
                    eo->className = eit->name;
                    eo->fields["__enumName"] = Value::makeString(ev.name);
                    eo->fields["__enumValue"] = Value::makeInt(val);
                    return Value::makeObject(eo);
                }
                idx++;
            }
        }
        // Static field on class (not in env — look in global_)
        const ClassDecl* sCls = findClass(id->name);
        if (sCls) {
            std::string qKey = sCls->name + "." + expr.member;
            if (global_->has(qKey))
                return global_->get(qKey);
            if (global_->has(id->name + "." + expr.member))
                return global_->get(id->name + "." + expr.member);
        }
        // Interface constant access: InterfaceName.CONST
        auto iit = ifaceDefs_.find(id->name);
        if (iit != ifaceDefs_.end()) {
            if (global_->has(id->name + "." + expr.member))
                return global_->get(id->name + "." + expr.member);
            return Value::makeNull();
        }
    }

    Value obj = eval(*expr.object, env);

    if (obj.isNull()) {
        if (expr.safe) return Value::makeNull();
        throwRuntimeError("Null pointer dereference on member '" + expr.member + "'", expr);
    }
    // Safe navigation on non-null object: proceed normally.
    if (obj.isObject() && obj.objVal) {
        auto& fields = obj.objVal->fields;
        auto fit = fields.find(expr.member);
        if (fit != fields.end()) return fit->second;
        // Method (return a bound method as function)
        const ClassDecl* cls = findClass(obj.objVal->className);
        if (cls) {
            // Check property getter
            for (auto& prop : cls->properties) {
                if (prop->name == expr.member && prop->hasGet && prop->getDecl) {
                    auto propEnv = env->child();
                    propEnv->declare("this",   obj);
                    propEnv->declare("__this", obj);
                    propEnv->declare("__class__", Value::makeString(cls->name));
                    try {
                        Value res = eval(*prop->getDecl, propEnv);
                        if (!res.isNull()) return res;
                    } catch (const ReturnSignal& r) { return r.value; }
                    return Value::makeNull();
                }
            }
            const FuncDecl* method = findMethod(*cls, expr.member);
            if (method) {
                auto fi    = std::make_shared<FunctionInstance>();
                fi->name   = method->name;
                fi->params = method->params;
                fi->body   = method->body.get();
                // Capture 'this' in closure
                auto thisEnv = env->child();
                thisEnv->declare("this",   obj);
                thisEnv->declare("__this", obj);
                thisEnv->declare("__class__", Value::makeString(obj.objVal->className));
                fi->closure = thisEnv;
                return Value::makeFunction(fi);
            }
        }
        // toJsonString built-in method
        if (expr.member == "toJsonString") {
            auto objValCopy = obj.objVal;
            auto clsPtr = cls;
            auto fi = std::make_shared<FunctionInstance>();
            fi->name = "toJsonString";
            fi->native = [objValCopy, clsPtr, this](std::vector<Value>) -> Value {
                std::ostringstream json;
                json << "{";
                bool first = true;
                for (auto& [name, val] : objValCopy->fields) {
                    if (name.rfind("_", 0) == 0) continue;
                    std::string jsonKey = name;
                    if (clsPtr) {
                        const FieldDecl* fd = findField(*clsPtr, name);
                        if (fd) {
                            for (auto& attr : fd->attributes) {
                                if (attr.name == "JsonProperty" && !attr.args.empty()) {
                                    if (auto* sl = dynamic_cast<const StringLiteralExpr*>(attr.args[0].get()))
                                        jsonKey = sl->value;
                                }
                            }
                        }
                    }
                    if (!first) json << ",";
                    json << "\"" << jsonKey << "\":";
                    if (val.isString()) json << "\"" << val.strVal << "\"";
                    else if (val.isBool()) json << (val.boolVal ? "true" : "false");
                    else if (val.isNull()) json << "null";
                    else if (val.isObject() && val.objVal) {
                        auto nit = val.objVal->fields.find("__enumName");
                        if (nit != val.objVal->fields.end() && nit->second.isString())
                            json << "\"" << nit->second.strVal << "\"";
                        else
                            json << val.toString();
                    }
                    else json << val.toString();
                    first = false;
                }
                json << "}";
                return Value::makeString(json.str());
            };
            return Value::makeFunction(fi);
        }
        // Struct methods
        auto sit = structDefs_.find(obj.objVal->className);
        if (sit != structDefs_.end()) {
            for (auto& sm : sit->second->methods) {
                if (sm->name == expr.member) {
                    auto fi = std::make_shared<FunctionInstance>();
                    fi->name   = sm->name;
                    fi->params = sm->params;
                    fi->body   = sm->body.get();
                    auto thisEnv = env->child();
                    thisEnv->declare("this",   obj);
                    thisEnv->declare("__this", obj);
                    fi->closure = thisEnv;
                    return Value::makeFunction(fi);
                }
            }
        }
        // fallthrough: check toString/length below
    }

    if (obj.isString()) {
        if (expr.member == "length") return Value::makeInt(static_cast<int64_t>(obj.strVal.size()));
    }

    if (obj.isArray()) {
        if (expr.member == "length" || expr.member == "size")
            return Value::makeInt(static_cast<int64_t>(obj.arrVal->elements.size()));
    }

    return Value::makeNull();
}

Value Evaluator::evalIndex(const IndexExpr& expr, std::shared_ptr<Environment> env) {
    Value obj = eval(*expr.object, env);
    Value idx = eval(*expr.index, env);

    if (obj.isArray()) {
        if (!idx.isInt()) throwRuntimeError("Array index must be integer", expr);
        size_t i = static_cast<size_t>(idx.intVal);
        if (i >= obj.arrVal->elements.size())
            throwRuntimeError("Array index out of bounds: " + std::to_string(i), expr);
        return obj.arrVal->elements[i];
    }
    if (obj.isString()) {
        if (!idx.isInt()) throwRuntimeError("String index must be integer", expr);
        size_t i = static_cast<size_t>(idx.intVal);
        if (i >= obj.strVal.size())
            throwRuntimeError("String index out of bounds", expr);
        return Value::makeChar(obj.strVal[i]);
    }
    if (obj.isObject() && obj.objVal) {
        const ClassDecl* cls = findClass(obj.objVal->className);
        if (cls) {
            for (auto& indexer : cls->indexers) {
                if (indexer->hasGet && indexer->getDecl) {
                    auto idxEnv = env->child();
                    idxEnv->declare("this",   obj);
                    idxEnv->declare("__this", obj);
                    // Multi-index: if the index expression is an array literal,
                    // evaluate each element and bind to each param
                    if (auto* arrLit = dynamic_cast<const ArrayLiteralExpr*>(expr.index.get())) {
                        for (size_t i = 0; i < indexer->params.size() && i < arrLit->elements.size(); i++) {
                            Value v = eval(*arrLit->elements[i], env);
                            idxEnv->declare(indexer->params[i].name, v);
                        }
                    } else {
                        if (!indexer->params.empty()) idxEnv->declare(indexer->params[0].name, idx);
                    }
                    try { eval(*indexer->getDecl, idxEnv); }
                    catch (const ReturnSignal& r) { return r.value; }
                    return Value::makeNull();
                }
            }
        }
    }
    return Value::makeNull();
}

Value Evaluator::evalNew(const NewExpr& expr, std::shared_ptr<Environment> env) {
    // Helper — create a native function value
    auto mkFn = [](const std::string& name, std::function<Value(std::vector<Value>)> fn) -> Value {
        auto fi = std::make_shared<FunctionInstance>();
        fi->name = name; fi->native = std::move(fn);
        return Value::makeFunction(fi);
    };

    // ── Built-in collections
    if (expr.type.name == "List") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "List";
        obj->fields["_data"] = Value::makeArray(std::make_shared<ArrayInstance>());
        return Value::makeObject(obj);
    }
    if (expr.type.name == "Map") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "Map";
        return Value::makeObject(obj);
    }
    if (expr.type.name == "Set") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "Set";
        obj->fields["_data"] = Value::makeArray(std::make_shared<ArrayInstance>());
        return Value::makeObject(obj);
    }
    if (expr.type.name == "Stack") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "List"; // reuse List methods (push/pop/peek via _data)
        obj->fields["_data"] = Value::makeArray(std::make_shared<ArrayInstance>());
        return Value::makeObject(obj);
    }
    if (expr.type.name == "Queue") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "List"; // reuse List methods (enqueue/dequeue/front via _data)
        obj->fields["_data"] = Value::makeArray(std::make_shared<ArrayInstance>());
        return Value::makeObject(obj);
    }

    // ── StringBuilder (native, always works even without prelude)
    if (expr.type.name == "StringBuilder") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "StringBuilder";
        auto buf = std::make_shared<std::string>();
        obj->fields["append"] = mkFn("append", [buf](std::vector<Value> args) -> Value {
            if (!args.empty()) *buf += args[0].toString();
            auto chainFi = std::make_shared<FunctionInstance>();
            chainFi->name = "append";
            chainFi->native = [buf](std::vector<Value> a) -> Value {
                if (!a.empty()) *buf += a[0].toString();
                return Value::makeNull();
            };
            return Value::makeFunction(chainFi);
        });
        obj->fields["appendLine"] = mkFn("appendLine", [buf](std::vector<Value> args) -> Value {
            if (!args.empty()) *buf += args[0].toString();
            *buf += "\n";
            auto chainFi = std::make_shared<FunctionInstance>();
            chainFi->name = "appendLine";
            chainFi->native = [buf](std::vector<Value> a) -> Value {
                if (!a.empty()) *buf += a[0].toString();
                *buf += "\n";
                return Value::makeNull();
            };
            return Value::makeFunction(chainFi);
        });
        obj->fields["clear"] = mkFn("clear", [buf](std::vector<Value>) -> Value {
            buf->clear();
            return Value::makeNull();
        });
        obj->fields["length"] = mkFn("length", [buf](std::vector<Value>) -> Value {
            return Value::makeInt(static_cast<int64_t>(buf->size()));
        });
        obj->fields["toString"] = mkFn("toString", [buf](std::vector<Value>) -> Value {
            return Value::makeString(*buf);
        });
        return Value::makeObject(obj);
    }

    // ── Pair (native)
    if (expr.type.name == "Pair") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "Pair";
        obj->fields["first"]  = expr.args.size() > 0 ? eval(*expr.args[0], env) : Value::makeNull();
        obj->fields["second"] = expr.args.size() > 1 ? eval(*expr.args[1], env) : Value::makeNull();
        auto firstVal  = obj->fields["first"];
        auto secondVal = obj->fields["second"];
        obj->fields["toString"] = mkFn("toString", [firstVal, secondVal](std::vector<Value>) -> Value {
            return Value::makeString("(" + firstVal.toString() + ", " + secondVal.toString() + ")");
        });
        return Value::makeObject(obj);
    }

    // ── Random
    if (expr.type.name == "Random") {
        uint64_t seed;
        if (!expr.args.empty()) {
            Value sv = eval(*expr.args[0], env);
            if (sv.isNull()) seed = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
            else seed = static_cast<uint64_t>(sv.isInt() ? sv.intVal : static_cast<int64_t>(sv.toDouble()));
        } else {
            seed = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        }
        auto rng = std::make_shared<std::mt19937_64>(seed);
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "Random";
        obj->fields["nextInt"]    = mkFn("nextInt",    [rng](std::vector<Value> args) -> Value {
            if (args.empty()) return Value::makeInt(static_cast<int64_t>((*rng)() & INT64_MAX));
            if (args.size() == 1) {
                int64_t mx = args[0].intVal; if (mx <= 1) return Value::makeInt(0);
                std::uniform_int_distribution<int64_t> d(0, mx - 1);
                return Value::makeInt(d(*rng));
            }
            std::uniform_int_distribution<int64_t> d(args[0].intVal, args[1].intVal - 1);
            return Value::makeInt(d(*rng));
        });
        // nextIntMax(max)  — [0, max)
        obj->fields["nextIntMax"] = mkFn("nextIntMax", [rng](std::vector<Value> args) -> Value {
            int64_t mx = args.empty() ? 2 : args[0].intVal;
            if (mx <= 1) return Value::makeInt(0);
            std::uniform_int_distribution<int64_t> d(0, mx - 1);
            return Value::makeInt(d(*rng));
        });
        // nextIntRange(min, max)  — [min, max)
        obj->fields["nextIntRange"] = mkFn("nextIntRange", [rng](std::vector<Value> args) -> Value {
            int64_t lo = args.size() > 0 ? args[0].intVal : 0;
            int64_t hi = args.size() > 1 ? args[1].intVal : lo + 1;
            if (hi <= lo) return Value::makeInt(lo);
            std::uniform_int_distribution<int64_t> d(lo, hi - 1);
            return Value::makeInt(d(*rng));
        });
        obj->fields["nextDouble"] = mkFn("nextDouble", [rng](std::vector<Value>) -> Value {
            std::uniform_real_distribution<double> d(0.0, 1.0);
            return Value::makeFloat(d(*rng));
        });
        // nextDoubleRange(min, max)  — [min, max)
        obj->fields["nextDoubleRange"] = mkFn("nextDoubleRange", [rng](std::vector<Value> args) -> Value {
            double lo = args.size() > 0 ? args[0].toDouble() : 0.0;
            double hi = args.size() > 1 ? args[1].toDouble() : 1.0;
            std::uniform_real_distribution<double> d(lo, hi);
            return Value::makeFloat(d(*rng));
        });
        obj->fields["nextBool"]   = mkFn("nextBool",   [rng](std::vector<Value>) -> Value {
            return Value::makeBool((*rng)() & 1);
        });
        // shuffleInts(arr)  — Fisher-Yates in-place on an int array
        obj->fields["shuffleInts"] = mkFn("shuffleInts", [rng](std::vector<Value> args) -> Value {
            if (args.empty() || !args[0].isArray()) return Value::makeNull();
            auto& elems = args[0].arrVal->elements;
            for (size_t i = elems.size(); i > 1; --i) {
                std::uniform_int_distribution<size_t> d(0, i - 1);
                std::swap(elems[i - 1], elems[d(*rng)]);
            }
            return Value::makeNull();
        });
        return Value::makeObject(obj);
    }

    // ── File
    if (expr.type.name == "File") {
        std::string path = expr.args.empty() ? "" : eval(*expr.args[0], env).toString();
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "File";
        obj->fields["path"] = Value::makeString(path);
        obj->fields["readAll"]   = mkFn("readAll", [path](std::vector<Value>) -> Value {
            std::ifstream f(path);
            if (!f.good()) throw UmeRuntimeException(Value::makeString("Cannot open file: " + path));
            std::ostringstream buf; buf << f.rdbuf();
            return Value::makeString(buf.str());
        });
        obj->fields["readLines"] = mkFn("readLines", [path](std::vector<Value>) -> Value {
            std::ifstream f(path);
            if (!f.good()) throw UmeRuntimeException(Value::makeString("Cannot open file: " + path));
            auto arr = std::make_shared<ArrayInstance>();
            std::string line;
            while (std::getline(f, line)) arr->elements.push_back(Value::makeString(line));
            auto o = std::make_shared<ObjectInstance>();
            o->className = "List"; o->fields["_data"] = Value::makeArray(arr);
            return Value::makeObject(o);
        });
        obj->fields["write"]     = mkFn("write", [path](std::vector<Value> args) -> Value {
            std::ofstream f(path);
            if (!f.good()) throw UmeRuntimeException(Value::makeString("Cannot write file: " + path));
            f << (args.empty() ? "" : args[0].toString());
            return Value::makeNull();
        });
        obj->fields["append"]    = mkFn("append", [path](std::vector<Value> args) -> Value {
            std::ofstream f(path, std::ios::app);
            if (!f.good()) throw UmeRuntimeException(Value::makeString("Cannot open file: " + path));
            f << (args.empty() ? "" : args[0].toString());
            return Value::makeNull();
        });
        obj->fields["exists"]    = mkFn("exists",    [path](std::vector<Value>) -> Value {
            return Value::makeBool(std::filesystem::exists(path));
        });
        obj->fields["delete"]    = mkFn("delete",    [path](std::vector<Value>) -> Value {
            std::filesystem::remove(path);
            return Value::makeNull();
        });
        return Value::makeObject(obj);
    }

    // ── Thread / Mutex / Atomic / Audio Native Classes
    if (expr.type.name == "Thread") {
        FuncPtr task = expr.args.empty() ? nullptr : eval(*expr.args[0], env).funcVal;
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "Thread";
        auto tHandle = std::make_shared<std::thread>();
        obj->fields["sleep"] = mkFn("sleep", [](std::vector<Value> args) -> Value {
            int64_t ms = args.empty() ? 0 : args[0].intVal;
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            return Value::makeNull();
        });
        obj->fields["start"] = mkFn("start", [tHandle, task, this](std::vector<Value>) -> Value {
            // Do not ask, I have no idea why it does not work and neither do I know how to fix it so I will just leave it like this.
            /*
            if (task) {
                *tHandle = std::thread([this, task]() {
                    try { callFunction(*task, {}); }
                    catch (...) { }
                });
            }
            */
            return Value::makeNull();
        });
        obj->fields["join"] = mkFn("join", [tHandle](std::vector<Value>) -> Value {
            if (tHandle->joinable()) tHandle->join();
            return Value::makeNull();
        });
        obj->fields["isAlive"] = mkFn("isAlive", [tHandle](std::vector<Value>) -> Value {
            return Value::makeBool(tHandle->joinable());
        });
        return Value::makeObject(obj);
    }
    if (expr.type.name == "Mutex") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "Mutex";
        auto mtx = std::make_shared<std::mutex>();
        obj->fields["lock"] = mkFn("lock", [mtx](std::vector<Value>) -> Value { mtx->lock(); return Value::makeNull(); });
        obj->fields["unlock"] = mkFn("unlock", [mtx](std::vector<Value>) -> Value { mtx->unlock(); return Value::makeNull(); });
        obj->fields["tryLock"] = mkFn("tryLock", [mtx](std::vector<Value>) -> Value { return Value::makeBool(mtx->try_lock()); });
        return Value::makeObject(obj);
    }
    if (expr.type.name == "AtomicInt") {
        int64_t initVal = expr.args.empty() ? 0 : eval(*expr.args[0], env).intVal;
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "AtomicInt";
        auto atom = std::make_shared<std::atomic<int64_t>>(initVal);
        obj->fields["get"] = mkFn("get", [atom](std::vector<Value>) -> Value { return Value::makeInt(atom->load()); });
        obj->fields["set"] = mkFn("set", [atom](std::vector<Value> a) -> Value { atom->store(a[0].intVal); return Value::makeNull(); });
        obj->fields["getAndIncrement"] = mkFn("getAndIncrement", [atom](std::vector<Value>) -> Value { return Value::makeInt((*atom)++); });
        obj->fields["addAndGet"] = mkFn("addAndGet", [atom](std::vector<Value> a) -> Value { return Value::makeInt(*atom += a[0].intVal); });
        return Value::makeObject(obj);
    }
    if (expr.type.name == "Task") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "Task";
        FuncPtr fn = expr.args.empty() ? nullptr : eval(*expr.args[0], env).funcVal;
        auto res = std::make_shared<Value>(Value::makeInt(0));
        if (fn) *res = callFunction(*fn, {});
        obj->fields["get"] = mkFn("get", [res](std::vector<Value>) -> Value { return *res; });
        obj->fields["wait"] = mkFn("wait", [](std::vector<Value>) -> Value { return Value::makeNull(); });
        obj->fields["isDone"] = mkFn("isDone", [](std::vector<Value>) -> Value { return Value::makeBool(true); });
        return Value::makeObject(obj);
    }
    if (expr.type.name == "AudioEngine") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "AudioEngine";
        obj->fields["setVolume"] = mkFn("setVolume", [](std::vector<Value>) -> Value { return Value::makeNull(); });
        return Value::makeObject(obj);
    }
    if (expr.type.name == "Sound") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "Sound";
        obj->fields["play"] = mkFn("play", [](std::vector<Value>) -> Value { return Value::makeNull(); });
        obj->fields["stop"] = mkFn("stop", [](std::vector<Value>) -> Value { return Value::makeNull(); });
        obj->fields["isPlaying"] = mkFn("isPlaying", [](std::vector<Value>) -> Value { return Value::makeBool(true); });
        obj->fields["setVolume"] = mkFn("setVolume", [](std::vector<Value>) -> Value { return Value::makeNull(); });
        obj->fields["setPitch"] = mkFn("setPitch", [](std::vector<Value>) -> Value { return Value::makeNull(); });
        obj->fields["setLooping"] = mkFn("setLooping", [](std::vector<Value>) -> Value { return Value::makeNull(); });
        return Value::makeObject(obj);
    }

    // ── HttpClient
    if (expr.type.name == "HttpClient") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "HttpClient";
        auto timeout = std::make_shared<int>(5000);
        auto defaultHeaders = std::make_shared<std::unordered_map<std::string, std::string>>();
        obj->fields["get"] = mkFn("get", [timeout, defaultHeaders](std::vector<Value> args) -> Value {
            // Simplified: returns a stub HttpResponse
            auto resp = std::make_shared<ObjectInstance>();
            resp->className = "HttpResponse";
            resp->fields["statusCode"] = Value::makeInt(200);
            resp->fields["body"] = Value::makeString("");
            auto hdrs = std::make_shared<ObjectInstance>(); hdrs->className = "Map";
            resp->fields["headers"] = Value::makeObject(hdrs);
            return Value::makeObject(resp);
        });
        obj->fields["post"] = obj->fields["get"];
        obj->fields["postJson"] = obj->fields["get"];
        obj->fields["setHeader"] = mkFn("setHeader", [defaultHeaders](std::vector<Value> args) -> Value {
            if (args.size() >= 2) (*defaultHeaders)[args[0].toString()] = args[1].toString();
            return Value::makeNull();
        });
        obj->fields["setTimeout"] = mkFn("setTimeout", [timeout](std::vector<Value> args) -> Value {
            if (!args.empty()) *timeout = args[0].intVal;
            return Value::makeNull();
        });
        return Value::makeObject(obj);
    }
    if (expr.type.name == "TcpSocket") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "TcpSocket";
        obj->fields["connect"] = mkFn("connect", [](auto) { return Value::makeNull(); });
        obj->fields["close"] = mkFn("close", [](auto) { return Value::makeNull(); });
        obj->fields["send"] = mkFn("send", [](auto) { return Value::makeNull(); });
        obj->fields["receive"] = mkFn("receive", [](auto) { return Value::makeString(""); });
        obj->fields["isConnected"] = mkFn("isConnected", [](auto) { return Value::makeBool(false); });
        return Value::makeObject(obj);
    }
    if (expr.type.name == "TcpListener") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "TcpListener";
        obj->fields["start"] = mkFn("start", [](auto) { return Value::makeNull(); });
        obj->fields["accept"] = mkFn("accept", [](auto) -> Value {
            auto sock = std::make_shared<ObjectInstance>(); sock->className = "TcpSocket";
            return Value::makeObject(sock);
        });
        obj->fields["close"] = mkFn("close", [](auto) { return Value::makeNull(); });
        return Value::makeObject(obj);
    }
    if (expr.type.name == "Url") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "Url";
        obj->fields["encode"] = mkFn("encode", [](std::vector<Value> args) -> Value {
            std::string s = args.empty() ? "" : args[0].toString();
            std::string res;
            for (char c : s) {
                if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~')
                    res += c;
                else { char buf[4]; std::snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c)); res += buf; }
            }
            return Value::makeString(res);
        });
        obj->fields["decode"] = mkFn("decode", [](std::vector<Value> args) -> Value {
            std::string s = args.empty() ? "" : args[0].toString();
            std::string res;
            for (size_t i = 0; i < s.size(); i++) {
                if (s[i] == '%' && i + 2 < s.size()) {
                    int val = 0; std::sscanf(s.substr(i+1, 2).c_str(), "%x", &val);
                    res += static_cast<char>(val); i += 2;
                } else if (s[i] == '+') res += ' ';
                else res += s[i];
            }
            return Value::makeString(res);
        });
        obj->fields["parseQuery"] = mkFn("parseQuery", [](std::vector<Value> args) -> Value {
            std::string s = args.empty() ? "" : args[0].toString();
            auto mapObj = std::make_shared<ObjectInstance>(); mapObj->className = "Map";
            size_t pos = 0;
            while (pos < s.size()) {
                size_t amp = s.find('&', pos);
                if (amp == std::string::npos) amp = s.size();
                std::string pair = s.substr(pos, amp - pos);
                size_t eq = pair.find('=');
                if (eq != std::string::npos)
                    mapObj->fields[pair.substr(0, eq)] = Value::makeString(pair.substr(eq + 1));
                pos = amp + 1;
            }
            return Value::makeObject(mapObj);
        });
        return Value::makeObject(obj);
    }
    if (expr.type.name == "HttpResponse") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "HttpResponse";
        obj->fields["statusCode"] = Value::makeInt(0);
        obj->fields["body"] = Value::makeString("");
        return Value::makeObject(obj);
    }
    if (expr.type.name == "ConditionVariable") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "ConditionVariable";
        auto cv = std::make_shared<std::condition_variable>();
        obj->fields["wait"] = mkFn("wait", [cv](std::vector<Value> args) -> Value {
            // Simplified — real impl needs the mutex
            return Value::makeNull();
        });
        obj->fields["signal"] = mkFn("signal", [cv](std::vector<Value>) -> Value { cv->notify_one(); return Value::makeNull(); });
        obj->fields["broadcast"] = mkFn("broadcast", [cv](std::vector<Value>) -> Value { cv->notify_all(); return Value::makeNull(); });
        return Value::makeObject(obj);
    }

    // ── User-defined struct
    {
        auto sit = structDefs_.find(expr.type.name);
        if (sit != structDefs_.end()) {
            auto obj = std::make_shared<ObjectInstance>();
            obj->className = expr.type.name;
            auto instanceEnv = env->child();
            Value thisVal = Value::makeObject(obj);
            instanceEnv->declare("this", thisVal);
            instanceEnv->declare("__this", thisVal);
            for (auto& f : sit->second->fields) {
                Value v = f->initializer ? eval(*f->initializer, instanceEnv) : Value::makeNull();
                obj->fields[f->name] = v;
            }
            // Structs are value types — they have no constructors in the AST.
            // Fields were already initialized above.
            return Value::makeObject(obj);
        }
    }

    // ── User-defined class
    const ClassDecl* cls = findClass(expr.type.name);
    if (!cls) {
        if (expr.type.name != "Exception" && expr.type.name != "Error") {
            throwRuntimeError("Undefined class: " + expr.type.name, expr);
        }
        auto obj       = std::make_shared<ObjectInstance>();
        obj->className = expr.type.name;
        if (!expr.args.empty()) {
            Value msg = eval(*expr.args[0], env);
            obj->fields["message"] = msg;
        }
        return Value::makeObject(obj);
    }

    // Special case: Exception class — always set message field from first arg
    // This handles both the prelude's Exception class and user-defined exceptions
    if (expr.type.name == "Exception") {
        auto obj = std::make_shared<ObjectInstance>();
        obj->className = "Exception";
        obj->classDef  = cls;
        instantiateFields(*obj, *cls, env);
        if (!expr.args.empty()) {
            Value msg = eval(*expr.args[0], env);
            obj->fields["message"] = msg;
        }
        std::vector<Value> args;
        for (auto& a : expr.args) args.push_back(eval(*a, env));
        try { runConstructor(*obj, *cls, std::move(args), env); }
        catch (...) { /* constructor errors shouldn't crash throw */ }
        return Value::makeObject(obj);
    }

    auto obj    = std::make_shared<ObjectInstance>();
    obj->className = cls->name;
    obj->classDef  = cls;

    // Initialize fields from class hierarchy
    instantiateFields(*obj, *cls, env);

    // Evaluate constructor args
    std::vector<Value> args;
    for (auto& a : expr.args) args.push_back(eval(*a, env));

    // Run constructor
    runConstructor(*obj, *cls, std::move(args), env);

    return Value::makeObject(obj);
}

void Evaluator::instantiateFields(ObjectInstance& obj, const ClassDecl& cls,
                                   std::shared_ptr<Environment> env) {
    auto instanceEnv = env->child();
    Value thisVal = Value::makeObject(std::shared_ptr<ObjectInstance>(&obj, [](ObjectInstance*){}));
    instanceEnv->declare("this",      thisVal);
    instanceEnv->declare("__this",    thisVal);
    instanceEnv->declare("__class__", Value::makeString(cls.name));

    // Initialize super fields first
    if (cls.superClass) {
        auto sit = classDefs_.find(cls.superClass->name);
        if (sit != classDefs_.end())
            instantiateFields(obj, *sit->second, instanceEnv);
    }
    // Initialize own instance fields (static fields are initialized in collectDeclarations)
    for (auto& f : cls.fields) {
        if (f->isStatic) continue;  // already initialized globally
        Value v = f->initializer ? eval(*f->initializer, instanceEnv) : Value::makeNull();
        obj.fields[f->name] = v;
    }
    // Initialize auto-properties with initializers
    for (auto& prop : cls.properties) {
        if (prop->initializer) {
            Value v = eval(*prop->initializer, instanceEnv);
            obj.fields[prop->name] = v;
        } else if (prop->hasGet && prop->hasSet && !prop->getDecl && !prop->setDecl) {
            // Auto-property without initializer — default to null
            obj.fields[prop->name] = Value::makeNull();
        }
    }
}

void Evaluator::runConstructor(ObjectInstance& obj, const ClassDecl& cls,
                                std::vector<Value> args,
                                std::shared_ptr<Environment> env) {
    if (!cls.constructors.empty()) {
        const ConstructorDecl* ctor = nullptr;
        for (auto& c : cls.constructors) {
            if (c->params.size() == args.size()) { ctor = c.get(); break; }
            if (!ctor) ctor = c.get();
        }
        if (!ctor) return;

        auto ctorEnv = env->child();
        Value thisVal = Value::makeObject(std::shared_ptr<ObjectInstance>(&obj, [](ObjectInstance*){}));
        ctorEnv->declare("this",      thisVal);
        ctorEnv->declare("__this",    thisVal);
        ctorEnv->declare("__class__", Value::makeString(cls.name));

        for (size_t i = 0; i < ctor->params.size() && i < args.size(); i++)
            ctorEnv->declare(ctor->params[i].name, args[i]);

        if (ctor->body) {
            try { eval(*ctor->body, ctorEnv); }
            catch (const ReturnSignal&) {}
        }

        // Sync any changed fields back
        if (ctorEnv->has("this")) {
            Value& t = ctorEnv->get("this");
            if (t.isObject() && t.objVal)
                for (auto& [k, v] : t.objVal->fields)
                    obj.fields[k] = v;
        }
        return;
    }

    for (auto& m : cls.methods) {
        if (m->name == "constructor" && m->body) {
            auto ctorEnv = env->child();
            Value thisVal = Value::makeObject(std::shared_ptr<ObjectInstance>(&obj, [](ObjectInstance*){}));
            ctorEnv->declare("this",      thisVal);
            ctorEnv->declare("__this",    thisVal);
            ctorEnv->declare("__class__", Value::makeString(cls.name));
            for (size_t i = 0; i < m->params.size() && i < args.size(); i++) {
                if (m->params[i].variadic) {
                    auto arr = std::make_shared<ArrayInstance>();
                    for (size_t j = i; j < args.size(); j++) arr->elements.push_back(args[j]);
                    ctorEnv->declare(m->params[i].name, Value::makeArray(arr));
                    break;
                }
                ctorEnv->declare(m->params[i].name, args[i]);
            }
            try { eval(*m->body, ctorEnv); }
            catch (const ReturnSignal&) {}
            // Sync fields
            if (ctorEnv->has("this")) {
                Value& t = ctorEnv->get("this");
                if (t.isObject() && t.objVal)
                    for (auto& [k, v] : t.objVal->fields)
                        obj.fields[k] = v;
            }
            return;
        }
    }
}

Value Evaluator::callMethod(Value& object, const std::string& method,
                             std::vector<Value> args,
                             std::shared_ptr<Environment> env) {
    if (object.isNull()) {
        if (method == "add" || method == "push" || method == "append") {
            object = Value::makeArray(std::make_shared<ArrayInstance>());
            return callArrayMethod(object, method, std::move(args));
        }
        if (method == "length" || method == "size") {
            return Value::makeInt(0);
        }
        throwRuntimeError("Null pointer dereference: cannot call method '" + method + "' on null", 0, 0);
    }

    // String methods
    if (object.isString()) return callStringMethod(object, method, std::move(args));
    // Array methods
    if (object.isArray())  return callArrayMethod(object, method, std::move(args));

    // Object method
    if (object.isObject() && object.objVal) {
        auto& obj = *object.objVal;

        // Built-in collection dispatch
        if (obj.className == "List" || obj.className == "Set" || obj.className == "Map")
            return callListMethod(obj, method, std::move(args));

        // Check for stored function in fields
        auto fit = obj.fields.find(method);
        if (fit != obj.fields.end() && fit->second.isFunction()) {
            return callFunction(*fit->second.funcVal, std::move(args));
        }

        if (method == "toJsonString") {
            std::ostringstream json;
            json << "{";
            bool first = true;
            const ClassDecl* jsonCls = obj.classDef ? obj.classDef : findClass(obj.className);
            for (auto& [name, val] : obj.fields) {
                if (name.rfind("_", 0) == 0) continue;
                // Find the field declaration to check for [JsonProperty]
                std::string jsonKey = name;
                bool serializable = true;
                if (jsonCls) {
                    const FieldDecl* fd = findField(*jsonCls, name);
                    if (fd) {
                        bool hasSerial = false;
                        for (auto& attr : fd->attributes) {
                            if (attr.name == "JsonProperty" && !attr.args.empty()) {
                                if (auto* sl = dynamic_cast<const StringLiteralExpr*>(attr.args[0].get()))
                                    jsonKey = sl->value;
                            }
                        }
                    }
                }
                if (!serializable) continue;
                if (!first) json << ",";
                json << "\"" << jsonKey << "\":";
                if (val.isString()) json << "\"" << val.strVal << "\"";
                else if (val.isBool()) json << (val.boolVal ? "true" : "false");
                else if (val.isNull()) json << "null";
                else if (val.isObject() && val.objVal) {
                    // Nested serializable — call its toJsonString
                    auto nit = val.objVal->fields.find("__enumName");
                    if (nit != val.objVal->fields.end() && nit->second.isString())
                        json << "\"" << nit->second.strVal << "\"";
                    else
                        json << val.toString();
                }
                else json << val.toString();
                first = false;
            }
            json << "}";
            return Value::makeString(json.str());
        }

        // Look up in class definition
        const ClassDecl* cls = obj.classDef ? obj.classDef : findClass(obj.className);
        if (cls) {
            const FuncDecl* m = findMethod(*cls, method);
            if (m && m->body) {
                auto mEnv = env->child();
                mEnv->declare("this",     object);
                mEnv->declare("__this",   object);
                mEnv->declare("__class__", Value::makeString(obj.className));
                for (size_t i = 0; i < m->params.size(); i++) {
                    auto& p = m->params[i];
                    if (i < args.size())
                        mEnv->declare(p.name, args[i]);
                    else if (p.defaultValue)
                        mEnv->declare(p.name, eval(*p.defaultValue, mEnv));
                    else
                        mEnv->declare(p.name, Value::makeNull());
                }
                try { eval(*m->body, mEnv); }
                catch (const ReturnSignal& r) { return r.value; }
                return Value::makeNull();
            }
        }
        // Look up in struct definition
        auto sit = structDefs_.find(obj.className);
        if (sit != structDefs_.end()) {
            for (auto& sm : sit->second->methods) {
                if (sm->name == method && sm->body) {
                    auto mEnv = env->child();
                    mEnv->declare("this",   object);
                    mEnv->declare("__this", object);
                    for (size_t i = 0; i < sm->params.size() && i < args.size(); i++)
                        mEnv->declare(sm->params[i].name, args[i]);
                    try { eval(*sm->body, mEnv); }
                    catch (const ReturnSignal& r) { return r.value; }
                    return Value::makeNull();
                }
            }
        }
    }

    // Enum methods
    if (object.isObject() && object.objVal) {
        auto eit = enumDefs_.find(object.objVal->className);
        if (eit != enumDefs_.end()) {
            const FuncDecl* m = findEnumMethod(*eit->second, method);
            if (m && m->body) {
                auto mEnv = global_->child();
                mEnv->declare("this", object);
                mEnv->declare("__this", object);
                mEnv->declare("__class__", Value::makeString(object.objVal->className));
                for (size_t i = 0; i < m->params.size() && i < args.size(); i++)
                    mEnv->declare(m->params[i].name, args[i]);
                try { eval(*m->body, mEnv); }
                catch (const ReturnSignal& r) { return r.value; }
                return Value::makeNull();
            }
        }
    }

    // toString — check if the class has a toString method first
    if (method == "toString") {
        if (object.isObject() && object.objVal) {
            // Enum value — return the name
            auto en = object.objVal->fields.find("__enumName");
            if (en != object.objVal->fields.end() && en->second.isString())
                return en->second;
            const ClassDecl* cls = object.objVal->classDef ? object.objVal->classDef : findClass(object.objVal->className);
            if (cls) {
                const FuncDecl* m = findMethod(*cls, "toString");
                if (m && m->body) {
                    auto mEnv = global_->child();
                    mEnv->declare("this", object);
                    mEnv->declare("__this", object);
                    mEnv->declare("__class__", Value::makeString(object.objVal->className));
                    try { eval(*m->body, mEnv); }
                    catch (const ReturnSignal& r) { return r.value; }
                }
            }
        }
        return Value::makeString(object.toString());
    }
    if (method == "length"  ) {
        if (object.isString()) return Value::makeInt(static_cast<int64_t>(object.strVal.size()));
        if (object.isArray())  return Value::makeInt(static_cast<int64_t>(object.arrVal->elements.size()));
    }
    // Universal: getMessage() returns "message" field (exception objects)
    if (method == "getMessage" && object.isObject() && object.objVal) {
        auto it = object.objVal->fields.find("message");
        if (it != object.objVal->fields.end()) return it->second;
        return Value::makeString("");
    }

    // Detailed error with available methods for objects
    if (object.isObject() && object.objVal) {
        auto& obj2 = *object.objVal;
        const ClassDecl* cls2 = obj2.classDef ? obj2.classDef : findClass(obj2.className);
        std::string avail;
        if (cls2) { for (auto& meth : cls2->methods) avail += meth->name + " "; }
        throwRuntimeError("Unknown method '" + method + "' on [" + obj2.className + " instance]" +
            (avail.empty() ? "" : " (available: " + avail + ")"), 0, 0);
    }
    throwRuntimeError("Unknown method '" + method + "' on " + object.toString(), 0, 0);
}

Value Evaluator::callBuiltinMethod(Value& object, const std::string& method,
                                    std::vector<Value> args) {
    if (object.isObject() && object.objVal) {
        auto& fields = object.objVal->fields;
        auto fit = fields.find(method);
        if (fit != fields.end() && fit->second.isFunction())
            return callFunction(*fit->second.funcVal, std::move(args));
    }
    throwRuntimeError("Unknown built-in method: " + method, 0, 0);
}

Value Evaluator::callStringMethod(Value& str, const std::string& method,
                                   std::vector<Value> args) {
    if (method == "getMessage" || method == "toString") return str;
    if (method == "toUpper")   return callStringMethod(str, "toUpperCase", args);
    if (method == "toLower")   return callStringMethod(str, "toLowerCase", args);
    if (method == "length")    return Value::makeInt(static_cast<int64_t>(str.strVal.size()));
    if (method == "charAt")    { int64_t i = args[0].intVal; return Value::makeChar(str.strVal[static_cast<size_t>(i)]); }
    if (method == "substring") {
        int64_t start = args[0].intVal;
        int64_t end   = args.size() > 1 ? args[1].intVal : static_cast<int64_t>(str.strVal.size());
        return Value::makeString(str.strVal.substr(static_cast<size_t>(start), static_cast<size_t>(end - start)));
    }
    if (method == "toUpperCase") {
        std::string s = str.strVal;
        for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return Value::makeString(s);
    }
    if (method == "toLowerCase") {
        std::string s = str.strVal;
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return Value::makeString(s);
    }
    if (method == "contains") {
        return Value::makeBool(str.strVal.find(args[0].toString()) != std::string::npos);
    }
    if (method == "startsWith") {
        std::string prefix = args[0].toString();
        return Value::makeBool(str.strVal.substr(0, prefix.size()) == prefix);
    }
    if (method == "endsWith") {
        std::string suffix = args[0].toString();
        if (suffix.size() > str.strVal.size()) return Value::makeBool(false);
        return Value::makeBool(str.strVal.substr(str.strVal.size() - suffix.size()) == suffix);
    }
    if (method == "trim") {
        std::string s = str.strVal;
        size_t l = s.find_first_not_of(" \t\n\r");
        size_t r = s.find_last_not_of(" \t\n\r");
        return Value::makeString(l == std::string::npos ? "" : s.substr(l, r - l + 1));
    }
    if (method == "replace") {
        std::string s = str.strVal;
        std::string from = args[0].toString();
        std::string to   = args[1].toString();
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
        return Value::makeString(s);
    }
    if (method == "indexOf") {
        size_t pos = str.strVal.find(args[0].toString());
        return Value::makeInt(pos == std::string::npos ? -1 : static_cast<int64_t>(pos));
    }
    if (method == "toString") return str;
    if (method == "split") {
        std::string delim = args.empty() ? " " : args[0].toString();
        auto arr = std::make_shared<ArrayInstance>();
        std::string s = str.strVal;
        size_t pos = 0;
        while ((pos = s.find(delim)) != std::string::npos) {
            arr->elements.push_back(Value::makeString(s.substr(0, pos)));
            s = s.substr(pos + delim.size());
        }
        arr->elements.push_back(Value::makeString(s));
        return Value::makeArray(arr);
    }
    throwRuntimeError("Unknown string method: " + method, 0, 0);
}

Value Evaluator::callArrayMethod(Value& arr, const std::string& method,
                                  std::vector<Value> args) {
    if (method == "length" || method == "size")
        return Value::makeInt(static_cast<int64_t>(arr.arrVal->elements.size()));
    if (method == "isEmpty")
        return Value::makeBool(arr.arrVal->elements.empty());
    if (method == "get") {
        int64_t i = args.empty() ? 0 : args[0].intVal;
        if (i < 0 || static_cast<size_t>(i) >= arr.arrVal->elements.size()) return Value::makeNull();
        return arr.arrVal->elements[static_cast<size_t>(i)];
    }
    if (method == "add" || method == "push" || method == "append") {
        if (!args.empty()) arr.arrVal->elements.push_back(args[0]);
        return Value::makeNull();
    }
    if (method == "clear") {
        arr.arrVal->elements.clear();
        return Value::makeNull();
    }
    if (method == "pop") {
        if (arr.arrVal->elements.empty()) return Value::makeNull();
        Value v = arr.arrVal->elements.back();
        arr.arrVal->elements.pop_back();
        return v;
    }
    if (method == "remove") {
        if (arr.arrVal->elements.empty()) return Value::makeNull();
        if (!args.empty() && args[0].isInt()) {
            int64_t i = args[0].intVal;
            if (i >= 0 && static_cast<size_t>(i) < (int64_t)arr.arrVal->elements.size()) {
                Value v = arr.arrVal->elements[static_cast<size_t>(i)];
                arr.arrVal->elements.erase(arr.arrVal->elements.begin() + i);
                return v;
            }
        } else if (!args.empty()) {
            for (auto it = arr.arrVal->elements.begin(); it != arr.arrVal->elements.end(); ++it) {
                if (*it == args[0]) {
                    arr.arrVal->elements.erase(it);
                    return Value::makeBool(true);
                }
            }
            return Value::makeBool(false);
        }
        Value v = arr.arrVal->elements.back();
        arr.arrVal->elements.pop_back();
        return v;
    }
    if (method == "contains" || method == "includes") {
        if (args.empty()) return Value::makeBool(false);
        for (auto& e : arr.arrVal->elements) {
            if (e == args[0]) return Value::makeBool(true);
        }
        return Value::makeBool(false);
    }
    if (method == "forEach" && !args.empty() && args[0].isFunction()) {
        for (auto& e : arr.arrVal->elements)
            callFunction(*args[0].funcVal, {e});
        return Value::makeNull();
    }
    if (method == "filter" && !args.empty() && args[0].isFunction()) {
        auto ra = std::make_shared<ArrayInstance>();
        for (auto& e : arr.arrVal->elements) {
            Value keep = callFunction(*args[0].funcVal, {e});
            if (keep.toBool()) ra->elements.push_back(e);
        }
        return Value::makeArray(ra);
    }
    if (method == "map" && !args.empty() && args[0].isFunction()) {
        auto ra = std::make_shared<ArrayInstance>();
        for (auto& e : arr.arrVal->elements)
            ra->elements.push_back(callFunction(*args[0].funcVal, {e}));
        return Value::makeArray(ra);
    }
    if (method == "reduce" && args.size() >= 2 && args[1].isFunction()) {
        Value acc = args[0];
        for (auto& e : arr.arrVal->elements)
            acc = callFunction(*args[1].funcVal, {acc, e});
        return acc;
    }
    if (method == "sort") {
        if (!args.empty() && args[0].isFunction()) {
            auto comp = args[0].funcVal;
            std::sort(arr.arrVal->elements.begin(), arr.arrVal->elements.end(),
                [this, &comp](const Value& a, const Value& b) -> bool {
                    Value r = callFunction(*comp, {a, b});
                    return r.isInt() ? r.intVal < 0 : false;
                });
        } else {
            std::sort(arr.arrVal->elements.begin(), arr.arrVal->elements.end());
        }
        return Value::makeNull();
    }
    if (method == "indexOf") {
        for (size_t i = 0; i < arr.arrVal->elements.size(); i++)
            if (arr.arrVal->elements[i] == args[0]) return Value::makeInt(static_cast<int64_t>(i));
        return Value::makeInt(-1);
    }
    if (method == "toString") {
        std::string res = "[";
        bool first = true;
        for (auto& e : arr.arrVal->elements) { if (!first) res += ", "; res += e.toString(); first = false; }
        res += "]";
        return Value::makeString(res);
    }
    throwRuntimeError("Unknown array method: " + method, 0, 0);
}

Value Evaluator::callListMethod(ObjectInstance& obj, const std::string& method,
                                 std::vector<Value> args) {
    // Map-specific (must come before List logic to avoid _data accesses)
    if (obj.className == "Map") {
        if (method == "toString") {
            std::string res = "{"; bool first = true;
            for (auto& [k, v] : obj.fields) { if (!first) res += ", "; res += k + ": " + v.toString(); first = false; }
            res += "}"; return Value::makeString(res);
        }
        if (method == "put") { obj.fields[args[0].toString()] = args[1]; return Value::makeNull(); }
        if (method == "get") { auto it = obj.fields.find(args[0].toString()); return it != obj.fields.end() ? it->second : Value::makeNull(); }
        if (method == "containsKey") { return Value::makeBool(obj.fields.count(args[0].toString()) > 0); }
        if (method == "remove") { obj.fields.erase(args[0].toString()); return Value::makeNull(); }
        if (method == "size" || method == "length") {
            return Value::makeInt(static_cast<int64_t>(obj.fields.size()));
        }
        if (method == "containsValue") {
            for (auto& [k, v] : obj.fields) if (v == args[0]) return Value::makeBool(true);
            return Value::makeBool(false);
        }
        if (method == "isEmpty") { return Value::makeBool(obj.fields.empty()); }
        if (method == "clear") { obj.fields.clear(); return Value::makeNull(); }
        if (method == "getOrNull") {
            auto it = obj.fields.find(args[0].toString());
            return it != obj.fields.end() ? it->second : Value::makeNull();
        }
        if (method == "keys") {
            auto arr = std::make_shared<ArrayInstance>();
            auto lo = std::make_shared<ObjectInstance>(); lo->className = "List";
            for (auto& [k, v] : obj.fields) arr->elements.push_back(Value::makeString(k));
            lo->fields["_data"] = Value::makeArray(arr);
            return Value::makeObject(lo);
        }
        if (method == "values") {
            auto arr = std::make_shared<ArrayInstance>();
            auto lo = std::make_shared<ObjectInstance>(); lo->className = "List";
            for (auto& [k, v] : obj.fields) arr->elements.push_back(v);
            lo->fields["_data"] = Value::makeArray(arr);
            return Value::makeObject(lo);
        }
        if (method == "forEach" && !args.empty() && args[0].isFunction()) {
            for (auto& [k, v] : obj.fields)
                callFunction(*args[0].funcVal, {Value::makeString(k), v});
            return Value::makeNull();
        }
        throwRuntimeError("Unknown Map method: " + method, 0, 0);
    }

    auto& dataField = obj.fields["_data"];

    if (method == "toString") {
        if (dataField.isArray()) {
            std::string res = (obj.className == "Set" ? "{" : "[");
            bool first = true;
            for (auto& e : dataField.arrVal->elements) {
                if (!first) res += ", ";
                res += e.toString();
                first = false;
            }
            res += (obj.className == "Set" ? "}" : "]");
            return Value::makeString(res);
        }
        return Value::makeString(obj.className == "Set" ? "{}" : "[]");
    }

    if (method == "toArray") {
        if (dataField.isArray()) return dataField;
        return Value::makeArray(std::make_shared<ArrayInstance>());
    }

    if (method == "addAll") {
        if (!dataField.isArray()) {
            dataField = Value::makeArray(std::make_shared<ArrayInstance>());
        }
        if (!args.empty()) {
            if (args[0].isArray()) {
                for (auto& item : args[0].arrVal->elements) dataField.arrVal->elements.push_back(item);
            } else if (args[0].isObject() && args[0].objVal) {
                auto it = args[0].objVal->fields.find("_data");
                if (it != args[0].objVal->fields.end() && it->second.isArray()) {
                    for (auto& item : it->second.arrVal->elements) dataField.arrVal->elements.push_back(item);
                }
            }
        }
        return Value::makeNull();
    }

    if (method == "add" || method == "push" || method == "append" || method == "enqueue") {
        if (!dataField.isArray()) {
            dataField = Value::makeArray(std::make_shared<ArrayInstance>());
        }
        dataField.arrVal->elements.push_back(args[0]);
        return Value::makeNull();
    }
    if (method == "get") {
        if (!dataField.isArray()) throwRuntimeError("List is not initialized", 0, 0);
        int64_t i = args[0].intVal;
        if (i < 0 || static_cast<size_t>(i) >= dataField.arrVal->elements.size()) {
            throwRuntimeError("Index out of bounds: " + std::to_string(i), 0, 0);
        }
        return dataField.arrVal->elements[static_cast<size_t>(i)];
    }
    if (method == "set") {
        if (!dataField.isArray()) throwRuntimeError("List is not initialized", 0, 0);
        int64_t i = args[0].intVal;
        if (i < 0 || static_cast<size_t>(i) >= dataField.arrVal->elements.size()) {
            throwRuntimeError("Index out of bounds: " + std::to_string(i), 0, 0);
        }
        dataField.arrVal->elements[static_cast<size_t>(i)] = args[1];
        return Value::makeNull();
    }
    if (method == "size" || method == "length") {
        if (!dataField.isArray()) return Value::makeInt(0);
        return Value::makeInt(static_cast<int64_t>(dataField.arrVal->elements.size()));
    }
    if (method == "isEmpty") {
        if (!dataField.isArray()) return Value::makeBool(true);
        return Value::makeBool(dataField.arrVal->elements.empty());
    }
    if (method == "contains") {
        if (!dataField.isArray()) return Value::makeBool(false);
        for (auto& e : dataField.arrVal->elements) if (e == args[0]) return Value::makeBool(true);
        return Value::makeBool(false);
    }
    if (method == "remove") {
        if (!dataField.isArray()) return Value::makeNull();
        int64_t i = args[0].intVal;
        auto& elems = dataField.arrVal->elements;
        if (i >= 0 && i < (int64_t)elems.size())
            elems.erase(elems.begin() + i);
        return Value::makeNull();
    }
    if (method == "clear") {
        if (dataField.isArray()) dataField.arrVal->elements.clear();
        return Value::makeNull();
    }
    // Stack methods (push is aliased to add above)
    if (method == "pop") {
        if (!dataField.isArray() || dataField.arrVal->elements.empty())
            throwRuntimeError("Stack underflow", 0, 0);
        Value v = dataField.arrVal->elements.back();
        dataField.arrVal->elements.pop_back();
        return v;
    }
    if (method == "peek") {
        if (!dataField.isArray() || dataField.arrVal->elements.empty())
            throwRuntimeError("Stack is empty", 0, 0);
        return dataField.arrVal->elements.back();
    }
    // Queue methods (enqueue is aliased to add above)
    if (method == "dequeue") {
        if (!dataField.isArray() || dataField.arrVal->elements.empty())
            throwRuntimeError("Queue is empty", 0, 0);
        Value v = dataField.arrVal->elements.front();
        dataField.arrVal->elements.erase(dataField.arrVal->elements.begin());
        return v;
    }
    if (method == "front") {
        if (!dataField.isArray() || dataField.arrVal->elements.empty())
            throwRuntimeError("Queue is empty", 0, 0);
        return dataField.arrVal->elements.front();
    }
    if (method == "forEach" && !args.empty() && args[0].isFunction()) {
        if (!dataField.isArray()) return Value::makeNull();
        for (auto& e : dataField.arrVal->elements)
            callFunction(*args[0].funcVal, {e});
        return Value::makeNull();
    }
    if (method == "filter" && !args.empty() && args[0].isFunction()) {
        auto resultObj = std::make_shared<ObjectInstance>();
        resultObj->className = "List";
        auto resultArr = std::make_shared<ArrayInstance>();
        if (dataField.isArray()) {
            for (auto& e : dataField.arrVal->elements) {
                Value keep = callFunction(*args[0].funcVal, {e});
                if (keep.toBool()) resultArr->elements.push_back(e);
            }
        }
        resultObj->fields["_data"] = Value::makeArray(resultArr);
        return Value::makeObject(resultObj);
    }
    if (method == "map" && !args.empty() && args[0].isFunction()) {
        auto resultObj = std::make_shared<ObjectInstance>();
        resultObj->className = "List";
        auto resultArr = std::make_shared<ArrayInstance>();
        if (dataField.isArray()) {
            for (auto& e : dataField.arrVal->elements)
                resultArr->elements.push_back(callFunction(*args[0].funcVal, {e}));
        }
        resultObj->fields["_data"] = Value::makeArray(resultArr);
        return Value::makeObject(resultObj);
    }
    if (method == "reduce" && args.size() >= 2 && args[1].isFunction()) {
        Value acc = args[0];
        if (dataField.isArray()) {
            for (auto& e : dataField.arrVal->elements)
                acc = callFunction(*args[1].funcVal, {acc, e});
        }
        return acc;
    }
    // List-specific extras
    if (obj.className == "List") {
        if (method == "indexOf") {
            if (!dataField.isArray()) return Value::makeInt(-1);
            for (size_t i = 0; i < dataField.arrVal->elements.size(); i++)
                if (dataField.arrVal->elements[i] == args[0]) return Value::makeInt(static_cast<int64_t>(i));
            return Value::makeInt(-1);
        }
        if (method == "slice") {
            auto lo = std::make_shared<ObjectInstance>(); lo->className = "List";
            auto ra = std::make_shared<ArrayInstance>();
            if (dataField.isArray()) {
                int64_t s = args.empty() ? 0 : args[0].intVal;
                int64_t e = args.size() > 1 ? args[1].intVal : static_cast<int64_t>(dataField.arrVal->elements.size());
                for (int64_t i = s; i < e && i < (int64_t)dataField.arrVal->elements.size(); i++)
                    ra->elements.push_back(dataField.arrVal->elements[static_cast<size_t>(i)]);
            }
            lo->fields["_data"] = Value::makeArray(ra);
            return Value::makeObject(lo);
        }
        if (method == "removeItem") {
            if (dataField.isArray()) {
                for (auto it = dataField.arrVal->elements.begin(); it != dataField.arrVal->elements.end(); ++it) {
                    if (*it == args[0]) { dataField.arrVal->elements.erase(it); break; }
                }
            }
            return Value::makeNull();
        }
        if (method == "sort") {
            if (dataField.isArray() && !args.empty() && args[0].isFunction()) {
                auto comp = args[0].funcVal;
                std::sort(dataField.arrVal->elements.begin(), dataField.arrVal->elements.end(),
                    [this, &comp](const Value& a, const Value& b) -> bool {
                        Value r = callFunction(*comp, {a, b});
                        return r.isInt() ? r.intVal < 0 : false;
                    });
            } else if (dataField.isArray()) {
                std::sort(dataField.arrVal->elements.begin(), dataField.arrVal->elements.end());
            }
            return Value::makeNull();
        }
    }
    // Map-specific
    if (obj.className == "Map") {
        if (method == "put") { obj.fields[args[0].toString()] = args[1]; return Value::makeNull(); }
        if (method == "get") { auto it = obj.fields.find(args[0].toString()); return it != obj.fields.end() ? it->second : Value::makeNull(); }
        if (method == "containsKey") { return Value::makeBool(obj.fields.count(args[0].toString()) > 0); }
        if (method == "size") { return Value::makeInt(static_cast<int64_t>(obj.fields.size())); }
    }
    // Set
    if (obj.className == "Set") {
        if (method == "add") {
            if (!dataField.isArray()) dataField = Value::makeArray(std::make_shared<ArrayInstance>());
            // Check for duplicates
            for (auto& e : dataField.arrVal->elements) if (e == args[0]) return Value::makeNull();
            dataField.arrVal->elements.push_back(args[0]);
            return Value::makeNull();
        }
        if (method == "contains") {
            if (!dataField.isArray()) return Value::makeBool(false);
            for (auto& e : dataField.arrVal->elements) if (e == args[0]) return Value::makeBool(true);
            return Value::makeBool(false);
        }
        if (method == "size") {
            if (!dataField.isArray()) return Value::makeInt(0);
            return Value::makeInt(static_cast<int64_t>(dataField.arrVal->elements.size()));
        }
        if (method == "remove") {
            if (dataField.isArray()) {
                for (auto it = dataField.arrVal->elements.begin(); it != dataField.arrVal->elements.end(); ++it) {
                    if (*it == args[0]) { dataField.arrVal->elements.erase(it); break; }
                }
            }
            return Value::makeNull();
        }
        if (method == "toList") {
            auto lo = std::make_shared<ObjectInstance>(); lo->className = "List";
            if (dataField.isArray()) lo->fields["_data"] = Value::makeArray(dataField.arrVal);
            else lo->fields["_data"] = Value::makeArray(std::make_shared<ArrayInstance>());
            return Value::makeObject(lo);
        }
        if (method == "union") {
            auto resultObj = std::make_shared<ObjectInstance>(); resultObj->className = "Set";
            auto ra = std::make_shared<ArrayInstance>();
            if (dataField.isArray()) for (auto& e : dataField.arrVal->elements) ra->elements.push_back(e);
            if (args.size() > 0 && args[0].isObject() && args[0].objVal) {
                auto& of = args[0].objVal->fields;
                auto it = of.find("_data");
                if (it != of.end() && it->second.isArray()) {
                    for (auto& e : it->second.arrVal->elements) {
                        bool found = false;
                        for (auto& x : ra->elements) if (x == e) { found = true; break; }
                        if (!found) ra->elements.push_back(e);
                    }
                }
            }
            resultObj->fields["_data"] = Value::makeArray(ra);
            return Value::makeObject(resultObj);
        }
        if (method == "intersect") {
            auto resultObj = std::make_shared<ObjectInstance>(); resultObj->className = "Set";
            auto ra = std::make_shared<ArrayInstance>();
            if (dataField.isArray() && args.size() > 0 && args[0].isObject() && args[0].objVal) {
                auto& of = args[0].objVal->fields;
                auto it = of.find("_data");
                if (it != of.end() && it->second.isArray()) {
                    for (auto& e : dataField.arrVal->elements) {
                        for (auto& x : it->second.arrVal->elements) {
                            if (e == x) { ra->elements.push_back(e); break; }
                        }
                    }
                }
            }
            resultObj->fields["_data"] = Value::makeArray(ra);
            return Value::makeObject(resultObj);
        }
        if (method == "difference") {
            auto resultObj = std::make_shared<ObjectInstance>(); resultObj->className = "Set";
            auto ra = std::make_shared<ArrayInstance>();
            if (dataField.isArray() && args.size() > 0 && args[0].isObject() && args[0].objVal) {
                auto& of = args[0].objVal->fields;
                auto it = of.find("_data");
                if (it != of.end() && it->second.isArray()) {
                    for (auto& e : dataField.arrVal->elements) {
                        bool found = false;
                        for (auto& x : it->second.arrVal->elements) if (e == x) { found = true; break; }
                        if (!found) ra->elements.push_back(e);
                    }
                }
            }
            resultObj->fields["_data"] = Value::makeArray(ra);
            return Value::makeObject(resultObj);
        }
        if (method == "forEach" && !args.empty() && args[0].isFunction()) {
            if (dataField.isArray()) for (auto& e : dataField.arrVal->elements) callFunction(*args[0].funcVal, {e});
            return Value::makeNull();
        }
    }
    // getMessage() on exception objects
    if (method == "getMessage") {
        auto it = obj.fields.find("message");
        if (it != obj.fields.end()) return it->second;
        return Value::makeString("");
    }

    throwRuntimeError("Unknown method '" + method + "' on " + obj.className, 0, 0);
}

Value Evaluator::callFunction(const FunctionInstance& func, std::vector<Value> args,
                               std::shared_ptr<Value> /*thisVal*/) {
    if (func.isNative()) return func.native(std::move(args));

    if (!func.body) return Value::makeNull();

    auto fnEnv = func.closure ? func.closure->child()
                              : global_->child();

    // Bind parameters
    for (size_t i = 0; i < func.params.size(); i++) {
        auto& p = func.params[i];
        if (p.variadic) {
            // Collect remaining args into array
            auto arr = std::make_shared<ArrayInstance>();
            for (size_t j = i; j < args.size(); j++)
                arr->elements.push_back(args[j]);
            fnEnv->declare(p.name, Value::makeArray(arr));
            break;
        }
        if (i < args.size())
            fnEnv->declare(p.name, args[i]);
        else if (p.defaultValue)
            fnEnv->declare(p.name, eval(*p.defaultValue, fnEnv));
        else
            fnEnv->declare(p.name, Value::makeNull());
    }

    try {
        eval(*func.body, fnEnv);
    } catch (const ReturnSignal& r) {
        return r.value;
    }
    return Value::makeNull();
}

Value Evaluator::evalCast(const CastExpr& expr, std::shared_ptr<Environment> env) {
    Value v = eval(*expr.value, env);
    const std::string& t = expr.targetType.name;
    if (t == "int"   || t == "long" || t == "short" || t == "byte") {
        if (v.isInt())   return v;
        if (v.isFloat()) return Value::makeInt(static_cast<int64_t>(v.floatVal));
        if (v.isString()) { try { return Value::makeInt(std::stoll(v.strVal)); } catch(...) {} }
        if (v.isObject() && v.objVal) {
            auto it = v.objVal->fields.find("__enumValue");
            if (it != v.objVal->fields.end() && it->second.isInt())
                return it->second;
        }
        return Value::makeInt(0);
    }
    if (t == "float" || t == "double") {
        if (v.isFloat()) return v;
        if (v.isInt())   return Value::makeFloat(static_cast<double>(v.intVal));
        if (v.isString()) { try { return Value::makeFloat(std::stod(v.strVal)); } catch(...) {} }
        return Value::makeFloat(0.0);
    }
    if (t == "string") return Value::makeString(v.toString());
    if (t == "bool")   return Value::makeBool(v.toBool());
    // Object downcast: check if the object's class is t or a subclass of t
    if (v.isObject() && v.objVal) {
        if (v.objVal->className == t) return v;
        // Walk the class hierarchy
        const ClassDecl* cls = findClass(v.objVal->className);
        while (cls) {
            if (cls->name == t) return v;
            if (cls->superClass) {
                cls = findClass(cls->superClass->name);
            } else break;
        }
        // Not a subclass — return null (cast failed)
        throwRuntimeError("Cannot cast " + v.objVal->className + " to " + t, expr);
    }
    return v;
}

Value Evaluator::evalLambda(const LambdaExpr& expr, std::shared_ptr<Environment> env) {
    auto fi     = std::make_shared<FunctionInstance>();
    fi->name    = "<lambda>";
    fi->params  = expr.params;
    fi->body    = expr.body.get();
    fi->closure = env;
    return Value::makeFunction(fi);
}

Value Evaluator::evalInterpolatedString(const InterpolatedStringExpr& expr,
                                         std::shared_ptr<Environment> env) {
    std::string result;
    for (auto& part : expr.parts) {
        if (!part.isExpr) result += part.text;
        else              result += eval(*part.expr, env).toString();
    }
    return Value::makeString(result);
}

Value Evaluator::evalNullCoalesce(const NullCoalesceExpr& expr, std::shared_ptr<Environment> env) {
    Value left = eval(*expr.left, env);
    if (!left.isNull()) return left;
    return eval(*expr.right, env);
}

Value Evaluator::evalNullAssert(const NullAssertExpr& expr, std::shared_ptr<Environment> env) {
    Value v = eval(*expr.expr, env);
    if (v.isNull()) throwRuntimeError("Null assertion failed", expr);
    return v;
}

Value Evaluator::evalArrayLiteral(const ArrayLiteralExpr& expr, std::shared_ptr<Environment> env) {
    auto arr = std::make_shared<ArrayInstance>();
    for (auto& e : expr.elements) arr->elements.push_back(eval(*e, env));
    return Value::makeArray(arr);
}

Value Evaluator::evalStructInit(const StructInitExpr& expr, std::shared_ptr<Environment> env) {
    auto obj = std::make_shared<ObjectInstance>();
    obj->className = "<struct>";
    for (auto& [name, val] : expr.fields)
        obj->fields[name] = eval(*val, env);
    return Value::makeObject(obj);
}

Value Evaluator::arith(const std::string& op, const Value& l, const Value& r) {
    // String concatenation
    if (op == "+") {
        if (l.isString() || r.isString())
            return Value::makeString(l.toString() + r.toString());
    }

    // Comparison
    if (op == "==" ) return Value::makeBool(l == r);
    if (op == "!=" ) return Value::makeBool(l != r);
    if (op == "<"  ) return Value::makeBool(l < r);
    if (op == ">"  ) return Value::makeBool(r < l);
    if (op == "<=" ) return Value::makeBool(l < r || l == r);
    if (op == ">=" ) return Value::makeBool(r < l || l == r);

    // Numeric
    bool useFloat = l.isFloat() || r.isFloat();
    if (useFloat) {
        double lv = l.toDouble(), rv = r.toDouble();
        if (op == "+" ) return Value::makeFloat(lv + rv);
        if (op == "-" ) return Value::makeFloat(lv - rv);
        if (op == "*" ) return Value::makeFloat(lv * rv);
        if (op == "/" ) { return Value::makeFloat(lv / rv); }  // IEEE 754: div by zero → NaN/Inf
        if (op == "%" ) return Value::makeFloat(std::fmod(lv, rv));
    } else {
        int64_t lv = l.intVal, rv = r.intVal;
        if (op == "+" ) return Value::makeInt(lv + rv);
        if (op == "-" ) return Value::makeInt(lv - rv);
        if (op == "*" ) return Value::makeInt(lv * rv);
        if (op == "/" ) { if (rv == 0) throw UmeRuntimeException(Value::makeString("Division by zero")); return Value::makeInt(lv / rv); }
        if (op == "%" ) { if (rv == 0) throw UmeRuntimeException(Value::makeString("Division by zero")); return Value::makeInt(lv % rv); }
        if (op == "&" ) return Value::makeInt(lv & rv);
        if (op == "|" ) return Value::makeInt(lv | rv);
        if (op == "^" ) return Value::makeInt(lv ^ rv);
        if (op == "<<") return Value::makeInt(lv << rv);
        if (op == ">>") return Value::makeInt(lv >> rv);
    }
    return Value::makeNull();
}

bool Evaluator::isTruthy(const Value& v) const { return v.toBool(); }

const ClassDecl* Evaluator::findClass(const std::string& name) const {
    auto it = classDefs_.find(name);
    if (it != classDefs_.end()) return it->second;
    for (auto& [cName, cDecl] : classDefs_) {
        if (cName == name || (cName.size() > name.size() && cName.compare(cName.size() - name.size(), name.size(), name) == 0)) {
            return cDecl;
        }
    }
    return nullptr;
}

const EnumDecl* Evaluator::findEnum(const std::string& name) const {
    auto it = enumDefs_.find(name);
    if (it != enumDefs_.end()) return it->second;
    for (auto& [eName, eDecl] : enumDefs_) {
        if (eName == name || (eName.size() > name.size() && eName.compare(eName.size() - name.size(), name.size(), name) == 0)) {
            return eDecl;
        }
    }
    return nullptr;
}

const FuncDecl* Evaluator::findMethod(const ClassDecl& cls, const std::string& name) const {
    for (auto& m : cls.methods) if (m->name == name) return m.get();
    // Walk superclass hierarchy
    if (cls.superClass) {
        const ClassDecl* parent = findClass(cls.superClass->name);
        if (parent) {
            const FuncDecl* m = findMethod(*parent, name);
            if (m) return m;
        }
    }
    // Check implemented interfaces for default method implementations
    for (auto& iface : cls.interfaces) {
        auto iit = ifaceDefs_.find(iface.name);
        if (iit != ifaceDefs_.end()) {
            for (auto& m : iit->second->methods) {
                if (m->name == name && m->body) return m.get();
            }
        }
    }
    return nullptr;
}

// Find a method on an enum value (enums can have methods too)
static const FuncDecl* findEnumMethod(const EnumDecl& enm, const std::string& name) {
    for (auto& m : enm.methods) if (m->name == name) return m.get();
    return nullptr;
}

const FieldDecl* Evaluator::findField(const ClassDecl& cls, const std::string& name) const {
    for (auto& f : cls.fields) if (f->name == name) return f.get();
    if (cls.superClass) {
        const ClassDecl* parent = findClass(cls.superClass->name);
        if (parent) return findField(*parent, name);
    }
    return nullptr;
}

}
