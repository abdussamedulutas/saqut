#ifndef SAQUT_PARSER_TOKEN
#define SAQUT_PARSER_TOKEN

#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "tokenizer/tokenizer.hpp"

typedef std::vector<Token*> TokenList;

// ============================================================
// TokenType enum
// ============================================================

enum class TokenType : uint16_t {
    IDENTIFIER,
    NUMBER,
    STRING,
    SVR_VOID,

    // Keywords
    KW_IF, KW_ELSE, KW_FOR, KW_WHILE, KW_DO,
    KW_SWITCH, KW_CASE, KW_DEFAULT, KW_BREAK, KW_CONTINUE,
    KW_RETURN, KW_CLASS, KW_INTERFACE, KW_ENUM,
    KW_EXTENDS, KW_IMPLEMENTS, KW_NEW,
    KW_PUBLIC, KW_PRIVATE, KW_PROTECTED, KW_STATIC,
    KW_FINAL, KW_ABSTRACT,
    KW_VOID, KW_BOOL, KW_INT, KW_FLOAT_TYPE, KW_DOUBLE,
    KW_CHAR, KW_STRING_TYPE,
    KW_TRUE, KW_FALSE, KW_NULL,
    KW_TRY, KW_CATCH, KW_FINALLY, KW_THROW, KW_THROWS, KW_ASSERT,
    KW_IMPORT, KW_PACKAGE, KW_NATIVE,
    KW_SYNCHRONIZED, KW_VOLATILE, KW_TRANSIENT,
    KW_CONST, KW_EXTERN, KW_TYPEDEF, KW_SIZEOF,
    KW_ALIGNOF, KW_DECLTYPE, KW_AUTO, KW_CONSTEXPR, KW_NOEXCEPT,

    // Operators (precedence order)
    DOT, ARROW, LBRACKET, RBRACKET, LPAREN, RPAREN,
    PLUS_PLUS, MINUS_MINUS,
    PLUS, MINUS, BANG, TILDE,
    STAR_STAR, CARET,
    STAR, SLASH, PERCENT,
    LSHIFT, RSHIFT,
    LESS, LESS_EQUAL, GREATER, GREATER_EQUAL,
    EQUAL_EQUAL, BANG_EQUAL,
    AMPERSAND, PIPE,
    AMPERSAND_AMPERSAND, PIPE_PIPE,
    TERNARY, COLON,
    EQUAL, PLUS_EQUAL, MINUS_EQUAL, STAR_EQUAL, SLASH_EQUAL,
    PERCENT_EQUAL, AMPERSAND_EQUAL, PIPE_EQUAL, CARET_EQUAL,
    LSHIFT_EQUAL, RSHIFT_EQUAL,

    // Other symbols
    LBRACE, RBRACE, SEMICOLON, COMMA, COLON_COLON,

    END_OF_FILE, UNKNOWN, COMMENT, PREPROCESSOR,
};

// ============================================================
// Keyword map
// ============================================================

inline const std::unordered_map<std::string_view, TokenType> KEYWORD_MAP = {
    {"if",          TokenType::KW_IF},
    {"else",        TokenType::KW_ELSE},
    {"for",         TokenType::KW_FOR},
    {"while",       TokenType::KW_WHILE},
    {"do",          TokenType::KW_DO},
    {"switch",      TokenType::KW_SWITCH},
    {"case",        TokenType::KW_CASE},
    {"default",     TokenType::KW_DEFAULT},
    {"break",       TokenType::KW_BREAK},
    {"continue",    TokenType::KW_CONTINUE},
    {"return",      TokenType::KW_RETURN},

    {"class",       TokenType::KW_CLASS},
    {"interface",   TokenType::KW_INTERFACE},
    {"enum",        TokenType::KW_ENUM},
    {"extends",     TokenType::KW_EXTENDS},
    {"implements",  TokenType::KW_IMPLEMENTS},
    {"new",         TokenType::KW_NEW},

    {"public",      TokenType::KW_PUBLIC},
    {"private",     TokenType::KW_PRIVATE},
    {"protected",   TokenType::KW_PROTECTED},
    {"static",      TokenType::KW_STATIC},
    {"final",       TokenType::KW_FINAL},
    {"abstract",    TokenType::KW_ABSTRACT},

    {"void",        TokenType::KW_VOID},
    {"bool",        TokenType::KW_BOOL},
    {"int",         TokenType::KW_INT},
    {"float",       TokenType::KW_FLOAT_TYPE},
    {"double",      TokenType::KW_DOUBLE},
    {"char",        TokenType::KW_CHAR},
    {"string",      TokenType::KW_STRING_TYPE},

    {"true",        TokenType::KW_TRUE},
    {"false",       TokenType::KW_FALSE},
    {"null",        TokenType::KW_NULL},

    {"try",         TokenType::KW_TRY},
    {"catch",       TokenType::KW_CATCH},
    {"finally",     TokenType::KW_FINALLY},
    {"throw",       TokenType::KW_THROW},
    {"throws",      TokenType::KW_THROWS},
    {"assert",      TokenType::KW_ASSERT},

    {"import",      TokenType::KW_IMPORT},
    {"package",     TokenType::KW_PACKAGE},
    {"native",      TokenType::KW_NATIVE},
    {"synchronized",TokenType::KW_SYNCHRONIZED},
    {"volatile",    TokenType::KW_VOLATILE},
    {"transient",   TokenType::KW_TRANSIENT},

    {"const",       TokenType::KW_CONST},
    {"extern",      TokenType::KW_EXTERN},
    {"typedef",     TokenType::KW_TYPEDEF},
    {"sizeof",      TokenType::KW_SIZEOF},
    {"auto",        TokenType::KW_AUTO},
    {"constexpr",   TokenType::KW_CONSTEXPR},
    {"noexcept",    TokenType::KW_NOEXCEPT},
};

// ============================================================
// Operator maps
// ============================================================

inline const std::unordered_map<std::string_view, TokenType> OPERATOR_MAP = {
    {"->",  TokenType::ARROW},
    {"::",  TokenType::COLON_COLON},
    {"==",  TokenType::EQUAL_EQUAL},
    {"!=",  TokenType::BANG_EQUAL},
    {"<=",  TokenType::LESS_EQUAL},
    {">=",  TokenType::GREATER_EQUAL},
    {"&&",  TokenType::AMPERSAND_AMPERSAND},
    {"||",  TokenType::PIPE_PIPE},
    {"++",  TokenType::PLUS_PLUS},
    {"--",  TokenType::MINUS_MINUS},
    {"<<",  TokenType::LSHIFT},
    {">>",  TokenType::RSHIFT},
    {"**",  TokenType::STAR_STAR},

    {"+=",  TokenType::PLUS_EQUAL},
    {"-=",  TokenType::MINUS_EQUAL},
    {"*=",  TokenType::STAR_EQUAL},
    {"/=",  TokenType::SLASH_EQUAL},
    {"%=",  TokenType::PERCENT_EQUAL},
    {"&=",  TokenType::AMPERSAND_EQUAL},
    {"|=",  TokenType::PIPE_EQUAL},
    {"^=",  TokenType::CARET_EQUAL},
    {"<<=", TokenType::LSHIFT_EQUAL},
    {">>=", TokenType::RSHIFT_EQUAL},

    {"+",   TokenType::PLUS},
    {"-",   TokenType::MINUS},
    {"*",   TokenType::STAR},
    {"/",   TokenType::SLASH},
    {"%",   TokenType::PERCENT},
    {"<",   TokenType::LESS},
    {">",   TokenType::GREATER},
    {"^",   TokenType::CARET},
    {"!",   TokenType::BANG},
    {"~",   TokenType::TILDE},
    {"&",   TokenType::AMPERSAND},
    {"|",   TokenType::PIPE},
    {"=",   TokenType::EQUAL},

    {"[",   TokenType::LBRACKET},
    {"]",   TokenType::RBRACKET},
    {"(",   TokenType::LPAREN},
    {")",   TokenType::RPAREN},
    {"{",   TokenType::LBRACE},
    {"}",   TokenType::RBRACE},
    {";",   TokenType::SEMICOLON},
    {",",   TokenType::COMMA},
    {":",   TokenType::COLON},
    {".",   TokenType::DOT},
    {"?",   TokenType::TERNARY},
};

inline const std::unordered_map<TokenType, std::string_view> OPERATOR_MAP_REV = {
    {TokenType::ARROW,              "->"},
    {TokenType::COLON_COLON,        "::"},
    {TokenType::EQUAL_EQUAL,        "=="},
    {TokenType::BANG_EQUAL,         "!="},
    {TokenType::LESS_EQUAL,         "<="},
    {TokenType::GREATER_EQUAL,      ">="},
    {TokenType::AMPERSAND_AMPERSAND,"&&"},
    {TokenType::PIPE_PIPE,          "||"},
    {TokenType::PLUS_PLUS,          "++"},
    {TokenType::MINUS_MINUS,        "--"},
    {TokenType::LSHIFT,             "<<"},
    {TokenType::RSHIFT,             ">>"},
    {TokenType::STAR_STAR,          "**"},
    {TokenType::PLUS_EQUAL,         "+="},
    {TokenType::MINUS_EQUAL,        "-="},
    {TokenType::STAR_EQUAL,         "*="},
    {TokenType::SLASH_EQUAL,        "/="},
    {TokenType::PERCENT_EQUAL,      "%="},
    {TokenType::AMPERSAND_EQUAL,    "&="},
    {TokenType::PIPE_EQUAL,         "|="},
    {TokenType::CARET_EQUAL,        "^="},
    {TokenType::LSHIFT_EQUAL,       "<<="},
    {TokenType::RSHIFT_EQUAL,       ">>="},
    {TokenType::PLUS,               "+"},
    {TokenType::MINUS,              "-"},
    {TokenType::STAR,               "*"},
    {TokenType::SLASH,              "/"},
    {TokenType::PERCENT,            "%"},
    {TokenType::LESS,               "<"},
    {TokenType::GREATER,            ">"},
    {TokenType::CARET,              "^"},
    {TokenType::BANG,               "!"},
    {TokenType::TILDE,              "~"},
    {TokenType::AMPERSAND,          "&"},
    {TokenType::PIPE,               "|"},
    {TokenType::EQUAL,              "="},
    {TokenType::LBRACKET,           "["},
    {TokenType::RBRACKET,           "]"},
    {TokenType::LPAREN,             "("},
    {TokenType::RPAREN,             ")"},
    {TokenType::LBRACE,             "{"},
    {TokenType::RBRACE,             "}"},
    {TokenType::SEMICOLON,          ";"},
    {TokenType::COMMA,              ","},
    {TokenType::COLON,              ":"},
    {TokenType::DOT,                "."},
    {TokenType::TERNARY,            "?"},
};

inline const std::unordered_map<TokenType, std::string_view> OPERATOR_MAP_STRREV = {
    {TokenType::ARROW,              "ARROW"},
    {TokenType::COLON_COLON,        "COLON_COLON"},
    {TokenType::EQUAL_EQUAL,        "EQUAL_EQUAL"},
    {TokenType::BANG_EQUAL,         "BANG_EQUAL"},
    {TokenType::LESS_EQUAL,         "LESS_EQUAL"},
    {TokenType::GREATER_EQUAL,      "GREATER_EQUAL"},
    {TokenType::AMPERSAND_AMPERSAND,"AMPERSAND_AMPERSAND"},
    {TokenType::PIPE_PIPE,          "PIPE_PIPE"},
    {TokenType::PLUS_PLUS,          "PLUS_PLUS"},
    {TokenType::MINUS_MINUS,        "MINUS_MINUS"},
    {TokenType::LSHIFT,             "LSHIFT"},
    {TokenType::RSHIFT,             "RSHIFT"},
    {TokenType::STAR_STAR,          "STAR_STAR"},
    {TokenType::PLUS_EQUAL,         "PLUS_EQUAL"},
    {TokenType::MINUS_EQUAL,        "MINUS_EQUAL"},
    {TokenType::STAR_EQUAL,         "STAR_EQUAL"},
    {TokenType::SLASH_EQUAL,        "SLASH_EQUAL"},
    {TokenType::PERCENT_EQUAL,      "PERCENT_EQUAL"},
    {TokenType::AMPERSAND_EQUAL,    "AMPERSAND_EQUAL"},
    {TokenType::PIPE_EQUAL,         "PIPE_EQUAL"},
    {TokenType::CARET_EQUAL,        "CARET_EQUAL"},
    {TokenType::LSHIFT_EQUAL,       "LSHIFT_EQUAL"},
    {TokenType::RSHIFT_EQUAL,       "RSHIFT_EQUAL"},
    {TokenType::PLUS,               "PLUS"},
    {TokenType::MINUS,              "MINUS"},
    {TokenType::STAR,               "STAR"},
    {TokenType::SLASH,              "SLASH"},
    {TokenType::PERCENT,            "PERCENT"},
    {TokenType::LESS,               "LESS"},
    {TokenType::GREATER,            "GREATER"},
    {TokenType::CARET,              "CARET"},
    {TokenType::BANG,               "BANG"},
    {TokenType::TILDE,              "TILDE"},
    {TokenType::AMPERSAND,          "AMPERSAND"},
    {TokenType::PIPE,               "PIPE"},
    {TokenType::EQUAL,              "EQUAL"},
    {TokenType::LBRACKET,           "LBRACKET"},
    {TokenType::RBRACKET,           "RBRACKET"},
    {TokenType::LPAREN,             "LPAREN"},
    {TokenType::RPAREN,             "RPAREN"},
    {TokenType::LBRACE,             "LBRACE"},
    {TokenType::RBRACE,             "RBRACE"},
    {TokenType::SEMICOLON,          "SEMICOLON"},
    {TokenType::COMMA,              "COMMA"},
    {TokenType::COLON,              "COLON"},
    {TokenType::DOT,                "DOT"},
    {TokenType::TERNARY,            "TERNARY"},
};

// ============================================================
// Precedence table (Pratt parsing)
// ============================================================

inline uint16_t TokenPrecedence(TokenType type) {
    switch (type) {
        // Level 18: Member access / call
        case TokenType::DOT:
        case TokenType::ARROW:
        case TokenType::LBRACKET:
        case TokenType::LPAREN:
            return 18;

        // Level 17: Postfix
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS:
            return 17;

        // Level 16: Unary prefix
        case TokenType::BANG:
        case TokenType::TILDE:
            return 16;

        // Level 15: Exponentiation
        case TokenType::STAR_STAR:
        case TokenType::CARET:
            return 15;

        // Level 14: Multiplicative
        case TokenType::STAR:
        case TokenType::SLASH:
        case TokenType::PERCENT:
            return 14;

        // Level 13: Additive
        case TokenType::PLUS:
        case TokenType::MINUS:
            return 13;

        // Level 12: Bit shift
        case TokenType::LSHIFT:
        case TokenType::RSHIFT:
            return 12;

        // Level 11: Relational
        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
            return 11;

        // Level 10: Equality
        case TokenType::EQUAL_EQUAL:
        case TokenType::BANG_EQUAL:
            return 10;

        // Level 9: Bitwise AND
        case TokenType::AMPERSAND:
            return 9;

        // Level 8: Bitwise XOR — CARET already handled in 15 as exponent
        // Level 7: Bitwise OR
        case TokenType::PIPE:
            return 7;

        // Level 6: Logical AND
        case TokenType::AMPERSAND_AMPERSAND:
            return 6;

        // Level 5: Logical OR
        case TokenType::PIPE_PIPE:
            return 5;

        // Level 4: Ternary
        case TokenType::TERNARY:
            return 4;
        case TokenType::COLON:
            return 3;

        // Level 2: Assignment
        case TokenType::EQUAL:
        case TokenType::PLUS_EQUAL:
        case TokenType::MINUS_EQUAL:
        case TokenType::STAR_EQUAL:
        case TokenType::SLASH_EQUAL:
        case TokenType::PERCENT_EQUAL:
        case TokenType::AMPERSAND_EQUAL:
        case TokenType::PIPE_EQUAL:
        case TokenType::CARET_EQUAL:
        case TokenType::LSHIFT_EQUAL:
        case TokenType::RSHIFT_EQUAL:
            return 2;

        // Level 1: Comma
        case TokenType::COMMA:
            return 1;

        default:
            return 0;
    }
}

// ============================================================
// Right-associative check
// ============================================================

inline bool RightAssociative(TokenType type) {
    switch (type) {
        case TokenType::STAR_STAR:
        case TokenType::CARET:
        case TokenType::EQUAL:
        case TokenType::PLUS_EQUAL:
        case TokenType::MINUS_EQUAL:
        case TokenType::STAR_EQUAL:
        case TokenType::SLASH_EQUAL:
        case TokenType::PERCENT_EQUAL:
        case TokenType::AMPERSAND_EQUAL:
        case TokenType::PIPE_EQUAL:
        case TokenType::CARET_EQUAL:
        case TokenType::LSHIFT_EQUAL:
        case TokenType::RSHIFT_EQUAL:
        case TokenType::TERNARY:
            return true;
        default:
            return false;
    }
}

// ============================================================
// ParserToken
// ============================================================

struct ParserToken {
    Token*    token = nullptr;
    TokenType type  = TokenType::SVR_VOID;

    bool is(TokenType t) const {
        return type == t;
    }

    bool is(std::initializer_list<TokenType> types) const {
        for (TokenType t : types)
            if (type == t) return true;
        return false;
    }

    uint16_t getPowerOperator() const {
        return TokenPrecedence(type);
    }

    bool isRightAssociative() const {
        return RightAssociative(type);
    }
};

#endif
