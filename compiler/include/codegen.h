#pragma once
// codegen.h — C++ Code Generator / Transpiler for Ume

#include "ast.h"
#include <string>
#include <sstream>
#include <unordered_set>

namespace Ume {

// ─────────────────────────────────────────────────────────────
// CodeGenerator
//
// Transpiles a Ume AST to C++ source code. The generated code
// includes the Ume runtime header (ume_runtime.h) and uses:
//   std::string           for string
//   std::shared_ptr<T>    for class instances
//   std::optional<T>      for nullable primitives
//   std::vector<T>        backing List<T>
//   std::unordered_map    backing Map<K,V>
// ─────────────────────────────────────────────────────────────
class CodeGenerator {
public:
    CodeGenerator();

    // Generate full C++ source from a parsed program
    std::string generate(const Program& program);
    std::string generate(const Program& program, const std::string& sourceFilename);

private:
    std::ostringstream      out_;
    int                     indent_        = 0;
    bool                    atStartOfLine_ = true;
    std::unordered_set<std::string> forwardDecls_;
    std::unordered_set<std::string> enumNames_;     // tracks declared enum types
    std::unordered_set<std::string> structNames_;   // tracks declared struct types (value semantics)
    std::unordered_set<std::string> classNames_;    // tracks declared class and interface types
    std::string             sourceFilename_;
    std::string             currentSourceFilename_;
    int                     currentSourceLine_ = -1;
    bool                    inMainFunction_   = false; // true while generating body of top-level main()

    // ── Indentation helpers ───────────────────────────────
    void indent();
    void dedent();
    void emit(const std::string& text);
    void emitLine(const std::string& line = "");
    void emitIndent();
    void emitLineDirective(const ASTNode& node);

    // ── Top-level generators ──────────────────────────────
    void genProgram(const Program& prog);
    void genNode(const ASTNode& node);

    // ── Declaration generators ────────────────────────────
    void genClassDecl(const ClassDecl& cls);
    void genInterfaceDecl(const InterfaceDecl& iface);
    void genEnumDecl(const EnumDecl& enm);
    void genStructDecl(const StructDecl& strct);
    void genFuncDecl(const FuncDecl& func, const std::string& ownerClass = "");
    void genOperatorDecl(const OperatorDecl& op, const std::string& className);

    // ── Statement generators ──────────────────────────────
    void genBlock(const BlockStmt& block);
    void genVarDecl(const VarDeclStmt& stmt);
    void genReturnStmt(const ReturnStmt& stmt);
    void genIfStmt(const IfStmt& stmt);
    void genWhileStmt(const WhileStmt& stmt);
    void genDoWhileStmt(const DoWhileStmt& stmt);
    void genForStmt(const ForStmt& stmt);
    void genForEachStmt(const ForEachStmt& stmt);
    void genSwitchStmt(const SwitchStmt& stmt);
    void genTryCatch(const TryCatchStmt& stmt);
    void genThrowStmt(const ThrowStmt& stmt);
    void genUnsafeBlock(const UnsafeBlock& block);
    void genExprStmt(const ExprStmt& stmt);

    // ── Expression generators ─────────────────────────────
    std::string genExpr(const ASTNode& node);
    std::string genBinary(const BinaryExpr& expr);
    std::string genUnary(const UnaryExpr& expr);
    std::string genAssign(const AssignExpr& expr);
    std::string genCall(const CallExpr& expr);
    std::string genMemberAccess(const MemberAccessExpr& expr);
    std::string genIndex(const IndexExpr& expr);
    std::string genNew(const NewExpr& expr);
    std::string genCast(const CastExpr& expr);
    std::string genTernary(const TernaryExpr& expr);
    std::string genLambda(const LambdaExpr& expr);
    std::string genArrayLiteral(const ArrayLiteralExpr& expr);
    std::string genStructInit(const StructInitExpr& expr);
    std::string genNullCoalesce(const NullCoalesceExpr& expr);
    std::string genNullAssert(const NullAssertExpr& expr);
    std::string genInterpolatedString(const InterpolatedStringExpr& expr);
    std::string genAlloc(const AllocExpr& expr);
    std::string genDeref(const DerefExpr& expr);

    // ── Type mapping ──────────────────────────────────────
    std::string mapType(const TypeAnnotation& ta, bool asParam = false);
    std::string mapTypeName(const std::string& umeName);
    std::string accessStr(AccessModifier access);

    // ── Helpers ───────────────────────────────────────────
    std::string genParamList(const std::vector<Parameter>& params);
    std::string genTypeParamList(const std::vector<std::string>& typeParams);
    std::string escapeCppString(const std::string& s);
};

} // namespace Ume
