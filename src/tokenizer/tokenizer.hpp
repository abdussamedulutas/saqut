// ============================================================================
// saQut Compiler — Sözcüksel Analiz Motoru (Tokenizer)
// ============================================================================
//
// DİZİN:   src/tokenizer/tokenizer.hpp
// KATMAN:  Katman 2 — Lexer'dan gelen karakter akışını token dizisine dönüştürür
// BAĞIMLI: lexer/lexer.hpp, tokenizer/token.hpp
//
// AMAÇ:
//   Kaynak koddaki karakterleri tanıyarak Parser'ın tüketeceği token dizisini
//   üretir. Operatör, delimiter ve keyword tabloları burada tanımlanır.
//   Lexer'ı composition (hmx) olarak barındırır; karakter düzeyindeki tüm
//   işlemler Lexer üzerinden yapılır.
//
// ============================================================================

#ifndef SAQUT_TOKENIZER
#define SAQUT_TOKENIZER

#include <string>
#include <vector>
#include <string_view>
#include "lexer/lexer.hpp"
#include "tokenizer/token.hpp"

// Operatör tablosu. Çok karakterliler (==, !=, ++, +=, vb.) önce gelir.
inline constexpr std::string_view operators[] = {
    "==", "!=", "<=", ">=", "&&", "||",
    "++", "--", "<<", ">>",
    "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=",
    "+",  "-",  "*",  "/",  "%",  "<",  ">",
    "^",  "!",  "~",  "&",  "|",
    "="
};

// Delimiter tablosu. Çok karakterliler (->, ::) önce gelir.
inline constexpr std::string_view delimiters[] = {
    "->", "::",
    "[",  "]",  "(",  ")",  "{",  "}",
    ";",  ",",  ":",
    "."
};

// Keyword tablosu.
inline constexpr std::string_view keywords[] = {
    "if",       "else",     "for",      "while",    "do",
    "as",
    "switch",   "case",     "default",  "break",    "continue",
    "return",   "try",      "catch",    "finally",  "throw",
    "throws",   "assert",
    "void",     "int",      "float",    "double",   "char",
    "string",   "bool",     "decimal",  "byte",
    "true",     "false",    "null",
    "class",    "struct",   "interface","enum",     "extends",  "implements",
    "new",      "public",   "private",  "protected",
    "static",   "final",    "abstract",
    "import",   "package",
    "const",    "extern",   "ffi",      "typedef",  "sizeof",
    "auto",     "constexpr","noexcept",
    "native",   "synchronized", "volatile", "transient"
};

class Tokenizer {
public:
    Lexer hmx;

    std::vector<Token*> scan(std::string input, std::string filePath = "");

private:
    Token*           scope();
    IdentifierToken* readIdentifier();
    StringToken*     readString();
    void skipOneLineComment();
    void skipMultiLineComment();
};

#endif // SAQUT_TOKENIZER
