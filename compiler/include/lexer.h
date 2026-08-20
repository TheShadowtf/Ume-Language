#pragma once
// lexer.h — Tokenizer for the Ume Language

#include <string>
#include <vector>
#include <unordered_map>

namespace Ume {

enum class TokenType {
    // ── Literals ──────────────────────────────────────────
    INTEGER_LITERAL,
    FLOAT_LITERAL,      // ends with f
    DOUBLE_LITERAL,
    STRING_LITERAL,
    CHAR_LITERAL,
    INTERP_STRING,      // $"..." — handed to parser for interpolation
    BOOL_LITERAL,       // true / false (also keywords)
    // ── Identifier ────────────────────────────────────────
    IDENTIFIER,
    // ── Type keywords ─────────────────────────────────────
    KW_INT, KW_LONG, KW_SHORT, KW_BYTE,
    KW_FLOAT, KW_DOUBLE, KW_BOOL, KW_CHAR,
    KW_STRING, KW_VOID, KW_ANY,
    // ── Control flow ──────────────────────────────────────
    KW_IF, KW_ELSE, KW_WHILE, KW_DO, KW_FOR, KW_FOREACH, KW_IN,
    KW_SWITCH, KW_CASE, KW_DEFAULT,
    KW_BREAK, KW_CONTINUE, KW_RETURN,
    // ── OOP ───────────────────────────────────────────────
    KW_CLASS, KW_INTERFACE, KW_ENUM, KW_STRUCT,
    KW_EXTENDS, KW_IMPLEMENTS,
    KW_PUBLIC, KW_PRIVATE, KW_PROTECTED, KW_INTERNAL,
    KW_STATIC, KW_ABSTRACT, KW_FINAL, KW_OVERRIDE,
    KW_NEW, KW_THIS, KW_SUPER,
    // ── Function / Variable ───────────────────────────────
    KW_FUNC, KW_VAR, KW_CONST,
    // ── Values ────────────────────────────────────────────
    KW_NULL, KW_TRUE, KW_FALSE,
    // ── Exception handling ────────────────────────────────
    KW_TRY, KW_CATCH, KW_FINALLY, KW_THROW,
    // ── Module system ─────────────────────────────────────
    KW_IMPORT, KW_PACKAGE, KW_NAMESPACE,
    // ── Special ───────────────────────────────────────────
    KW_OPERATOR, KW_CAST, KW_ALLOC, KW_FREE, KW_UNSAFE, KW_AS,
    // ── Arithmetic operators ──────────────────────────────
    PLUS, MINUS, STAR, SLASH, PERCENT,
    // ── Assignment operators ──────────────────────────────
    ASSIGN,
    PLUS_ASSIGN, MINUS_ASSIGN, STAR_ASSIGN, SLASH_ASSIGN, PERCENT_ASSIGN,
    // ── Comparison operators ──────────────────────────────
    EQ, NEQ, LT, GT, LTE, GTE,
    // ── Logical operators ─────────────────────────────────
    AND, OR, NOT,
    // ── Bitwise operators ─────────────────────────────────
    BIT_AND, BIT_OR, BIT_XOR, BIT_NOT,
    LSHIFT, RSHIFT,
    // ── Increment / Decrement ─────────────────────────────
    INCREMENT, DECREMENT,
    // ── Arrow operators ───────────────────────────────────
    ARROW,          // ->
    FAT_ARROW,      // =>
    // ── Null safety ───────────────────────────────────────
    QUESTION,       // ?
    SAFE_DOT,       // ?.
    NULL_COALESCE,  // ??
    // ── Delimiters ────────────────────────────────────────
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,
    SEMICOLON, COLON, DOUBLE_COLON,
    DOT, COMMA,
    AT, HASH, DOLLAR, TILDE,
    ELLIPSIS,       // ...
    // ── Preprocessor ──────────────────────────────────────
    DIRECTIVE_INCLUDE,  // #include
    DIRECTIVE_IMPORT,   // #import
    DIRECTIVE_LINE,     // #line
    // ── Misc ──────────────────────────────────────────────
    EOF_TOKEN,
    ERROR
};

struct Token {
    TokenType   type;
    std::string value;
    int         line;
    int         column;
    std::string filename;

    Token(TokenType t, std::string v, int l, int c, std::string f = "")
        : type(t), value(std::move(v)), line(l), column(c), filename(std::move(f)) {}

    bool is(TokenType t)                         const { return type == t; }
    bool isAny(std::initializer_list<TokenType>) const;
};

// ─────────────────────────────────────────────────────────────
// Lexer
// ─────────────────────────────────────────────────────────────
class Lexer {
public:
    explicit Lexer(std::string source, std::string filename = "<unknown>");

    std::vector<Token> tokenize();
    static std::string tokenTypeName(TokenType t);

private:
    std::string source_;
    std::string filename_;
    size_t      pos_    = 0;
    int         line_   = 1;
    int         column_ = 1;

    static const std::unordered_map<std::string, TokenType> kKeywords;

    char    current()               const;
    char    peek(int offset = 1)    const;
    char    advance();
    bool    match(char expected);
    bool    isAtEnd()               const;
    void    skipWhitespace();
    void    skipLineComment();
    void    skipBlockComment();

    Token makeToken(TokenType t, std::string value = "");
    Token lexNumber();
    Token lexString();
    Token lexInterpolatedString();
    Token lexChar();
    Token lexIdentifierOrKeyword();
    Token lexPreprocessorDirective();
    Token lexOperatorOrDelimiter();
    Token errorToken(const std::string& msg);
};

} // namespace Ume
