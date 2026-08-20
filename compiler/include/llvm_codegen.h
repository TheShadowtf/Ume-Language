#pragma once
// llvm_codegen.h — LLVM IR text emitter for the Ume Language
//
// Emits human-readable LLVM IR (.ll) suitable for compilation via:
//   clang -O2 out.ll -o out
//
// Strategy: alloca/store/load for all variables (no phi nodes required).
// mem2reg + opt passes will turn these into SSA form when run through clang.

#include "ast.h"
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <functional>
#include <utility>

namespace Ume {

struct LLVMCodegenOptions {
    bool emitComments = true;
    bool optimize     = false;
};

struct LLVMCodegenResult {
    bool        success = false;
    std::string errorMessage;
    std::string irOutput;   // the complete .ll text
};

class LLVMCodegen {
public:
    explicit LLVMCodegen(LLVMCodegenOptions opts = {});

    LLVMCodegenResult generate(const Program& program);

private:
    LLVMCodegenOptions opts_;

    // ── Output sections
    std::ostringstream types_;    // %struct / %class type defs
    std::ostringstream globals_;  // global constants + extern decls
    std::ostringstream helpers_;  // private helper functions
    std::ostringstream funcs_;    // user-defined functions
    std::ostringstream* cur_ = nullptr; // points to active section

    // ── Counters
    int tmpN_  = 0;
    int lblN_  = 0;
    int gstrN_ = 0;

    // ── Current function context
    std::string curRetTy_;
    std::string curFnName_;
    std::string curClassName_;
    bool        hasMainWrapper_ = false;

    // ── Variable scope: name -> (allocaReg, llvmType)
    struct VarInfo { std::string alloca; std::string type; };
    using VarMap = std::unordered_map<std::string, VarInfo>;
    std::vector<VarMap> scopes_;

    // ── Global string constant cache: raw -> @.str.N
    std::unordered_map<std::string, std::string> strCache_;

    // ── Known class declarations (for struct layout)
    std::unordered_map<std::string, const ClassDecl*> classDecls_;

    // ── Helpers
    std::string tmp()   { return "%t" + std::to_string(tmpN_++); }
    std::string lbl()   { return "lbl" + std::to_string(lblN_++); }
    void emit  (const std::string& s) { *cur_ << s; }
    void emitln(const std::string& s = "") { *cur_ << s << "\n"; }

    std::string llvmTy(const TypeAnnotation& ta) const;
    std::string llvmTy(const std::string& name)  const;
    std::string defaultVal(const std::string& ty)  const;
    std::string globalStr (const std::string& raw);   // intern & declare

    // Type coercion: emit convert instructions, returns new register
    std::string coerce(const std::string& val,
                       const std::string& fromTy,
                       const std::string& toTy);

    void pushScope();
    void popScope();
    void defVar(const std::string& name,
                const std::string& allocaReg,
                const std::string& ty);
    VarInfo* lookupVar(const std::string& name);

    // ── Typed value (register, llvmType)
    using TV = std::pair<std::string, std::string>;

    TV toBool    (const TV& v);          // returns (reg, "i1")
    TV toStr     (const TV& v);          // returns (reg, "i8*")
    TV emitStrConcat(const TV& a, const TV& b); // returns (reg, "i8*")

    // ── Code generation
    void collectClasses(const Program& prog);
    void emitBuiltins();

    void genProgramDecl(const ASTNode& node);
    void genClassDecl  (const ClassDecl& cls);
    void genFuncDecl   (const FuncDecl& fn, const std::string& className = "");

    // Statements — return true if control flow was terminated
    bool genStmt   (const ASTNode& node);
    bool genBlock  (const BlockStmt& b);
    void genVarDecl(const VarDeclStmt& v);
    bool genReturn (const ReturnStmt& r);
    bool genIf     (const IfStmt& s);
    bool genWhile  (const WhileStmt& s);
    bool genFor    (const ForStmt& s);
    void genForEach(const ForEachStmt& s);

    // Expressions — return (register, llvmType)
    TV genExpr       (const ASTNode& node);
    TV genBinary     (const BinaryExpr& e);
    TV genUnary      (const UnaryExpr& e);
    TV genAssign     (const AssignExpr& e);
    TV genCall       (const CallExpr& e);
    TV genMemberCall (const MemberAccessExpr& ma,
                      const std::vector<ASTNodePtr>& args);
    TV genMemberAccess(const MemberAccessExpr& e);
    TV genNew        (const NewExpr& e);
    TV genTernary    (const TernaryExpr& e);
    TV genInterpolated(const InterpolatedStringExpr& e);
    TV genCast       (const CastExpr& e);
    TV genIndex      (const IndexExpr& e);
};

}
