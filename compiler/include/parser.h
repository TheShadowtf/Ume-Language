#pragma once
// parser.h — Recursive Descent Parser for the Ume Language

#include "lexer.h"
#include "ast.h"
#include <stdexcept>
#include <functional>

namespace Ume {

// ─────────────────────────────────────────────────────────────
// ParseError — thrown on syntax errors
// ─────────────────────────────────────────────────────────────
class ParseError : public std::runtime_error {
public:
    int line;
    int column;
    std::string filename;
    std::string expectedToken;
    std::string foundToken;
    std::string hint;

    ParseError(const std::string& msg, int line, int column, std::string filename = "",
               std::string expectedToken = "", std::string foundToken = "", std::string hint = "")
        : std::runtime_error(msg), line(line), column(column), filename(std::move(filename)),
          expectedToken(std::move(expectedToken)), foundToken(std::move(foundToken)), hint(std::move(hint)) {}
};

// ─────────────────────────────────────────────────────────────
// Parser
// ─────────────────────────────────────────────────────────────
class Parser {
public:
    explicit Parser(std::vector<Token> tokens, std::string filename = "<unknown>");

    std::unique_ptr<Program> parse();

private:
    std::vector<Token> tokens_;
    std::string        filename_;
    std::string        rootFilename_;
    size_t             pos_ = 0;
    int                pendingGT_ = 0; // for >> in nested generics

    // ── Token navigation ─────────────────────────────────
    const Token& current()                              const;
    const Token& peek(int offset = 1)                   const;
    const Token& advance();
    bool         check(TokenType t)                     const;
    bool         match(TokenType t);
    bool         matchAny(std::initializer_list<TokenType> types);
    Token        expect(TokenType t, const std::string& msg);
    bool         consumeGT(); // consumes a '>', splitting >> if needed
    bool         isAtEnd()                              const;

    ParseError error(const std::string& msg)            const;
    ParseError errorAt(const Token& tok, const std::string& msg) const;
    ParseError errorWithContext(TokenType expected, const std::string& msg) const;

    template <typename T, typename... Args>
    std::unique_ptr<T> makeNode(const Token& tok, Args&&... args) const {
        auto node = std::make_unique<T>(std::forward<Args>(args)...);
        node->line = tok.line;
        node->column = tok.column;
        node->filename = tok.filename.empty() ? filename_ : tok.filename;
        return node;
    }

    // ── Type annotation parsing ───────────────────────────
    TypeAnnotation parseTypeAnnotation();
    TypeAnnotation parseTypeAnnotationBasic();

    // ── Access modifiers ─────────────────────────────────
    AccessModifier parseAccessModifier();
    bool           hasAccessModifier()                  const;

    // ── Top-level declarations ────────────────────────────
    std::vector<Attribute> parseAttributes();
    ASTNodePtr parseTopLevel();
    ASTNodePtr parseIncludeDirective();
    ASTNodePtr parseImportDirective();
    ASTNodePtr parsePackageDecl();
    ASTNodePtr parseNamespaceDecl();
    ASTNodePtr parseImportDecl();
    ASTNodePtr parseFuncDecl(AccessModifier access, bool isStatic, bool isAbstract,
                             bool isOverride, bool isFinal);
    ASTNodePtr parseClassDecl(AccessModifier access);
    ASTNodePtr parseInterfaceDecl(AccessModifier access);
    ASTNodePtr parseEnumDecl(AccessModifier access);
    ASTNodePtr parseStructDecl();

    // ── Class member parsing ──────────────────────────────
    std::unique_ptr<ConstructorDecl> parseConstructor(AccessModifier access,
                                                       const std::string& className);
    std::unique_ptr<DestructorDecl>  parseDestructor();
    std::unique_ptr<FieldDecl>       parseField(AccessModifier access, bool isStatic, bool isConst);
    std::unique_ptr<PropertyDecl>    parseProperty(AccessModifier access, bool isStatic, TypeAnnotation type, std::string name);
    std::unique_ptr<IndexerDecl>     parseIndexer(AccessModifier access, TypeAnnotation type);
    std::unique_ptr<OperatorDecl>    parseOperatorDecl(AccessModifier access);

    // ── Parameters ───────────────────────────────────────
    std::vector<Parameter> parseParamList();
    Parameter              parseParameter();

    // ── Statements ───────────────────────────────────────
    ASTNodePtr parseStatement();
    ASTNodePtr parseBlock();
    ASTNodePtr parseVarDecl(bool isConst = false);
    ASTNodePtr parseReturnStmt();
    ASTNodePtr parseIfStmt();
    ASTNodePtr parseWhileStmt();
    ASTNodePtr parseDoWhileStmt();
    ASTNodePtr parseForStmt();
    ASTNodePtr parseSwitchStmt();
    ASTNodePtr parseTryCatchStmt();
    ASTNodePtr parseThrowStmt();
    ASTNodePtr parseUnsafeBlock();

    // ── Expressions (precedence climbing) ────────────────
    ASTNodePtr parseExpression();
    ASTNodePtr parseAssignment();
    ASTNodePtr parseTernary();
    ASTNodePtr parseNullCoalesce();
    ASTNodePtr parseOr();
    ASTNodePtr parseAnd();
    ASTNodePtr parseBitOr();
    ASTNodePtr parseBitXor();
    ASTNodePtr parseBitAnd();
    ASTNodePtr parseEquality();
    ASTNodePtr parseRelational();
    ASTNodePtr parseShift();
    ASTNodePtr parseAdditive();
    ASTNodePtr parseMultiplicative();
    ASTNodePtr parseUnary();
    ASTNodePtr parsePostfix(ASTNodePtr expr);
    ASTNodePtr parsePrimary();

    // ── Lambda parsing ────────────────────────────────────
    ASTNodePtr parseLambda();

    // ── Argument list ─────────────────────────────────────
    std::vector<ASTNodePtr> parseArgList();

    // ── Interpolated string ───────────────────────────────
    ASTNodePtr parseInterpolatedString(const std::string& raw);

    // ── Helpers ───────────────────────────────────────────
    bool isTypeStart() const;
    bool isForEachLoop();
    bool isLambdaStart();
    std::string currentIdentifierValue() const;
    // Find a likely token that should have started a block for better error locations
    Token findLikelyBlockStarter() const;
};

} // namespace Ume
