#pragma once
// ast.h — AST Node Definitions for the Ume Language
// All expression, statement, and declaration nodes are defined here.

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>

namespace Ume {

struct TypeAnnotation {
    std::string name;                       // base type name (int, string, Dog, ...)
    std::vector<TypeAnnotation> typeArgs;   // generic args: List<int>
    bool nullable   = false;                // T?
    bool isArray    = false;                // T[]
    int  arrayRank  = 0;                    // 1 for [], 2 for [][], 3 for [][][]
    int  arraySize  = -1;                   // -1=dynamic, N=fixed
    bool isFunc     = false;                // func<...> type
    std::vector<TypeAnnotation> funcParams; // param types for func<>
    bool isPointer  = false;                // T*
    int pointerLevel = 0;                   // ***

    TypeAnnotation() = default;
    explicit TypeAnnotation(std::string n, bool nullable = false)
        : name(std::move(n)), nullable(nullable) {}

    bool isVoid()  const { return name == "void"; }
    bool isVar()   const { return name == "var";  }
    bool isConst() const { return name == "const"; }

    std::string toString() const;
};

struct ASTNode; // forward declare for default values

struct Parameter {
    TypeAnnotation type;
    std::string    name;
    bool           variadic     = false; // int... values
    std::unique_ptr<ASTNode> defaultValue;

    Parameter() = default;
    Parameter(TypeAnnotation t, std::string n, bool variadic = false)
        : type(std::move(t)), name(std::move(n)), variadic(variadic) {}
    // Copy: defaultValue is not deep-cloneable without a virtual clone(),
    // so copies omit it (the original FuncDecl keeps ownership).
    Parameter(const Parameter& o)
        : type(o.type), name(o.name), variadic(o.variadic) {}
    Parameter& operator=(const Parameter& o) {
        type = o.type; name = o.name; variadic = o.variadic;
        defaultValue = nullptr;
        return *this;
    }
    Parameter(Parameter&&) = default;
    Parameter& operator=(Parameter&&) = default;
};

enum class AccessModifier { Public, Private, Protected, Internal, Default };

struct ASTNode {
    int line   = 0;
    int column = 0;
    std::string filename;
    virtual ~ASTNode() = default;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

struct Attribute {
    std::string name;
    std::vector<ASTNodePtr> args;
};

struct IntLiteralExpr : ASTNode {
    int64_t value;
    explicit IntLiteralExpr(int64_t v) : value(v) {}
};

struct FloatLiteralExpr : ASTNode {
    double value;
    bool   isFloat; // true=float literal, false=double literal
    FloatLiteralExpr(double v, bool isFloat) : value(v), isFloat(isFloat) {}
};

struct StringLiteralExpr : ASTNode {
    std::string value;
    explicit StringLiteralExpr(std::string v) : value(std::move(v)) {}
};

struct CharLiteralExpr : ASTNode {
    char value;
    explicit CharLiteralExpr(char v) : value(v) {}
};

struct BoolLiteralExpr : ASTNode {
    bool value;
    explicit BoolLiteralExpr(bool v) : value(v) {}
};

struct NullLiteralExpr : ASTNode {};

// String interpolation: $"Hello {name}!"
struct InterpolatedStringPart {
    bool            isExpr = false;
    std::string     text;               // literal segment
    ASTNodePtr      expr;               // expression segment

    explicit InterpolatedStringPart(std::string t)
        : isExpr(false), text(std::move(t)) {}
    explicit InterpolatedStringPart(ASTNodePtr e)
        : isExpr(true), expr(std::move(e)) {}
    InterpolatedStringPart(InterpolatedStringPart&&) = default;
    InterpolatedStringPart& operator=(InterpolatedStringPart&&) = default;
};

struct InterpolatedStringExpr : ASTNode {
    std::vector<InterpolatedStringPart> parts;
};

struct IdentifierExpr : ASTNode {
    std::string name;
    explicit IdentifierExpr(std::string n) : name(std::move(n)) {}
};

struct BinaryExpr : ASTNode {
    std::string op;
    ASTNodePtr  left;
    ASTNodePtr  right;
    BinaryExpr(std::string op, ASTNodePtr l, ASTNodePtr r)
        : op(std::move(op)), left(std::move(l)), right(std::move(r)) {}
};

struct UnaryExpr : ASTNode {
    std::string op;
    ASTNodePtr  operand;
    bool        prefix; // ++x = prefix; x++ = postfix
    UnaryExpr(std::string op, ASTNodePtr operand, bool prefix)
        : op(std::move(op)), operand(std::move(operand)), prefix(prefix) {}
};

struct AssignExpr : ASTNode {
    ASTNodePtr  target;
    std::string op;    // = += -= *= /= %=
    ASTNodePtr  value;
    AssignExpr(ASTNodePtr t, std::string op, ASTNodePtr v)
        : target(std::move(t)), op(std::move(op)), value(std::move(v)) {}
};

struct CallExpr : ASTNode {
    ASTNodePtr                        callee;
    std::vector<ASTNodePtr>           args;
    std::vector<std::string>          typeArgs; // explicit type params: f<int>(...)
    CallExpr(ASTNodePtr c, std::vector<ASTNodePtr> a, std::vector<std::string> ta = {})
        : callee(std::move(c)), args(std::move(a)), typeArgs(std::move(ta)) {}
};

struct MemberAccessExpr : ASTNode {
    ASTNodePtr  object;
    std::string member;
    bool        safe = false; // ?. operator
    bool        isProperty = false; // Set by semantic analyzer
    MemberAccessExpr(ASTNodePtr o, std::string m, bool safe = false)
        : object(std::move(o)), member(std::move(m)), safe(safe) {}
};

struct IndexExpr : ASTNode {
    ASTNodePtr object;
    ASTNodePtr index;
    IndexExpr(ASTNodePtr o, ASTNodePtr i)
        : object(std::move(o)), index(std::move(i)) {}
};

struct NewExpr : ASTNode {
    TypeAnnotation          type;
    std::vector<ASTNodePtr> args;
    NewExpr(TypeAnnotation t, std::vector<ASTNodePtr> a)
        : type(std::move(t)), args(std::move(a)) {}
};

struct CastExpr : ASTNode {
    TypeAnnotation targetType;
    ASTNodePtr     value;
    CastExpr(TypeAnnotation t, ASTNodePtr v)
        : targetType(std::move(t)), value(std::move(v)) {}
};

struct TernaryExpr : ASTNode {
    ASTNodePtr condition;
    ASTNodePtr thenExpr;
    ASTNodePtr elseExpr;
    TernaryExpr(ASTNodePtr c, ASTNodePtr t, ASTNodePtr e)
        : condition(std::move(c)), thenExpr(std::move(t)), elseExpr(std::move(e)) {}
};

struct LambdaExpr : ASTNode {
    std::vector<Parameter> params;
    ASTNodePtr             body; // BlockStmt or expression
    LambdaExpr(std::vector<Parameter> p, ASTNodePtr b)
        : params(std::move(p)), body(std::move(b)) {}
};

struct ArrayLiteralExpr : ASTNode {
    std::vector<ASTNodePtr> elements;
};

// Struct/object initializer: { x: 1.0, y: 2.0 }
struct StructInitExpr : ASTNode {
    ASTNodePtr typeName;
    std::vector<std::pair<std::string, ASTNodePtr>> fields;
};

struct NullCoalesceExpr : ASTNode {
    ASTNodePtr left;
    ASTNodePtr right;
    NullCoalesceExpr(ASTNodePtr l, ASTNodePtr r)
        : left(std::move(l)), right(std::move(r)) {}
};

struct NullAssertExpr : ASTNode {
    ASTNodePtr expr;
    explicit NullAssertExpr(ASTNodePtr e) : expr(std::move(e)) {}
};

// unsafe: alloc<T>(count)
struct AllocExpr : ASTNode {
    TypeAnnotation type;
    ASTNodePtr     count;
    AllocExpr(TypeAnnotation t, ASTNodePtr c) : type(std::move(t)), count(std::move(c)) {}
};

// unsafe: free(ptr)
struct FreeExpr : ASTNode {
    ASTNodePtr pointer;
    explicit FreeExpr(ASTNodePtr p) : pointer(std::move(p)) {}
};

// unsafe: *ptr
struct DerefExpr : ASTNode {
    ASTNodePtr pointer;
    explicit DerefExpr(ASTNodePtr p) : pointer(std::move(p)) {}
};

// unsafe: &value
struct AddressOfExpr : ASTNode {
    ASTNodePtr value;
    explicit AddressOfExpr(ASTNodePtr v) : value(std::move(v)) {}
};

struct ExprStmt : ASTNode {
    ASTNodePtr expr;
    explicit ExprStmt(ASTNodePtr e) : expr(std::move(e)) {}
};

struct BlockStmt : ASTNode {
    std::vector<ASTNodePtr> stmts;
};

struct VarDeclStmt : ASTNode {
    TypeAnnotation type;       // includes nullable flag; name="var" for inference
    bool           isConst = false;
    std::string    name;
    ASTNodePtr     initializer; // may be null
    VarDeclStmt(TypeAnnotation t, bool isConst, std::string n, ASTNodePtr init)
        : type(std::move(t)), isConst(isConst), name(std::move(n)), initializer(std::move(init)) {}
};

struct ReturnStmt : ASTNode {
    ASTNodePtr value; // null for void return
    explicit ReturnStmt(ASTNodePtr v = nullptr) : value(std::move(v)) {}
};

struct IfStmt : ASTNode {
    ASTNodePtr condition;
    ASTNodePtr thenBranch;
    ASTNodePtr elseBranch; // may be null
    IfStmt(ASTNodePtr c, ASTNodePtr t, ASTNodePtr e = nullptr)
        : condition(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {}
};

struct WhileStmt : ASTNode {
    ASTNodePtr condition;
    ASTNodePtr body;
    WhileStmt(ASTNodePtr c, ASTNodePtr b) : condition(std::move(c)), body(std::move(b)) {}
};

struct DoWhileStmt : ASTNode {
    ASTNodePtr body;
    ASTNodePtr condition;
    DoWhileStmt(ASTNodePtr b, ASTNodePtr c) : body(std::move(b)), condition(std::move(c)) {}
};

struct ForStmt : ASTNode {
    ASTNodePtr init;      // VarDeclStmt | ExprStmt | null
    ASTNodePtr condition; // may be null
    ASTNodePtr update;    // expression, may be null
    ASTNodePtr body;
    ForStmt(ASTNodePtr i, ASTNodePtr c, ASTNodePtr u, ASTNodePtr b)
        : init(std::move(i)), condition(std::move(c)), update(std::move(u)), body(std::move(b)) {}
};

struct ForEachStmt : ASTNode {
    TypeAnnotation varType;
    std::string    varName;
    ASTNodePtr     iterable;
    ASTNodePtr     body;
    ForEachStmt(TypeAnnotation t, std::string n, ASTNodePtr it, ASTNodePtr b)
        : varType(std::move(t)), varName(std::move(n)), iterable(std::move(it)), body(std::move(b)) {}
};

struct SwitchCase {
    ASTNodePtr              value;    // null = default case
    bool                    isDefault = false;
    std::vector<ASTNodePtr> stmts;
};

struct SwitchStmt : ASTNode {
    ASTNodePtr             value;
    std::vector<SwitchCase> cases;
};

struct BreakStmt    : ASTNode {};
struct ContinueStmt : ASTNode {};

struct CatchClause {
    TypeAnnotation type;
    std::string    varName;
    ASTNodePtr     body;
};

struct TryCatchStmt : ASTNode {
    ASTNodePtr              tryBody;
    std::vector<CatchClause> catches;
    ASTNodePtr              finallyBody; // may be null
};

struct ThrowStmt : ASTNode {
    ASTNodePtr value;
    explicit ThrowStmt(ASTNodePtr v) : value(std::move(v)) {}
};

struct UnsafeBlock : ASTNode {
    ASTNodePtr body;
    explicit UnsafeBlock(ASTNodePtr b) : body(std::move(b)) {}
};

struct IncludeDirective : ASTNode {
    std::string path;
    explicit IncludeDirective(std::string p) : path(std::move(p)) {}
};

struct PackageDecl : ASTNode {
    std::string name;
    explicit PackageDecl(std::string n) : name(std::move(n)) {}
};

struct NamespaceDecl : ASTNode {
    std::string             name;
    std::vector<ASTNodePtr> declarations;
    explicit NamespaceDecl(std::string n) : name(std::move(n)) {}
};

struct ImportDecl : ASTNode {
    std::string path;
    bool        wildcard = false;
    std::string symbol;
    ImportDecl(std::string p, bool w = false, std::string s = "")
        : path(std::move(p)), wildcard(w), symbol(std::move(s)) {}
};

struct FuncDecl : ASTNode {
    std::vector<Attribute> attributes;
    AccessModifier         access    = AccessModifier::Default;
    bool                   isStatic  = false;
    bool                   isAbstract= false;
    bool                   isOverride= false;
    bool                   isFinal   = false;
    TypeAnnotation         returnType;
    std::string            name;
    std::vector<std::string>  typeParams; // generic type params <T, U>
    std::vector<Parameter> params;
    ASTNodePtr             body;  // null for abstract
};

struct FieldDecl : ASTNode {
    std::vector<Attribute> attributes;
    AccessModifier access   = AccessModifier::Default;
    bool           isStatic = false;
    bool           isConst  = false;
    TypeAnnotation type;
    std::string    name;
    ASTNodePtr     initializer; // may be null
};

struct ConstructorDecl : ASTNode {
    AccessModifier         access = AccessModifier::Default;
    std::vector<Parameter> params;
    ASTNodePtr             body;
};

struct DestructorDecl : ASTNode {
    ASTNodePtr body;
};

struct OperatorDecl : ASTNode {
    AccessModifier         access = AccessModifier::Public;
    std::string            op;         // +, -, ==, etc.
    std::vector<Parameter> params;
    TypeAnnotation         returnType;
    ASTNodePtr             body;
};

struct PropertyDecl : ASTNode {
    std::vector<Attribute> attributes;
    AccessModifier access   = AccessModifier::Default;
    bool           isStatic = false;
    TypeAnnotation type;
    std::string    name;
    bool           hasGet   = false;
    ASTNodePtr     getDecl; // Body (BlockStmt or Expr), null if auto-property
    bool           hasSet   = false;
    ASTNodePtr     setDecl; // Body (BlockStmt or Expr), null if auto-property
    ASTNodePtr     initializer; // may be null
};

struct IndexerDecl : ASTNode {
    AccessModifier         access = AccessModifier::Default;
    TypeAnnotation         type;
    std::vector<Parameter> params;
    bool                   hasGet = false;
    ASTNodePtr             getDecl;
    bool                   hasSet = false;
    ASTNodePtr             setDecl;
};


struct ClassDecl : ASTNode {
    std::vector<Attribute> attributes;
    AccessModifier                      access     = AccessModifier::Default;
    bool                                isAbstract = false;
    bool                                isFinal    = false;
    bool                                isStatic   = false;
    std::string                         name;
    std::vector<std::string>            typeParams;
    std::optional<TypeAnnotation>       superClass;
    std::vector<TypeAnnotation>         interfaces;
    std::vector<std::unique_ptr<FieldDecl>>       fields;
    std::vector<std::unique_ptr<ConstructorDecl>> constructors;
    std::unique_ptr<DestructorDecl>               destructor;
    std::vector<std::unique_ptr<FuncDecl>>        methods;
    std::vector<std::unique_ptr<OperatorDecl>>    operators;
    std::vector<std::unique_ptr<PropertyDecl>>    properties;
    std::vector<std::unique_ptr<IndexerDecl>>     indexers;
};

struct InterfaceDecl : ASTNode {
    AccessModifier                   access = AccessModifier::Default;
    std::string                      name;
    std::vector<std::string>         typeParams;
    std::vector<std::unique_ptr<FuncDecl>>   methods;
    std::vector<std::unique_ptr<FieldDecl>>  constants;
    std::vector<std::unique_ptr<PropertyDecl>> properties;
    std::vector<std::unique_ptr<IndexerDecl>>  indexers;
};

struct EnumValue {
    std::string name;
    ASTNodePtr  value; // may be null (auto-numbered)
};

struct EnumDecl : ASTNode {
    std::vector<Attribute> attributes;
    AccessModifier                      access = AccessModifier::Default;
    std::string                         name;
    std::vector<EnumValue>              values;
    std::vector<std::unique_ptr<FieldDecl>>      fields;
    std::vector<std::unique_ptr<ConstructorDecl>> constructors;
    std::vector<std::unique_ptr<FuncDecl>>       methods;
};

struct StructDecl : ASTNode {
    std::vector<Attribute> attributes;
    std::string                          name;
    std::vector<std::unique_ptr<FieldDecl>>  fields;
    std::vector<std::unique_ptr<FuncDecl>>   methods;
};

struct Program : ASTNode {
    std::string             filename;
    std::vector<ASTNodePtr> declarations;
};

}
