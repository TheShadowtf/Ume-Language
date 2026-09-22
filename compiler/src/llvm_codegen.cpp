// llvm_codegen.cpp — LLVM IR text emitter for Ume Language (Phase 3)
//
// Strategy: alloca/store/load for every local variable.
// This avoids building SSA phi nodes manually; LLVM's mem2reg pass
// (run automatically by clang -O1+) promotes them to registers.
//
// Compile output with: clang -O2 out.ll -o out   (Windows/Linux/macOS)
//                  or: clang-cl /O2 out.ll /Fe:out.exe  (MSVC-compatible)

#include "../include/llvm_codegen.h"
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace Ume {

LLVMCodegen::LLVMCodegen(LLVMCodegenOptions opts) : opts_(std::move(opts)) {}

std::string LLVMCodegen::llvmTy(const std::string& name) const {
    if (name == "int"  || name == "long")             return "i64";
    if (name == "short")                              return "i32";
    if (name == "byte")                               return "i8";
    if (name == "double" || name == "float")          return "double";
    if (name == "bool")                               return "i1";
    if (name == "char")                               return "i8";
    if (name == "string")                             return "i8*";
    if (name == "void")                               return "void";
    if (name == "any"  || name == "var")              return "i8*";
    // User-defined types — opaque pointer (works with LLVM 14 and 15+ opaque pointers)
    return "i8*";
}

std::string LLVMCodegen::llvmTy(const TypeAnnotation& ta) const {
    if (ta.nullable) return "i8*";
    if (ta.isArray)  return "i8*";
    return llvmTy(ta.name);
}

std::string LLVMCodegen::defaultVal(const std::string& ty) const {
    if (ty == "i64" || ty == "i32" || ty == "i8") return "0";
    if (ty == "i1")     return "false";
    if (ty == "double") return "0.0";
    return "null";
}

std::string LLVMCodegen::globalStr(const std::string& raw) {
    auto it = strCache_.find(raw);
    if (it != strCache_.end()) return it->second;

    std::string name = "@.str." + std::to_string(gstrN_++);
    strCache_[raw] = name;

    // Escape for LLVM IR string syntax
    std::string esc;
    for (unsigned char c : raw) {
        switch (c) {
        case '"':  esc += "\\22"; break;
        case '\\': esc += "\\5C"; break;
        case '\n': esc += "\\0A"; break;
        case '\t': esc += "\\09"; break;
        case '\r': esc += "\\0D"; break;
        default:
            if (c < 32 || c > 126) {
                char buf[5]; snprintf(buf, sizeof(buf), "\\%02X", (unsigned)c);
                esc += buf;
            } else {
                esc += (char)c;
            }
        }
    }

    size_t len = raw.size() + 1; // +1 for null terminator
    globals_ << name << " = private unnamed_addr constant ["
             << len << " x i8] c\"" << esc << "\\00\", align 1\n";
    return name;
}

void LLVMCodegen::pushScope() { scopes_.push_back({}); }
void LLVMCodegen::popScope()  { if (!scopes_.empty()) scopes_.pop_back(); }

void LLVMCodegen::defVar(const std::string& n, const std::string& a, const std::string& t) {
    if (!scopes_.empty()) scopes_.back()[n] = {a, t};
}

LLVMCodegen::VarInfo* LLVMCodegen::lookupVar(const std::string& n) {
    for (int i = (int)scopes_.size() - 1; i >= 0; i--) {
        auto it = scopes_[i].find(n);
        if (it != scopes_[i].end()) return &it->second;
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────
// Type coercion — emits conversion instructions as needed
// Returns the (possibly new) register holding the coerced value
// ─────────────────────────────────────────────────────────────
std::string LLVMCodegen::coerce(const std::string& val,
                                 const std::string& fromTy,
                                 const std::string& toTy) {
    if (fromTy == toTy || toTy.empty()) return val;

    std::string r = tmp();

    // Widening / narrowing integers
    if (fromTy == "i1"  && toTy == "i64") { emit("  " + r + " = zext i1 "    + val + " to i64\n");    return r; }
    if (fromTy == "i8"  && toTy == "i64") { emit("  " + r + " = sext i8 "    + val + " to i64\n");    return r; }
    if (fromTy == "i32" && toTy == "i64") { emit("  " + r + " = sext i32 "   + val + " to i64\n");    return r; }
    if (fromTy == "i64" && toTy == "i32") { emit("  " + r + " = trunc i64 "  + val + " to i32\n");   return r; }
    if (fromTy == "i64" && toTy == "i8")  { emit("  " + r + " = trunc i64 "  + val + " to i8\n");    return r; }
    if (fromTy == "i64" && toTy == "i1")  { emit("  " + r + " = icmp ne i64 "+ val + ", 0\n");       return r; }
    if (fromTy == "i8"  && toTy == "i1")  { emit("  " + r + " = icmp ne i8 " + val + ", 0\n");       return r; }
    if (fromTy == "i1"  && toTy == "i8")  { emit("  " + r + " = zext i1 "    + val + " to i8\n");    return r; }
    if (fromTy == "i32" && toTy == "i1")  { emit("  " + r + " = icmp ne i32 "+ val + ", 0\n");       return r; }

    // Integer <-> float
    if (fromTy == "i64"    && toTy == "double") { emit("  " + r + " = sitofp i64 "    + val + " to double\n"); return r; }
    if (fromTy == "i32"    && toTy == "double") { emit("  " + r + " = sitofp i32 "    + val + " to double\n"); return r; }
    if (fromTy == "i8"     && toTy == "double") { emit("  " + r + " = sitofp i8 "     + val + " to double\n"); return r; }
    if (fromTy == "double" && toTy == "i64")    { emit("  " + r + " = fptosi double " + val + " to i64\n");    return r; }
    if (fromTy == "double" && toTy == "i32")    { emit("  " + r + " = fptosi double " + val + " to i32\n");    return r; }
    if (fromTy == "i1"     && toTy == "double") {
        std::string ext = tmp();
        emit("  " + ext + " = zext i1 " + val + " to i64\n");
        emit("  " + r   + " = sitofp i64 " + ext + " to double\n");
        return r;
    }

    // Pointer casts
    if (toTy == "i8*" && fromTy.back() == '*') {
        emit("  " + r + " = bitcast " + fromTy + " " + val + " to i8*\n"); return r;
    }
    if (fromTy == "i8*" && toTy.back() == '*') {
        emit("  " + r + " = bitcast i8* " + val + " to " + toTy + "\n"); return r;
    }
    if (fromTy == "i8*" && toTy == "i64") {
        emit("  " + r + " = ptrtoint i8* " + val + " to i64\n"); return r;
    }
    if (fromTy == "i64" && toTy == "i8*") {
        emit("  " + r + " = inttoptr i64 " + val + " to i8*\n"); return r;
    }

    // No-op fallback
    return val;
}

LLVMCodegen::TV LLVMCodegen::toBool(const TV& v) {
    if (v.second == "i1") return v;
    std::string r = tmp();
    if (v.second == "i64" || v.second == "i32")
        emit("  " + r + " = icmp ne " + v.second + " " + v.first + ", 0\n");
    else if (v.second == "i8")
        emit("  " + r + " = icmp ne i8 " + v.first + ", 0\n");
    else if (v.second == "double")
        emit("  " + r + " = fcmp one double " + v.first + ", 0.0\n");
    else if (v.second == "i8*")
        emit("  " + r + " = icmp ne i8* " + v.first + ", null\n");
    else
        emit("  " + r + " = icmp ne i64 0, 0\n"); // fallback: always true
    return {r, "i1"};
}

LLVMCodegen::TV LLVMCodegen::toStr(const TV& v) {
    if (v.second == "i8*") return v;
    std::string r = tmp();
    if (v.second == "i64" || v.second == "i32" || v.second == "i8") {
        std::string v64 = v.first;
        if (v.second != "i64") v64 = coerce(v.first, v.second, "i64");
        emit("  " + r + " = call i8* @__ume_i64tostr(i64 " + v64 + ")\n");
    } else if (v.second == "double") {
        emit("  " + r + " = call i8* @__ume_dbltostr(double " + v.first + ")\n");
    } else if (v.second == "i1") {
        std::string ext = tmp();
        emit("  " + ext + " = zext i1 " + v.first + " to i64\n");
        emit("  " + r   + " = call i8* @__ume_booltostr(i64 " + ext + ")\n");
    } else {
        // Unknown pointer type — return as-is
        return v;
    }
    return {r, "i8*"};
}

LLVMCodegen::TV LLVMCodegen::emitStrConcat(const TV& a, const TV& b) {
    auto [as, at] = toStr(a);
    auto [bs, bt] = toStr(b);
    std::string r = tmp();
    emit("  " + r + " = call i8* @__ume_strconcat(i8* " + as + ", i8* " + bs + ")\n");
    return {r, "i8*"};
}

void LLVMCodegen::emitBuiltins() {
    globals_ << "\n; ── External C library declarations ──────────────────────────\n";
    globals_ << "declare i32    @printf(i8* nocapture readonly, ...)\n";
    globals_ << "declare i32    @scanf(i8* nocapture readonly, ...)\n";
    globals_ << "declare i32    @puts(i8* nocapture readonly)\n";
    globals_ << "declare i8*    @malloc(i64)\n";
    globals_ << "declare void   @free(i8*)\n";
    globals_ << "declare i64    @strlen(i8*)\n";
    globals_ << "declare i8*    @strcpy(i8*, i8*)\n";
    globals_ << "declare i8*    @strcat(i8*, i8*)\n";
    globals_ << "declare i32    @strcmp(i8*, i8*)\n";
    globals_ << "declare i8*    @strdup(i8*)\n";
    globals_ << "declare i64    @atol(i8*)\n";
    globals_ << "declare double @atof(i8*)\n";
    globals_ << "declare i64    @snprintf(i8*, i64, i8*, ...)\n";
    globals_ << "declare void   @exit(i32) noreturn\n";
    globals_ << "declare double @sqrt(double)\n";
    globals_ << "declare double @pow(double, double)\n";
    globals_ << "declare double @fabs(double)\n";
    globals_ << "declare double @floor(double)\n";
    globals_ << "declare double @ceil(double)\n";
    globals_ << "declare double @round(double)\n";
    globals_ << "declare double @sin(double)\n";
    globals_ << "declare double @cos(double)\n";
    globals_ << "declare double @tan(double)\n";
    globals_ << "declare double @asin(double)\n";
    globals_ << "declare double @acos(double)\n";
    globals_ << "declare double @atan(double)\n";
    globals_ << "declare double @atan2(double, double)\n";
    globals_ << "declare double @log(double)\n";
    globals_ << "declare double @log2(double)\n";
    globals_ << "declare double @log10(double)\n";
    globals_ << "declare double @exp(double)\n";
    globals_ << "\n";

    // ── Helper functions (private linkage, inlined by optimizer) ──
    cur_ = &helpers_;
    helpers_ << "\n; ── Ume runtime helpers ───────────────────────────────────────\n";

    // __ume_strconcat(a, b) -> i8*
    helpers_ << "define private i8* @__ume_strconcat(i8* %a, i8* %b) {\n";
    helpers_ << "  %la  = call i64 @strlen(i8* %a)\n";
    helpers_ << "  %lb  = call i64 @strlen(i8* %b)\n";
    helpers_ << "  %t1  = add i64 %la, %lb\n";
    helpers_ << "  %sz  = add i64 %t1, 1\n";
    helpers_ << "  %buf = call i8* @malloc(i64 %sz)\n";
    helpers_ << "  call i8* @strcpy(i8* %buf, i8* %a)\n";
    helpers_ << "  call i8* @strcat(i8* %buf, i8* %b)\n";
    helpers_ << "  ret i8* %buf\n}\n\n";

    // __ume_i64tostr(v) -> i8*
    helpers_ << "define private i8* @__ume_i64tostr(i64 %v) {\n";
    helpers_ << "  %buf  = call i8* @malloc(i64 32)\n";
    helpers_ << "  %fmtp = getelementptr inbounds [5 x i8], [5 x i8]* @__fmt_lld, i64 0, i64 0\n";
    helpers_ << "  call i64 @snprintf(i8* %buf, i64 32, i8* %fmtp, i64 %v)\n";
    helpers_ << "  ret i8* %buf\n}\n\n";

    // __ume_dbltostr(v) -> i8*
    helpers_ << "define private i8* @__ume_dbltostr(double %v) {\n";
    helpers_ << "  %buf  = call i8* @malloc(i64 64)\n";
    helpers_ << "  %fmtp = getelementptr inbounds [3 x i8], [3 x i8]* @__fmt_g, i64 0, i64 0\n";
    helpers_ << "  call i64 @snprintf(i8* %buf, i64 64, i8* %fmtp, double %v)\n";
    helpers_ << "  ret i8* %buf\n}\n\n";

    // __ume_booltostr(v: i64) -> i8*
    helpers_ << "define private i8* @__ume_booltostr(i64 %v) {\n";
    helpers_ << "  %c  = icmp ne i64 %v, 0\n";
    helpers_ << "  br i1 %c, label %btrue, label %bfalse\n";
    helpers_ << "btrue:\n";
    helpers_ << "  %pt = getelementptr inbounds [5 x i8], [5 x i8]* @__str_true,  i64 0, i64 0\n";
    helpers_ << "  ret i8* %pt\n";
    helpers_ << "bfalse:\n";
    helpers_ << "  %pf = getelementptr inbounds [6 x i8], [6 x i8]* @__str_false, i64 0, i64 0\n";
    helpers_ << "  ret i8* %pf\n}\n\n";

    // ── Well-known format / literal strings ───────────────────
    globals_ << "@__fmt_lld   = private unnamed_addr constant [5 x i8] c\"%lld\\00\",    align 1\n";
    globals_ << "@__fmt_g     = private unnamed_addr constant [3 x i8] c\"%g\\00\",      align 1\n";
    globals_ << "@__fmt_s     = private unnamed_addr constant [3 x i8] c\"%s\\00\",      align 1\n";
    globals_ << "@__fmt_snl   = private unnamed_addr constant [4 x i8] c\"%s\\0A\\00\",  align 1\n";
    globals_ << "@__fmt_lldnl = private unnamed_addr constant [6 x i8] c\"%lld\\0A\\00\",align 1\n";
    globals_ << "@__fmt_gnl   = private unnamed_addr constant [4 x i8] c\"%g\\0A\\00\",  align 1\n";
    globals_ << "@__str_true  = private unnamed_addr constant [5 x i8] c\"true\\00\",   align 1\n";
    globals_ << "@__str_false = private unnamed_addr constant [6 x i8] c\"false\\00\",  align 1\n";
    globals_ << "\n";
}

void LLVMCodegen::collectClasses(const Program& prog) {
    for (auto& node : prog.declarations)
        if (auto* cls = dynamic_cast<const ClassDecl*>(node.get()))
            classDecls_[cls->name] = cls;
}

LLVMCodegenResult LLVMCodegen::generate(const Program& program) {
    // Reset
    types_.str(""); types_.clear();
    globals_.str(""); globals_.clear();
    helpers_.str(""); helpers_.clear();
    funcs_.str(""); funcs_.clear();
    tmpN_ = lblN_ = gstrN_ = 0;
    scopes_.clear(); classDecls_.clear(); strCache_.clear();
    hasMainWrapper_ = false;

    try {
        collectClasses(program);
        emitBuiltins();

        // Emit named struct types
        types_ << "; ── Struct types ──────────────────────────────────────────────\n";
        for (auto& node : program.declarations) {
            if (auto* cls = dynamic_cast<const ClassDecl*>(node.get())) {
                types_ << "%class." << cls->name << " = type {";
                bool first = true;
                if (cls->superClass) { types_ << " i8*"; first = false; } // parent ptr
                for (auto& f : cls->fields) {
                    if (!first) types_ << ",";
                    types_ << " " << llvmTy(f->type);
                    first = false;
                }
                if (first) types_ << " i8"; // empty struct needs at least one byte
                types_ << " }\n";
            }
        }
        types_ << "\n";

        // Generate functions
        cur_ = &funcs_;
        for (auto& node : program.declarations)
            genProgramDecl(*node);

        if (!hasMainWrapper_) {
            return {false, "No entry point found. Program must define 'public static func void main()' in class Main or a top-level 'func void main()'.", ""};
        }

        // Assemble
        std::ostringstream out;
        out << "; Ume Language — LLVM IR (Phase 3 backend)\n";
        out << "; Compile: clang -O2 -o <out> <file.ll>\n\n";
        out << "target datalayout = \"e-m:w-p270:32:32-p271:32:32-p272:64:64"
               "-i64:64-f80:128-n8:16:32:64-S128\"\n";
        out << "target triple = \"x86_64-pc-windows-msvc19.51.36243\"\n\n";
        out << types_.str();
        out << globals_.str();
        out << helpers_.str();
        out << funcs_.str();

        return {true, "", out.str()};
    } catch (const std::exception& ex) {
        return {false, ex.what(), ""};
    }
}

void LLVMCodegen::genProgramDecl(const ASTNode& node) {
    cur_ = &funcs_;
    if (auto* cls = dynamic_cast<const ClassDecl*>(&node)) { genClassDecl(*cls); return; }
    if (auto* fn  = dynamic_cast<const FuncDecl*>(&node))  { genFuncDecl(*fn);   return; }
    // PackageDecl, ImportDecl, IncludeDirective — skip
}

void LLVMCodegen::genClassDecl(const ClassDecl& cls) {
    // Constructors → @ClassName__new(params) -> %class.ClassName*
    for (auto& ctor : cls.constructors) {
        std::string selfTy = "%class." + cls.name + "*";
        cur_ = &funcs_;
        funcs_ << "\ndefine " << selfTy << " @" << cls.name << "__new(";
        bool first = true;
        for (auto& p : ctor->params) {
            if (!first) funcs_ << ", ";
            funcs_ << llvmTy(p.type) << " %" << p.name;
            first = false;
        }
        funcs_ << ") {\nentry:\n";
        tmpN_ = 0; lblN_ = 0;
        pushScope();
        curClassName_ = cls.name;

        // Allocate struct on heap
        size_t fieldCount = cls.fields.size() + (cls.superClass ? 1 : 0);
        size_t sz = std::max(fieldCount, (size_t)1) * 8;
        std::string rawP = tmp();
        funcs_ << "  " << rawP << " = call i8* @malloc(i64 " << sz << ")\n";
        std::string selfP = tmp();
        funcs_ << "  " << selfP << " = bitcast i8* " << rawP << " to " << selfTy << "\n";

        // Put self in an alloca so body can reference 'this'
        std::string selfA = tmp();
        funcs_ << "  " << selfA << " = alloca " << selfTy << "\n";
        funcs_ << "  store " << selfTy << " " << selfP << ", " << selfTy << "* " << selfA << "\n";
        defVar("this", selfA, selfTy);

        // Alloca params
        for (auto& p : ctor->params) {
            std::string t = llvmTy(p.type);
            std::string a = tmp();
            funcs_ << "  " << a << " = alloca " << t << "\n";
            funcs_ << "  store " << t << " %" << p.name << ", " << t << "* " << a << "\n";
            defVar(p.name, a, t);
        }

        // Execute body
        if (ctor->body)
            if (auto* blk = dynamic_cast<const BlockStmt*>(ctor->body.get()))
                genBlock(*blk);

        // Return self
        std::string retR = tmp();
        funcs_ << "  " << retR << " = load " << selfTy << ", " << selfTy << "* " << selfA << "\n";
        funcs_ << "  ret " << selfTy << " " << retR << "\n}\n";
        popScope();
        curClassName_ = "";
    }

    // Methods
    for (auto& m : cls.methods) genFuncDecl(*m, cls.name);

    // Emit @main wrapper if this is the Main class with a static main()
    if (cls.name == "Main" && !hasMainWrapper_) {
        for (auto& m : cls.methods) {
            if (m->name == "main") {
                if (m->isStatic) {
                    funcs_ << "\ndefine i32 @main(i32 %argc, i8** %argv) {\nentry:\n";
                    funcs_ << "  call void @Main__main()\n";
                    funcs_ << "  ret i32 0\n}\n";
                    hasMainWrapper_ = true;
                    break;
                } else {
                    throw std::runtime_error("Entry point 'main' in class 'Main' must be declared static ('public static func void main()').");
                }
            }
        }
    }
}

void LLVMCodegen::genFuncDecl(const FuncDecl& fn, const std::string& cls) {
    std::string retTy  = llvmTy(fn.returnType);
    std::string fnName = cls.empty() ? fn.name : (cls + "__" + fn.name);

    // Top-level main() without a class → wrap in @main
    if (cls.empty() && fn.name == "main" && !hasMainWrapper_) {
        funcs_ << "\ndefine i32 @main(i32 %argc, i8** %argv) {\nentry:\n";
        funcs_ << "  call " << retTy << " @__ume_main()\n";
        funcs_ << "  ret i32 0\n}\n";
        hasMainWrapper_ = true;
        fnName = "__ume_main";
    }

    cur_ = &funcs_;
    funcs_ << "\ndefine " << retTy << " @" << fnName << "(";
    bool first = true;
    if (!cls.empty() && !fn.isStatic) {
        funcs_ << "%class." << cls << "* %self";
        first = false;
    }
    for (auto& p : fn.params) {
        if (!first) funcs_ << ", ";
        funcs_ << llvmTy(p.type) << " %" << p.name;
        first = false;
    }
    funcs_ << ") {\nentry:\n";

    tmpN_ = 0; lblN_ = 0;
    curRetTy_    = retTy;
    curFnName_   = fnName;
    curClassName_ = cls;
    pushScope();

    // Alloca self
    if (!cls.empty() && !fn.isStatic) {
        std::string selfTy = "%class." + cls + "*";
        std::string a = tmp();
        funcs_ << "  " << a << " = alloca " << selfTy << "\n";
        funcs_ << "  store " << selfTy << " %self, " << selfTy << "* " << a << "\n";
        defVar("this", a, selfTy);
    }
    // Alloca params
    for (auto& p : fn.params) {
        std::string t = llvmTy(p.type);
        std::string a = tmp();
        funcs_ << "  " << a << " = alloca " << t << "\n";
        funcs_ << "  store " << t << " %" << p.name << ", " << t << "* " << a << "\n";
        defVar(p.name, a, t);
    }

    bool terminated = false;
    if (fn.body) {
        if (auto* blk = dynamic_cast<const BlockStmt*>(fn.body.get()))
            terminated = genBlock(*blk);
        else
            terminated = genStmt(*fn.body);
    }

    if (!terminated) {
        if      (retTy == "void")   funcs_ << "  ret void\n";
        else if (retTy == "i64")    funcs_ << "  ret i64 0\n";
        else if (retTy == "i32")    funcs_ << "  ret i32 0\n";
        else if (retTy == "double") funcs_ << "  ret double 0.0\n";
        else if (retTy == "i1")     funcs_ << "  ret i1 false\n";
        else if (retTy == "i8*")    funcs_ << "  ret i8* null\n";
        else                        funcs_ << "  ret " << retTy << " zeroinitializer\n";
    }
    funcs_ << "}\n";
    popScope();
    curClassName_ = "";
}

bool LLVMCodegen::genStmt(const ASTNode& node) {
    if (auto* n = dynamic_cast<const BlockStmt*>(&node))    return genBlock(*n);
    if (auto* n = dynamic_cast<const VarDeclStmt*>(&node))  { genVarDecl(*n); return false; }
    if (auto* n = dynamic_cast<const ReturnStmt*>(&node))   return genReturn(*n);
    if (auto* n = dynamic_cast<const IfStmt*>(&node))       return genIf(*n);
    if (auto* n = dynamic_cast<const WhileStmt*>(&node))    return genWhile(*n);
    if (auto* n = dynamic_cast<const ForStmt*>(&node))      return genFor(*n);
    if (auto* n = dynamic_cast<const ForEachStmt*>(&node))  { genForEach(*n); return false; }
    if (auto* n = dynamic_cast<const ExprStmt*>(&node))     { genExpr(*n->expr); return false; }
    if (dynamic_cast<const BreakStmt*>(&node)) {
        emit("  br label %brk\n"); emit(lbl() + ":\n"); return true;
    }
    if (dynamic_cast<const ContinueStmt*>(&node)) {
        emit("  br label %ctn\n"); emit(lbl() + ":\n"); return true;
    }
    if (auto* n = dynamic_cast<const ThrowStmt*>(&node)) {
        emit("  call void @exit(i32 1)\n  unreachable\n");
        emit(lbl() + ":\n"); return true;
    }
    if (auto* n = dynamic_cast<const TryCatchStmt*>(&node)) {
        emit("  ; try/catch (exception handling not fully supported in LLVM MVP)\n");
        if (n->tryBody)     genStmt(*n->tryBody);
        if (n->finallyBody) genStmt(*n->finallyBody);
        return false;
    }
    if (auto* n = dynamic_cast<const DoWhileStmt*>(&node)) {
        std::string bodyL = lbl(), condL = lbl(), endL = lbl();
        emit("  br label %" + bodyL + "\n");
        emit(bodyL + ":\n"); genStmt(*n->body);
        emit("  br label %" + condL + "\n");
        emit(condL + ":\n");
        auto [cv, ct] = genExpr(*n->condition);
        auto [bv, bt] = toBool({cv, ct});
        emit("  br i1 " + bv + ", label %" + bodyL + ", label %" + endL + "\n");
        emit(endL + ":\n"); return false;
    }
    if (auto* n = dynamic_cast<const SwitchStmt*>(&node)) {
        auto [sv, st] = genExpr(*n->value);
        std::string endL = lbl();
        for (auto& c : n->cases) {
            if (!c.isDefault && c.value) {
                auto [cv, ct] = genExpr(*c.value);
                std::string cmpR = tmp(), thenL = lbl(), skipL = lbl();
                emit("  " + cmpR + " = icmp eq " + st + " " + sv + ", " + coerce(cv, ct, st) + "\n");
                emit("  br i1 " + cmpR + ", label %" + thenL + ", label %" + skipL + "\n");
                emit(thenL + ":\n");
                for (auto& s : c.stmts) genStmt(*s);
                emit("  br label %" + endL + "\n");
                emit(skipL + ":\n");
            }
        }
        for (auto& c : n->cases)
            if (c.isDefault) for (auto& s : c.stmts) genStmt(*s);
        emit("  br label %" + endL + "\n");
        emit(endL + ":\n"); return false;
    }
    return false;
}

bool LLVMCodegen::genBlock(const BlockStmt& b) {
    pushScope();
    bool terminated = false;
    for (auto& s : b.stmts) {
        if (terminated) break;
        terminated = genStmt(*s);
    }
    popScope();
    return terminated;
}

void LLVMCodegen::genVarDecl(const VarDeclStmt& v) {
    std::string ty = llvmTy(v.type);
    std::string a  = tmp();
    emit("  " + a + " = alloca " + ty + "\n");
    if (v.initializer) {
        auto [val, valTy] = genExpr(*v.initializer);
        emit("  store " + ty + " " + coerce(val, valTy, ty) + ", " + ty + "* " + a + "\n");
    } else {
        emit("  store " + ty + " " + defaultVal(ty) + ", " + ty + "* " + a + "\n");
    }
    defVar(v.name, a, ty);
}

bool LLVMCodegen::genReturn(const ReturnStmt& r) {
    if (!r.value) {
        emit("  ret void\n");
    } else {
        auto [val, ty] = genExpr(*r.value);
        emit("  ret " + curRetTy_ + " " + coerce(val, ty, curRetTy_) + "\n");
    }
    emit(lbl() + ":\n"); // dead label for code after ret
    return true;
}

bool LLVMCodegen::genIf(const IfStmt& s) {
    auto [cv, ct] = genExpr(*s.condition);
    auto [bv, bt] = toBool({cv, ct});
    std::string thenL = lbl(), elseL = lbl(), endL = lbl();
    emit("  br i1 " + bv + ", label %" + thenL
         + ", label %" + (s.elseBranch ? elseL : endL) + "\n");
    emit(thenL + ":\n");
    bool thenTerm = genStmt(*s.thenBranch);
    if (!thenTerm) emit("  br label %" + endL + "\n");
    if (s.elseBranch) {
        emit(elseL + ":\n");
        bool elseTerm = genStmt(*s.elseBranch);
        if (!elseTerm) emit("  br label %" + endL + "\n");
    }
    emit(endL + ":\n");
    return false;
}

bool LLVMCodegen::genWhile(const WhileStmt& s) {
    std::string condL = lbl(), bodyL = lbl(), endL = lbl();
    emit("  br label %" + condL + "\n");
    emit(condL + ":\n");
    auto [cv, ct] = genExpr(*s.condition);
    auto [bv, bt] = toBool({cv, ct});
    emit("  br i1 " + bv + ", label %" + bodyL + ", label %" + endL + "\n");
    emit(bodyL + ":\n");
    genStmt(*s.body);
    emit("  br label %" + condL + "\n");
    emit(endL + ":\n");
    return false;
}

bool LLVMCodegen::genFor(const ForStmt& s) {
    pushScope();
    if (s.init) genStmt(*s.init);
    std::string condL = lbl(), bodyL = lbl(), endL = lbl();
    emit("  br label %" + condL + "\n");
    emit(condL + ":\n");
    if (s.condition) {
        auto [cv, ct] = genExpr(*s.condition);
        auto [bv, bt] = toBool({cv, ct});
        emit("  br i1 " + bv + ", label %" + bodyL + ", label %" + endL + "\n");
    } else {
        emit("  br label %" + bodyL + "\n");
    }
    emit(bodyL + ":\n");
    genStmt(*s.body);
    if (s.update) genExpr(*s.update);
    emit("  br label %" + condL + "\n");
    emit(endL + ":\n");
    popScope();
    return false;
}

void LLVMCodegen::genForEach(const ForEachStmt&) {
    std::string endL = lbl();
    emit("  ; foreach (not fully supported in LLVM MVP)\n");
    emit("  br label %" + endL + "\n");
    emit(endL + ":\n");
}

LLVMCodegen::TV LLVMCodegen::genExpr(const ASTNode& node) {
    if (auto* e = dynamic_cast<const IntLiteralExpr*>(&node))
        return {std::to_string(e->value), "i64"};

    if (auto* e = dynamic_cast<const FloatLiteralExpr*>(&node)) {
        std::ostringstream oss; oss << e->value;
        std::string s = oss.str();
        if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) s += ".0";
        return {s, "double"};
    }

    if (auto* e = dynamic_cast<const StringLiteralExpr*>(&node)) {
        std::string g = globalStr(e->value);
        size_t len = e->value.size() + 1;
        std::string r = tmp();
        emit("  " + r + " = getelementptr inbounds ["
             + std::to_string(len) + " x i8], ["
             + std::to_string(len) + " x i8]* " + g + ", i64 0, i64 0\n");
        return {r, "i8*"};
    }

    if (auto* e = dynamic_cast<const BoolLiteralExpr*>(&node))
        return {e->value ? "true" : "false", "i1"};

    if (auto* e = dynamic_cast<const CharLiteralExpr*>(&node))
        return {std::to_string((int)(unsigned char)e->value), "i8"};

    if (dynamic_cast<const NullLiteralExpr*>(&node))
        return {"null", "i8*"};

    if (auto* e = dynamic_cast<const IdentifierExpr*>(&node)) {
        if (e->name == "null")  return {"null", "i8*"};
        if (e->name == "true")  return {"true", "i1"};
        if (e->name == "false") return {"false", "i1"};
        auto* vi = lookupVar(e->name);
        if (!vi) return {"0", "i64"};
        std::string r = tmp();
        emit("  " + r + " = load " + vi->type + ", " + vi->type + "* " + vi->alloca + "\n");
        return {r, vi->type};
    }

    if (auto* e = dynamic_cast<const BinaryExpr*>(&node))         return genBinary(*e);
    if (auto* e = dynamic_cast<const UnaryExpr*>(&node))          return genUnary(*e);
    if (auto* e = dynamic_cast<const AssignExpr*>(&node))         return genAssign(*e);
    if (auto* e = dynamic_cast<const CallExpr*>(&node))           return genCall(*e);
    if (auto* e = dynamic_cast<const MemberAccessExpr*>(&node))   return genMemberAccess(*e);
    if (auto* e = dynamic_cast<const NewExpr*>(&node))            return genNew(*e);
    if (auto* e = dynamic_cast<const TernaryExpr*>(&node))        return genTernary(*e);
    if (auto* e = dynamic_cast<const InterpolatedStringExpr*>(&node)) return genInterpolated(*e);
    if (auto* e = dynamic_cast<const CastExpr*>(&node))           return genCast(*e);
    if (auto* e = dynamic_cast<const IndexExpr*>(&node))          return genIndex(*e);
    if (auto* e = dynamic_cast<const NullAssertExpr*>(&node))     return genExpr(*e->expr);
    if (auto* e = dynamic_cast<const NullCoalesceExpr*>(&node)) {
        auto [lv, lt] = genExpr(*e->left);
        auto [rv, rt] = genExpr(*e->right);
        std::string isNullR = tmp(), r = tmp();
        std::string lp = coerce(lv, lt, "i8*");
        std::string rp = coerce(rv, rt, "i8*");
        emit("  " + isNullR + " = icmp eq i8* " + lp + ", null\n");
        emit("  " + r + " = select i1 " + isNullR + ", i8* " + rp + ", i8* " + lp + "\n");
        return {r, "i8*"};
    }
    if (auto* e = dynamic_cast<const ArrayLiteralExpr*>(&node)) {
        size_t n = e->elements.size();
        std::string rawR = tmp(), ptrR = tmp();
        emit("  " + rawR + " = call i8* @malloc(i64 " + std::to_string(n * 8 + 8) + ")\n");
        emit("  " + ptrR + " = bitcast i8* " + rawR + " to i64*\n");
        for (size_t i = 0; i < n; i++) {
            auto [ev, et] = genExpr(*e->elements[i]);
            std::string gepR = tmp();
            emit("  " + gepR + " = getelementptr i64, i64* " + ptrR + ", i64 " + std::to_string(i) + "\n");
            emit("  store i64 " + coerce(ev, et, "i64") + ", i64* " + gepR + "\n");
        }
        return {rawR, "i8*"};
    }
    if (auto* e = dynamic_cast<const AllocExpr*>(&node)) {
        auto [sv, st] = genExpr(*e->count);
        std::string r = tmp();
        emit("  " + r + " = call i8* @malloc(i64 " + coerce(sv, st, "i64") + ")\n");
        return {r, "i8*"};
    }
    if (auto* e = dynamic_cast<const FreeExpr*>(&node)) {
        auto [pv, pt] = genExpr(*e->pointer);
        emit("  call void @free(i8* " + coerce(pv, pt, "i8*") + ")\n");
        return {"0", "i64"};
    }
    if (auto* e = dynamic_cast<const LambdaExpr*>(&node)) {
        emit("  ; lambda (not supported in LLVM MVP)\n");
        return {"null", "i8*"};
    }
    return {"0", "i64"};
}

LLVMCodegen::TV LLVMCodegen::genBinary(const BinaryExpr& e) {
    auto [lv, lt] = genExpr(*e.left);
    auto [rv, rt] = genExpr(*e.right);
    const std::string& op = e.op;

    // String concatenation
    if (op == "+" && (lt == "i8*" || rt == "i8*"))
        return emitStrConcat({lv, lt}, {rv, rt});

    // Float arithmetic / comparison
    bool isFloat = (lt == "double" || rt == "double");
    if (isFloat) {
        std::string fl = coerce(lv, lt, "double"), fr = coerce(rv, rt, "double");
        std::string r = tmp();
        if (op == "+")  { emit("  " + r + " = fadd double " + fl + ", " + fr + "\n"); return {r, "double"}; }
        if (op == "-")  { emit("  " + r + " = fsub double " + fl + ", " + fr + "\n"); return {r, "double"}; }
        if (op == "*")  { emit("  " + r + " = fmul double " + fl + ", " + fr + "\n"); return {r, "double"}; }
        if (op == "/")  { emit("  " + r + " = fdiv double " + fl + ", " + fr + "\n"); return {r, "double"}; }
        std::string c = tmp();
        if      (op == "==") emit("  " + c + " = fcmp oeq double " + fl + ", " + fr + "\n");
        else if (op == "!=") emit("  " + c + " = fcmp one double " + fl + ", " + fr + "\n");
        else if (op == "<")  emit("  " + c + " = fcmp olt double " + fl + ", " + fr + "\n");
        else if (op == "<=") emit("  " + c + " = fcmp ole double " + fl + ", " + fr + "\n");
        else if (op == ">")  emit("  " + c + " = fcmp ogt double " + fl + ", " + fr + "\n");
        else if (op == ">=") emit("  " + c + " = fcmp oge double " + fl + ", " + fr + "\n");
        else { emit("  ; unknown float op: " + op + "\n"); return {fl, "double"}; }
        return {c, "i1"};
    }

    // Logical short-circuit (already computed both sides — not true short-circuit, but correct)
    if (op == "&&") {
        auto [bl, _l] = toBool({lv, lt}); auto [br, _r] = toBool({rv, rt});
        std::string r = tmp(); emit("  " + r + " = and i1 " + bl + ", " + br + "\n"); return {r, "i1"};
    }
    if (op == "||") {
        auto [bl, _l] = toBool({lv, lt}); auto [br, _r] = toBool({rv, rt});
        std::string r = tmp(); emit("  " + r + " = or i1 " + bl + ", " + br + "\n"); return {r, "i1"};
    }

    // Integer arithmetic / bitwise
    std::string il = coerce(lv, lt, "i64"), ir = coerce(rv, rt, "i64"), r = tmp();
    if (op == "+")  { emit("  " + r + " = add nsw i64 " + il + ", " + ir + "\n"); return {r, "i64"}; }
    if (op == "-")  { emit("  " + r + " = sub nsw i64 " + il + ", " + ir + "\n"); return {r, "i64"}; }
    if (op == "*")  { emit("  " + r + " = mul nsw i64 " + il + ", " + ir + "\n"); return {r, "i64"}; }
    if (op == "/")  { emit("  " + r + " = sdiv i64 "     + il + ", " + ir + "\n"); return {r, "i64"}; }
    if (op == "%")  { emit("  " + r + " = srem i64 "     + il + ", " + ir + "\n"); return {r, "i64"}; }
    if (op == "&")  { emit("  " + r + " = and i64 "      + il + ", " + ir + "\n"); return {r, "i64"}; }
    if (op == "|")  { emit("  " + r + " = or i64 "       + il + ", " + ir + "\n"); return {r, "i64"}; }
    if (op == "^")  { emit("  " + r + " = xor i64 "      + il + ", " + ir + "\n"); return {r, "i64"}; }
    if (op == "<<") { emit("  " + r + " = shl i64 "      + il + ", " + ir + "\n"); return {r, "i64"}; }
    if (op == ">>") { emit("  " + r + " = ashr i64 "     + il + ", " + ir + "\n"); return {r, "i64"}; }

    // Integer comparison
    std::string c = tmp();
    if      (op == "==") emit("  " + c + " = icmp eq i64 "  + il + ", " + ir + "\n");
    else if (op == "!=") emit("  " + c + " = icmp ne i64 "  + il + ", " + ir + "\n");
    else if (op == "<")  emit("  " + c + " = icmp slt i64 " + il + ", " + ir + "\n");
    else if (op == "<=") emit("  " + c + " = icmp sle i64 " + il + ", " + ir + "\n");
    else if (op == ">")  emit("  " + c + " = icmp sgt i64 " + il + ", " + ir + "\n");
    else if (op == ">=") emit("  " + c + " = icmp sge i64 " + il + ", " + ir + "\n");
    else { emit("  ; unknown binary op: " + op + "\n"); return {il, "i64"}; }
    return {c, "i1"};
}

LLVMCodegen::TV LLVMCodegen::genUnary(const UnaryExpr& e) {
    if ((e.op == "++" || e.op == "--")) {
        if (auto* id = dynamic_cast<const IdentifierExpr*>(e.operand.get())) {
            auto* vi = lookupVar(id->name);
            if (vi) {
                std::string cur = tmp();
                emit("  " + cur + " = load " + vi->type + ", " + vi->type + "* " + vi->alloca + "\n");
                std::string n = tmp();
                std::string cur64 = coerce(cur, vi->type, "i64");
                if (e.op == "++") emit("  " + n + " = add nsw i64 " + cur64 + ", 1\n");
                else              emit("  " + n + " = sub nsw i64 " + cur64 + ", 1\n");
                std::string stored = coerce(n, "i64", vi->type);
                emit("  store " + vi->type + " " + stored + ", " + vi->type + "* " + vi->alloca + "\n");
                return {e.prefix ? n : cur, "i64"};
            }
        }
    }
    auto [v, t] = genExpr(*e.operand);
    if (e.op == "-") {
        std::string r = tmp();
        if (t == "double") emit("  " + r + " = fneg double " + v + "\n");
        else               emit("  " + r + " = sub nsw i64 0, " + coerce(v, t, "i64") + "\n");
        return {r, t == "double" ? "double" : "i64"};
    }
    if (e.op == "!") {
        auto [bv, bt] = toBool({v, t});
        std::string r = tmp();
        emit("  " + r + " = xor i1 " + bv + ", true\n");
        return {r, "i1"};
    }
    if (e.op == "~") {
        std::string r = tmp();
        emit("  " + r + " = xor i64 " + coerce(v, t, "i64") + ", -1\n");
        return {r, "i64"};
    }
    return {v, t};
}

LLVMCodegen::TV LLVMCodegen::genAssign(const AssignExpr& e) {
    auto [rv, rt] = genExpr(*e.value);

    auto doStore = [&](const std::string& allocaR, const std::string& varTy) {
        std::string newVal = coerce(rv, rt, varTy);
        if (e.op != "=") {
            std::string cur = tmp();
            emit("  " + cur + " = load " + varTy + ", " + varTy + "* " + allocaR + "\n");
            std::string l64 = coerce(cur, varTy, "i64");
            std::string r64 = coerce(rv, rt, "i64");
            std::string n = tmp();
            if      (e.op == "+=") emit("  " + n + " = add nsw i64 " + l64 + ", " + r64 + "\n");
            else if (e.op == "-=") emit("  " + n + " = sub nsw i64 " + l64 + ", " + r64 + "\n");
            else if (e.op == "*=") emit("  " + n + " = mul nsw i64 " + l64 + ", " + r64 + "\n");
            else if (e.op == "/=") emit("  " + n + " = sdiv i64 "    + l64 + ", " + r64 + "\n");
            else if (e.op == "%=") emit("  " + n + " = srem i64 "    + l64 + ", " + r64 + "\n");
            newVal = coerce(n, "i64", varTy);
        }
        emit("  store " + varTy + " " + newVal + ", " + varTy + "* " + allocaR + "\n");
    };

    if (auto* id = dynamic_cast<const IdentifierExpr*>(e.target.get())) {
        if (auto* vi = lookupVar(id->name)) doStore(vi->alloca, vi->type);
        return {rv, rt};
    }
    if (auto* ma = dynamic_cast<const MemberAccessExpr*>(e.target.get())) {
        // Member field store via GEP
        auto [objV, objT] = genExpr(*ma->object);
        std::string className;
        if (objT.size() > 7 && objT.substr(0, 7) == "%class.") {
            className = objT.substr(7);
            if (!className.empty() && className.back() == '*') className.pop_back();
        }
        if (!className.empty() && classDecls_.count(className)) {
            const ClassDecl* cls = classDecls_.at(className);
            for (size_t i = 0; i < cls->fields.size(); i++) {
                if (cls->fields[i]->name == ma->member) {
                    std::string fTy = llvmTy(cls->fields[i]->type);
                    size_t idx = i + (cls->superClass ? 1 : 0);
                    std::string gepR = tmp();
                    emit("  " + gepR + " = getelementptr inbounds %class." + className
                         + ", %class." + className + "* " + objV
                         + ", i32 0, i32 " + std::to_string(idx) + "\n");
                    emit("  store " + fTy + " " + coerce(rv, rt, fTy)
                         + ", " + fTy + "* " + gepR + "\n");
                    return {rv, rt};
                }
            }
        }
        emit("  ; member assign ." + ma->member + " (not resolved)\n");
    }
    return {rv, rt};
}

LLVMCodegen::TV LLVMCodegen::genCall(const CallExpr& e) {
    if (auto* ma = dynamic_cast<const MemberAccessExpr*>(e.callee.get()))
        return genMemberCall(*ma, e.args);

    if (auto* id = dynamic_cast<const IdentifierExpr*>(e.callee.get())) {
        std::vector<TV> argVals;
        for (auto& a : e.args) argVals.push_back(genExpr(*a));
        std::string r = tmp();
        emit("  " + r + " = call i64 @" + id->name + "(");
        for (size_t i = 0; i < argVals.size(); i++) {
            if (i) emit(", ");
            emit("i64 " + coerce(argVals[i].first, argVals[i].second, "i64"));
        }
        emit(")\n");
        return {r, "i64"};
    }
    emit("  ; unresolved call\n");
    return {"0", "i64"};
}

LLVMCodegen::TV LLVMCodegen::genMemberCall(const MemberAccessExpr& ma,
                                             const std::vector<ASTNodePtr>& args) {
    // ── Console
    if (auto* id = dynamic_cast<const IdentifierExpr*>(ma.object.get())) {
        if (id->name == "Console") {
            bool nl = (ma.member == "println");
            if (args.empty()) {
                if (nl) {
                    std::string g = globalStr("\n"), r = tmp();
                    emit("  " + r + " = getelementptr inbounds [2 x i8], [2 x i8]* " + g + ", i64 0, i64 0\n");
                    emit("  call i32 (i8*, ...) @printf(i8* " + r + ")\n");
                }
                return {"0", "i64"};
            }
            auto [av, at] = genExpr(*args[0]);
            std::string argReg, argTy, fmtStr;
            if (at == "i8*") {
                fmtStr = nl ? "%s\n" : "%s"; argReg = av; argTy = "i8*";
            } else if (at == "double") {
                fmtStr = nl ? "%g\n" : "%g"; argReg = av; argTy = "double";
            } else if (at == "i1") {
                auto [sv, st] = toStr({av, at});
                fmtStr = nl ? "%s\n" : "%s"; argReg = sv; argTy = "i8*";
            } else {
                fmtStr = nl ? "%lld\n" : "%lld";
                argReg = coerce(av, at, "i64"); argTy = "i64";
            }
            std::string fmtG = globalStr(fmtStr);
            size_t fmtLen = fmtStr.size() + 1;
            std::string fmtR = tmp();
            emit("  " + fmtR + " = getelementptr inbounds [" + std::to_string(fmtLen)
                 + " x i8], [" + std::to_string(fmtLen) + " x i8]* " + fmtG + ", i64 0, i64 0\n");
            emit("  call i32 (i8*, ...) @printf(i8* " + fmtR + ", " + argTy + " " + argReg + ")\n");
            return {"0", "i64"};
        }

        // ── Math
        if (id->name == "Math") {
            std::vector<TV> av;
            for (auto& a : args) av.push_back(genExpr(*a));
            auto toD = [&](const TV& v) { return coerce(v.first, v.second, "double"); };
            std::string r = tmp();
            if (av.empty()) return {"0.0", "double"};
            if (ma.member == "sqrt")   { emit("  " + r + " = call double @sqrt(double "  + toD(av[0]) + ")\n"); return {r, "double"}; }
            if (ma.member == "floor")  { emit("  " + r + " = call double @floor(double " + toD(av[0]) + ")\n"); return {r, "double"}; }
            if (ma.member == "ceil")   { emit("  " + r + " = call double @ceil(double "  + toD(av[0]) + ")\n"); return {r, "double"}; }
            if (ma.member == "round")  { emit("  " + r + " = call double @round(double " + toD(av[0]) + ")\n"); return {r, "double"}; }
            if (ma.member == "sin")    { emit("  " + r + " = call double @sin(double "   + toD(av[0]) + ")\n"); return {r, "double"}; }
            if (ma.member == "cos")    { emit("  " + r + " = call double @cos(double "   + toD(av[0]) + ")\n"); return {r, "double"}; }
            if (ma.member == "tan")    { emit("  " + r + " = call double @tan(double "   + toD(av[0]) + ")\n"); return {r, "double"}; }
            if (ma.member == "log")    { emit("  " + r + " = call double @log(double "   + toD(av[0]) + ")\n"); return {r, "double"}; }
            if (ma.member == "log2")   { emit("  " + r + " = call double @log2(double "  + toD(av[0]) + ")\n"); return {r, "double"}; }
            if (ma.member == "log10")  { emit("  " + r + " = call double @log10(double " + toD(av[0]) + ")\n"); return {r, "double"}; }
            if (ma.member == "exp")    { emit("  " + r + " = call double @exp(double "   + toD(av[0]) + ")\n"); return {r, "double"}; }
            if (ma.member == "abs" && !av.empty() && av[0].second == "i64") {
                std::string neg = tmp(), cmp = tmp();
                emit("  " + neg + " = sub nsw i64 0, " + av[0].first + "\n");
                emit("  " + cmp + " = icmp slt i64 "   + av[0].first + ", 0\n");
                emit("  " + r   + " = select i1 " + cmp + ", i64 " + neg + ", i64 " + av[0].first + "\n");
                return {r, "i64"};
            }
            if (ma.member == "abs")    { emit("  " + r + " = call double @fabs(double "  + toD(av[0]) + ")\n"); return {r, "double"}; }
            if (ma.member == "pow" && av.size() >= 2) {
                emit("  " + r + " = call double @pow(double " + toD(av[0]) + ", double " + toD(av[1]) + ")\n");
                return {r, "double"};
            }
            if (ma.member == "atan2" && av.size() >= 2) {
                emit("  " + r + " = call double @atan2(double " + toD(av[0]) + ", double " + toD(av[1]) + ")\n");
                return {r, "double"};
            }
            if (ma.member == "min" && av.size() >= 2) {
                if (av[0].second == "double" || av[1].second == "double") {
                    std::string cmp = tmp();
                    std::string a = toD(av[0]), b = toD(av[1]);
                    emit("  " + cmp + " = fcmp ole double " + a + ", " + b + "\n");
                    emit("  " + r   + " = select i1 " + cmp + ", double " + a + ", double " + b + "\n");
                    return {r, "double"};
                }
                std::string cmp = tmp();
                emit("  " + cmp + " = icmp slt i64 " + av[0].first + ", " + av[1].first + "\n");
                emit("  " + r   + " = select i1 " + cmp + ", i64 " + av[0].first + ", i64 " + av[1].first + "\n");
                return {r, "i64"};
            }
            if (ma.member == "max" && av.size() >= 2) {
                if (av[0].second == "double" || av[1].second == "double") {
                    std::string cmp = tmp();
                    std::string a = toD(av[0]), b = toD(av[1]);
                    emit("  " + cmp + " = fcmp ogt double " + a + ", " + b + "\n");
                    emit("  " + r   + " = select i1 " + cmp + ", double " + a + ", double " + b + "\n");
                    return {r, "double"};
                }
                std::string cmp = tmp();
                emit("  " + cmp + " = icmp sgt i64 " + av[0].first + ", " + av[1].first + "\n");
                emit("  " + r   + " = select i1 " + cmp + ", i64 " + av[0].first + ", i64 " + av[1].first + "\n");
                return {r, "i64"};
            }
            emit("  ; unknown Math." + ma.member + "\n");
            return {"0.0", "double"};
        }

        // ── System.exit
        if (id->name == "System" && ma.member == "exit") {
            TV code = args.empty() ? TV{"0", "i64"} : genExpr(*args[0]);
            std::string c32 = tmp();
            emit("  " + c32 + " = trunc i64 " + coerce(code.first, code.second, "i64") + " to i32\n");
            emit("  call void @exit(i32 " + c32 + ")\n  unreachable\n");
            emit(lbl() + ":\n");
            return {"0", "i64"};
        }
    }

    // ── obj.toString()
    if (ma.member == "toString") {
        auto [objV, objT] = genExpr(*ma.object);
        return toStr({objV, objT});
    }

    // ── string.length()
    if (ma.member == "length") {
        auto [objV, objT] = genExpr(*ma.object);
        if (objT == "i8*") {
            std::string r = tmp();
            emit("  " + r + " = call i64 @strlen(i8* " + objV + ")\n");
            return {r, "i64"};
        }
    }

    // ── User-defined method call
    auto [objV, objT] = genExpr(*ma.object);
    std::vector<TV> argVals;
    for (auto& a : args) argVals.push_back(genExpr(*a));

    std::string className;
    if (objT.size() > 7 && objT.substr(0, 7) == "%class.") {
        className = objT.substr(7);
        if (!className.empty() && className.back() == '*') className.pop_back();
    }
    if (!className.empty()) {
        std::string r = tmp();
        emit("  " + r + " = call i64 @" + className + "__" + ma.member
             + "(%class." + className + "* " + objV);
        for (auto& av : argVals)
            emit(", i64 " + coerce(av.first, av.second, "i64"));
        emit(")\n");
        return {r, "i64"};
    }

    emit("  ; member call ." + ma.member + " (not resolved)\n");
    return {"0", "i64"};
}

LLVMCodegen::TV LLVMCodegen::genMemberAccess(const MemberAccessExpr& e) {
    // Math constants
    if (auto* id = dynamic_cast<const IdentifierExpr*>(e.object.get())) {
        if (id->name == "Math") {
            if (e.member == "PI")  return {"3.14159265358979323846", "double"};
            if (e.member == "E")   return {"2.71828182845904523536", "double"};
            if (e.member == "TAU") return {"6.28318530717958647692", "double"};
        }
    }

    auto [objV, objT] = genExpr(*e.object);

    // String.length (non-call form)
    if (e.member == "length" && objT == "i8*") {
        std::string r = tmp();
        emit("  " + r + " = call i64 @strlen(i8* " + objV + ")\n");
        return {r, "i64"};
    }

    // Field access on known struct
    std::string className;
    if (objT.size() > 7 && objT.substr(0, 7) == "%class.") {
        className = objT.substr(7);
        if (!className.empty() && className.back() == '*') className.pop_back();
    }
    if (!className.empty() && classDecls_.count(className)) {
        const ClassDecl* cls = classDecls_.at(className);
        for (size_t i = 0; i < cls->fields.size(); i++) {
            if (cls->fields[i]->name == e.member) {
                std::string fTy = llvmTy(cls->fields[i]->type);
                size_t idx = i + (cls->superClass ? 1 : 0);
                std::string gepR = tmp(), r = tmp();
                emit("  " + gepR + " = getelementptr inbounds %class." + className
                     + ", %class." + className + "* " + objV
                     + ", i32 0, i32 " + std::to_string(idx) + "\n");
                emit("  " + r + " = load " + fTy + ", " + fTy + "* " + gepR + "\n");
                return {r, fTy};
            }
        }
    }

    emit("  ; field access ." + e.member + " (not resolved)\n");
    return {"0", "i64"};
}

LLVMCodegen::TV LLVMCodegen::genNew(const NewExpr& e) {
    if (classDecls_.count(e.type.name)) {
        std::string ty = "%class." + e.type.name + "*";
        std::vector<TV> argVals;
        for (auto& a : e.args) argVals.push_back(genExpr(*a));
        std::string r = tmp();
        emit("  " + r + " = call " + ty + " @" + e.type.name + "__new(");
        for (size_t i = 0; i < argVals.size(); i++) {
            if (i) emit(", ");
            emit("i64 " + coerce(argVals[i].first, argVals[i].second, "i64"));
        }
        emit(")\n");
        return {r, ty};
    }
    // Unknown type — raw malloc
    std::string r = tmp();
    emit("  " + r + " = call i8* @malloc(i64 64)\n");
    return {r, "i8*"};
}

LLVMCodegen::TV LLVMCodegen::genTernary(const TernaryExpr& e) {
    auto [cv, ct] = genExpr(*e.condition);
    auto [bv, bt] = toBool({cv, ct});
    auto [tv, tt] = genExpr(*e.thenExpr);
    auto [ev, et] = genExpr(*e.elseExpr);
    std::string r = tmp();
    emit("  " + r + " = select i1 " + bv + ", " + tt + " " + tv
         + ", " + tt + " " + coerce(ev, et, tt) + "\n");
    return {r, tt};
}

LLVMCodegen::TV LLVMCodegen::genInterpolated(const InterpolatedStringExpr& e) {
    std::string emptyG = globalStr("");
    std::string curR = tmp();
    emit("  " + curR + " = getelementptr inbounds [1 x i8], [1 x i8]* "
         + emptyG + ", i64 0, i64 0\n");
    TV cur = {curR, "i8*"};
    for (auto& part : e.parts) {
        if (!part.isExpr) {
            std::string g = globalStr(part.text);
            size_t len = part.text.size() + 1;
            std::string r = tmp();
            emit("  " + r + " = getelementptr inbounds [" + std::to_string(len) + " x i8], ["
                 + std::to_string(len) + " x i8]* " + g + ", i64 0, i64 0\n");
            cur = emitStrConcat(cur, {r, "i8*"});
        } else {
            auto tv = genExpr(*part.expr);
            cur = emitStrConcat(cur, toStr(tv));
        }
    }
    return cur;
}

LLVMCodegen::TV LLVMCodegen::genCast(const CastExpr& e) {
    auto [v, t] = genExpr(*e.value);
    std::string toTy = llvmTy(e.targetType);
    return {coerce(v, t, toTy), toTy};
}

LLVMCodegen::TV LLVMCodegen::genIndex(const IndexExpr& e) {
    auto [objV, objT] = genExpr(*e.object);
    auto [idxV, idxT] = genExpr(*e.index);
    std::string arrP = tmp(), gepR = tmp(), r = tmp();
    emit("  " + arrP + " = bitcast i8* " + coerce(objV, objT, "i8*") + " to i64*\n");
    emit("  " + gepR + " = getelementptr i64, i64* " + arrP + ", i64 " + coerce(idxV, idxT, "i64") + "\n");
    emit("  " + r    + " = load i64, i64* " + gepR + "\n");
    return {r, "i64"};
}

}
