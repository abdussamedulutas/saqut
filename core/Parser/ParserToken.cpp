#include "../Tokenizer.cpp"
#include "../Tools.cpp"
#include <cstdint>
#include <unordered_map>
#include <initializer_list>

#ifndef PARSER_TOKEN
#define PARSER_TOKEN

typedef std::vector<Token *> TokenList;

enum class TokenType : uint16_t
{
    // --- Değerler ve Tanımlayıcılar ---
    IDENTIFIER,     // değişken/fonksiyon isimleri
    NUMBER,        // 42, 0xFF, 0b1010
    STRING,         // "merhaba"
    SVR_VOID,
    
    // --- KEYWORD'ler (Alfabetik) ---
    KW_IF,          // if
    KW_ELSE,        // else
    KW_FOR,         // for
    KW_WHILE,       // while
    KW_DO,          // do
    KW_SWITCH,      // switch
    KW_CASE,        // case
    KW_DEFAULT,     // default
    KW_BREAK,       // break
    KW_CONTINUE,    // continue
    KW_RETURN,      // return
    KW_CLASS,       // class
    KW_INTERFACE,   // interface
    KW_ENUM,        // enum
    KW_EXTENDS,     // extends
    KW_IMPLEMENTS,  // implements
    KW_NEW,         // new
    KW_PUBLIC,      // public
    KW_PRIVATE,     // private
    KW_PROTECTED,   // protected
    KW_STATIC,      // static
    KW_FINAL,       // final
    KW_ABSTRACT,    // abstract
    KW_VOID,        // void
    KW_BOOL,        // bool
    KW_INT,         // int
    KW_FLOAT_TYPE,  // float
    KW_DOUBLE,      // double
    KW_CHAR,        // char
    KW_STRING_TYPE, // string
    KW_TRUE,        // true
    KW_FALSE,       // false
    KW_NULL,        // null
    KW_TRY,         // try
    KW_CATCH,       // catch
    KW_FINALLY,     // finally
    KW_THROW,       // throw
    KW_THROWS,      // throws
    KW_ASSERT,      // assert
    KW_IMPORT,      // import
    KW_PACKAGE,     // package
    KW_NATIVE,      // native
    KW_SYNCHRONIZED,// synchronized
    KW_VOLATILE,    // volatile
    KW_TRANSIENT,   // transient
    KW_CONST,       // const (C++ style)
    KW_EXTERN,      // extern
    KW_TYPEDEF,     // typedef
    KW_SIZEOF,      // sizeof
    KW_ALIGNOF,     // alignof
    KW_DECLTYPE,    // decltype
    KW_AUTO,        // auto
    KW_CONSTEXPR,   // constexpr
    KW_NOEXCEPT,    // noexcept
    
    // --- Operatörler (öncelik sırasına göre) ---
    
    // Seviye 1: Üye erişimi ve çağrı
    DOT,            // .
    ARROW,          // ->
    LBRACKET,       // [
    RBRACKET,       // ]
    LPAREN,         // (
    RPAREN,         // )
    
    // Seviye 2: Postfix
    PLUS_PLUS,      // ++ (postfix)
    MINUS_MINUS,    // -- (postfix)
    
    // Seviye 3: Prefix/Unary
    PLUS,           // + (unary)
    MINUS,          // - (unary)
    BANG,           // ! (logical NOT)
    TILDE,          // ~ (bitwise NOT)
    
    // Seviye 4: Üs alma
    STAR_STAR,      // ** (Python-style üs)
    CARET,          // ^ (bazı dillerde üs)
    
    // Seviye 5: Çarpma/bölme
    STAR,           // *
    SLASH,          // /
    PERCENT,        // %
    
    // Seviye 6: Toplama/çıkarma
    // PLUS ve MINUS yukarıda var (unary olarak da kullanılır)
    
    // Seviye 7: Bitsel kaydırma
    LSHIFT,         // <<
    RSHIFT,         // >>
    
    // Seviye 8: İlişkisel
    LESS,           // <
    LESS_EQUAL,     // <=
    GREATER,        // >
    GREATER_EQUAL,  // >=
    
    // Seviye 9: Eşitlik
    EQUAL_EQUAL,    // ==
    BANG_EQUAL,     // !=
    
    // Seviye 10: Bitsel VE
    AMPERSAND,      // &
    
    // Seviye 11: Bitsel XOR
    // CARET yukarıda var
    
    // Seviye 12: Bitsel VEYA
    PIPE,           // |
    
    // Seviye 13: Mantıksal VE
    AMPERSAND_AMPERSAND,  // &&
    
    // Seviye 14: Mantıksal VEYA
    PIPE_PIPE,            // ||
    
    // Seviye 15: Ternary
    TERNARY,       // ?
    COLON,          // : (ternary için)
    
    // Seviye 16: Atama
    EQUAL,          // =
    PLUS_EQUAL,     // +=
    MINUS_EQUAL,    // -=
    STAR_EQUAL,     // *=
    SLASH_EQUAL,    // /=
    PERCENT_EQUAL,  // %=
    AMPERSAND_EQUAL,// &=
    PIPE_EQUAL,     // |=
    CARET_EQUAL,    // ^=
    LSHIFT_EQUAL,   // <<=
    RSHIFT_EQUAL,   // >>=
    
    // --- Diğer Semboller ---
    LBRACE,         // {
    RBRACE,         // }
    SEMICOLON,      // ;
    COMMA,          // ,
    COLON_COLON,    // ::
    
    // --- Özel ---
    END_OF_FILE,    // Dosya sonu
    UNKNOWN,        // Bilinmeyen karakter
    COMMENT,        // // veya /* */
    PREPROCESSOR,   // #include, #define
};


// lexer.cpp - Keyword map ekleyelim
static const std::unordered_map<std::string_view, TokenType> KEYWORD_MAP = {
    // --- Control flow ---
    {"if", TokenType::KW_IF},
    {"else", TokenType::KW_ELSE},
    {"for", TokenType::KW_FOR},
    {"while", TokenType::KW_WHILE},
    {"do", TokenType::KW_DO},
    {"switch", TokenType::KW_SWITCH},
    {"case", TokenType::KW_CASE},
    {"default", TokenType::KW_DEFAULT},
    {"break", TokenType::KW_BREAK},
    {"continue", TokenType::KW_CONTINUE},
    {"return", TokenType::KW_RETURN},
    
    // --- OOP ---
    {"class", TokenType::KW_CLASS},
    {"interface", TokenType::KW_INTERFACE},
    {"enum", TokenType::KW_ENUM},
    {"extends", TokenType::KW_EXTENDS},
    {"implements", TokenType::KW_IMPLEMENTS},
    {"new", TokenType::KW_NEW},
    
    // --- Access modifiers ---
    {"public", TokenType::KW_PUBLIC},
    {"private", TokenType::KW_PRIVATE},
    {"protected", TokenType::KW_PROTECTED},
    {"static", TokenType::KW_STATIC},
    {"final", TokenType::KW_FINAL},
    {"abstract", TokenType::KW_ABSTRACT},
    
    // --- Types ---
    {"void", TokenType::KW_VOID},
    {"bool", TokenType::KW_BOOL},
    {"int", TokenType::KW_INT},
    {"float", TokenType::KW_FLOAT_TYPE},
    {"double", TokenType::KW_DOUBLE},
    {"char", TokenType::KW_CHAR},
    {"string", TokenType::KW_STRING_TYPE},
    
    // --- Literals ---
    {"true", TokenType::KW_TRUE},
    {"false", TokenType::KW_FALSE},
    {"null", TokenType::KW_NULL},
    
    // --- Exception handling ---
    {"try", TokenType::KW_TRY},
    {"catch", TokenType::KW_CATCH},
    {"finally", TokenType::KW_FINALLY},
    {"throw", TokenType::KW_THROW},
    {"throws", TokenType::KW_THROWS},
    {"assert", TokenType::KW_ASSERT},
    
    // --- Modules/packages ---
    {"import", TokenType::KW_IMPORT},
    {"package", TokenType::KW_PACKAGE},
    
    // --- C/C++ specific ---
    {"const", TokenType::KW_CONST},
    {"extern", TokenType::KW_EXTERN},
    {"typedef", TokenType::KW_TYPEDEF},
    {"sizeof", TokenType::KW_SIZEOF},
    {"auto", TokenType::KW_AUTO},
    {"constexpr", TokenType::KW_CONSTEXPR},
    {"noexcept", TokenType::KW_NOEXCEPT},
    {"native", TokenType::KW_NATIVE},
    {"synchronized", TokenType::KW_SYNCHRONIZED},
    {"volatile", TokenType::KW_VOLATILE},
    {"transient", TokenType::KW_TRANSIENT},
};


// Operatör string'lerinden TokenType'e map
static const std::unordered_map<std::string_view, TokenType> OPERATOR_MAP = {
    // --- 2 karakterli operatörler (uzun olanlar önce!) ---
    {"->", TokenType::ARROW},
    {"::", TokenType::COLON_COLON},
    {"==", TokenType::EQUAL_EQUAL},
    {"!=", TokenType::BANG_EQUAL},
    {"<=", TokenType::LESS_EQUAL},
    {">=", TokenType::GREATER_EQUAL},
    {"&&", TokenType::AMPERSAND_AMPERSAND},
    {"||", TokenType::PIPE_PIPE},
    {"++", TokenType::PLUS_PLUS},
    {"--", TokenType::MINUS_MINUS},
    {"<<", TokenType::LSHIFT},
    {">>", TokenType::RSHIFT},
    {"**", TokenType::STAR_STAR},
    
    // --- Atama operatörleri ---
    {"+=", TokenType::PLUS_EQUAL},
    {"-=", TokenType::MINUS_EQUAL},
    {"*=", TokenType::STAR_EQUAL},
    {"/=", TokenType::SLASH_EQUAL},
    {"%=", TokenType::PERCENT_EQUAL},
    {"&=", TokenType::AMPERSAND_EQUAL},
    {"|=", TokenType::PIPE_EQUAL},
    {"^=", TokenType::CARET_EQUAL},
    {"<<=", TokenType::LSHIFT_EQUAL},
    {">>=", TokenType::RSHIFT_EQUAL},
    
    // --- 1 karakterli operatörler ---
    {"+", TokenType::PLUS},
    {"-", TokenType::MINUS},
    {"*", TokenType::STAR},
    {"/", TokenType::SLASH},
    {"%", TokenType::PERCENT},
    {"<", TokenType::LESS},
    {">", TokenType::GREATER},
    {"^", TokenType::CARET},
    {"!", TokenType::BANG},
    {"~", TokenType::TILDE},
    {"&", TokenType::AMPERSAND},
    {"|", TokenType::PIPE},
    {"=", TokenType::EQUAL},
    
    // --- Delimiter'lar ---
    {"[", TokenType::LBRACKET},
    {"]", TokenType::RBRACKET},
    {"(", TokenType::LPAREN},
    {")", TokenType::RPAREN},
    {"{", TokenType::LBRACE},
    {"}", TokenType::RBRACE},
    {";", TokenType::SEMICOLON},
    {",", TokenType::COMMA},
    {":", TokenType::COLON},
    {".", TokenType::DOT},
    {"?", TokenType::TERNARY},
};
static const std::unordered_map<TokenType,std::string_view> OPERATOR_MAP_REV = {
    {TokenType::ARROW,"->"},
    {TokenType::COLON_COLON,"::"},
    {TokenType::EQUAL_EQUAL,"=="},
    {TokenType::BANG_EQUAL,"!="},
    {TokenType::LESS_EQUAL,"<="},
    {TokenType::GREATER_EQUAL,">="},
    {TokenType::AMPERSAND_AMPERSAND,"&&"},
    {TokenType::PIPE_PIPE,"||"},
    {TokenType::PLUS_PLUS,"++"},
    {TokenType::MINUS_MINUS,"--"},
    {TokenType::LSHIFT,"<<"},
    {TokenType::RSHIFT,">>"},
    {TokenType::STAR_STAR,"**"},
    {TokenType::PLUS_EQUAL,"+="},
    {TokenType::MINUS_EQUAL,"-="},
    {TokenType::STAR_EQUAL,"*="},
    {TokenType::SLASH_EQUAL,"/="},
    {TokenType::PERCENT_EQUAL,"%="},
    {TokenType::AMPERSAND_EQUAL,"&="},
    {TokenType::PIPE_EQUAL,"|="},
    {TokenType::CARET_EQUAL,"^="},
    {TokenType::LSHIFT_EQUAL,"<<="},
    {TokenType::RSHIFT_EQUAL,">>="},
    {TokenType::PLUS,"+"},
    {TokenType::MINUS,"-"},
    {TokenType::STAR,"*"},
    {TokenType::SLASH,"/"},
    {TokenType::PERCENT,"%"},
    {TokenType::LESS,"<"},
    {TokenType::GREATER,">"},
    {TokenType::CARET,"^"},
    {TokenType::BANG,"!"},
    {TokenType::TILDE,"~"},
    {TokenType::AMPERSAND,"&"},
    {TokenType::PIPE,"|"},
    {TokenType::EQUAL,"="},
    {TokenType::LBRACKET,"["},
    {TokenType::RBRACKET,"]"},
    {TokenType::LPAREN,"("},
    {TokenType::RPAREN,")"},
    {TokenType::LBRACE,"{"},
    {TokenType::RBRACE,"}"},
    {TokenType::SEMICOLON,";"},
    {TokenType::COMMA,","},
    {TokenType::COLON,":"},
    {TokenType::DOT,"."},
    {TokenType::TERNARY,"?"},
};
static const std::unordered_map<TokenType,std::string_view> OPERATOR_MAP_STRREV = {
    {TokenType::ARROW,"ARROW"},
    {TokenType::COLON_COLON,"COLON_COLON"},
    {TokenType::EQUAL_EQUAL,"EQUAL_EQUAL"},
    {TokenType::BANG_EQUAL,"BANG_EQUAL"},
    {TokenType::LESS_EQUAL,"LESS_EQUAL"},
    {TokenType::GREATER_EQUAL,"GREATER_EQUAL"},
    {TokenType::AMPERSAND_AMPERSAND,"AMPERSAND_AMPERSAND"},
    {TokenType::PIPE_PIPE,"PIPE_PIPE"},
    {TokenType::PLUS_PLUS,"PLUS_PLUS"},
    {TokenType::MINUS_MINUS,"MINUS_MINUS"},
    {TokenType::LSHIFT,"LSHIFT"},
    {TokenType::RSHIFT,"RSHIFT"},
    {TokenType::STAR_STAR,"STAR_STAR"},
    {TokenType::PLUS_EQUAL,"PLUS_EQUAL"},
    {TokenType::MINUS_EQUAL,"MINUS_EQUAL"},
    {TokenType::STAR_EQUAL,"STAR_EQUAL"},
    {TokenType::SLASH_EQUAL,"SLASH_EQUAL"},
    {TokenType::PERCENT_EQUAL,"PERCENT_EQUAL"},
    {TokenType::AMPERSAND_EQUAL,"AMPERSAND_EQUAL"},
    {TokenType::PIPE_EQUAL,"PIPE_EQUAL"},
    {TokenType::CARET_EQUAL,"CARET_EQUAL"},
    {TokenType::LSHIFT_EQUAL,"LSHIFT_EQUAL"},
    {TokenType::RSHIFT_EQUAL,"RSHIFT_EQUAL"},
    {TokenType::PLUS,"PLUS"},
    {TokenType::MINUS,"MINUS"},
    {TokenType::STAR,"STAR"},
    {TokenType::SLASH,"SLASH"},
    {TokenType::PERCENT,"PERCENT"},
    {TokenType::LESS,"LESS"},
    {TokenType::GREATER,"GREATER"},
    {TokenType::CARET,"CARET"},
    {TokenType::BANG,"BANG"},
    {TokenType::TILDE,"TILDE"},
    {TokenType::AMPERSAND,"AMPERSAND"},
    {TokenType::PIPE,"PIPE"},
    {TokenType::EQUAL,"EQUAL"},
    {TokenType::LBRACKET,"LBRACKET"},
    {TokenType::RBRACKET,"RBRACKET"},
    {TokenType::LPAREN,"LPAREN"},
    {TokenType::RPAREN,"RPAREN"},
    {TokenType::LBRACE,"LBRACE"},
    {TokenType::RBRACE,"RBRACE"},
    {TokenType::SEMICOLON,"SEMICOLON"},
    {TokenType::COMMA,"COMMA"},
    {TokenType::COLON,"COLON"},
    {TokenType::DOT,"DOT"},
    {TokenType::TERNARY,"TERNARY"},
};


uint16_t TokenPrecedence(TokenType type) {
    switch (type) {
        // Seviye 17: Gruplama/çağrı
        case TokenType::DOT:
        case TokenType::ARROW:
        case TokenType::LBRACKET:
        case TokenType::LPAREN:
            return 18;
            
        // Seviye 16: Postfix
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS:
            return 17;
            
        // Seviye 15: Unary/Prefix
        case TokenType::BANG:      // !
        case TokenType::TILDE:     // ~
            return 16;
            
        // Seviye 14: Üs alma
        case TokenType::STAR_STAR: // **
        case TokenType::CARET:     // ^
            return 15;
            
        // Seviye 13: Çarpma/bölme
        case TokenType::STAR:      // *
        case TokenType::SLASH:     // /
        case TokenType::PERCENT:   // %
            return 14;
            
        case TokenType::PLUS:      // +
        case TokenType::MINUS:     // -
            return 13;
            
        // Seviye 11: Bitsel kaydırma
        case TokenType::LSHIFT:    // <<
        case TokenType::RSHIFT:    // >>
            return 12;
            
        // Seviye 10: İlişkisel
        case TokenType::LESS:      // <
        case TokenType::LESS_EQUAL:// <=
        case TokenType::GREATER:   // >
        case TokenType::GREATER_EQUAL: // >=
            return 11;
            
        // Seviye 9: Eşitlik
        case TokenType::EQUAL_EQUAL:   // ==
        case TokenType::BANG_EQUAL:    // !=
            return 10;
            
        // Seviye 8: Bitsel VE
        case TokenType::AMPERSAND: // &
            return 9;
            
        // Seviye 7: Bitsel XOR
        // CARET burada binary XOR olarak
            return 8;
            
        // Seviye 6: Bitsel VEYA
        case TokenType::PIPE:      // |
            return 7;
            
        // Seviye 5: Mantıksal VE
        case TokenType::AMPERSAND_AMPERSAND: // &&
            return 6;
            
        // Seviye 4: Mantıksal VEYA
        case TokenType::PIPE_PIPE: // ||
            return 5;
            
        // Seviye 3: Ternary (özel işlem)
        case TokenType::TERNARY:  // ?
            return 4;
        case TokenType::COLON:     // : (ternary için)
            return 3;             // özel değer
            
        // Seviye 2: Atama
        case TokenType::EQUAL:     // =
        case TokenType::PLUS_EQUAL:// +=
        case TokenType::MINUS_EQUAL:// -=
        case TokenType::STAR_EQUAL:// *=
        case TokenType::SLASH_EQUAL:// /=
        case TokenType::PERCENT_EQUAL:// %=
        case TokenType::AMPERSAND_EQUAL:// &=
        case TokenType::PIPE_EQUAL:// |=
        case TokenType::CARET_EQUAL:// ^=
        case TokenType::LSHIFT_EQUAL:// <<=
        case TokenType::RSHIFT_EQUAL:// >>=
            return 2;
            
        // Seviye 1: Virgül
        case TokenType::COMMA:     // ,
            return 1;
            
        default:
            return 0;  // Önceliksiz
    }
}

bool RightAssociative(TokenType type)
{
    switch (type) {
        // Sağdan sola işleyen operatörler:
        case TokenType::STAR_STAR:  // ** (üs - bazı dillerde)
        case TokenType::CARET:      // ^ (üs)
        case TokenType::EQUAL:      // =
        case TokenType::PLUS_EQUAL: // +=
        case TokenType::MINUS_EQUAL:// -=
        case TokenType::STAR_EQUAL: // *=
        case TokenType::SLASH_EQUAL:// /=
        case TokenType::PERCENT_EQUAL:// %=
        case TokenType::AMPERSAND_EQUAL:// &=
        case TokenType::PIPE_EQUAL: // |=
        case TokenType::CARET_EQUAL:// ^=
        case TokenType::LSHIFT_EQUAL:// <<=
        case TokenType::RSHIFT_EQUAL:// >>=
        case TokenType::TERNARY:   // ? (ternary)
            return true;
        // Soldan sağa işleyenler:
        default:
            return false;
    }
}

struct ParserToken
{
    Token token;
    TokenType type;
    bool is(TokenType type){
        return this->type == type;
    }
    bool is(std::initializer_list<TokenType> types){
        for (TokenType t : types) {
            if (this->type == t) {
                return true;
            }
        }
        return false;
    }
    uint16_t getPowerOperator()
    {
        return TokenPrecedence(this->type);
    }
    bool isRightAssociative()
    {
        return RightAssociative(this->type);
    }
};

#endif