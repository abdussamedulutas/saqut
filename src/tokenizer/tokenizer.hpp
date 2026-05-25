#ifndef SAQUT_TOKENIZER
#define SAQUT_TOKENIZER

#include <iostream>
#include <string>
#include <vector>
#include "lexer/lexer.hpp"

// ============================================================
// Token classes
// ============================================================

class Token {
protected:
    std::string type;
public:
    int start = 0;
    int end   = 0;
    std::string token;

    std::string gettype() { return type; }
    virtual ~Token() = default;
};

class StringToken : public Token {
public:
    StringToken()             { type = "string"; }
    std::string context;
    int size = 0;
};

class NumberToken : public Token {
public:
    NumberToken()             { type = "number"; }
    bool isFloat    = false;
    bool hasEpsilon = false;
    int base        = 10;
};

class OperatorToken : public Token {
public:
    OperatorToken()           { type = "operator"; }
};

class DelimiterToken : public Token {
public:
    DelimiterToken()          { type = "delimiter"; }
};

class KeywordToken : public Token {
public:
    KeywordToken()            { type = "keyword"; }
};

class IdentifierToken : public Token {
public:
    IdentifierToken()         { type = "identifier"; }
    std::string context;
    int size = 0;
};

// ============================================================
// Token tables
// ============================================================

#include <string_view>

inline constexpr std::string_view operators[] = {
    "==", "!=", "<=", ">=", "&&", "||",
    "++", "--", "<<", ">>",
    "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=",
    "+",  "-",  "*",  "/",  "%",  "<",  ">",
    "^",  "!",  "~",  "&",  "|",
    "="
};

inline constexpr std::string_view delimiters[] = {
    "->", "::",
    "[",  "]",  "(",  ")",  "{",  "}",
    ";",  ",",  ":",
    "."
};

inline constexpr std::string_view keywords[] = {
    // Control flow
    "if",       "else",     "for",      "while",    "do",
    "switch",   "case",     "default",  "break",    "continue",
    "return",   "try",      "catch",    "finally",  "throw",
    "throws",   "assert",
    // Types
    "void",     "int",      "float",    "double",   "char",
    "string",   "bool",
    // Literals
    "true",     "false",    "null",
    // OOP
    "class",    "interface","enum",     "extends",  "implements",
    "new",      "public",   "private",  "protected",
    "static",   "final",    "abstract",
    // Modules
    "import",   "package",
    // C/C++
    "const",    "extern",   "typedef",  "sizeof",
    "auto",     "constexpr","noexcept",
    "native",   "synchronized", "volatile", "transient"
};

// ============================================================
// Tokenizer
// ============================================================

class Tokenizer {
public:
    Lexer hmx;

    std::vector<Token*> scan(std::string input);

private:
    Token*          scope();
    IdentifierToken* readIdentifier();
    StringToken*     readString();
    void skipOneLineComment();
    void skipMultiLineComment();
};

// ============================================================
// Tokenizer implementation
// ============================================================

inline std::vector<Token*> Tokenizer::scan(std::string input) {
    std::vector<Token*> tokens;
    hmx.setText(input);
    while (true) {
        Token* token = scope();
        if (token->token == "EOL") break;
        tokens.push_back(token);
        if (hmx.isEnd()) break;
    }
    return tokens;
}

inline Token* Tokenizer::scope() {
    hmx.skipWhiteSpace();

    if (hmx.include("//", true))  skipOneLineComment();
    if (hmx.include("/*", true))  skipMultiLineComment();

    if (hmx.isEnd()) {
        Token* t = new Token();
        t->token = "EOL";
        return t;
    }

    // String literals
    if (hmx.getchar() == '"')
        return readString();

    // Numbers
    if (hmx.isNumeric()) {
        INumber lem = hmx.readNumeric();
        NumberToken* nt = new NumberToken();
        nt->base       = lem.base;
        nt->start      = lem.start;
        nt->end        = lem.end;
        nt->hasEpsilon = lem.hasEpsilon;
        nt->isFloat    = lem.isFloat;
        nt->token      = lem.token;
        return nt;
    }

    // Keywords (check boundary: keyword must not be prefix of longer identifier)
    for (const auto& kw : keywords) {
        if (hmx.include(std::string(kw), false)) {
            char next = hmx.getchar(static_cast<int>(kw.size()));
            if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') ||
                (next >= '0' && next <= '9') || next == '_' || next == '$') {
                continue;  // part of longer identifier, not a real keyword
            }
            KeywordToken* kt = new KeywordToken();
            kt->start = hmx.getOffset();
            hmx.toChar(static_cast<int>(kw.size()));
            kt->end   = hmx.getOffset();
            kt->token = kw;
            return kt;
        }
    }

    // Delimiters
    for (const auto& del : delimiters) {
        if (hmx.include(std::string(del), false)) {
            DelimiterToken* dt = new DelimiterToken();
            dt->start = hmx.getOffset();
            hmx.toChar(static_cast<int>(del.size()));
            dt->end   = hmx.getOffset();
            dt->token = del;
            return dt;
        }
    }

    // Operators
    for (const auto& op : operators) {
        if (hmx.include(std::string(op), false)) {
            OperatorToken* ot = new OperatorToken();
            ot->start = hmx.getOffset();
            hmx.toChar(static_cast<int>(op.size()));
            ot->end   = hmx.getOffset();
            ot->token = op;
            return ot;
        }
    }

    // Identifier (fallback)
    return readIdentifier();
}

inline IdentifierToken* Tokenizer::readIdentifier() {
    hmx.beginPosition();
    IdentifierToken* it = new IdentifierToken();
    it->start = hmx.getOffset();

    while (!hmx.isEnd()) {
        char c = hmx.getchar();
        bool read = false;

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            read = true;
            it->token.push_back(c);
        } else if (c == '_' || c == '$') {
            read = true;
            it->token.push_back(c);
        }

        if (read) {
            hmx.nextChar();
        } else {
            break;
        }
    }

    it->end  = hmx.getOffset();
    it->size = static_cast<int>(it->context.size());
    hmx.acceptPosition();
    return it;
}

inline StringToken* Tokenizer::readString() {
    hmx.beginPosition();
    StringToken* st = new StringToken();
    bool started = false;
    bool ended   = false;
    st->start = hmx.getOffset();

    while (!hmx.isEnd()) {
        char c = hmx.getchar();
        st->token.push_back(c);
        switch (c) {
            case '"':
                if (!started) {
                    started = true;
                } else {
                    ended = true;
                }
                break;
            case '\\':
                hmx.nextChar();
                c = hmx.getchar();
                st->token.push_back(c);
                st->context.push_back(c);
                break;
            default:
                st->context.push_back(c);
                break;
        }
        hmx.nextChar();
        if (ended) break;
    }

    st->end  = hmx.getOffset();
    st->size = static_cast<int>(st->context.size());
    hmx.acceptPosition();
    return st;
}

inline void Tokenizer::skipOneLineComment() {
    while (!hmx.isEnd()) {
        if (hmx.getchar() == '\n') {
            hmx.nextChar();
            hmx.skipWhiteSpace();
            return;
        }
        hmx.nextChar();
    }
}

inline void Tokenizer::skipMultiLineComment() {
    while (!hmx.isEnd()) {
        if (hmx.include("*/", true)) {
            hmx.skipWhiteSpace();
            return;
        }
        hmx.nextChar();
    }
}

#endif
