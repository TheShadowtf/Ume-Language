// semantic.cpp — Semantic Analyzer / Type Checker for Ume
#include "../include/semantic.h"
#include <sstream>
#include <algorithm>

namespace Ume {

// ─────────────────────────────────────────────────────────────
// TypeInfo
// ─────────────────────────────────────────────────────────────
static const std::vector<std::string> kPrimitives = {
    "int","long","short","byte","float","double","bool","char","string","void","any"
};

bool TypeInfo::isPrimitive() const {
    return std::find(kPrimitives.begin(), kPrimitives.end(), name) != kPrimitives.end();
}

bool TypeInfo::isCompatibleWith(const TypeInfo& other) const {
    if (name == "any" || other.name == "any") return true;
    if (name == other.name) return true;
    // numeric widening
    static const std::vector<std::string> nums = {"byte","short","int","long","float","double"};
    auto ia = std::find(nums.begin(), nums.end(), name);
    auto ib = std::find(nums.begin(), nums.end(), other.name);
    if (ia != nums.end() && ib != nums.end()) return true;
    return false;
}

std::string TypeInfo::toString() const {
    std::string s = name;
    if (nullable) s += "?";
    if (isArray)  s += "[]";
    return s;
}

// ─────────────────────────────────────────────────────────────
// SymbolTable
// ─────────────────────────────────────────────────────────────
void SymbolTable::pushScope() { scopes_.emplace_back(); }
void SymbolTable::popScope()  { if (!scopes_.empty()) scopes_.pop_back(); }

void SymbolTable::declare(const std::string& name, TypeInfo type) {
    if (scopes_.empty()) scopes_.emplace_back();
    scopes_.back()[name] = std::move(type);
}

std::optional<TypeInfo> SymbolTable::lookup(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto jt = it->find(name);
        if (jt != it->end()) return jt->second;
    }
    return std::nullopt;
}

bool SymbolTable::isDeclaredInCurrentScope(const std::string& name) const {
    if (scopes_.empty()) return false;
    return scopes_.back().count(name) > 0;
}

// ─────────────────────────────────────────────────────────────
// SemanticAnalyzer
// ─────────────────────────────────────────────────────────────
SemanticAnalyzer::SemanticAnalyzer() {
    // Pre-register built-in types
    for (const auto& p : kPrimitives) {
        TypeInfo ti; ti.name = p;
        ClassInfo ci; ci.name = p;
        classes_[p] = ci;
    }
}

void SemanticAnalyzer::warn(const std::string& msg, int line) {
    std::string w = msg;
    if (line > 0) w = "line " + std::to_string(line) + ": " + msg;
    warnings_.push_back(w);
}

void SemanticAnalyzer::analyze(Program& program) {
    // Pass 1: collect declarations
    collectDeclarations(program);
    // Pass 2: validate bodies
    symbols_.pushScope();
    for (auto& decl : program.declarations)
        checkNode(*decl);
    symbols_.popScope();
}

// ─────────────────────────────────────────────────────────────
// Pass 1: collect
// ─────────────────────────────────────────────────────────────
void SemanticAnalyzer::collectDeclarations(Program& program) {
    for (auto& node : program.declarations) {
        if (auto* cls  = dynamic_cast<ClassDecl*>(node.get()))     collectClass(*cls);
        if (auto* ifc  = dynamic_cast<InterfaceDecl*>(node.get())) collectInterface(*ifc);
        if (auto* enm  = dynamic_cast<EnumDecl*>(node.get()))      collectEnum(*enm);
        if (auto* str  = dynamic_cast<StructDecl*>(node.get()))     collectStruct(*str);
        if (auto* func = dynamic_cast<FuncDecl*>(node.get()))      collectFunc(*func);
    }
}

void SemanticAnalyzer::collectClass(ClassDecl& cls) {
    ClassInfo ci;
    ci.name       = cls.name;
    ci.isAbstract = cls.isAbstract;
    ci.isFinal    = cls.isFinal;
    if (cls.superClass) ci.superClass = cls.superClass->name;
    for (auto& ifc : cls.interfaces) ci.interfaces.push_back(ifc.name);
    for (auto& f : cls.fields) {
        checkAttributes(f->attributes);
        TypeInfo ti = resolveAnnotation(f->type);
        ci.fields[f->name] = ti;
    }
    for (auto& p : cls.properties) {
        checkAttributes(p->attributes);
        TypeInfo ti = resolveAnnotation(p->type);
        ci.properties[p->name] = ti;
    }
    for (auto& idx : cls.indexers) {
        FunctionSignature sig; sig.returnType = resolveAnnotation(idx->type);
        for (auto& p : idx->params) sig.paramTypes.push_back(resolveAnnotation(p.type));
        ci.indexers.push_back(sig);
    }
    for (auto& m : cls.methods) {
        FunctionSignature sig;
        sig.returnType = resolveAnnotation(m->returnType);
        sig.isStatic   = m->isStatic;
        sig.isAbstract = m->isAbstract;
        for (auto& p : m->params) sig.paramTypes.push_back(resolveAnnotation(p.type));
        ci.methods[m->name] = sig;
    }
    classes_[cls.name] = std::move(ci);
}

void SemanticAnalyzer::collectInterface(InterfaceDecl& iface) {
    ClassInfo ci;
    ci.name       = iface.name;
    ci.isAbstract = true;
    for (auto& m : iface.methods) {
        FunctionSignature sig;
        sig.returnType = resolveAnnotation(m->returnType);
        sig.isAbstract = !static_cast<bool>(m->body);
        for (auto& p : m->params) sig.paramTypes.push_back(resolveAnnotation(p.type));
        ci.methods[m->name] = sig;
    }
    for (auto& p : iface.properties) {
        ci.properties[p->name] = resolveAnnotation(p->type);
    }
    for (auto& idx : iface.indexers) {
        FunctionSignature sig; sig.returnType = resolveAnnotation(idx->type);
        for (auto& p : idx->params) sig.paramTypes.push_back(resolveAnnotation(p.type));
        ci.indexers.push_back(sig);
    }
    classes_[iface.name] = std::move(ci);
}

void SemanticAnalyzer::collectEnum(EnumDecl& enm) {
    TypeInfo ti; ti.name = enm.name; ti.isEnum = true;
    enumTypes_[enm.name] = ti;
    ClassInfo ci; ci.name = enm.name;
    for (auto& v : enm.values) {
        TypeInfo vt; vt.name = enm.name;
        ci.fields[v.name] = vt;
    }
    classes_[enm.name] = std::move(ci);
}

void SemanticAnalyzer::collectStruct(StructDecl& str) {
    ClassInfo ci; ci.name = str.name;
    for (auto& f : str.fields) ci.fields[f->name] = resolveAnnotation(f->type);
    for (auto& m : str.methods) {
        FunctionSignature sig; sig.returnType = resolveAnnotation(m->returnType);
        for (auto& p : m->params) sig.paramTypes.push_back(resolveAnnotation(p.type));
        ci.methods[m->name] = sig;
    }
    classes_[str.name] = std::move(ci);
}

void SemanticAnalyzer::collectFunc(FuncDecl& func) {
    FunctionSignature sig;
    sig.returnType = resolveAnnotation(func.returnType);
    sig.isStatic   = func.isStatic;
    for (auto& p : func.params) sig.paramTypes.push_back(resolveAnnotation(p.type));
    functions_[func.name] = sig;
}

// ─────────────────────────────────────────────────────────────
// Pass 2: check
// ─────────────────────────────────────────────────────────────
void SemanticAnalyzer::checkNode(ASTNode& node) {
    if (auto* cls  = dynamic_cast<ClassDecl*>(&node))   checkClassDecl(*cls);
    if (auto* func = dynamic_cast<FuncDecl*>(&node))    checkFuncDecl(*func);
    if (auto* var  = dynamic_cast<VarDeclStmt*>(&node)) checkVarDecl(*var);
    if (auto* blk  = dynamic_cast<BlockStmt*>(&node))   checkBlock(*blk);
    if (auto* stmt = dynamic_cast<IfStmt*>(&node))      checkIfStmt(*stmt);
    if (auto* stmt = dynamic_cast<WhileStmt*>(&node))   checkWhileStmt(*stmt);
    if (auto* stmt = dynamic_cast<ForStmt*>(&node))     checkForStmt(*stmt);
    if (auto* stmt = dynamic_cast<ForEachStmt*>(&node)) checkForEachStmt(*stmt);
    if (auto* stmt = dynamic_cast<ReturnStmt*>(&node))  checkReturnStmt(*stmt);
    if (auto* stmt = dynamic_cast<ThrowStmt*>(&node))   checkThrowStmt(*stmt);
    if (auto* stmt = dynamic_cast<TryCatchStmt*>(&node)) checkTryCatch(*stmt);
    if (auto* stmt = dynamic_cast<ExprStmt*>(&node))    inferExprType(*stmt->expr);
    if (auto* unsafe = dynamic_cast<UnsafeBlock*>(&node)) {
        bool old = inUnsafe_; inUnsafe_ = true;
        checkNode(*unsafe->body);
        inUnsafe_ = old;
    }
}

void SemanticAnalyzer::checkAttributes(const std::vector<Attribute>& attrs) {
    for (const auto& attr : attrs) {
        for (const auto& arg : attr.args) {
            inferExprType(*arg);
        }
    }
}

void SemanticAnalyzer::checkFuncDecl(FuncDecl& func) {
    checkAttributes(func.attributes);
    TypeInfo rt      = resolveAnnotation(func.returnType);
    currentReturnType_ = rt;
    symbols_.pushScope();
    for (auto& p : func.params)
        symbols_.declare(p.name, resolveAnnotation(p.type));
    if (func.body) checkNode(*func.body);
    symbols_.popScope();
}

void SemanticAnalyzer::checkClassDecl(ClassDecl& cls) {
    checkAttributes(cls.attributes);
    std::string saved = currentClass_;
    currentClass_     = cls.name;
    symbols_.pushScope();
    // Declare fields in scope
    auto cit = classes_.find(cls.name);
    if (cit != classes_.end()) {
        for (auto& [n, t] : cit->second.fields) symbols_.declare(n, t);
        for (auto& [n, t] : cit->second.properties) symbols_.declare(n, t);
    }
    for (auto& m : cls.methods)   checkFuncDecl(*m);
    for (auto& p : cls.properties) {
        checkAttributes(p->attributes);
        TypeInfo savedRet = currentReturnType_;
        currentReturnType_ = resolveAnnotation(p->type);
        if (p->getDecl) checkNode(*p->getDecl);
        currentReturnType_ = savedRet;
        
        if (p->setDecl) {
            symbols_.pushScope();
            symbols_.declare("value", resolveAnnotation(p->type));
            checkNode(*p->setDecl);
            symbols_.popScope();
        }
    }
    for (auto& idx : cls.indexers) {
        symbols_.pushScope();
        for (auto& p : idx->params) symbols_.declare(p.name, resolveAnnotation(p.type));
        
        TypeInfo savedRet = currentReturnType_;
        currentReturnType_ = resolveAnnotation(idx->type);
        if (idx->getDecl) checkNode(*idx->getDecl);
        currentReturnType_ = savedRet;
        
        if (idx->setDecl) {
            symbols_.declare("value", resolveAnnotation(idx->type));
            checkNode(*idx->setDecl);
        }
        symbols_.popScope();
    }
    for (auto& ctor : cls.constructors) {
        symbols_.pushScope();
        for (auto& p : ctor->params) symbols_.declare(p.name, resolveAnnotation(p.type));
        if (ctor->body) checkNode(*ctor->body);
        symbols_.popScope();
    }
    symbols_.popScope();
    currentClass_ = saved;
}

void SemanticAnalyzer::checkBlock(BlockStmt& block) {
    symbols_.pushScope();
    for (auto& stmt : block.stmts) checkNode(*stmt);
    symbols_.popScope();
}

void SemanticAnalyzer::checkVarDecl(VarDeclStmt& stmt) {
    if (symbols_.isDeclaredInCurrentScope(stmt.name))
        warn("Variable '" + stmt.name + "' already declared in this scope", stmt.line);
    TypeInfo ti;
    if (stmt.initializer) {
        TypeInfo initType = inferExprType(*stmt.initializer);
        if (stmt.type.isVar()) {
            ti = initType;
        } else {
            ti = resolveAnnotation(stmt.type);
            if (!isAssignable(ti, initType))
                warn("Type mismatch in variable '" + stmt.name + "'", stmt.line);
        }
    } else {
        if (stmt.type.isVar())
            warn("'var' variable '" + stmt.name + "' requires an initializer", stmt.line);
        ti = resolveAnnotation(stmt.type);
    }
    symbols_.declare(stmt.name, ti);
}

void SemanticAnalyzer::checkIfStmt(IfStmt& stmt) {
    inferExprType(*stmt.condition);
    checkNode(*stmt.thenBranch);
    if (stmt.elseBranch) checkNode(*stmt.elseBranch);
}

void SemanticAnalyzer::checkWhileStmt(WhileStmt& stmt) {
    inferExprType(*stmt.condition);
    checkNode(*stmt.body);
}

void SemanticAnalyzer::checkForStmt(ForStmt& stmt) {
    symbols_.pushScope();
    if (stmt.init)      checkNode(*stmt.init);
    if (stmt.condition) inferExprType(*stmt.condition);
    if (stmt.update)    inferExprType(*stmt.update);
    checkNode(*stmt.body);
    symbols_.popScope();
}

void SemanticAnalyzer::checkForEachStmt(ForEachStmt& stmt) {
    symbols_.pushScope();
    inferExprType(*stmt.iterable);
    TypeInfo ti = resolveAnnotation(stmt.varType);
    symbols_.declare(stmt.varName, ti);
    checkNode(*stmt.body);
    symbols_.popScope();
}

void SemanticAnalyzer::checkReturnStmt(ReturnStmt& stmt) {
    if (stmt.value) {
        TypeInfo rt = inferExprType(*stmt.value);
        if (!currentReturnType_.isVoid() && !isAssignable(currentReturnType_, rt))
            warn("Return type mismatch", stmt.line);
    }
}

void SemanticAnalyzer::checkThrowStmt(ThrowStmt& stmt) {
    inferExprType(*stmt.value);
}

void SemanticAnalyzer::checkTryCatch(TryCatchStmt& stmt) {
    checkNode(*stmt.tryBody);
    for (auto& cc : stmt.catches) {
        symbols_.pushScope();
        TypeInfo ti = resolveAnnotation(cc.type);
        symbols_.declare(cc.varName, ti);
        checkNode(*cc.body);
        symbols_.popScope();
    }
    if (stmt.finallyBody) checkNode(*stmt.finallyBody);
}

// ─────────────────────────────────────────────────────────────
// Type inference
// ─────────────────────────────────────────────────────────────
TypeInfo SemanticAnalyzer::inferExprType(ASTNode& expr) {
    TypeInfo unknown; unknown.name = "any";

    if (dynamic_cast<IntLiteralExpr*>(&expr))    { TypeInfo t; t.name="int";    return t; }
    if (dynamic_cast<FloatLiteralExpr*>(&expr))  { TypeInfo t; t.name="double"; return t; }
    if (dynamic_cast<StringLiteralExpr*>(&expr)) { TypeInfo t; t.name="string"; return t; }
    if (dynamic_cast<CharLiteralExpr*>(&expr))   { TypeInfo t; t.name="char";   return t; }
    if (dynamic_cast<BoolLiteralExpr*>(&expr))   { TypeInfo t; t.name="bool";   return t; }
    if (dynamic_cast<NullLiteralExpr*>(&expr))   { TypeInfo t; t.name="null"; t.nullable=true; return t; }

    if (auto* id = dynamic_cast<IdentifierExpr*>(&expr)) {
        auto opt = symbols_.lookup(id->name);
        if (opt) return *opt;
        // Check if it's a class name
        auto cit = classes_.find(id->name);
        if (cit != classes_.end()) { TypeInfo t; t.name=id->name; t.isClass=true; return t; }
        return unknown;
    }

    if (auto* bin = dynamic_cast<BinaryExpr*>(&expr))     return inferBinaryType(*bin);
    if (auto* call = dynamic_cast<CallExpr*>(&expr))      return inferCallType(*call);
    if (auto* ma   = dynamic_cast<MemberAccessExpr*>(&expr)) return inferMemberType(*ma);
    if (auto* ne   = dynamic_cast<NewExpr*>(&expr))       return inferNewType(*ne);

    if (auto* assign = dynamic_cast<AssignExpr*>(&expr)) {
        inferExprType(*assign->target);
        return inferExprType(*assign->value);
    }
    if (auto* ternary = dynamic_cast<TernaryExpr*>(&expr)) return inferExprType(*ternary->thenExpr);
    if (auto* nc = dynamic_cast<NullCoalesceExpr*>(&expr)) return inferExprType(*nc->right);

    if (auto* cast = dynamic_cast<CastExpr*>(&expr)) return resolveAnnotation(cast->targetType);
    if (auto* si = dynamic_cast<StructInitExpr*>(&expr)) {
        if (si->typeName) return inferExprType(*si->typeName);
        return unknown;
    }
    if (auto* alloc = dynamic_cast<AllocExpr*>(&expr)) {
        TypeInfo t = resolveAnnotation(alloc->type);
        return t;
    }
    if (auto* deref = dynamic_cast<DerefExpr*>(&expr)) {
        TypeInfo t = inferExprType(*deref->pointer);
        if (!t.name.empty() && t.name.back() == '*') t.name.pop_back();
        return t;
    }
    if (auto* addrof = dynamic_cast<AddressOfExpr*>(&expr)) {
        TypeInfo t = inferExprType(*addrof->value);
        t.name += "*";
        return t;
    }
    if (auto* free = dynamic_cast<FreeExpr*>(&expr)) {
        TypeInfo t; t.name = "void"; return t;
    }

    if (auto* interp = dynamic_cast<InterpolatedStringExpr*>(&expr)) {
        TypeInfo t; t.name = "string"; return t;
    }

    if (auto* lambda = dynamic_cast<LambdaExpr*>(&expr)) {
        TypeInfo t; t.name = "func"; return t;
    }

    return unknown;
}

TypeInfo SemanticAnalyzer::inferBinaryType(BinaryExpr& expr) {
    TypeInfo l = inferExprType(*expr.left);
    TypeInfo r = inferExprType(*expr.right);
    const std::string& op = expr.op;
    // Comparison/logical ops → bool
    if (op=="==" || op=="!=" || op=="<" || op==">" || op=="<=" || op==">=" ||
        op=="&&" || op=="||")
    { TypeInfo t; t.name="bool"; return t; }
    // String concatenation
    if (op=="+" && (l.name=="string" || r.name=="string")) { TypeInfo t; t.name="string"; return t; }
    // Numeric: pick wider type
    static const std::vector<std::string> nums = {"byte","short","int","long","float","double"};
    auto il = std::find(nums.begin(), nums.end(), l.name);
    auto ir = std::find(nums.begin(), nums.end(), r.name);
    if (il != nums.end() && ir != nums.end())
        return (il >= ir) ? l : r;
    return l;
}

TypeInfo SemanticAnalyzer::inferCallType(CallExpr& expr) {
    for (auto& arg : expr.args) inferExprType(*arg);
    // If callee is an identifier, look up function
    if (auto* id = dynamic_cast<IdentifierExpr*>(expr.callee.get())) {
        auto fit = functions_.find(id->name);
        if (fit != functions_.end()) return fit->second.returnType;
    }
    // If callee is a member access, try class method
    if (auto* ma = dynamic_cast<MemberAccessExpr*>(expr.callee.get())) {
        TypeInfo objType = inferExprType(*ma->object);
        auto cit = classes_.find(objType.name);
        if (cit != classes_.end()) {
            auto mit = cit->second.methods.find(ma->member);
            if (mit != cit->second.methods.end()) return mit->second.returnType;
        }
    }
    TypeInfo t; t.name = "any"; return t;
}

TypeInfo SemanticAnalyzer::inferMemberType(MemberAccessExpr& expr) {
    TypeInfo objType = inferExprType(*expr.object);
    auto cit = classes_.find(objType.name);
    if (cit != classes_.end()) {
        auto fit = cit->second.fields.find(expr.member);
        if (fit != cit->second.fields.end()) return fit->second;
        auto pit = cit->second.properties.find(expr.member);
        if (pit != cit->second.properties.end()) {
            expr.isProperty = true;
            return pit->second;
        }
        auto mit = cit->second.methods.find(expr.member);
        if (mit != cit->second.methods.end()) return mit->second.returnType;
    }
    TypeInfo t; t.name = "any"; return t;
}

TypeInfo SemanticAnalyzer::inferNewType(NewExpr& expr) {
    TypeInfo t;
    t.name = expr.type.name;
    auto cit = classes_.find(expr.type.name);
    if (cit != classes_.end()) t.isClass = true;
    t.isArray = expr.type.isArray;
    return t;
}

TypeInfo SemanticAnalyzer::resolveAnnotation(const TypeAnnotation& ta) {
    TypeInfo ti;
    ti.name     = ta.name;
    ti.nullable = ta.nullable;
    ti.isArray  = ta.isArray;
    auto cit    = classes_.find(ta.name);
    if (cit != classes_.end()) ti.isClass = !cit->second.fields.empty() || !cit->second.methods.empty();
    if (enumTypes_.count(ta.name)) ti.isEnum = true;
    return ti;
}

bool SemanticAnalyzer::isAssignable(const TypeInfo& target, const TypeInfo& source) const {
    if (target.name == "any" || source.name == "any") return true;
    if (source.name == "null" && (target.nullable || target.isClass || target.isArray)) return true;
    return target.isCompatibleWith(source);
}

} // namespace Ume
