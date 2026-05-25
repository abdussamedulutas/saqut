// ============================================================================
// saQut Compiler — Parser Token Tipleri ve Operatör Öncelik Tablosu
// ============================================================================
//
// DİZİN:   src/parser/token.hpp
// KATMAN:  Katman 3 — Tokenizer ile Parser arasında köprü
// BAĞIMLI: Tokenizer (src/tokenizer/tokenizer.hpp)
// KULLANAN: AST (src/parser/ast.hpp), Parser (src/parser/parser.hpp)
//
// AMAÇ:
//   Tokenizer'ın ürettiği ham Token'ları (string tipli) Parser'ın anlayacağı
//   anlamsal tiplere (TokenType enum) dönüştürür. Ayrıca operatör önceliğini
//   (precedence) ve birleşme yönünü (associativity) merkezi olarak tanımlar.
//
//   Bu dosya, Pratt parser'ın "kalbi"dir — tüm operatör önceliği ve birleşme
//   kuralları burada tek bir yerde tanımlanır.
//
// ADR-002: Neden Merkezi Operatör Öncelik Tablosu?
//   Recursive descent parser'larda operatör önceliği, her seviye için ayrı
//   bir fonksiyon yazılarak (parseAddExpr, parseMulExpr, ...) kod tekrarına
//   neden olur. Yeni bir operatör eklemek için yeni fonksiyon + mevcut
//   fonksiyonları değiştirmek gerekir.
//
//   Pratt parser'da tüm öncelik bilgisi TEK BİR TABLODA (TokenPrecedence)
//   toplanır. Yeni operatör eklemek = tabloya bir satır eklemek.
//
// TASARIM KARARLARI:
//   1. TokenType enum: uint16_t tabanlı. Neden? 65K'dan fazla token tipi
//      olmayacak, 2 byte yeterli. Bellek tasarrufu AST'de fark eder.
//
//   2. Üç harita (KEYWORD_MAP, OPERATOR_MAP, OPERATOR_MAP_REV, OPERATOR_MAP_STRREV):
//      - KEYWORD_MAP: "if" → KW_IF, string'den TokenType'a
//      - OPERATOR_MAP: "+" → PLUS, operatör string'inden TokenType'a
//      - OPERATOR_MAP_REV: PLUS → "+", log çıktısı için ters harita
//      - OPERATOR_MAP_STRREV: PLUS → "PLUS", enum ismini string olarak verir
//      Neden dört harita? Çünkü std::unordered_map tek yönlüdür.
//      bidirectional_map kütüphanesi kullanılabilirdi ama bağımlılık istemedik.
//
//   3. TokenPrecedence(): 18 seviyeli öncelik sistemi.
//      C/C++/Java standartlarına uygun. Yüksek sayı = yüksek öncelik.
//      Seviye 18 (en yüksek): üye erişimi (., ->, [], (), ::)
//      Seviye 1 (en düşük):  virgül (,)
//      Seviye 0: önceliksiz (değerler, EOF, vb.)
//
//   4. RightAssociative(): Hangi operatörler sağdan sola birleşir?
//      - Atama (=, +=, vb.)
//      - Üs alma (**, ^) — matematiksel sağ birleşme: a^b^c = a^(b^c)
//      - Ternary (?:)
//      Diğer tüm operatörler soldan sağa birleşir.
//
//   5. ParserToken yapısı:
//      Token* token: Tokenizer'ın ürettiği Token'a pointer. Değer kopyası
//        DEĞİL. Neden pointer? Çünkü Token polimorfik (NumberToken, StringToken,
//        vb.) ve değer kopyası object slicing'e neden olur.
//        BUG FIX (commit 40579ca): Eskiden Token token (değer) vardı.
//      TokenType type: Token'ın anlamsal tipi.
//      is() / getPowerOperator() / isRightAssociative(): kolaylık metotları.
//
// BİLİNEN SINIRLAMALAR (TODO):
//   TODO: Özel operatörler: ?., ??, |>, >>=, vb. (ileride eklenebilir)
//   TODO: Kullanıcı tanımlı operatör önceliği (DSL'ler için)
//   TODO: Token konum bilgisi (satır/sütun) ParserToken'a eklenmeli
//
// ============================================================================

#ifndef SAQUT_PARSER_TOKEN
#define SAQUT_PARSER_TOKEN

#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "tokenizer/tokenizer.hpp"

// ============================================================================
// TokenList — Token Vektörü Tip Kısaltması
// ============================================================================
//
// Tokenizer::scan() tarafından üretilen, Parser::parse() tarafından tüketilen
// token listesi. Ham pointer'lar içerir — bellek yönetimi çağırana aittir.
//
// TODO: std::vector<std::unique_ptr<Token>> ile otomatik bellek yönetimi
//
typedef std::vector<Token*> TokenList;

// ============================================================================
// TokenType — Anlamsal Token Tipleri (Enum)
// ============================================================================
//
// Tokenizer'ın ürettiği string tipli token'ları ("number", "operator", ...)
// Parser'ın anlayacağı anlamsal tiplere dönüştürür.
//
// KATEGORİLER:
//   1. Değerler: IDENTIFIER, NUMBER, STRING, SVR_VOID (geçersiz/EOF)
//   2. Keyword'ler: KW_IF ... KW_NOEXCEPT (alfabetik sıralı)
//   3. Operatörler: Öncelik sırasına göre gruplanmış
//      - Seviye 1:   DOT, ARROW, LBRACKET, RBRACKET, LPAREN, RPAREN
//      - Seviye 2:   PLUS_PLUS, MINUS_MINUS (postfix)
//      - Seviye 3:   PLUS, MINUS, BANG, TILDE (unary prefix)
//      - Seviye 4:   STAR_STAR, CARET (üs)
//      - Seviye 5:   STAR, SLASH, PERCENT (çarpma/bölme)
//      - Seviye 6-16: devamı...
//   4. Diğer: LBRACE, RBRACE, SEMICOLON, COMMA, COLON_COLON
//   5. Özel: END_OF_FILE, UNKNOWN, COMMENT, PREPROCESSOR
//
// NEDEN uint16_t? Bellek optimizasyonu. Her AST düğümü bir TokenType taşır.
// Binlerce düğümde 2 byte vs 4 byte fark eder.
//
enum class TokenType : uint16_t {
    // --- Değerler ve Tanımlayıcılar ---
    IDENTIFIER,      // değişken/fonksiyon ismi
    NUMBER,          // 42, 0xFF, 0b1010, 3.14
    STRING,          // "merhaba"
    SVR_VOID,        // Geçersiz/EOF sinyali (Parser içinde kullanılır)

    // --- Kontrol Akışı Keyword'leri ---
    KW_IF,           // if
    KW_ELSE,         // else
    KW_FOR,          // for
    KW_WHILE,        // while
    KW_DO,           // do
    KW_SWITCH,       // switch
    KW_CASE,         // case
    KW_DEFAULT,      // default
    KW_BREAK,        // break
    KW_CONTINUE,     // continue
    KW_RETURN,       // return

    // --- OOP Keyword'leri ---
    KW_CLASS,        // class
    KW_INTERFACE,    // interface
    KW_ENUM,         // enum
    KW_EXTENDS,      // extends
    KW_IMPLEMENTS,   // implements
    KW_NEW,          // new
    KW_PUBLIC,       // public
    KW_PRIVATE,      // private
    KW_PROTECTED,    // protected
    KW_STATIC,       // static
    KW_FINAL,        // final
    KW_ABSTRACT,     // abstract

    // --- Tip Keyword'leri ---
    KW_VOID,         // void
    KW_BOOL,         // bool
    KW_INT,          // int
    KW_FLOAT_TYPE,   // float  (FLOAT math.h'de tanımlı olabilir, TYPE eki var)
    KW_DOUBLE,       // double
    KW_CHAR,         // char
    KW_STRING_TYPE,  // string

    // --- Literal Keyword'ler ---
    KW_TRUE,         // true
    KW_FALSE,        // false
    KW_NULL,         // null

    // --- İstisna Yönetimi ---
    KW_TRY,          // try
    KW_CATCH,        // catch
    KW_FINALLY,      // finally
    KW_THROW,        // throw
    KW_THROWS,       // throws
    KW_ASSERT,       // assert

    // --- Modül/Paket ---
    KW_IMPORT,       // import
    KW_PACKAGE,      // package

    // --- C/C++ Ekleri ---
    KW_NATIVE,       // native (JNI)
    KW_SYNCHRONIZED, // synchronized (Java)
    KW_VOLATILE,     // volatile
    KW_TRANSIENT,    // transient
    KW_CONST,        // const
    KW_EXTERN,       // extern
    KW_TYPEDEF,      // typedef
    KW_SIZEOF,       // sizeof
    KW_ALIGNOF,      // alignof
    KW_DECLTYPE,     // decltype
    KW_AUTO,         // auto
    KW_CONSTEXPR,    // constexpr
    KW_NOEXCEPT,     // noexcept

    // ================================================================
    // Operatörler — Öncelik sırasına göre gruplanmış
    // ================================================================

    // Seviye 1 (18): Üye erişimi ve çağrı — En yüksek öncelik
    DOT,             // .
    ARROW,           // ->
    LBRACKET,        // [
    RBRACKET,        // ]
    LPAREN,          // (
    RPAREN,          // )

    // Seviye 2 (17): Postfix
    PLUS_PLUS,       // ++ (postfix)
    MINUS_MINUS,     // -- (postfix)

    // Seviye 3 (16): Unary Prefix
    PLUS,            // + (unary)
    MINUS,           // - (unary)
    BANG,            // ! (mantıksal değil)
    TILDE,           // ~ (bitsel değil)

    // Seviye 4 (15): Üs alma
    STAR_STAR,       // ** (Python tarzı üs)
    CARET,           // ^ (bazı dillerde üs)

    // Seviye 5 (14): Çarpma/Bölme
    STAR,            // *
    SLASH,           // /
    PERCENT,         // %

    // Seviye 6 (13): Toplama/Çıkarma — PLUS ve MINUS yukarıda (unary + binary)
    // Seviye 7 (12): Bitsel kaydırma
    LSHIFT,          // <<
    RSHIFT,          // >>

    // Seviye 8 (11): İlişkisel karşılaştırma
    LESS,            // <
    LESS_EQUAL,      // <=
    GREATER,         // >
    GREATER_EQUAL,   // >=

    // Seviye 9 (10): Eşitlik
    EQUAL_EQUAL,     // ==
    BANG_EQUAL,      // !=

    // Seviye 10 (9): Bitsel VE
    AMPERSAND,       // &

    // Seviye 11 (8): Bitsel XOR — CARET yukarıda (üs veya XOR)

    // Seviye 12 (7): Bitsel VEYA
    PIPE,            // |

    // Seviye 13 (6): Mantıksal VE
    AMPERSAND_AMPERSAND, // &&

    // Seviye 14 (5): Mantıksal VEYA
    PIPE_PIPE,       // ||

    // Seviye 15 (4): Üçlü koşul (ternary)
    TERNARY,         // ?
    COLON,           // : (ternary ve etiket için)

    // Seviye 16 (3): Atama
    EQUAL,           // =
    PLUS_EQUAL,      // +=
    MINUS_EQUAL,     // -=
    STAR_EQUAL,      // *=
    SLASH_EQUAL,     // /=
    PERCENT_EQUAL,   // %=
    AMPERSAND_EQUAL, // &=
    PIPE_EQUAL,      // |=
    CARET_EQUAL,     // ^=
    LSHIFT_EQUAL,    // <<=
    RSHIFT_EQUAL,    // >>=

    // --- Diğer Semboller ---
    LBRACE,          // {
    RBRACE,          // }
    SEMICOLON,       // ;
    COMMA,           // ,
    COLON_COLON,     // ::

    // --- Özel ---
    END_OF_FILE,     // Dosya sonu
    UNKNOWN,         // Bilinmeyen karakter
    COMMENT,         // Yorum (// veya /* */) — şu anda token üretilmez
    PREPROCESSOR,    // Önişlemci (#) — şu anda kullanılmıyor
};

// ============================================================================
// KEYWORD_MAP — Keyword String → TokenType
// ============================================================================
//
// Tokenizer'ın ürettiği KeywordToken'ların token değerini (örn: "if")
// Parser'ın anlayacağı TokenType'a (KW_IF) dönüştürür.
//
// std::unordered_map: O(1) ortalama arama. const: derleme zamanı sabiti.
// std::string_view: string kopyalamadan kaçınır.
//
// NOT: Bu harita, Tokenizer'daki keywords[] dizisi ile eşleşmelidir.
//      Birinde ekleme yapılırsa diğerine de eklenmelidir.
//
inline const std::unordered_map<std::string_view, TokenType> KEYWORD_MAP = {
    // --- Control flow ---
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

    // --- OOP ---
    {"class",       TokenType::KW_CLASS},
    {"interface",   TokenType::KW_INTERFACE},
    {"enum",        TokenType::KW_ENUM},
    {"extends",     TokenType::KW_EXTENDS},
    {"implements",  TokenType::KW_IMPLEMENTS},
    {"new",         TokenType::KW_NEW},

    // --- Access modifiers ---
    {"public",      TokenType::KW_PUBLIC},
    {"private",     TokenType::KW_PRIVATE},
    {"protected",   TokenType::KW_PROTECTED},
    {"static",      TokenType::KW_STATIC},
    {"final",       TokenType::KW_FINAL},
    {"abstract",    TokenType::KW_ABSTRACT},

    // --- Types ---
    {"void",        TokenType::KW_VOID},
    {"bool",        TokenType::KW_BOOL},
    {"int",         TokenType::KW_INT},
    {"float",       TokenType::KW_FLOAT_TYPE},
    {"double",      TokenType::KW_DOUBLE},
    {"char",        TokenType::KW_CHAR},
    {"string",      TokenType::KW_STRING_TYPE},

    // --- Literals ---
    {"true",        TokenType::KW_TRUE},
    {"false",       TokenType::KW_FALSE},
    {"null",        TokenType::KW_NULL},

    // --- Exception handling ---
    {"try",         TokenType::KW_TRY},
    {"catch",       TokenType::KW_CATCH},
    {"finally",     TokenType::KW_FINALLY},
    {"throw",       TokenType::KW_THROW},
    {"throws",      TokenType::KW_THROWS},
    {"assert",      TokenType::KW_ASSERT},

    // --- Modules/packages ---
    {"import",      TokenType::KW_IMPORT},
    {"package",     TokenType::KW_PACKAGE},

    // --- C/C++ specific ---
    {"const",       TokenType::KW_CONST},
    {"extern",      TokenType::KW_EXTERN},
    {"typedef",     TokenType::KW_TYPEDEF},
    {"sizeof",      TokenType::KW_SIZEOF},
    {"auto",        TokenType::KW_AUTO},
    {"constexpr",   TokenType::KW_CONSTEXPR},
    {"noexcept",    TokenType::KW_NOEXCEPT},
    {"native",      TokenType::KW_NATIVE},
    {"synchronized",TokenType::KW_SYNCHRONIZED},
    {"volatile",    TokenType::KW_VOLATILE},
    {"transient",   TokenType::KW_TRANSIENT},
};

// ============================================================================
// OPERATOR_MAP — Operatör/Delimiter String → TokenType
// ============================================================================
//
// Tokenizer'ın ürettiği OperatorToken ve DelimiterToken'ları TokenType'a
// dönüştürür. Her iki token tipi de aynı haritayı kullanır çünkü parser
// seviyesinde delimiter'lar da operatör gibi işlenir.
//
// SIRALAMA ÖNEMLİ DEĞİL (unordered_map).
// Ama Tokenizer'daki operators[] ve delimiters[] dizilerindeki sıralama
// önemlidir — çok karakterliler önce gelmelidir.
//
inline const std::unordered_map<std::string_view, TokenType> OPERATOR_MAP = {
    // --- 2 karakterli ---
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

    // --- Birleşik atama ---
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

    // --- 1 karakterli operatörler ---
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

    // --- Delimiter'lar (operatör gibi işlenir) ---
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

// ============================================================================
// OPERATOR_MAP_REV — TokenType → Operatör String (Log için)
// ============================================================================
//
// AST ağacını konsola yazdırırken (log) TokenType enum değerini insan
// tarafından okunabilir operatör sembolüne dönüştürür.
// Örn: TokenType::PLUS → "+"
//
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

// ============================================================================
// OPERATOR_MAP_STRREV — TokenType → Enum İsmi (Log için)
// ============================================================================
//
// AST log çıktısında operatörün enum ismini gösterir.
// Örn: TokenType::PLUS → "PLUS"
//
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

// ============================================================================
// TokenPrecedence — Operatör Öncelik Tablosu
// ============================================================================
//
// Pratt parser'ın kalbi. Her TokenType için bir öncelik seviyesi döndürür.
// Yüksek sayı = daha sıkı bağlanma (daha yüksek öncelik).
//
// ÖNCELİK SEVİYELERİ (yüksekten düşüğe):
//   18: Üye erişimi       . -> [ ] ( )
//   17: Postfix           ++ --
//   16: Unary prefix      ! ~
//   15: Üs alma           ** ^
//   14: Çarpma/Bölme      * / %
//   13: Toplama/Çıkarma   + -
//   12: Bitsel kaydırma   << >>
//   11: İlişkisel         < <= > >=
//   10: Eşitlik           == !=
//    9: Bitsel VE         &
//    8: Bitsel XOR        ^  (üs olarak 15'te de var — bağlama göre)
//    7: Bitsel VEYA       |
//    6: Mantıksal VE      &&
//    5: Mantıksal VEYA    ||
//    4: Ternary           ?
//    3: Ternary else      :
//    2: Atama             = += -= vb.
//    1: Virgül            ,
//    0: Önceliksiz        (değerler, EOF, bilinmeyen)
//
// NOT: C/C++'da ^ operatörü bitsel XOR'tur (seviye 8), ama Python'da üs (seviye 15).
//      saQut'ta ^ hem üs hem XOR olarak kullanılabilir (AST'de bağlam belirler).
//      Şimdilik ^ seviye 15 (üs) olarak ayarlı.
//
// BUG FIX (commit 438bc0e): Seviye 8'deki ölü kod (CARET için case olmadan
//   return 8) temizlendi. CARET zaten seviye 15'te STAR_STAR ile birlikte
//   işleniyor.
//
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

        // Level 16: Unary prefix — sadece her zaman prefix olanlar
        case TokenType::BANG:      // !
        case TokenType::TILDE:     // ~
            return 16;

        // Level 15: Exponentiation
        case TokenType::STAR_STAR: // **
        case TokenType::CARET:     // ^ (Python tarzı üs)
            return 15;

        // Level 14: Multiplicative
        case TokenType::STAR:      // *
        case TokenType::SLASH:     // /
        case TokenType::PERCENT:   // %
            return 14;

        // Level 13: Additive — PLUS ve MINUS hem unary hem binary
        case TokenType::PLUS:      // +
        case TokenType::MINUS:     // -
            return 13;

        // Level 12: Bit shift
        case TokenType::LSHIFT:    // <<
        case TokenType::RSHIFT:    // >>
            return 12;

        // Level 11: Relational
        case TokenType::LESS:      // <
        case TokenType::LESS_EQUAL:// <=
        case TokenType::GREATER:   // >
        case TokenType::GREATER_EQUAL: // >=
            return 11;

        // Level 10: Equality
        case TokenType::EQUAL_EQUAL:   // ==
        case TokenType::BANG_EQUAL:    // !=
            return 10;

        // Level 9: Bitwise AND
        case TokenType::AMPERSAND: // &
            return 9;

        // Level 8: Bitwise XOR — şu anda CARET seviye 15'te (üs)
        // Level 7: Bitwise OR
        case TokenType::PIPE:      // |
            return 7;

        // Level 6: Logical AND
        case TokenType::AMPERSAND_AMPERSAND: // &&
            return 6;

        // Level 5: Logical OR
        case TokenType::PIPE_PIPE: // ||
            return 5;

        // Level 4: Ternary
        case TokenType::TERNARY:  // ?
            return 4;
        case TokenType::COLON:     // : (ternary için)
            return 3;             // ternary'den düşük, atamadan yüksek

        // Level 2: Assignment
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

        // Level 1: Comma
        case TokenType::COMMA:     // ,
            return 1;

        default:
            return 0;  // Önceliksiz: değerler, EOF, bilinmeyen
    }
}

// ============================================================================
// RightAssociative — Sağdan Sola Birleşme Kontrolü
// ============================================================================
//
// Hangi operatörler sağdan sola birleşir?
//
// Sağ birleşmeli operatörler (a OP b OP c = a OP (b OP c)):
//   - Üs alma: **, ^ (matematiksel: 2^3^2 = 2^(3^2) = 2^9 = 512)
//   - Atama: =, +=, -=, vb. (a = b = 5 → a = (b = 5))
//   - Ternary: ?: (a ? b : c ? d : e → a ? b : (c ? d : e))
//
// Sol birleşmeli operatörler (a OP b OP c = (a OP b) OP c):
//   - Tüm diğerleri: +, -, *, /, ==, &&, vb.
//
inline bool RightAssociative(TokenType type) {
    switch (type) {
        case TokenType::STAR_STAR:  // ** (üs)
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
        default:
            return false;
    }
}

// ============================================================================
// ParserToken — Parser'ın Kullandığı Token Yapısı
// ============================================================================
//
// Tokenizer'ın ürettiği ham Token ile Parser'ın ihtiyaç duyduğu anlamsal
// tipi (TokenType) bir arada tutar.
//
// ALANLAR:
//   token (Token*): Tokenizer'dan gelen orijinal token. Neden pointer?
//     Çünkü Token polimorfik bir sınıf hiyerarşisidir. Değer kopyası (Token)
//     object slicing'e neden olur — alt sınıf verileri (NumberToken.isFloat,
//     StringToken.context) kaybolur.
//     BUG FIX (commit 40579ca): Eskiden Token token (değer) tutuyordu.
//
//   type (TokenType): Token'ın anlamsal tipi. Örn: NUMBER, PLUS, KW_IF.
//
// METOTLAR:
//   is(TokenType): Bu token belirtilen tipte mi?
//   is({...}): Bu token listedeki tiplerden biri mi?
//   getPowerOperator(): Bu token bir operatör ise önceliğini döndür.
//   isRightAssociative(): Bu operatör sağ birleşmeli mi?
//
struct ParserToken {
    Token*    token = nullptr;               // Tokenizer'dan gelen orijinal token
    TokenType type  = TokenType::SVR_VOID;   // Anlamsal tip

    // Tek tip kontrolü
    bool is(TokenType t) const {
        return type == t;
    }

    // Çoklu tip kontrolü — örn: is({KW_INT, KW_FLOAT, KW_VOID})
    bool is(std::initializer_list<TokenType> types) const {
        for (TokenType t : types)
            if (type == t) return true;
        return false;
    }

    // Operatör önceliği (Pratt parser için)
    uint16_t getPowerOperator() const {
        return TokenPrecedence(type);
    }

    // Sağ birleşmeli mi?
    bool isRightAssociative() const {
        return RightAssociative(type);
    }
};

#endif // SAQUT_PARSER_TOKEN
