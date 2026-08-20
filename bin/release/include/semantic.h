#pragma once
// semantic.h — Semantic Analyzer / Type Checker for Ume

#include "ast.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <stdexcept>
#include <memory>

namespace Ume {

// ─────────────────────────────────────────────────────────────
// Resolved type info (used during analysis)
// ─────────────────────────────────────────────────────────────
struct TypeInfo {
    std::string name;
    bool        nullable = false;
    bool        isArray  = false;
    bool        isClass  = false;
    bool        isEnum   = false;
    bool        isStruct = false;

    bool isVoid()   const { return name == "void"; }
    bool isPrimitive() const;
    bool isCompatibleWith(const TypeInfo& other) const;
    std::string toString() const;
};

struct FunctionSignature {
    TypeInfo              returnType;
    std::vector<TypeInfo> paramTypes;
    bool                  isStatic = false;
    bool                  isAbstract = false;
};

struct ClassInfo {
    std::string                                 name;
    std::optional<std::string>                  superClass;
    std::vector<std::string>                    interfaces;
    std::unordered_map<std::string, TypeInfo>   fields;
    std::unordered_map<std::string, FunctionSignature> methods;
    std::unordered_map<std::string, TypeInfo>   properties;
    std::vector<FunctionSignature>              indexers;
    bool                                        isAbstract = false;
    bool                                        isFinal    = false;
};

// ─────────────────────────────────────────────────────────────
// SemanticError
// ─────────────────────────────────────────────────────────────
class SemanticError : public std::runtime_error {
public:
    int line;
    int column;
    std::string filename;
    SemanticError(const std::string& msg, int line = 0, int column = 0, std::string filename = "")
        : std::runtime_error(msg), line(line), column(column), filename(std::move(filename)) {}
};

// ─────────────────────────────────────────────────────────────
// Symbol table (scoped)
// ─────────────────────────────────────────────────────────────
class SymbolTable {
public:
    void pushScope();
    void popScope();

    void declare(const std::string& name, TypeInfo type);
    std::optional<TypeInfo> lookup(const std::string& name) const;
    bool isDeclaredInCurrentScope(const std::string& name) const;

private:
    std::vector<std::unordered_map<std::string, TypeInfo>> scopes_;
};

// ─────────────────────────────────────────────────────────────
// SemanticAnalyzer
// ─────────────────────────────────────────────────────────────
class SemanticAnalyzer {
public:
    SemanticAnalyzer();

    // Analyze a full program; throws SemanticError on violations
    void analyze(Program& program);

    // Accumulated warnings
    const std::vector<std::string>& warnings() const { return warnings_; }

private:
    SymbolTable                                          symbols_;
    std::unordered_map<std::string, ClassInfo>           classes_;
    std::unordered_map<std::string, FunctionSignature>   functions_;
    std::unordered_map<std::string, TypeInfo>            enumTypes_;
    std::vector<std::string>                             warnings_;
    std::string                                          currentClass_;
    TypeInfo                                             currentReturnType_;
    bool                                                 inUnsafe_ = false;

    // ── First pass: collect top-level declarations ────────
    void collectDeclarations(Program& program);
    void collectClass(ClassDecl& cls);
    void collectInterface(InterfaceDecl& iface);
    void collectEnum(EnumDecl& enm);
    void collectStruct(StructDecl& strct);
    void collectFunc(FuncDecl& func);

    // ── Second pass: validate bodies ─────────────────────
    void checkNode(ASTNode& node);
    void checkAttributes(const std::vector<Attribute>& attrs);
    void checkFuncDecl(FuncDecl& func);
    void checkClassDecl(ClassDecl& cls);
    void checkBlock(BlockStmt& block);
    void checkVarDecl(VarDeclStmt& stmt);
    void checkIfStmt(IfStmt& stmt);
    void checkWhileStmt(WhileStmt& stmt);
    void checkForStmt(ForStmt& stmt);
    void checkForEachStmt(ForEachStmt& stmt);
    void checkReturnStmt(ReturnStmt& stmt);
    void checkThrowStmt(ThrowStmt& stmt);
    void checkTryCatch(TryCatchStmt& stmt);

    TypeInfo inferExprType(ASTNode& expr);
    TypeInfo inferBinaryType(BinaryExpr& expr);
    TypeInfo inferCallType(CallExpr& expr);
    TypeInfo inferMemberType(MemberAccessExpr& expr);
    TypeInfo inferNewType(NewExpr& expr);

    TypeInfo resolveAnnotation(const TypeAnnotation& ta);
    bool     isAssignable(const TypeInfo& target, const TypeInfo& source) const;
    void     warn(const std::string& msg, int line = 0);
};

} // namespace Ume
