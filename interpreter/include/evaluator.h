#pragma once
// evaluator.h — Tree-Walk Evaluator and Runtime Value Types for Ume

#include "../../compiler/include/ast.h"
#include "environment.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <variant>
#include <stdexcept>
#include <sstream>

namespace Ume {

// ─────────────────────────────────────────────────────────────
// Forward declarations
// ─────────────────────────────────────────────────────────────
struct Value;
struct ObjectInstance;
struct ArrayInstance;
struct FunctionInstance;
class  Evaluator;

using ValuePtr    = std::shared_ptr<Value>;
using ObjPtr      = std::shared_ptr<ObjectInstance>;
using ArrPtr      = std::shared_ptr<ArrayInstance>;
using FuncPtr     = std::shared_ptr<FunctionInstance>;

// ─────────────────────────────────────────────────────────────
// Runtime Value
// ─────────────────────────────────────────────────────────────
struct Value {
    enum class Kind {
        Null, Bool, Int, Float, Char, String, Object, Array, Function
    };

    Kind    kind   = Kind::Null;
    bool    boolVal{};
    int64_t intVal{};
    double  floatVal{};
    char    charVal{};
    std::string strVal;
    ObjPtr  objVal;
    ArrPtr  arrVal;
    FuncPtr funcVal;

    // ── Factory helpers ───────────────────────────────────
    static Value makeNull()                         { return Value{}; }
    static Value makeBool(bool b)                   { Value v; v.kind = Kind::Bool;   v.boolVal  = b; return v; }
    static Value makeInt(int64_t i)                 { Value v; v.kind = Kind::Int;    v.intVal   = i; return v; }
    static Value makeFloat(double f)                { Value v; v.kind = Kind::Float;  v.floatVal = f; return v; }
    static Value makeChar(char c)                   { Value v; v.kind = Kind::Char;   v.charVal  = c; return v; }
    static Value makeString(std::string s)          { Value v; v.kind = Kind::String; v.strVal   = std::move(s); return v; }
    static Value makeObject(ObjPtr o)               { Value v; v.kind = Kind::Object; v.objVal   = std::move(o); return v; }
    static Value makeArray(ArrPtr a)                { Value v; v.kind = Kind::Array;  v.arrVal   = std::move(a); return v; }
    static Value makeFunction(FuncPtr f)            { Value v; v.kind = Kind::Function; v.funcVal = std::move(f); return v; }

    bool isNull()     const { return kind == Kind::Null; }
    bool isBool()     const { return kind == Kind::Bool; }
    bool isInt()      const { return kind == Kind::Int; }
    bool isFloat()    const { return kind == Kind::Float; }
    bool isNumeric()  const { return kind == Kind::Int || kind == Kind::Float; }
    bool isChar()     const { return kind == Kind::Char; }
    bool isString()   const { return kind == Kind::String; }
    bool isObject()   const { return kind == Kind::Object; }
    bool isArray()    const { return kind == Kind::Array; }
    bool isFunction() const { return kind == Kind::Function; }

    double  toDouble() const;
    bool    toBool()   const;
    std::string toString() const;

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const { return !(*this == other); }
    bool operator<(const Value& other)  const;
};

// ─────────────────────────────────────────────────────────────
// Runtime class instance
// ─────────────────────────────────────────────────────────────
struct ObjectInstance {
    std::string                              className;
    std::unordered_map<std::string, Value>   fields;
    const ClassDecl*                         classDef = nullptr; // non-owning
};

// ─────────────────────────────────────────────────────────────
// Runtime array
// ─────────────────────────────────────────────────────────────
struct ArrayInstance {
    std::vector<Value> elements;
};

// ─────────────────────────────────────────────────────────────
// Runtime function / lambda / method closure
// ─────────────────────────────────────────────────────────────
struct FunctionInstance {
    std::string                      name;
    std::vector<Parameter>           params;  // copied from AST
    const ASTNode*                   body      = nullptr; // non-owning
    std::shared_ptr<Environment>     closure;
    // For native (built-in) functions
    std::function<Value(std::vector<Value>)> native;
    bool isNative() const { return (bool)native; }
};

// ─────────────────────────────────────────────────────────────
// Control-flow signals (thrown as C++ exceptions internally)
// ─────────────────────────────────────────────────────────────
struct ReturnSignal {
    Value value;
    explicit ReturnSignal(Value v) : value(std::move(v)) {}
};
struct BreakSignal  {};
struct ContinueSignal {};

// ─────────────────────────────────────────────────────────────
// Ume runtime exception (from `throw`)
// ─────────────────────────────────────────────────────────────
class UmeRuntimeException : public std::exception {
public:
    Value       value;  // the thrown Ume value
    int         line     = 0;
    int         column   = 0;
    std::string filename;
    std::string callStack;

    explicit UmeRuntimeException(Value v, int line = 0, int col = 0, std::string filename = "")
        : value(std::move(v)), line(line), column(col), filename(std::move(filename)) {}

    const char* what() const noexcept override;
};

// ─────────────────────────────────────────────────────────────
// Evaluator — tree-walk interpreter
// ─────────────────────────────────────────────────────────────
class Evaluator {
public:
    Evaluator();

    // Evaluate a full parsed program; returns exit code
    int run(const Program& program);

    // Load declarations from a library program without running main()
    void loadLibrary(const Program& prog);

    // Evaluate a single node in the given environment
    Value eval(const ASTNode& node, std::shared_ptr<Environment> env);

    // Set program arguments (exposed to System.args())
    void setArgs(std::vector<std::string> args) { programArgs_ = std::move(args); }

private:
    // Stored class / enum / struct definitions
    std::unordered_map<std::string, const ClassDecl*>     classDefs_;
    std::unordered_map<std::string, const InterfaceDecl*> ifaceDefs_;
    std::unordered_map<std::string, const EnumDecl*>      enumDefs_;
    std::unordered_map<std::string, const StructDecl*>    structDefs_;

    // Global environment
    std::shared_ptr<Environment> global_;

    // Program arguments (from CLI)
    std::vector<std::string> programArgs_;

    // ── Bootstrapping ─────────────────────────────────────
    void registerBuiltins(std::shared_ptr<Environment> env);
    void registerConsole(std::shared_ptr<Environment> env);
    void registerMathExtended(std::shared_ptr<Environment> env);
    void registerSystem(std::shared_ptr<Environment> env);
    void registerFileSystem(std::shared_ptr<Environment> env);
    void registerStringUtils(std::shared_ptr<Environment> env);
    void registerGraphics(std::shared_ptr<Environment> env);
    void collectDeclarations(const Program& program,
                             std::shared_ptr<Environment> env);

    // ── Statement evaluation ──────────────────────────────
    Value evalBlock(const BlockStmt& block, std::shared_ptr<Environment> env);
    Value evalVarDecl(const VarDeclStmt& stmt, std::shared_ptr<Environment> env);
    Value evalIf(const IfStmt& stmt, std::shared_ptr<Environment> env);
    Value evalWhile(const WhileStmt& stmt, std::shared_ptr<Environment> env);
    Value evalDoWhile(const DoWhileStmt& stmt, std::shared_ptr<Environment> env);
    Value evalFor(const ForStmt& stmt, std::shared_ptr<Environment> env);
    Value evalForEach(const ForEachStmt& stmt, std::shared_ptr<Environment> env);
    Value evalSwitch(const SwitchStmt& stmt, std::shared_ptr<Environment> env);
    Value evalTryCatch(const TryCatchStmt& stmt, std::shared_ptr<Environment> env);
    Value evalUnsafe(const UnsafeBlock& block, std::shared_ptr<Environment> env);

    // ── Expression evaluation ─────────────────────────────
    Value evalBinary(const BinaryExpr& expr, std::shared_ptr<Environment> env);
    Value evalUnary(const UnaryExpr& expr, std::shared_ptr<Environment> env);
    Value evalAssign(const AssignExpr& expr, std::shared_ptr<Environment> env);
    Value evalCall(const CallExpr& expr, std::shared_ptr<Environment> env);
    Value evalMemberAccess(const MemberAccessExpr& expr, std::shared_ptr<Environment> env);
    Value evalIndex(const IndexExpr& expr, std::shared_ptr<Environment> env);
    Value evalNew(const NewExpr& expr, std::shared_ptr<Environment> env);
    Value evalCast(const CastExpr& expr, std::shared_ptr<Environment> env);
    Value evalLambda(const LambdaExpr& expr, std::shared_ptr<Environment> env);
    Value evalInterpolatedString(const InterpolatedStringExpr& expr, std::shared_ptr<Environment> env);
    Value evalNullCoalesce(const NullCoalesceExpr& expr, std::shared_ptr<Environment> env);
    Value evalNullAssert(const NullAssertExpr& expr, std::shared_ptr<Environment> env);
    Value evalStructInit(const StructInitExpr& expr, std::shared_ptr<Environment> env);
    Value evalArrayLiteral(const ArrayLiteralExpr& expr, std::shared_ptr<Environment> env);

    // ── Function call helpers ─────────────────────────────
    Value callFunction(const FunctionInstance& func,
                       std::vector<Value> args,
                       std::shared_ptr<Value> thisVal = nullptr);
    Value callMethod(Value& object, const std::string& method,
                     std::vector<Value> args,
                     std::shared_ptr<Environment> env);
    void  instantiateFields(ObjectInstance& obj, const ClassDecl& cls,
                            std::shared_ptr<Environment> env);
    void  runConstructor(ObjectInstance& obj, const ClassDecl& cls,
                         std::vector<Value> args,
                         std::shared_ptr<Environment> env);

    // ── Built-in method dispatch ──────────────────────────
    Value callBuiltinMethod(Value& object, const std::string& method,
                            std::vector<Value> args);
    Value callStringMethod(Value& str, const std::string& method,
                           std::vector<Value> args);
    Value callArrayMethod(Value& arr, const std::string& method,
                          std::vector<Value> args);
    Value callListMethod(ObjectInstance& obj, const std::string& method,
                         std::vector<Value> args);

    // ── Lvalue resolution for assignment ─────────────────
    Value& resolveAssignTarget(const ASTNode& node, std::shared_ptr<Environment> env);

    // ── Arithmetic / comparison helpers ──────────────────
    Value arith(const std::string& op, const Value& l, const Value& r);
    Value compare(const std::string& op, const Value& l, const Value& r);
    bool  isTruthy(const Value& v) const;

    // ── Class lookup helpers ──────────────────────────────
    const ClassDecl* findClass(const std::string& name) const;
    const EnumDecl*  findEnum(const std::string& name) const;
    const FuncDecl*  findMethod(const ClassDecl& cls, const std::string& name) const;
    const FieldDecl* findField(const ClassDecl& cls, const std::string& name) const;
};

} // namespace Ume
