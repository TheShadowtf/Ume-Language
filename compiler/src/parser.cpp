// parser.cpp — Recursive Descent Parser for the Ume Language
#include "../include/parser.h"
#include <sstream>
#include <cassert>

namespace Ume {

Parser::Parser(std::vector<Token> tokens, std::string filename)
    : tokens_(std::move(tokens)), filename_(std::move(filename)), rootFilename_(filename_) {
    // Ensure there's always an EOF
    if (tokens_.empty() || tokens_.back().type != TokenType::EOF_TOKEN)
        tokens_.emplace_back(TokenType::EOF_TOKEN, "", 0, 0, filename_);
    if (!tokens_.empty() && !tokens_[0].filename.empty())
        filename_ = tokens_[0].filename;
}

const Token& Parser::current() const { return tokens_[pos_]; }
const Token& Parser::peek(int offset) const {
    size_t idx = pos_ + static_cast<size_t>(offset);
    return idx < tokens_.size() ? tokens_[idx] : tokens_.back();
}
const Token& Parser::advance() {
    if (!isAtEnd()) ++pos_;
    if (!current().filename.empty()) filename_ = current().filename;
    return tokens_[pos_ > 0 ? pos_ - 1 : 0];
}
bool Parser::check(TokenType t)  const { return current().type == t; }
bool Parser::isAtEnd()           const { return current().type == TokenType::EOF_TOKEN; }

bool Parser::match(TokenType t) {
    if (!check(t)) return false;
    advance(); return true;
}
bool Parser::matchAny(std::initializer_list<TokenType> types) {
    for (auto t : types) if (match(t)) return true;
    return false;
}
Token Parser::expect(TokenType t, const std::string& msg) {
    if (!check(t)) throw errorWithContext(t, msg);
    return advance();
}

bool Parser::consumeGT() {
    if (pendingGT_ > 0) { pendingGT_--; return true; }
    if (check(TokenType::GT)) { advance(); return true; }
    if (check(TokenType::RSHIFT)) {
        advance(); // consume the >>
        pendingGT_++; // one > remains for the enclosing generic
        return true;
    }
    if (check(TokenType::GTE)) {
        advance(); // consume >=
        // The = is not a > — but in type context this is unusual. Treat as one > consumed.
        return true;
    }
    return false;
}
static std::string tokenText(TokenType t) {
    switch (t) {
    case TokenType::LPAREN:      return "(";
    case TokenType::RPAREN:      return ")";
    case TokenType::LBRACE:      return "{";
    case TokenType::RBRACE:      return "}";
    case TokenType::LBRACKET:    return "[";
    case TokenType::RBRACKET:    return "]";
    case TokenType::SEMICOLON:   return ";";
    case TokenType::COLON:       return ":";
    case TokenType::COMMA:       return ",";
    case TokenType::DOT:         return ".";
    case TokenType::ARROW:       return "->";
    case TokenType::FAT_ARROW:   return "=>";
    case TokenType::SAFE_DOT:    return "?.";
    case TokenType::ELLIPSIS:    return "...";
    case TokenType::LT:          return "<";
    case TokenType::GT:          return ">";
    case TokenType::EQ:          return "==";
    case TokenType::NEQ:         return "!=";
    case TokenType::ASSIGN:      return "=";
    case TokenType::PLUS_ASSIGN: return "+=";
    case TokenType::MINUS_ASSIGN:return "-=";
    case TokenType::STAR_ASSIGN: return "*=";
    case TokenType::SLASH_ASSIGN:return "/=";
    case TokenType::PERCENT_ASSIGN:return "%=";
    default: return Lexer::tokenTypeName(t);
    }
}

static std::string tokenDisplay(const Token& tok) {
    if (!tok.value.empty()) return tok.value;
    return tokenText(tok.type);
}

static std::string syntaxHintForToken(TokenType tok) {
    switch (tok) {
    case TokenType::RBRACE:   return "There may be a missing '{' or an unmatched block.";
    case TokenType::RPAREN:   return "There may be a missing '(' or an unmatched grouping.";
    case TokenType::RBRACKET: return "There may be a missing '[' or an unmatched array/index.";
    case TokenType::GT:       return "There may be a missing '<' or an unclosed generic/cast.";
    case TokenType::EOF_TOKEN: return "There may be an unclosed block, parenthesis, bracket, or generic delimiter before end of file.";
    default: return "";
    }
}

static std::string syntaxHintForExpected(TokenType expected, TokenType found) {
    if (found == TokenType::EOF_TOKEN) {
        switch (expected) {
        case TokenType::RBRACE:    return "There may be an unclosed '{' before end of file.";
        case TokenType::RPAREN:    return "There may be an unclosed '(' before end of file.";
        case TokenType::RBRACKET:  return "There may be an unclosed '[' before end of file.";
        case TokenType::GT:        return "There may be an unclosed '<' before end of file.";
        default:                   return "There may be an unclosed syntax element before end of file.";
        }
    }
    switch (found) {
    case TokenType::RBRACE:   return "There may be a missing '{' before this '}'.";
    case TokenType::RPAREN:   return "There may be a missing '(' before this ')'.";
    case TokenType::RBRACKET: return "There may be a missing '[' before this ']'.";
    case TokenType::GT:       return "There may be a missing '<' before this '>'.";
    default: return syntaxHintForToken(found);
    }
}

ParseError Parser::error(const std::string& msg) const {
    std::string fn = current().filename.empty() ? filename_ : current().filename;
    std::string hint = syntaxHintForToken(current().type);
    return ParseError(msg, current().line, current().column, fn, "", tokenDisplay(current()), hint);
}

ParseError Parser::errorAt(const Token& tok, const std::string& msg) const {
    std::string fn = tok.filename.empty() ? filename_ : tok.filename;
    std::string hint = syntaxHintForToken(tok.type);
    return ParseError(msg, tok.line, tok.column, fn, "", tokenDisplay(tok), hint);
}

ParseError Parser::errorWithContext(TokenType expected, const std::string& msg) const {
    std::string fn = current().filename.empty() ? filename_ : current().filename;
    std::string expStr = tokenText(expected);
    std::string fndStr = tokenDisplay(current());
    std::string hint = syntaxHintForExpected(expected, current().type);

    if (current().type == TokenType::ERROR) {
        std::string mainMsg = current().value.empty() ? "Invalid character" : current().value;
        std::string label = "invalid character here";
        if (hint.empty()) hint = "This character is not recognized as a valid token or operator in Ume.";
        return ParseError(mainMsg, current().line, current().column, fn, expStr, fndStr, hint);
    }

    if (expected == TokenType::LT && pos_ > 0) {
        TokenType prevType = tokens_[pos_ - 1].type;
        if (prevType == TokenType::KW_ALLOC) {
            hint = "'alloc' is a generic allocation operator requiring a type parameter, e.g., alloc<Type>(size)";
        } else if (prevType == TokenType::KW_CAST) {
            hint = "'cast' is a generic casting operator requiring a type parameter, e.g., cast<Type>(expression)";
        }
    }

    if (expected == TokenType::IDENTIFIER) {
        if (hint.empty()) {
            hint = "'" + fndStr + "' is an unexpected symbol here. Identifiers (variable and function names) must start with a letter (A-Z, a-z) or underscore (_).";
        }
    }

    std::string mainMsg = msg;
    if (mainMsg.empty()) {
        if (current().type == TokenType::EOF_TOKEN) {
            mainMsg = "Expected '" + expStr + "' before end of file";
        } else {
            mainMsg = "Expected '" + expStr + "' but found '" + fndStr + "'";
        }
    } else {
        if (current().type != TokenType::EOF_TOKEN && !fndStr.empty() && mainMsg.find(fndStr) == std::string::npos) {
            mainMsg += ", found '" + fndStr + "'";
        }
    }

    return ParseError(mainMsg, current().line, current().column, fn, expStr, fndStr, hint);
}

Token Parser::findLikelyBlockStarter() const {
    // Tokens that commonly introduce a block
    const TokenType starters[] = {
        TokenType::KW_FUNC, TokenType::KW_CLASS, TokenType::KW_IF,
        TokenType::KW_FOR,  TokenType::KW_WHILE, TokenType::KW_SWITCH,
        TokenType::KW_TRY,  TokenType::KW_STRUCT, TokenType::KW_ENUM,
        TokenType::KW_INTERFACE
    };
    if (pos_ == 0) return current();
    for (int i = static_cast<int>(pos_) - 1; i >= 0; --i) {
        const Token& t = tokens_[i];
        for (auto s : starters) if (t.type == s) return t;
    }
    return current();
}

static bool isTypeKeyword(TokenType t) {
    switch (t) {
    case TokenType::KW_INT: case TokenType::KW_LONG:  case TokenType::KW_SHORT:
    case TokenType::KW_BYTE: case TokenType::KW_FLOAT: case TokenType::KW_DOUBLE:
    case TokenType::KW_BOOL: case TokenType::KW_CHAR:  case TokenType::KW_STRING:
    case TokenType::KW_VOID: case TokenType::KW_ANY:   case TokenType::IDENTIFIER:
    case TokenType::KW_VAR:  case TokenType::KW_FUNC:
        return true;
    default: return false;
    }
}

TypeAnnotation Parser::parseTypeAnnotationBasic() {
    TypeAnnotation ta;
    if (check(TokenType::KW_FUNC)) {
        advance();
        ta.name   = "func";
        ta.isFunc = true;
        if (match(TokenType::LT)) {
            // func<retType, paramType, ...> syntax
            while (!(check(TokenType::GT) || check(TokenType::RSHIFT) || pendingGT_ > 0) && !isAtEnd()) {
                ta.funcParams.push_back(parseTypeAnnotation());
                if (!match(TokenType::COMMA)) break;
            }
            consumeGT();
        } else if (isTypeStart()) {
            // func retType (paramType, ...) inline function type syntax
            ta.funcParams.push_back(parseTypeAnnotation()); // [0] = return type
            if (match(TokenType::LPAREN)) {
                while (!check(TokenType::RPAREN) && !isAtEnd()) {
                    ta.funcParams.push_back(parseTypeAnnotation());
                    if (!match(TokenType::COMMA)) break;
                }
                expect(TokenType::RPAREN, "Expected ')' in function type");
            }
        }
    } else {
        ta.name = current().value;
        advance();
    }
    // Generic args: List<int>
    if (check(TokenType::LT)) {
        advance();
        while (!(check(TokenType::GT) || check(TokenType::RSHIFT) || pendingGT_ > 0) && !isAtEnd()) {
            ta.typeArgs.push_back(parseTypeAnnotation());
            if (!match(TokenType::COMMA)) break;
        }
        consumeGT();
    }
    return ta;
}

TypeAnnotation Parser::parseTypeAnnotation() {
    TypeAnnotation ta = parseTypeAnnotationBasic();

    // Array suffix: int[]  or  int[5]  or  int[][][]
    while (check(TokenType::LBRACKET)) {
        advance();
        ta.isArray = true;
        ta.arrayRank++;
        if (check(TokenType::INTEGER_LITERAL)) {
            ta.arraySize = std::stoi(current().value);
            advance();
        }
        expect(TokenType::RBRACKET, "Expected ']' in array type");
    }

    // Nullable suffix: int?
    if (check(TokenType::QUESTION)) { advance(); ta.nullable = true; }

    // Pointer suffix: int*
    while (check(TokenType::STAR)) {
        advance();
        ta.isPointer = true;
        ta.pointerLevel++;
    }

    return ta;
}

bool Parser::hasAccessModifier() const {
    switch (current().type) {
    case TokenType::KW_PUBLIC: case TokenType::KW_PRIVATE:
    case TokenType::KW_PROTECTED: case TokenType::KW_INTERNAL:
        return true;
    default: return false;
    }
}
AccessModifier Parser::parseAccessModifier() {
    switch (current().type) {
    case TokenType::KW_PUBLIC:    advance(); return AccessModifier::Public;
    case TokenType::KW_PRIVATE:   advance(); return AccessModifier::Private;
    case TokenType::KW_PROTECTED: advance(); return AccessModifier::Protected;
    case TokenType::KW_INTERNAL:  advance(); return AccessModifier::Internal;
    default:                                 return AccessModifier::Default;
    }
}

Parameter Parser::parseParameter() {
    bool variadic = false;
    if (match(TokenType::ELLIPSIS)) variadic = true;

    TypeAnnotation ta = parseTypeAnnotation();

    // Variadic: int... values or ...any args
    if (!variadic && check(TokenType::ELLIPSIS)) { advance(); variadic = true; }

    std::string name = expect(TokenType::IDENTIFIER, "Expected parameter name").value;

    Parameter p(std::move(ta), std::move(name), variadic);

    // Default value
    if (match(TokenType::ASSIGN))
        p.defaultValue = parseExpression();

    return p;
}

std::vector<Parameter> Parser::parseParamList() {
    expect(TokenType::LPAREN, "Expected '(' before parameter list");
    std::vector<Parameter> params;
    while (!check(TokenType::RPAREN) && !isAtEnd()) {
        params.push_back(parseParameter());
        if (!match(TokenType::COMMA)) break;
    }
    expect(TokenType::RPAREN, "Expected ')' after parameter list");
    return params;
}

std::unique_ptr<Program> Parser::parse() {
    auto prog        = std::make_unique<Program>();
    prog->filename   = rootFilename_;
    prog->line       = 1;

    while (!isAtEnd()) {
        prog->declarations.push_back(parseTopLevel());
    }
    return prog;
}

std::vector<Attribute> Parser::parseAttributes() {
    std::vector<Attribute> attrs;
    while (check(TokenType::LBRACKET)) {
        advance(); // consume '['
        while (!check(TokenType::RBRACKET) && !isAtEnd()) {
            Attribute attr;
            attr.name = expect(TokenType::IDENTIFIER, "Expected attribute name").value;
            if (match(TokenType::LPAREN)) {
                if (!check(TokenType::RPAREN)) {
                    do {
                        attr.args.push_back(parseExpression());
                    } while (match(TokenType::COMMA));
                }
                expect(TokenType::RPAREN, "Expected ')' after attribute arguments");
            }
            attrs.push_back(std::move(attr));
            if (!match(TokenType::COMMA)) break;
        }
        expect(TokenType::RBRACKET, "Expected ']' after attributes");
    }
    return attrs;
}

ASTNodePtr Parser::parseTopLevel() {
    // Preprocessor
    if (check(TokenType::DIRECTIVE_INCLUDE)) return parseIncludeDirective();
    if (check(TokenType::DIRECTIVE_IMPORT))  return parseImportDirective();
    // Package / Namespace
    if (check(TokenType::KW_PACKAGE))        return parsePackageDecl();
    if (check(TokenType::KW_NAMESPACE))      return parseNamespaceDecl();
    // Import
    if (check(TokenType::KW_IMPORT))         return parseImportDecl();

    // Gather attributes and modifiers
    std::vector<Attribute> attrs = parseAttributes();
    AccessModifier access   = AccessModifier::Default;
    bool isStatic   = false, isAbstract = false, isFinal = false, isOverride = false;

    // Modifier loop
    bool scanning = true;
    while (scanning) {
        switch (current().type) {
        case TokenType::KW_PUBLIC:
        case TokenType::KW_PRIVATE:
        case TokenType::KW_PROTECTED:
        case TokenType::KW_INTERNAL:
            access = parseAccessModifier();
            break;
        case TokenType::KW_STATIC:   isStatic   = true; advance(); break;
        case TokenType::KW_ABSTRACT: isAbstract = true; advance(); break;
        case TokenType::KW_FINAL:    isFinal    = true; advance(); break;
        case TokenType::KW_OVERRIDE: isOverride = true; advance(); break;
        default: scanning = false;
        }
    }

    ASTNodePtr declNode = nullptr;
    switch (current().type) {
    case TokenType::KW_CLASS:     declNode = parseClassDecl(access); break;
    case TokenType::KW_INTERFACE: declNode = parseInterfaceDecl(access); break;
    case TokenType::KW_ENUM:      declNode = parseEnumDecl(access); break;
    case TokenType::KW_STRUCT:    declNode = parseStructDecl(); break;
    case TokenType::KW_FUNC:      declNode = parseFuncDecl(access, isStatic, isAbstract, isOverride, isFinal); break;
    default:
        // Top-level variable
        if (isTypeStart()) {
            return parseVarDecl(false);
        }
        if (check(TokenType::ERROR)) {
            throw errorAt(current(), current().value.empty() ? "Invalid character" : current().value);
        }
        if (check(TokenType::RBRACE)) {
            throw errorAt(current(), "Unexpected '}' at top-level. There may be a missing '{' or an extra '}'.");
        }
        if (check(TokenType::RPAREN))
            throw error("Unexpected ')' at top-level. There may be a missing '(' or an extra ')'.");
        if (check(TokenType::RBRACKET))
            throw error("Unexpected ']' at top-level. There may be a missing '[' or an extra ']'.");
        throw errorAt(current(), "Expected top-level declaration, found unexpected symbol '" + tokenDisplay(current()) + "'");
    }

    if (auto c = dynamic_cast<ClassDecl*>(declNode.get())) c->attributes = std::move(attrs);
    else if (auto e = dynamic_cast<EnumDecl*>(declNode.get())) e->attributes = std::move(attrs);
    else if (auto s = dynamic_cast<StructDecl*>(declNode.get())) s->attributes = std::move(attrs);
    else if (auto f = dynamic_cast<FuncDecl*>(declNode.get())) f->attributes = std::move(attrs);
    
    return declNode;
}

ASTNodePtr Parser::parseIncludeDirective() {
    int l = current().line, c = current().column;
    auto path = current().value;
    advance();
    auto node = std::make_unique<IncludeDirective>(path);
    node->line = l; node->column = c;
    return node;
}

ASTNodePtr Parser::parseImportDirective() {
    int l = current().line, c = current().column;
    auto rawPath = current().value;
    advance();
    bool wild = rawPath.size() >= 2 && rawPath.substr(rawPath.size() - 2) == ".*";
    auto node = std::make_unique<ImportDecl>(rawPath, wild);
    node->line = l; node->column = c;
    return node;
}

ASTNodePtr Parser::parsePackageDecl() {
    int l = current().line, c = current().column;
    advance(); // 'package'
    std::string name;
    if (check(TokenType::IDENTIFIER) || isTypeStart()) {
        name += current().value; advance();
    } else {
        name += expect(TokenType::IDENTIFIER, "Expected package name").value;
    }
    while (check(TokenType::DOT)) {
        advance();
        if (check(TokenType::IDENTIFIER) || isTypeStart()) {
            name += "." + current().value; advance();
        } else {
            name += "." + expect(TokenType::IDENTIFIER, "Expected identifier after '.'").value;
        }
    }
    expect(TokenType::SEMICOLON, "Expected ';' after package declaration");
    auto node = std::make_unique<PackageDecl>(name);
    node->line = l; node->column = c;
    return node;
}

ASTNodePtr Parser::parseNamespaceDecl() {
    int l = current().line, c = current().column;
    advance(); // 'namespace'
    std::string name;
    name += expect(TokenType::IDENTIFIER, "Expected namespace name").value;
    while (check(TokenType::DOT)) {
        advance();
        name += "." + expect(TokenType::IDENTIFIER, "Expected identifier after '.'").value;
    }
    if (match(TokenType::SEMICOLON)) {
        auto node = std::make_unique<PackageDecl>(name);
        node->line = l; node->column = c;
        return node;
    }
    expect(TokenType::LBRACE, "Expected '{' or ';' after namespace name");
    auto node = std::make_unique<NamespaceDecl>(name);
    node->line = l; node->column = c;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        node->declarations.push_back(parseTopLevel());
    }
    expect(TokenType::RBRACE, "Expected '}' after namespace body");
    return node;
}

ASTNodePtr Parser::parseImportDecl() {
    int l = current().line, c = current().column;
    advance(); // 'import'
    std::string path;
    if (check(TokenType::STRING_LITERAL)) {
        path = current().value;
        advance();
    } else {
        if (check(TokenType::IDENTIFIER) || isTypeStart()) { path += current().value; advance(); }
        else { path += expect(TokenType::IDENTIFIER, "Expected import path").value; }
        while (check(TokenType::DOT)) {
            advance();
            if (check(TokenType::STAR)) { advance(); path += ".*"; break; }
            if (check(TokenType::IDENTIFIER) || isTypeStart()) { path += "." + current().value; advance(); }
            else { path += "." + expect(TokenType::IDENTIFIER, "Expected identifier after '.'").value; }
        }
    }
    bool wild = path.size() >= 2 && path.substr(path.size() - 2) == ".*";
    expect(TokenType::SEMICOLON, "Expected ';' after import declaration");
    auto node = std::make_unique<ImportDecl>(path, wild);
    node->line = l; node->column = c;
    return node;
}

ASTNodePtr Parser::parseFuncDecl(AccessModifier access, bool isStatic,
                                  bool isAbstract, bool isOverride, bool isFinal) {
    int l = current().line, c = current().column;
    expect(TokenType::KW_FUNC, "Expected 'func'");

    auto decl          = std::make_unique<FuncDecl>();
    decl->access       = access;
    decl->isStatic     = isStatic;
    decl->isAbstract   = isAbstract;
    decl->isOverride   = isOverride;
    decl->isFinal      = isFinal;
    decl->line         = l;
    decl->column       = c;

    decl->returnType   = parseTypeAnnotation();
    decl->name         = expect(TokenType::IDENTIFIER, "Expected function name").value;

    // Generic type params: func T max<T>(T a, T b)
    if (check(TokenType::LT)) {
        advance();
        while (!check(TokenType::GT) && !isAtEnd()) {
            decl->typeParams.push_back(expect(TokenType::IDENTIFIER,"Expected type param").value);
            if (!match(TokenType::COMMA)) break;
        }
        expect(TokenType::GT, "Expected '>' after type params");
    }

    decl->params = parseParamList();

    if (isAbstract || check(TokenType::SEMICOLON)) {
        expect(TokenType::SEMICOLON, "Expected ';' after function declaration");
    } else {
        decl->body = parseBlock();
    }
    return decl;
}

ASTNodePtr Parser::parseClassDecl(AccessModifier access) {
    int l = current().line, c = current().column;

    bool isAbstract = false, isFinal = false, isStatic = false;
    if (match(TokenType::KW_ABSTRACT)) isAbstract = true;
    if (match(TokenType::KW_STATIC))   isStatic   = true;
    if (match(TokenType::KW_FINAL))    isFinal    = true;

    expect(TokenType::KW_CLASS, "Expected 'class'");

    auto decl        = std::make_unique<ClassDecl>();
    decl->access     = access;
    decl->isAbstract = isAbstract;
    decl->isFinal    = isFinal;
    decl->isStatic   = isStatic;
    decl->line       = l; decl->column = c;

    decl->name = expect(TokenType::IDENTIFIER, "Expected class name").value;

    // Generic type params
    if (check(TokenType::LT)) {
        advance();
        while (!check(TokenType::GT) && !isAtEnd()) {
            decl->typeParams.push_back(
                expect(TokenType::IDENTIFIER,"Expected type parameter").value);
            if (!match(TokenType::COMMA)) break;
        }
        expect(TokenType::GT, "Expected '>'");
    }

    // extends
    if (match(TokenType::KW_EXTENDS))
        decl->superClass = parseTypeAnnotation();

    // implements
    if (match(TokenType::KW_IMPLEMENTS)) {
        decl->interfaces.push_back(parseTypeAnnotation());
        while (match(TokenType::COMMA))
            decl->interfaces.push_back(parseTypeAnnotation());
    }

    expect(TokenType::LBRACE, "Expected '{' in class body");

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        // destructor
        if (check(TokenType::TILDE)) {
            decl->destructor = parseDestructor();
            continue;
        }
        std::vector<Attribute> attrs = parseAttributes();
        AccessModifier mAccess = AccessModifier::Default;

        bool mStatic = false, mAbstract = false, mFinal = false,
             mOverride = false, mConst = false;
        bool scan = true;
        while (scan) {
            switch (current().type) {
            case TokenType::KW_PUBLIC:
            case TokenType::KW_PRIVATE:
            case TokenType::KW_PROTECTED:
            case TokenType::KW_INTERNAL:
                mAccess = parseAccessModifier();
                break;
            case TokenType::KW_STATIC:   mStatic   = true; advance(); break;
            case TokenType::KW_ABSTRACT: mAbstract = true; advance(); break;
            case TokenType::KW_FINAL:    mFinal    = true; advance(); break;
            case TokenType::KW_OVERRIDE: mOverride = true; advance(); break;
            case TokenType::KW_CONST:    mConst    = true; advance(); break;
            default: scan = false;
            }
        }

        // Nested type declarations
        if (check(TokenType::KW_CLASS)) {
            decl->methods.push_back(std::unique_ptr<FuncDecl>(
                static_cast<FuncDecl*>(parseClassDecl(mAccess).release())));
            // Actually nested classes aren't fully supported, but at least don't crash
            continue;
        }
        if (check(TokenType::KW_STRUCT)) {
            parseStructDecl();
            continue;
        }
        if (check(TokenType::KW_ENUM)) {
            parseEnumDecl(mAccess);
            continue;
        }
        if (check(TokenType::KW_INTERFACE)) {
            parseInterfaceDecl(mAccess);
            continue;
        }
        if (check(TokenType::KW_OPERATOR)) {
            decl->operators.push_back(parseOperatorDecl(mAccess));
        } else if (check(TokenType::KW_FUNC)) {
            if (peek(1).type == TokenType::LT) {
                TypeAnnotation type = parseTypeAnnotation();
                std::string name = expect(TokenType::IDENTIFIER, "Expected member name").value;
                auto fd = std::make_unique<FieldDecl>();
                fd->attributes = std::move(attrs);
                fd->access = mAccess; fd->isStatic = mStatic; fd->isConst = mConst;
                fd->type = type; fd->name = name;
                if (match(TokenType::ASSIGN)) fd->initializer = parseExpression();
                decl->fields.push_back(std::move(fd));
                expect(TokenType::SEMICOLON, "Expected ';' after field declaration");
            } else {
                decl->methods.push_back(std::unique_ptr<FuncDecl>(
                    static_cast<FuncDecl*>(
                        parseFuncDecl(mAccess, mStatic, mAbstract, mOverride, mFinal).release())));
                decl->methods.back()->attributes = std::move(attrs);
            }
        } else if (check(TokenType::IDENTIFIER) && peek().type == TokenType::LPAREN) {
            // Constructor: ClassName(...)
            decl->constructors.push_back(parseConstructor(mAccess, decl->name));
        } else {
            TypeAnnotation type = parseTypeAnnotation();
            if (check(TokenType::KW_THIS)) {
                decl->indexers.push_back(parseIndexer(mAccess, type));
            } else {
                std::string name = expect(TokenType::IDENTIFIER, "Expected member name").value;
                if (check(TokenType::LBRACE) || check(TokenType::FAT_ARROW)) {
                    decl->properties.push_back(parseProperty(mAccess, mStatic, type, name));
                    decl->properties.back()->attributes = std::move(attrs);
                } else if (check(TokenType::LPAREN)) {
                    std::string modStr;
                    if (mAccess == AccessModifier::Public) modStr += "public ";
                    else if (mAccess == AccessModifier::Private) modStr += "private ";
                    else if (mAccess == AccessModifier::Protected) modStr += "protected ";
                    else if (mAccess == AccessModifier::Internal) modStr += "internal ";
                    if (mStatic) modStr += "static ";
                    throw error("Functions must be declared with 'func' keyword (e.g. '" + modStr + "func " + type.name + " " + name + "(...)')");
                } else {
                    bool firstField = true;
                    while (true) {
                        auto fd = std::make_unique<FieldDecl>();
                        if (firstField) {
                            fd->attributes = std::move(attrs);
                            firstField = false;
                        }
                        fd->access = mAccess; fd->isStatic = mStatic; fd->isConst = mConst;
                        fd->type = type; fd->name = name;
                        if (match(TokenType::ASSIGN)) fd->initializer = parseExpression();
                        decl->fields.push_back(std::move(fd));
                        
                        if (match(TokenType::COMMA)) {
                            name = expect(TokenType::IDENTIFIER, "Expected member name").value;
                        } else {
                            break;
                        }
                    }
                    expect(TokenType::SEMICOLON, "Expected ';' after field declaration");
                }
            }
        }
    }

    expect(TokenType::RBRACE, "Expected '}' after class body");
    return decl;
}

ASTNodePtr Parser::parseInterfaceDecl(AccessModifier access) {
    int l = current().line, c = current().column;
    expect(TokenType::KW_INTERFACE, "Expected 'interface'");
    auto decl    = std::make_unique<InterfaceDecl>();
    decl->access = access; decl->line = l; decl->column = c;
    decl->name   = expect(TokenType::IDENTIFIER, "Expected interface name").value;

    if (check(TokenType::LT)) {
        advance();
        while (!check(TokenType::GT) && !isAtEnd()) {
            decl->typeParams.push_back(expect(TokenType::IDENTIFIER,"").value);
            if (!match(TokenType::COMMA)) break;
        }
        expect(TokenType::GT, "Expected '>'");
    }

    expect(TokenType::LBRACE, "Expected '{'");
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        AccessModifier ma = parseAccessModifier();
        if (check(TokenType::KW_CONST)) {
            advance();
            decl->constants.push_back(parseField(ma, false, true));
        } else {
            expect(TokenType::KW_FUNC, "Expected 'func' in interface");
            auto fd         = std::make_unique<FuncDecl>();
            fd->access      = ma;
            fd->returnType  = parseTypeAnnotation();
            fd->name        = expect(TokenType::IDENTIFIER,"Expected method name").value;
            fd->params      = parseParamList();
            if (check(TokenType::LBRACE))
                fd->body = parseBlock(); // default implementation
            else
                expect(TokenType::SEMICOLON, "Expected ';' after abstract interface method");
            decl->methods.push_back(std::move(fd));
        }
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return decl;
}

ASTNodePtr Parser::parseEnumDecl(AccessModifier access) {
    int l = current().line, c = current().column;
    expect(TokenType::KW_ENUM, "Expected 'enum'");
    auto decl    = std::make_unique<EnumDecl>();
    decl->access = access; decl->line = l; decl->column = c;
    decl->name   = expect(TokenType::IDENTIFIER, "Expected enum name").value;
    expect(TokenType::LBRACE, "Expected '{'");

    // Parse enum values first
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        // Enum values: NAME, NAME = val, or NAME(args)
        if (check(TokenType::IDENTIFIER)) {
            // Check if this is a field/method (after a ';' separator)
            EnumValue ev;
            ev.name = current().value;
            advance();
            if (match(TokenType::ASSIGN))
                ev.value = parseExpression();
            decl->values.push_back(std::move(ev));
            if (match(TokenType::SEMICOLON)) break; // end of values
            match(TokenType::COMMA);
        } else {
            break;
        }
    }

    // Parse methods after the semicolon
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        AccessModifier ma = parseAccessModifier();
        bool mStatic = match(TokenType::KW_STATIC);
        if (check(TokenType::KW_FUNC)) {
            decl->methods.push_back(std::unique_ptr<FuncDecl>(
                static_cast<FuncDecl*>(
                    parseFuncDecl(ma, mStatic, false, false, false).release())));
        } else {
            decl->fields.push_back(parseField(ma, mStatic, false));
        }
    }

    expect(TokenType::RBRACE, "Expected '}'");
    return decl;
}

ASTNodePtr Parser::parseStructDecl() {
    int l = current().line, c = current().column;
    expect(TokenType::KW_STRUCT, "Expected 'struct'");
    auto decl  = std::make_unique<StructDecl>();
    decl->line = l; decl->column = c;
    decl->name = expect(TokenType::IDENTIFIER, "Expected struct name").value;
    expect(TokenType::LBRACE, "Expected '{'");
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        AccessModifier ma = parseAccessModifier();
        bool mStatic = match(TokenType::KW_STATIC);
        if (check(TokenType::KW_FUNC)) {
            decl->methods.push_back(std::unique_ptr<FuncDecl>(
                static_cast<FuncDecl*>(
                    parseFuncDecl(ma, mStatic, false, false, false).release())));
        } else {
            decl->fields.push_back(parseField(ma, mStatic, false));
        }
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return decl;
}

std::unique_ptr<ConstructorDecl> Parser::parseConstructor(AccessModifier access,
                                                           const std::string& /*className*/) {
    auto ctor    = std::make_unique<ConstructorDecl>();
    ctor->access = access;
    advance(); // consume class name used as ctor name
    ctor->params = parseParamList();
    ctor->body   = parseBlock();
    return ctor;
}

std::unique_ptr<DestructorDecl> Parser::parseDestructor() {
    advance(); // ~
    expect(TokenType::IDENTIFIER, "Expected class name in destructor");
    expect(TokenType::LPAREN, "Expected '('");
    expect(TokenType::RPAREN, "Expected ')'");
    auto d  = std::make_unique<DestructorDecl>();
    d->body = parseBlock();
    return d;
}

std::unique_ptr<FieldDecl> Parser::parseField(AccessModifier access, bool isStatic, bool isConst) {
    auto fd      = std::make_unique<FieldDecl>();
    fd->access   = access;
    fd->isStatic = isStatic;
    fd->isConst  = isConst;
    fd->type     = parseTypeAnnotation();
    fd->name     = expect(TokenType::IDENTIFIER, "Expected field name").value;
    if (check(TokenType::LPAREN)) {
        throw error("Functions must be declared with 'func' keyword (e.g. 'func " + fd->type.name + " " + fd->name + "(...)')");
    }
    if (match(TokenType::ASSIGN))
        fd->initializer = parseExpression();
    expect(TokenType::SEMICOLON, "Expected ';' after field declaration");
    return fd;
}

std::unique_ptr<PropertyDecl> Parser::parseProperty(AccessModifier access, bool isStatic, TypeAnnotation type, std::string name) {
    auto prop = std::make_unique<PropertyDecl>();
    prop->access = access;
    prop->isStatic = isStatic;
    prop->type = std::move(type);
    prop->name = std::move(name);

    if (match(TokenType::FAT_ARROW)) {
        prop->hasGet = true;
        prop->getDecl = parseExpression();
        expect(TokenType::SEMICOLON, "Expected ';' after property expression body");
        return prop;
    }

    expect(TokenType::LBRACE, "Expected '{' for property body");
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        if (check(TokenType::IDENTIFIER) && current().value == "get") {
            advance();
            prop->hasGet = true;
            if (match(TokenType::SEMICOLON)) {
                // auto-property
            } else if (match(TokenType::FAT_ARROW)) {
                prop->getDecl = parseExpression();
                expect(TokenType::SEMICOLON, "Expected ';' after property expression body");
            } else {
                prop->getDecl = parseBlock();
            }
        } else if (check(TokenType::IDENTIFIER) && current().value == "set") {
            advance();
            prop->hasSet = true;
            if (match(TokenType::SEMICOLON)) {
                // auto-property
            } else if (match(TokenType::FAT_ARROW)) {
                prop->setDecl = parseExpression();
                expect(TokenType::SEMICOLON, "Expected ';' after property expression body");
            } else {
                prop->setDecl = parseBlock();
            }
        } else {
            throw error("Expected 'get' or 'set' inside property body");
        }
    }
    expect(TokenType::RBRACE, "Expected '}' after property body");

    if (match(TokenType::ASSIGN)) {
        prop->initializer = parseExpression();
        expect(TokenType::SEMICOLON, "Expected ';' after property initializer");
    }

    return prop;
}

std::unique_ptr<IndexerDecl> Parser::parseIndexer(AccessModifier access, TypeAnnotation type) {
    auto idx = std::make_unique<IndexerDecl>();
    idx->access = access;
    idx->type = std::move(type);

    expect(TokenType::KW_THIS, "Expected 'this' for indexer");
    expect(TokenType::LBRACKET, "Expected '[' for indexer");
    while (!check(TokenType::RBRACKET) && !isAtEnd()) {
        idx->params.push_back(parseParameter());
        if (!match(TokenType::COMMA)) break;
    }
    expect(TokenType::RBRACKET, "Expected ']' after indexer parameters");

    expect(TokenType::LBRACE, "Expected '{' for indexer body");
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        if (check(TokenType::IDENTIFIER) && current().value == "get") {
            advance();
            idx->hasGet = true;
            if (match(TokenType::SEMICOLON)) {
            } else if (match(TokenType::FAT_ARROW)) {
                idx->getDecl = parseExpression();
                expect(TokenType::SEMICOLON, "Expected ';'");
            } else {
                idx->getDecl = parseBlock();
            }
        } else if (check(TokenType::IDENTIFIER) && current().value == "set") {
            advance();
            idx->hasSet = true;
            if (match(TokenType::SEMICOLON)) {
            } else if (match(TokenType::FAT_ARROW)) {
                idx->setDecl = parseExpression();
                expect(TokenType::SEMICOLON, "Expected ';'");
            } else {
                idx->setDecl = parseBlock();
            }
        } else {
            throw error("Expected 'get' or 'set' inside indexer body");
        }
    }
    expect(TokenType::RBRACE, "Expected '}' after indexer body");

    return idx;
}

std::unique_ptr<OperatorDecl> Parser::parseOperatorDecl(AccessModifier access) {
    expect(TokenType::KW_OPERATOR, "Expected 'operator'");
    auto od     = std::make_unique<OperatorDecl>();
    od->access  = access;
    // Capture the operator symbol
    od->op      = current().value; advance();
    od->params  = parseParamList();
    // Return type: -> Type
    if (match(TokenType::ARROW))
        od->returnType = parseTypeAnnotation();
    od->body    = parseBlock();
    return od;
}

ASTNodePtr Parser::parseBlock() {
    int l = current().line, c = current().column;
    expect(TokenType::LBRACE, "Expected '{'");
    auto block  = std::make_unique<BlockStmt>();
    block->line = l; block->column = c;
    while (!check(TokenType::RBRACE) && !isAtEnd())
        block->stmts.push_back(parseStatement());
    expect(TokenType::RBRACE, "Expected '}'");
    return block;
}

ASTNodePtr Parser::parseStatement() {
    switch (current().type) {
    case TokenType::LBRACE:     return parseBlock();
    case TokenType::KW_RETURN:  return parseReturnStmt();
    case TokenType::KW_IF:      return parseIfStmt();
    case TokenType::KW_WHILE:   return parseWhileStmt();
    case TokenType::KW_DO:      return parseDoWhileStmt();
    case TokenType::KW_FOR:     return parseForStmt();
    case TokenType::KW_FOREACH: return parseForStmt();
    case TokenType::KW_SWITCH:  return parseSwitchStmt();
    case TokenType::KW_TRY:     return parseTryCatchStmt();
    case TokenType::KW_THROW:   return parseThrowStmt();
    case TokenType::KW_UNSAFE:  return parseUnsafeBlock();
    case TokenType::KW_BREAK: {
        auto n = std::make_unique<BreakStmt>(); n->line = current().line;
        advance(); expect(TokenType::SEMICOLON, "Expected ';' after 'break'"); return n;
    }
    case TokenType::KW_CONTINUE: {
        auto n = std::make_unique<ContinueStmt>(); n->line = current().line;
        advance(); expect(TokenType::SEMICOLON, "Expected ';' after 'continue'"); return n;
    }
    case TokenType::KW_CONST: return parseVarDecl(true);
    // Nested class / struct / enum
    case TokenType::KW_CLASS:     { auto a = parseAccessModifier(); return parseClassDecl(a); }
    case TokenType::KW_STRUCT:    return parseStructDecl();
    case TokenType::KW_ENUM:      { auto a = parseAccessModifier(); return parseEnumDecl(a); }
    default:
        if (check(TokenType::ERROR)) {
            throw errorAt(current(), current().value.empty() ? "Invalid character" : current().value);
        }
        if (isTypeStart() && !isLambdaStart()) {
            // Could be a variable declaration
            // Lookahead: TYPE NAME = ...  or  TYPE NAME;
            size_t savedPos = pos_;
            try {
                return parseVarDecl(false);
            } catch (...) {
                pos_ = savedPos;
            }
        }
        {
            auto expr = parseExpression();
            expect(TokenType::SEMICOLON, "Expected ';' after expression");
            auto stmt = std::make_unique<ExprStmt>(std::move(expr));
            return stmt;
        }
    }
}

ASTNodePtr Parser::parseVarDecl(bool isConst) {
    int l = current().line, c = current().column;
    if (isConst) advance(); // consume 'const'
    TypeAnnotation ta;
    if (check(TokenType::KW_VAR)) {
        ta.name = "var"; advance();
    } else {
        ta = parseTypeAnnotation();
    }
    auto block = std::make_unique<BlockStmt>();
    while (true) {
        std::string name = expect(TokenType::IDENTIFIER, "Expected variable name").value;
        if (check(TokenType::LPAREN)) {
            throw error("Functions must be declared with 'func' keyword (e.g. 'func " + ta.name + " " + name + "(...)')");
        }
        ASTNodePtr init;
        if (match(TokenType::ASSIGN))
            init = parseExpression();
            
        auto node  = std::make_unique<VarDeclStmt>(ta, isConst, name, std::move(init));
        node->line = l; node->column = c;
        block->stmts.push_back(std::move(node));
        
        if (!match(TokenType::COMMA)) break;
    }
    expect(TokenType::SEMICOLON, "Expected ';' after variable declaration");
    
    if (block->stmts.size() == 1) return std::move(block->stmts[0]);
    return block;
}

ASTNodePtr Parser::parseReturnStmt() {
    int l = current().line, c = current().column;
    advance(); // 'return'
    ASTNodePtr val;
    if (!check(TokenType::SEMICOLON))
        val = parseExpression();
    expect(TokenType::SEMICOLON, "Expected ';' after return statement");
    auto node = std::make_unique<ReturnStmt>(std::move(val));
    node->line = l; node->column = c;
    return node;
}

ASTNodePtr Parser::parseIfStmt() {
    int l = current().line, c = current().column;
    advance(); // 'if'
    expect(TokenType::LPAREN, "Expected '(' after 'if'");
    auto cond = parseExpression();
    expect(TokenType::RPAREN, "Expected ')'");
    auto thenB = parseStatement();
    ASTNodePtr elseB;
    if (match(TokenType::KW_ELSE))
        elseB = parseStatement();
    auto node  = std::make_unique<IfStmt>(std::move(cond), std::move(thenB), std::move(elseB));
    node->line = l; node->column = c;
    return node;
}

ASTNodePtr Parser::parseWhileStmt() {
    int l = current().line, c = current().column;
    advance();
    expect(TokenType::LPAREN, "Expected '(' after 'while'");
    auto cond = parseExpression();
    expect(TokenType::RPAREN, "Expected ')'");
    auto body = parseStatement();
    auto node  = std::make_unique<WhileStmt>(std::move(cond), std::move(body));
    node->line = l; node->column = c;
    return node;
}

ASTNodePtr Parser::parseDoWhileStmt() {
    int l = current().line, c = current().column;
    advance(); // 'do'
    auto body = parseStatement();
    expect(TokenType::KW_WHILE, "Expected 'while' after 'do'");
    expect(TokenType::LPAREN, "Expected '('");
    auto cond = parseExpression();
    expect(TokenType::RPAREN, "Expected ')'");
    expect(TokenType::SEMICOLON, "Expected ';'");
    auto node  = std::make_unique<DoWhileStmt>(std::move(body), std::move(cond));
    node->line = l; node->column = c;
    return node;
}

bool Parser::isForEachLoop() {
    // Lookahead: TYPE NAME (: or IN)
    // Called after '(' has already been consumed by parseForStmt.
    size_t saved = pos_;
    bool result  = false;
    try {
        if (!isTypeStart()) { pos_ = saved; return false; }
        parseTypeAnnotation();
        if (check(TokenType::IDENTIFIER)) {
            advance();
            result = check(TokenType::COLON) || check(TokenType::KW_IN) ||
                      (check(TokenType::IDENTIFIER) && current().value == "of");
        }
    } catch (...) {}
    pos_ = saved;
    return result;
}

ASTNodePtr Parser::parseForStmt() {
    int l = current().line, c = current().column;
    advance(); // 'for'
    expect(TokenType::LPAREN, "Expected '(' after 'for'");

    if (isForEachLoop()) {
        TypeAnnotation ta;
        if (check(TokenType::KW_VAR)) { ta.name = "var"; advance(); }
        else ta = parseTypeAnnotation();
        std::string varName = expect(TokenType::IDENTIFIER,"Expected variable name").value;
        if (check(TokenType::KW_IN)) {
            advance(); // consume 'in'
        } else if (check(TokenType::COLON)) {
            advance(); // consume ':'
        } else if (check(TokenType::IDENTIFIER) && current().value == "of") {
            advance(); // accept 'of' as alias for 'in'
        } else {
            throw std::runtime_error("Expected 'in' or ':' in for-each loop");
        }
        auto iter = parseExpression();
        expect(TokenType::RPAREN, "Expected ')'");
        auto body = parseStatement();
        auto node  = std::make_unique<ForEachStmt>(std::move(ta), varName, std::move(iter), std::move(body));
        node->line = l; node->column = c;
        return node;
    }

    // C-style for
    ASTNodePtr init;
    if (!check(TokenType::SEMICOLON)) {
        if (check(TokenType::KW_CONST)) { init = parseVarDecl(true); }
        else if (isTypeStart()) {
            size_t saved = pos_;
            try { init = parseVarDecl(false); }
            catch (...) { pos_ = saved; auto e = parseExpression(); expect(TokenType::SEMICOLON, "Expected ';' in for"); init = std::make_unique<ExprStmt>(std::move(e)); }
        } else {
            auto e = parseExpression(); expect(TokenType::SEMICOLON, "Expected ';' in for");
            init = std::make_unique<ExprStmt>(std::move(e));
        }
    } else { advance(); } // empty init ;

    ASTNodePtr cond;
    if (!check(TokenType::SEMICOLON)) cond = parseExpression();
    expect(TokenType::SEMICOLON, "Expected ';' in for");

    ASTNodePtr update;
    if (!check(TokenType::RPAREN)) {
        // Support comma-separated update expressions: i++, j--
        auto block = std::make_unique<BlockStmt>();
        while (!check(TokenType::RPAREN) && !isAtEnd()) {
            auto e = parseExpression();
            auto stmt = std::make_unique<ExprStmt>(std::move(e));
            block->stmts.push_back(std::move(stmt));
            if (!match(TokenType::COMMA)) break;
        }
        if (block->stmts.size() == 1) update = std::move(block->stmts[0]);
        else update = std::move(block);
    }
    expect(TokenType::RPAREN, "Expected ')'");

    auto body = parseStatement();
    auto node  = std::make_unique<ForStmt>(std::move(init), std::move(cond), std::move(update), std::move(body));
    node->line = l; node->column = c;
    return node;
}

ASTNodePtr Parser::parseSwitchStmt() {
    int l = current().line, c = current().column;
    advance(); // 'switch'
    expect(TokenType::LPAREN, "Expected '(' after 'switch'");
    auto val = parseExpression();
    expect(TokenType::RPAREN, "Expected ')'");
    expect(TokenType::LBRACE, "Expected '{'");

    auto sw  = std::make_unique<SwitchStmt>();
    sw->line = l; sw->column = c;
    sw->value = std::move(val);

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        SwitchCase sc;
        if (match(TokenType::KW_CASE)) {
            sc.value = parseExpression();
            expect(TokenType::COLON, "Expected ':' after case value");
        } else {
            expect(TokenType::KW_DEFAULT, "Expected 'case' or 'default'");
            expect(TokenType::COLON, "Expected ':' after 'default'");
            sc.isDefault = true;
        }
        while (!check(TokenType::KW_CASE) && !check(TokenType::KW_DEFAULT)
               && !check(TokenType::RBRACE) && !isAtEnd())
            sc.stmts.push_back(parseStatement());
        sw->cases.push_back(std::move(sc));
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return sw;
}

ASTNodePtr Parser::parseTryCatchStmt() {
    int l = current().line, c = current().column;
    advance(); // 'try'
    auto trycatch       = std::make_unique<TryCatchStmt>();
    trycatch->line      = l; trycatch->column = c;
    trycatch->tryBody   = parseBlock();

    while (check(TokenType::KW_CATCH)) {
        advance();
        expect(TokenType::LPAREN, "Expected '(' after 'catch'");
        CatchClause cc;
        cc.type    = parseTypeAnnotation();
        cc.varName = expect(TokenType::IDENTIFIER, "Expected catch variable name").value;
        expect(TokenType::RPAREN, "Expected ')'");
        cc.body    = parseBlock();
        trycatch->catches.push_back(std::move(cc));
    }

    if (match(TokenType::KW_FINALLY))
        trycatch->finallyBody = parseBlock();

    return trycatch;
}

ASTNodePtr Parser::parseThrowStmt() {
    int l = current().line, c = current().column;
    advance(); // 'throw'
    auto val  = parseExpression();
    expect(TokenType::SEMICOLON, "Expected ';' after throw statement");
    auto node  = std::make_unique<ThrowStmt>(std::move(val));
    node->line = l; node->column = c;
    return node;
}

ASTNodePtr Parser::parseUnsafeBlock() {
    int l = current().line, c = current().column;
    advance(); // 'unsafe'
    auto body  = parseBlock();
    auto node  = std::make_unique<UnsafeBlock>(std::move(body));
    node->line = l; node->column = c;
    return node;
}

std::vector<ASTNodePtr> Parser::parseArgList() {
    expect(TokenType::LPAREN, "Expected '('");
    std::vector<ASTNodePtr> args;
    while (!check(TokenType::RPAREN) && !isAtEnd()) {
        args.push_back(parseExpression());
        if (!match(TokenType::COMMA)) break;
    }
    expect(TokenType::RPAREN, "Expected ')'");
    return args;
}

ASTNodePtr Parser::parseExpression() { return parseAssignment(); }

ASTNodePtr Parser::parseAssignment() {
    auto left = parseTernary();
    static const std::initializer_list<TokenType> assignOps = {
        TokenType::ASSIGN,       TokenType::PLUS_ASSIGN,
        TokenType::MINUS_ASSIGN, TokenType::STAR_ASSIGN,
        TokenType::SLASH_ASSIGN, TokenType::PERCENT_ASSIGN,
    };
    if (current().isAny(assignOps)) {
        std::string op = current().value; advance();
        auto right = parseAssignment(); // right-associative
        auto node  = std::make_unique<AssignExpr>(std::move(left), op, std::move(right));
        return node;
    }
    return left;
}

ASTNodePtr Parser::parseTernary() {
    auto cond = parseNullCoalesce();
    if (match(TokenType::QUESTION)) {
        auto thenE = parseExpression();
        expect(TokenType::COLON, "Expected ':' in ternary");
        auto elseE = parseTernary();
        return std::make_unique<TernaryExpr>(std::move(cond), std::move(thenE), std::move(elseE));
    }
    return cond;
}

ASTNodePtr Parser::parseNullCoalesce() {
    auto left = parseOr();
    while (check(TokenType::NULL_COALESCE)) {
        advance();
        auto right = parseOr();
        left = std::make_unique<NullCoalesceExpr>(std::move(left), std::move(right));
    }
    return left;
}

ASTNodePtr Parser::parseOr() {
    auto left = parseAnd();
    while (check(TokenType::OR)) {
        std::string op = current().value; advance();
        auto right = parseAnd();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ASTNodePtr Parser::parseAnd() {
    auto left = parseBitOr();
    while (check(TokenType::AND)) {
        std::string op = current().value; advance();
        auto right = parseBitOr();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ASTNodePtr Parser::parseBitOr() {
    auto left = parseBitXor();
    while (check(TokenType::BIT_OR)) {
        std::string op = current().value; advance();
        auto right = parseBitXor();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ASTNodePtr Parser::parseBitXor() {
    auto left = parseBitAnd();
    while (check(TokenType::BIT_XOR)) {
        std::string op = current().value; advance();
        auto right = parseBitAnd();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ASTNodePtr Parser::parseBitAnd() {
    auto left = parseEquality();
    while (check(TokenType::BIT_AND)) {
        std::string op = current().value; advance();
        auto right = parseEquality();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ASTNodePtr Parser::parseEquality() {
    auto left = parseRelational();
    while (check(TokenType::EQ) || check(TokenType::NEQ)) {
        std::string op = current().value; advance();
        auto right = parseRelational();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ASTNodePtr Parser::parseRelational() {
    auto left = parseShift();
    while (current().isAny({TokenType::LT, TokenType::GT, TokenType::LTE, TokenType::GTE})) {
        std::string op = current().value; advance();
        auto right = parseShift();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ASTNodePtr Parser::parseShift() {
    auto left = parseAdditive();
    while (check(TokenType::LSHIFT) || check(TokenType::RSHIFT)) {
        std::string op = current().value; advance();
        auto right = parseAdditive();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ASTNodePtr Parser::parseAdditive() {
    auto left = parseMultiplicative();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        std::string op = current().value; advance();
        auto right = parseMultiplicative();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ASTNodePtr Parser::parseMultiplicative() {
    auto left = parseUnary();
    while (current().isAny({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        std::string op = current().value; advance();
        auto right = parseUnary();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ASTNodePtr Parser::parseUnary() {
    // Prefix operators
    if (current().isAny({TokenType::NOT, TokenType::MINUS, TokenType::BIT_NOT})) {
        std::string op = current().value; int l = current().line; advance();
        auto operand = parseUnary();
        auto node = std::make_unique<UnaryExpr>(op, std::move(operand), true);
        node->line = l; return node;
    }
    if (check(TokenType::INCREMENT) || check(TokenType::DECREMENT)) {
        std::string op = current().value; int l = current().line; advance();
        auto operand = parseUnary();
        auto node = std::make_unique<UnaryExpr>(op, std::move(operand), true);
        node->line = l; return node;
    }
    // unsafe: *ptr dereference
    if (check(TokenType::STAR)) {
        advance();
        auto operand = parseUnary();
        return std::make_unique<DerefExpr>(std::move(operand));
    }
    // unsafe: &value
    if (check(TokenType::BIT_AND)) {
        advance();
        auto operand = parseUnary();
        return std::make_unique<AddressOfExpr>(std::move(operand));
    }

    // C-style cast: (Type)expr
    if (check(TokenType::LPAREN)) {
        bool isType = isTypeKeyword(peek(1).type);
        bool isIdent = (peek(1).type == TokenType::IDENTIFIER);
        if ((isType || isIdent) && peek(2).type == TokenType::RPAREN) {
            TokenType nextT = peek(3).type;
            bool isBinaryOpOrDelim = (
                nextT == TokenType::COMMA || nextT == TokenType::SEMICOLON ||
                nextT == TokenType::RPAREN || nextT == TokenType::RBRACKET ||
                nextT == TokenType::RBRACE || nextT == TokenType::ASSIGN ||
                nextT == TokenType::PLUS_ASSIGN || nextT == TokenType::MINUS_ASSIGN ||
                nextT == TokenType::STAR_ASSIGN || nextT == TokenType::SLASH_ASSIGN ||
                nextT == TokenType::EQ || nextT == TokenType::NEQ ||
                nextT == TokenType::LTE || nextT == TokenType::GTE ||
                nextT == TokenType::AND || nextT == TokenType::OR ||
                nextT == TokenType::BIT_OR || nextT == TokenType::BIT_XOR ||
                nextT == TokenType::QUESTION || nextT == TokenType::COLON ||
                nextT == TokenType::DOT || nextT == TokenType::SAFE_DOT ||
                nextT == TokenType::ARROW || nextT == TokenType::FAT_ARROW ||
                (!isType && (nextT == TokenType::PLUS || nextT == TokenType::STAR || nextT == TokenType::SLASH || nextT == TokenType::PERCENT || nextT == TokenType::LT || nextT == TokenType::GT))
            );
            if (!isBinaryOpOrDelim) {
                int l = current().line, c = current().column;
                std::string fn = current().filename.empty() ? filename_ : current().filename;
                advance(); // consume '('
                TypeAnnotation ta = parseTypeAnnotation();
                expect(TokenType::RPAREN, "Expected ')' after cast type");
                auto operand = parseUnary();
                auto node = std::make_unique<CastExpr>(std::move(ta), std::move(operand));
                node->line = l; node->column = c; node->filename = fn;
                return node;
            }
        }
    }

    auto expr = parsePrimary();
    return parsePostfix(std::move(expr));
}

ASTNodePtr Parser::parsePostfix(ASTNodePtr expr) {
    while (true) {
        if (check(TokenType::DOT) || check(TokenType::SAFE_DOT)) {
            bool safe = check(TokenType::SAFE_DOT); advance();
            std::string member = expect(TokenType::IDENTIFIER, "Expected member name after '.'").value;
            expr = std::make_unique<MemberAccessExpr>(std::move(expr), member, safe);
        } else if (check(TokenType::LBRACKET)) {
            advance();
            // Support multi-index: m[1, 2] — pack into an array literal
            auto firstIndex = parseExpression();
            if (check(TokenType::COMMA)) {
                auto arr = std::make_unique<ArrayLiteralExpr>();
                arr->elements.push_back(std::move(firstIndex));
                while (match(TokenType::COMMA)) {
                    arr->elements.push_back(parseExpression());
                }
                expect(TokenType::RBRACKET, "Expected ']'");
                expr = std::make_unique<IndexExpr>(std::move(expr), std::move(arr));
            } else {
                expect(TokenType::RBRACKET, "Expected ']'");
                expr = std::make_unique<IndexExpr>(std::move(expr), std::move(firstIndex));
            }
        } else if (check(TokenType::LPAREN)) {
            // Call
            auto args = parseArgList();
            expr = std::make_unique<CallExpr>(std::move(expr), std::move(args));
        } else if (check(TokenType::INCREMENT) || check(TokenType::DECREMENT)) {
            std::string op = current().value; advance();
            expr = std::make_unique<UnaryExpr>(op, std::move(expr), false); // postfix
        } else if (check(TokenType::KW_AS)) {
            advance();
            TypeAnnotation ta = parseTypeAnnotation();
            expr = std::make_unique<CastExpr>(std::move(ta), std::move(expr));
        } else if (check(TokenType::NOT) && /* null assert */ peek(-1).type != TokenType::NEQ) {
            // null assertion x!
            advance();
            expr = std::make_unique<NullAssertExpr>(std::move(expr));
        } else if (check(TokenType::LT)) {
            size_t savedPos = pos_;
            try {
                advance(); // '<'
                std::vector<std::string> typeArgs;
                while (!(check(TokenType::GT) || check(TokenType::RSHIFT) || pendingGT_ > 0) && !isAtEnd()) {
                    typeArgs.push_back(parseTypeAnnotationBasic().name);
                    if (!match(TokenType::COMMA)) break;
                }
                if (!consumeGT()) { pos_ = savedPos; break; }
                if (!check(TokenType::LPAREN)) { pos_ = savedPos; break; }
                auto args = parseArgList();
                expr = std::make_unique<CallExpr>(std::move(expr), std::move(args), typeArgs);
                continue;
            } catch (...) {
                pos_ = savedPos;
            }
            break;
        } else if (check(TokenType::LBRACE)) {
            size_t savedPos = pos_;
            try {
                advance(); // '{'
                auto si = std::make_unique<StructInitExpr>();
                si->typeName = std::move(expr);
                while (!check(TokenType::RBRACE) && !isAtEnd()) {
                    std::string field = expect(TokenType::IDENTIFIER,"").value;
                    expect(TokenType::ASSIGN, "");
                    auto val = parseExpression();
                    si->fields.emplace_back(field, std::move(val));
                    if (!match(TokenType::COMMA)) break;
                }
                expect(TokenType::RBRACE, "");
                expr = std::move(si);
                continue;
            } catch (...) {
                pos_ = savedPos;
            }
            break;
        } else {
            break;
        }
    }
    return expr;
}

bool Parser::isLambdaStart() {
    size_t saved = pos_;
    bool result  = false;
    try {
        if (check(TokenType::LPAREN)) {
            advance();
            if (check(TokenType::RPAREN)) { advance(); result = check(TokenType::ARROW) || check(TokenType::FAT_ARROW); }
            else {
                while (!isAtEnd()) {
                    if (isTypeStart()) parseTypeAnnotation();
                    if (check(TokenType::IDENTIFIER)) advance();
                    if (!match(TokenType::COMMA)) break;
                }
                if (match(TokenType::RPAREN))
                    result = check(TokenType::ARROW) || check(TokenType::FAT_ARROW);
            }
        } else if (check(TokenType::IDENTIFIER)) {
            advance();
            result = check(TokenType::ARROW) || check(TokenType::FAT_ARROW);
        }
    } catch (...) {}
    pos_ = saved;
    return result;
}

ASTNodePtr Parser::parseLambda() {
    std::vector<Parameter> params;
    if (check(TokenType::IDENTIFIER) && (peek(1).type == TokenType::ARROW || peek(1).type == TokenType::FAT_ARROW)) {
        std::string name = advance().value;
        TypeAnnotation ta; ta.name = "var";
        params.emplace_back(std::move(ta), std::move(name), false);
    } else {
        expect(TokenType::LPAREN, "Expected '('");
        while (!check(TokenType::RPAREN) && !isAtEnd()) {
            if (isTypeStart() && !peek(1).isAny({TokenType::COMMA, TokenType::RPAREN, TokenType::ASSIGN})) {
                params.push_back(parseParameter());
            } else if (check(TokenType::IDENTIFIER)) {
                std::string name = advance().value;
                TypeAnnotation ta; ta.name = "var";
                params.emplace_back(std::move(ta), std::move(name), false);
            } else {
                break;
            }
            if (!match(TokenType::COMMA)) break;
        }
        expect(TokenType::RPAREN, "Expected ')'");
    }
    // Accept -> or =>
    if (!match(TokenType::ARROW) && !match(TokenType::FAT_ARROW))
        throw error("Expected '->' in lambda");
    ASTNodePtr body;
    if (check(TokenType::LBRACE)) body = parseBlock();
    else {
        // Arrow expression: wrap in a return so callFunction auto-returns it
        auto expr = parseExpression();
        body = std::make_unique<ReturnStmt>(std::move(expr));
    }
    return std::make_unique<LambdaExpr>(std::move(params), std::move(body));
}

ASTNodePtr Parser::parseInterpolatedString(const std::string& raw) {
    auto node = std::make_unique<InterpolatedStringExpr>();
    size_t i = 0;
    while (i < raw.size()) {
        if (raw[i] == '{') {
            // extract expression string up to matching }
            size_t start = i + 1;
            int depth = 1;
            size_t j = start;
            while (j < raw.size() && depth > 0) {
                if (raw[j] == '{') depth++;
                else if (raw[j] == '}') depth--;
                if (depth > 0) j++;
                else break;
            }
            std::string exprSrc = raw.substr(start, j - start);
            Lexer innerLex(exprSrc, "<interpolation>");
            auto innerToks = innerLex.tokenize();
            Parser innerParser(std::move(innerToks), "<interpolation>");
            auto exprAst = innerParser.parseExpression();
            node->parts.emplace_back(std::move(exprAst));
            i = j + 1;
        } else {
            std::string text;
            while (i < raw.size() && raw[i] != '{') {
                if (raw[i] == '\\' && i + 1 < raw.size()) {
                    i++;
                    switch (raw[i]) {
                    case 'n': text += '\n'; break; case 't': text += '\t'; break;
                    case 'r': text += '\r'; break; case '"': text += '"';  break;
                    default:  text += raw[i]; break;
                    }
                } else {
                    text += raw[i];
                }
                i++;
            }
            if (!text.empty()) node->parts.emplace_back(text);
        }
    }
    return node;
}

bool Parser::isTypeStart() const {
    return isTypeKeyword(current().type);
}

ASTNodePtr Parser::parsePrimary() {
    int l = current().line, c = current().column;
    std::string fn = current().filename.empty() ? filename_ : current().filename;

    // Lambda check before type-start
    if (isLambdaStart()) return parseLambda();

    // Literals
    if (check(TokenType::INTEGER_LITERAL)) {
        int64_t v = std::stoll(current().value);
        advance();
        auto n = std::make_unique<IntLiteralExpr>(v); n->line=l;n->column=c;n->filename=fn;
        return n;
    }
    if (check(TokenType::FLOAT_LITERAL)) {
        double v = std::stod(current().value);
        advance();
        auto n = std::make_unique<FloatLiteralExpr>(v, true); n->line=l;n->column=c;n->filename=fn;
        return n;
    }
    if (check(TokenType::DOUBLE_LITERAL)) {
        double v = std::stod(current().value);
        advance();
        auto n = std::make_unique<FloatLiteralExpr>(v, false); n->line=l;n->column=c;n->filename=fn;
        return n;
    }
    if (check(TokenType::STRING_LITERAL)) {
        std::string v = current().value; advance();
        auto n = std::make_unique<StringLiteralExpr>(std::move(v)); n->line=l;n->column=c;n->filename=fn;
        return n;
    }
    if (check(TokenType::INTERP_STRING)) {
        std::string raw = current().value; advance();
        return parseInterpolatedString(raw);
    }
    if (check(TokenType::CHAR_LITERAL)) {
        char v = current().value.empty() ? '\0' : current().value[0]; advance();
        auto n = std::make_unique<CharLiteralExpr>(v); n->line=l;n->column=c;n->filename=fn;
        return n;
    }
    if (check(TokenType::KW_TRUE))  { advance(); auto n=std::make_unique<BoolLiteralExpr>(true); n->line=l;n->column=c;n->filename=fn; return n; }
    if (check(TokenType::KW_FALSE)) { advance(); auto n=std::make_unique<BoolLiteralExpr>(false); n->line=l;n->column=c;n->filename=fn; return n; }
    if (check(TokenType::KW_NULL))  { advance(); auto n=std::make_unique<NullLiteralExpr>(); n->line=l;n->column=c;n->filename=fn; return n; }

    // 'this'
    if (check(TokenType::KW_THIS)) {
        advance();
        auto n = std::make_unique<IdentifierExpr>("this"); n->line=l;n->column=c;n->filename=fn;
        return n;
    }
    // 'super'
    if (check(TokenType::KW_SUPER)) {
        advance();
        // super(args) — treat as call to "super"
        auto callee = std::make_unique<IdentifierExpr>("super"); callee->line=l;callee->column=c;callee->filename=fn;
        if (check(TokenType::LPAREN)) {
            auto args = parseArgList();
            auto call = std::make_unique<CallExpr>(std::move(callee), std::move(args));
            call->line=l;call->column=c;call->filename=fn;
            return call;
        }
        return callee;
    }

    // 'new'
    if (check(TokenType::KW_NEW)) {
        advance();
        TypeAnnotation ta = parseTypeAnnotationBasic();
        // Check for array allocation: new T[size] or new T[dim1][dim2][dim3]
        if (check(TokenType::LBRACKET)) {
            std::vector<ASTNodePtr> sizes;
            while (check(TokenType::LBRACKET)) {
                advance(); // consume [
                if (!check(TokenType::RBRACKET)) {
                    sizes.push_back(parseExpression());
                } else {
                    sizes.push_back(std::make_unique<IntLiteralExpr>(0));
                }
                expect(TokenType::RBRACKET, "Expected ']' after array size");
            }
            ta.isArray = true;
            ta.arrayRank = static_cast<int>(sizes.size());
            if (sizes.size() == 1) {
                auto n = std::make_unique<AllocExpr>(std::move(ta), std::move(sizes[0]));
                n->line = l; n->column = c; n->filename = fn;
                return n;
            } else {
                auto arrSizes = std::make_unique<ArrayLiteralExpr>();
                arrSizes->elements = std::move(sizes);
                auto n = std::make_unique<AllocExpr>(std::move(ta), std::move(arrSizes));
                n->line = l; n->column = c; n->filename = fn;
                return n;
            }
        }
        // Generic args already captured in ta.typeArgs
        auto args = parseArgList();
        auto n    = std::make_unique<NewExpr>(std::move(ta), std::move(args));
        n->line = l; n->column = c; n->filename = fn;
        return n;
    }

    // cast<T>(expr)
    if (check(TokenType::KW_CAST)) {
        advance();
        expect(TokenType::LT, "Expected '<' after 'cast'");
        TypeAnnotation ta = parseTypeAnnotation();
        expect(TokenType::GT, "Expected '>' after cast type");
        expect(TokenType::LPAREN, "Expected '('");
        auto val = parseExpression();
        expect(TokenType::RPAREN, "Expected ')'");
        auto n = std::make_unique<CastExpr>(std::move(ta), std::move(val));
        n->line = l; n->column = c; n->filename = fn;
        return n;
    }

    // alloc<T>(count) or alloc(count)
    if (check(TokenType::KW_ALLOC)) {
        Token allocTok = advance();
        TypeAnnotation ta("any");
        if (match(TokenType::LT)) {
            ta = parseTypeAnnotation();
            consumeGT();
        }
        expect(TokenType::LPAREN, "Expected '(' after 'alloc'");
        ASTNodePtr cnt;
        if (!check(TokenType::RPAREN)) {
            cnt = parseExpression();
        } else {
            cnt = std::make_unique<IntLiteralExpr>(1);
        }
        expect(TokenType::RPAREN, "Expected ')'");
        return makeNode<AllocExpr>(allocTok, std::move(ta), std::move(cnt));
    }

    // free(ptr)
    if (check(TokenType::KW_FREE)) {
        advance();
        expect(TokenType::LPAREN, "Expected '('");
        auto ptr = parseExpression();
        expect(TokenType::RPAREN, "Expected ')'");
        auto n = std::make_unique<FreeExpr>(std::move(ptr));
        n->line = l; n->column = c; n->filename = fn;
        return n;
    }

    // Grouping / lambda
    if (check(TokenType::LPAREN)) {
        advance();
        auto expr = parseExpression();
        expect(TokenType::RPAREN, "Expected ')'");
        return expr;
    }

    // Array literal [1, 2, 3]
    if (check(TokenType::LBRACKET)) {
        advance();
        auto arr = std::make_unique<ArrayLiteralExpr>();
        arr->line = l; arr->column = c; arr->filename = fn;
        while (!check(TokenType::RBRACKET) && !isAtEnd()) {
            arr->elements.push_back(parseExpression());
            if (!match(TokenType::COMMA)) break;
        }
        expect(TokenType::RBRACKET, "Expected ']'");
        return arr;
    }

    // Struct init { key: val, ... }
    if (check(TokenType::LBRACE)) {
        // Distinguish block from struct init by lookahead: { IDENT :
        if (peek().type == TokenType::IDENTIFIER && peek(2).type == TokenType::COLON) {
            advance(); // {
            auto si = std::make_unique<StructInitExpr>();
            si->line = l; si->column = c; si->filename = fn;
            while (!check(TokenType::RBRACE) && !isAtEnd()) {
                std::string field = expect(TokenType::IDENTIFIER,"Expected field name").value;
                expect(TokenType::COLON, "Expected ':' in struct init");
                auto val = parseExpression();
                si->fields.emplace_back(field, std::move(val));
                if (!match(TokenType::COMMA)) break;
            }
            expect(TokenType::RBRACE, "Expected '}'");
            return si;
        }
    }

    // Identifier (possibly qualified: A.B.C)
    if (check(TokenType::IDENTIFIER) || isTypeKeyword(current().type)) {
        std::string name = current().value; advance();
        auto n = std::make_unique<IdentifierExpr>(name);
        n->line = l; n->column = c; n->filename = fn;
        return n;
    }

    throw error(std::string("Unexpected token '") + current().value + "'");
}

}
