// lexer.cpp — Tokenizer implementation for the Ume Language
#include "../include/lexer.h"

#include <stdexcept>
#include <sstream>
#include <cctype>
#include <algorithm>

namespace Ume {

// Keyword table
const std::unordered_map<std::string, TokenType> Lexer::kKeywords = {
    // Types
    {"int",        TokenType::KW_INT},
    {"long",       TokenType::KW_LONG},
    {"short",      TokenType::KW_SHORT},
    {"byte",       TokenType::KW_BYTE},
    {"float",      TokenType::KW_FLOAT},
    {"double",     TokenType::KW_DOUBLE},
    {"bool",       TokenType::KW_BOOL},
    {"char",       TokenType::KW_CHAR},
    {"string",     TokenType::KW_STRING},
    {"void",       TokenType::KW_VOID},
    {"any",        TokenType::KW_ANY},
    // Control flow
    {"if",         TokenType::KW_IF},
    {"else",       TokenType::KW_ELSE},
    {"while",      TokenType::KW_WHILE},
    {"do",         TokenType::KW_DO},
    {"for",        TokenType::KW_FOR},
    {"foreach",    TokenType::KW_FOREACH},
    {"in",         TokenType::KW_IN},
    {"switch",     TokenType::KW_SWITCH},
    {"case",       TokenType::KW_CASE},
    {"default",    TokenType::KW_DEFAULT},
    {"break",      TokenType::KW_BREAK},
    {"continue",   TokenType::KW_CONTINUE},
    {"return",     TokenType::KW_RETURN},
    // OOP
    {"class",      TokenType::KW_CLASS},
    {"interface",  TokenType::KW_INTERFACE},
    {"enum",       TokenType::KW_ENUM},
    {"struct",     TokenType::KW_STRUCT},
    {"extends",    TokenType::KW_EXTENDS},
    {"implements", TokenType::KW_IMPLEMENTS},
    {"public",     TokenType::KW_PUBLIC},
    {"private",    TokenType::KW_PRIVATE},
    {"protected",  TokenType::KW_PROTECTED},
    {"internal",   TokenType::KW_INTERNAL},
    {"static",     TokenType::KW_STATIC},
    {"abstract",   TokenType::KW_ABSTRACT},
    {"final",      TokenType::KW_FINAL},
    {"override",   TokenType::KW_OVERRIDE},
    {"new",        TokenType::KW_NEW},
    {"this",       TokenType::KW_THIS},
    {"super",      TokenType::KW_SUPER},
    // Function / variable
    {"func",       TokenType::KW_FUNC},
    {"var",        TokenType::KW_VAR},
    {"auto",       TokenType::KW_VAR},
    {"const",      TokenType::KW_CONST},
    // Values
    {"null",       TokenType::KW_NULL},
    {"true",       TokenType::KW_TRUE},
    {"false",      TokenType::KW_FALSE},
    // Exceptions
    {"try",        TokenType::KW_TRY},
    {"catch",      TokenType::KW_CATCH},
    {"finally",    TokenType::KW_FINALLY},
    {"throw",      TokenType::KW_THROW},
    // Modules
    {"import",     TokenType::KW_IMPORT},
    {"package",    TokenType::KW_PACKAGE},
    {"namespace",  TokenType::KW_NAMESPACE},
    // Special
    {"operator",   TokenType::KW_OPERATOR},
    {"cast",       TokenType::KW_CAST},
    {"alloc",      TokenType::KW_ALLOC},
    {"free",       TokenType::KW_FREE},
    {"unsafe",     TokenType::KW_UNSAFE},
    {"as",         TokenType::KW_AS},
};

Lexer::Lexer(std::string source, std::string filename)
    : source_(std::move(source)), filename_(std::move(filename)) {}

char Lexer::current() const {
    return isAtEnd() ? '\0' : source_[pos_];
}

char Lexer::peek(int offset) const {
    size_t idx = pos_ + static_cast<size_t>(offset);
    return idx < source_.size() ? source_[idx] : '\0';
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') { ++line_; column_ = 1; }
    else           { ++column_; }
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source_[pos_] != expected) return false;
    advance();
    return true;
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

Token Lexer::makeToken(TokenType t, std::string value) {
    return Token(t, std::move(value), line_, column_, filename_);
}

Token Lexer::errorToken(const std::string& msg) {
    return Token(TokenType::ERROR, msg, line_, column_, filename_);
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = current();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek() == '/') {
            skipLineComment();
        } else if (c == '/' && peek() == '*') {
            skipBlockComment();
        } else {
            break;
        }
    }
}

void Lexer::skipLineComment() {
    while (!isAtEnd() && current() != '\n') advance();
}

void Lexer::skipBlockComment() {
    advance(); advance(); // consume /*
    while (!isAtEnd()) {
        if (current() == '*' && peek() == '/') {
            advance(); advance(); return;
        }
        advance();
    }
    // unterminated block comment — not fatal for lexer
}

Token Lexer::lexNumber() {
    int    startLine   = line_;
    int    startColumn = column_;
    std::string num;
    bool   isFloat = false;

    // Hex literal  0x5F  ->  convert to decimal so stoll(value) works everywhere
    if (current() == '0' && (peek() == 'x' || peek() == 'X')) {
        advance(); advance(); // consume 0x
        std::string hex;
        while (!isAtEnd() && std::isxdigit(static_cast<unsigned char>(current())))
            hex += advance();
        if (hex.empty()) return Token(TokenType::ERROR, "Invalid hex literal", startLine, startColumn, filename_);
        int64_t val = 0;
        try { val = std::stoll(hex, nullptr, 16); }
        catch (...) { return Token(TokenType::ERROR, "Hex literal too large", startLine, startColumn, filename_); }
        return Token(TokenType::INTEGER_LITERAL, std::to_string(val), startLine, startColumn, filename_);
    }

    // Binary literal  0b1010  ->  convert to decimal
    if (current() == '0' && (peek() == 'b' || peek() == 'B')) {
        advance(); advance(); // consume 0b
        std::string bin;
        while (!isAtEnd() && (current() == '0' || current() == '1'))
            bin += advance();
        if (bin.empty()) return Token(TokenType::ERROR, "Invalid binary literal", startLine, startColumn, filename_);
        int64_t val = 0;
        for (char c : bin) val = (val << 1) | (c - '0');
        return Token(TokenType::INTEGER_LITERAL, std::to_string(val), startLine, startColumn, filename_);
    }

    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(current())))
        num += advance();

    if (!isAtEnd() && current() == '.' && std::isdigit(static_cast<unsigned char>(peek()))) {
        isFloat = true;
        num += advance(); // '.'
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(current())))
            num += advance();
    }

    if (!isAtEnd() && (current() == 'e' || current() == 'E')) {
        isFloat = true;
        num += advance();
        if (!isAtEnd() && (current() == '+' || current() == '-')) num += advance();
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(current())))
            num += advance();
    }

    if (!isAtEnd() && current() == 'f') {
        advance();
        return Token(TokenType::FLOAT_LITERAL, num, startLine, startColumn, filename_);
    }

    if (isFloat)
        return Token(TokenType::DOUBLE_LITERAL, num, startLine, startColumn, filename_);

    return Token(TokenType::INTEGER_LITERAL, num, startLine, startColumn, filename_);
}

Token Lexer::lexString() {
    int startLine = line_, startCol = column_;
    advance(); // opening "
    std::string value;
    while (!isAtEnd() && current() != '"') {
        if (current() == '\\') {
            advance();
            switch (current()) {
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                case 'r':  value += '\r'; break;
                case '"':  value += '"';  break;
                case '\\': value += '\\'; break;
                case '0':  value += '\0'; break;
                default:   value += current(); break;
            }
            advance();
        } else {
            value += advance();
        }
    }
    if (isAtEnd()) return errorToken("Unterminated string literal");
    advance(); // closing "
    return Token(TokenType::STRING_LITERAL, value, startLine, startCol, filename_);
}

// Interpolated string $"Hello {name}!"
// We store the raw string content (between $ " and ") and let the parser
// split it into literal + expression segments.
Token Lexer::lexInterpolatedString() {
    int startLine = line_, startCol = column_;
    advance(); // $
    advance(); // opening "
    std::string raw;
    int depth = 0;
    while (!isAtEnd()) {
        char c = current();
        if (c == '\\') {
            raw += advance();
            if (!isAtEnd()) raw += advance();
            continue;
        }
        if (c == '{') { depth++; raw += advance(); continue; }
        if (c == '}') {
            if (depth > 0) { depth--; raw += advance(); continue; }
            // lone } outside expression — include literally
            raw += advance(); continue;
        }
        if (c == '"' && depth == 0) { advance(); break; } // closing "
        raw += advance();
    }
    return Token(TokenType::INTERP_STRING, raw, startLine, startCol, filename_);
}

Token Lexer::lexChar() {
    int startLine = line_, startCol = column_;
    advance(); // opening '
    std::string value;
    if (current() == '\\') {
        advance();
        switch (current()) {
            case 'n':  value = "\n"; break;
            case 't':  value = "\t"; break;
            case 'r':  value = "\r"; break;
            case '\'': value = "'";  break;
            case '\\': value = "\\"; break;
            case '0':  value = "\0"; break;
            default:   value = std::string(1, current()); break;
        }
        advance();
    } else if (!isAtEnd() && current() != '\'') {
        value = std::string(1, advance());
    }
    if (!isAtEnd() && current() == '\'') advance();
    return Token(TokenType::CHAR_LITERAL, value, startLine, startCol, filename_);
}

Token Lexer::lexIdentifierOrKeyword() {
    int startLine = line_, startCol = column_;
    std::string ident;
    while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(current())) || current() == '_'))
        ident += advance();

    auto it = kKeywords.find(ident);
    if (it != kKeywords.end())
        return Token(it->second, ident, startLine, startCol, filename_);

    return Token(TokenType::IDENTIFIER, ident, startLine, startCol, filename_);
}

Token Lexer::lexPreprocessorDirective() {
    int startLine = line_, startCol = column_;
    advance(); // '#'
    std::string dir;
    while (!isAtEnd() && std::isalpha(static_cast<unsigned char>(current())))
        dir += advance();

    if (dir == "include") {
        // skip whitespace
        while (!isAtEnd() && (current() == ' ' || current() == '\t')) advance();
        std::string path;
        char delim = current() == '<' ? '>' : '"';
        advance(); // opening < or "
        while (!isAtEnd() && current() != delim) path += advance();
        if (!isAtEnd()) advance(); // closing
        return Token(TokenType::DIRECTIVE_INCLUDE, path, startLine, startCol, filename_);
    }
    if (dir == "import") {
        while (!isAtEnd() && (current() == ' ' || current() == '\t')) advance();
        std::string path;
        if (current() == '"' || current() == '<') {
            char delim = current() == '<' ? '>' : '"';
            advance();
            while (!isAtEnd() && current() != delim) path += advance();
            if (!isAtEnd()) advance();
        } else {
            while (!isAtEnd() && current() != ';' && current() != '\n') {
                if (current() != ' ' && current() != '\t' && current() != '\r')
                    path += current();
                advance();
            }
        }
        if (!isAtEnd() && current() == ';') advance();
        return Token(TokenType::DIRECTIVE_IMPORT, path, startLine, startCol, filename_);
    }
    if (dir == "line") {
        while (!isAtEnd() && (current() == ' ' || current() == '\t')) advance();
        int newLine = 0;
        std::string path;
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(current()))) {
            newLine = newLine * 10 + (current() - '0');
            advance();
        }
        while (!isAtEnd() && (current() == ' ' || current() == '\t')) advance();
        if (!isAtEnd() && (current() == '"' || current() == '<')) {
            char delim = current() == '<' ? '>' : '"';
            advance();
            while (!isAtEnd() && current() != delim) path += advance();
            if (!isAtEnd()) advance();
        }
        while (!isAtEnd() && current() != '\n') advance();
        if (!isAtEnd() && current() == '\n') {
            advance();
            line_ = newLine;
            column_ = 1;
            if (!path.empty()) filename_ = path;
        }
        return Token(TokenType::DIRECTIVE_LINE, path, startLine, startCol, filename_);
    }
    // Unknown directive — skip rest of line
    while (!isAtEnd() && current() != '\n') advance();
    return Token(TokenType::IDENTIFIER, "#" + dir, startLine, startCol, filename_);
}

Token Lexer::lexOperatorOrDelimiter() {
    int startLine = line_, startCol = column_;
    char c = advance();

    switch (c) {
    case '(': return makeToken(TokenType::LPAREN,    "(");
    case ')': return makeToken(TokenType::RPAREN,    ")");
    case '{': return makeToken(TokenType::LBRACE,    "{");
    case '}': return makeToken(TokenType::RBRACE,    "}");
    case '[': return makeToken(TokenType::LBRACKET,  "[");
    case ']': return makeToken(TokenType::RBRACKET,  "]");
    case ';': return makeToken(TokenType::SEMICOLON, ";");
    case ',': return makeToken(TokenType::COMMA,     ",");
    case '@': return makeToken(TokenType::AT,        "@");
    case '~': return makeToken(TokenType::BIT_NOT,   "~");
    case '^': return makeToken(TokenType::BIT_XOR,   "^");

    case ':':
        if (match(':')) return makeToken(TokenType::DOUBLE_COLON, "::");
        return makeToken(TokenType::COLON, ":");

    case '.':
        if (current() == '.' && peek() == '.') {
            advance(); advance();
            return makeToken(TokenType::ELLIPSIS, "...");
        }
        return makeToken(TokenType::DOT, ".");

    case '?':
        if (match('.')) return makeToken(TokenType::SAFE_DOT,      "?.");
        if (match('?')) return makeToken(TokenType::NULL_COALESCE, "??");
        return makeToken(TokenType::QUESTION, "?");

    case '!':
        if (match('=')) return makeToken(TokenType::NEQ, "!=");
        return makeToken(TokenType::NOT, "!");

    case '=':
        if (match('=')) return makeToken(TokenType::EQ,        "==");
        if (match('>')) return makeToken(TokenType::FAT_ARROW,  "=>");
        return makeToken(TokenType::ASSIGN, "=");

    case '<':
        if (match('<')) return makeToken(TokenType::LSHIFT, "<<");
        if (match('=')) return makeToken(TokenType::LTE,    "<=");
        return makeToken(TokenType::LT, "<");

    case '>':
        if (match('>')) return makeToken(TokenType::RSHIFT, ">>");
        if (match('=')) return makeToken(TokenType::GTE,    ">=");
        return makeToken(TokenType::GT, ">");

    case '+':
        if (match('+')) return makeToken(TokenType::INCREMENT,   "++");
        if (match('=')) return makeToken(TokenType::PLUS_ASSIGN, "+=");
        return makeToken(TokenType::PLUS, "+");

    case '-':
        if (match('-')) return makeToken(TokenType::DECREMENT,    "--");
        if (match('=')) return makeToken(TokenType::MINUS_ASSIGN, "-=");
        if (match('>')) return makeToken(TokenType::ARROW,        "->");
        return makeToken(TokenType::MINUS, "-");

    case '*':
        if (match('=')) return makeToken(TokenType::STAR_ASSIGN, "*=");
        return makeToken(TokenType::STAR, "*");

    case '/':
        if (match('=')) return makeToken(TokenType::SLASH_ASSIGN, "/=");
        return makeToken(TokenType::SLASH, "/");

    case '%':
        if (match('=')) return makeToken(TokenType::PERCENT_ASSIGN, "%=");
        return makeToken(TokenType::PERCENT, "%");

    case '&':
        if (match('&')) return makeToken(TokenType::AND,     "&&");
        return makeToken(TokenType::BIT_AND, "&");

    case '|':
        if (match('|')) return makeToken(TokenType::OR,     "||");
        return makeToken(TokenType::BIT_OR, "|");

    default:
        return errorToken(std::string("Unexpected character: ") + c);
    }
}

bool Token::isAny(std::initializer_list<TokenType> types) const {
    for (auto t : types) if (type == t) return true;
    return false;
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespace();
        if (isAtEnd()) break;

        char c = current();

        if (c == '#') {
            Token tok = lexPreprocessorDirective();
            if (tok.type != TokenType::DIRECTIVE_LINE)
                tokens.push_back(std::move(tok));
            continue;
        }

        if (c == '$' && peek() == '"') {
            tokens.push_back(lexInterpolatedString());
            continue;
        }

        if (c == '"') { tokens.push_back(lexString()); continue; }
        if (c == '\'') { tokens.push_back(lexChar()); continue; }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(lexNumber());
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(lexIdentifierOrKeyword());
            continue;
        }

        tokens.push_back(lexOperatorOrDelimiter());
    }

    tokens.emplace_back(TokenType::EOF_TOKEN, "", line_, column_);
    return tokens;
}

std::string Lexer::tokenTypeName(TokenType t) {
    switch (t) {
#define CASE(x) case TokenType::x: return #x
        CASE(INTEGER_LITERAL); CASE(FLOAT_LITERAL); CASE(DOUBLE_LITERAL);
        CASE(STRING_LITERAL);  CASE(CHAR_LITERAL);  CASE(INTERP_STRING);
        CASE(IDENTIFIER);
        CASE(KW_INT);   CASE(KW_LONG);  CASE(KW_SHORT); CASE(KW_BYTE);
        CASE(KW_FLOAT); CASE(KW_DOUBLE);CASE(KW_BOOL);  CASE(KW_CHAR);
        CASE(KW_STRING);CASE(KW_VOID);  CASE(KW_ANY);
        CASE(KW_IF); CASE(KW_ELSE);  CASE(KW_WHILE);  CASE(KW_DO);
        CASE(KW_FOR); CASE(KW_SWITCH); CASE(KW_CASE); CASE(KW_DEFAULT);
        CASE(KW_BREAK); CASE(KW_CONTINUE); CASE(KW_RETURN);
        CASE(KW_CLASS); CASE(KW_INTERFACE); CASE(KW_ENUM); CASE(KW_STRUCT);
        CASE(KW_EXTENDS); CASE(KW_IMPLEMENTS);
        CASE(KW_PUBLIC); CASE(KW_PRIVATE); CASE(KW_PROTECTED); CASE(KW_INTERNAL);
        CASE(KW_STATIC); CASE(KW_ABSTRACT); CASE(KW_FINAL); CASE(KW_OVERRIDE);
        CASE(KW_NEW); CASE(KW_THIS); CASE(KW_SUPER);
        CASE(KW_FUNC); CASE(KW_VAR); CASE(KW_CONST);
        CASE(KW_NULL); CASE(KW_TRUE); CASE(KW_FALSE);
        CASE(KW_TRY); CASE(KW_CATCH); CASE(KW_FINALLY); CASE(KW_THROW);
        CASE(KW_IMPORT); CASE(KW_PACKAGE); CASE(KW_NAMESPACE);
        CASE(KW_OPERATOR); CASE(KW_CAST); CASE(KW_ALLOC); CASE(KW_FREE); CASE(KW_UNSAFE); CASE(KW_AS);
        CASE(PLUS); CASE(MINUS); CASE(STAR); CASE(SLASH); CASE(PERCENT);
        CASE(ASSIGN); CASE(PLUS_ASSIGN); CASE(MINUS_ASSIGN); CASE(STAR_ASSIGN);
        CASE(SLASH_ASSIGN); CASE(PERCENT_ASSIGN);
        CASE(EQ); CASE(NEQ); CASE(LT); CASE(GT); CASE(LTE); CASE(GTE);
        CASE(AND); CASE(OR); CASE(NOT);
        CASE(BIT_AND); CASE(BIT_OR); CASE(BIT_XOR); CASE(BIT_NOT);
        CASE(LSHIFT); CASE(RSHIFT);
        CASE(INCREMENT); CASE(DECREMENT);
        CASE(ARROW); CASE(FAT_ARROW);
        CASE(QUESTION); CASE(SAFE_DOT); CASE(NULL_COALESCE);
        CASE(LPAREN); CASE(RPAREN); CASE(LBRACE); CASE(RBRACE);
        CASE(LBRACKET); CASE(RBRACKET);
        CASE(SEMICOLON); CASE(COLON); CASE(DOUBLE_COLON);
        CASE(DOT); CASE(COMMA); CASE(AT); CASE(HASH); CASE(DOLLAR); CASE(TILDE);
        CASE(ELLIPSIS);
        CASE(DIRECTIVE_INCLUDE);
        CASE(DIRECTIVE_IMPORT);
        CASE(EOF_TOKEN); CASE(ERROR);
#undef CASE
        default: return "<unknown>";
    }
}

}
