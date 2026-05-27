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
    "switch",   "case",     "default",  "break",    "continue",
    "return",   "try",      "catch",    "finally",  "throw",
    "throws",   "assert",
    "void",     "int",      "float",    "double",   "char",
    "string",   "bool",
    "true",     "false",    "null",
    "class",    "struct",   "interface","enum",     "extends",  "implements",
    "new",      "public",   "private",  "protected",
    "static",   "final",    "abstract",
    "import",   "package",
    "const",    "extern",   "typedef",  "sizeof",
    "auto",     "constexpr","noexcept",
    "native",   "synchronized", "volatile", "transient"
};

class Tokenizer {
public:
    Lexer hmx;

    std::vector<Token*> scan(std::string input);

private:
    Token*           scope();
    IdentifierToken* readIdentifier();
    StringToken*     readString();
    void skipOneLineComment();
    void skipMultiLineComment();
};

#endif // SAQUT_TOKENIZER
