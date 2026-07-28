// ============================================================================
// saQut CLI — tokens komutu
// ============================================================================

#ifndef SAQUT_CLI_TOKENS
#define SAQUT_CLI_TOKENS

#include <cstdio>
#include <iostream>
#include "cli/args.hpp"
#include "tokenizer/tokenizer.hpp"

// Token kaydı text sözleşmesi: bir token = bir fiziksel satır. Tokenizer,
// string literal escape dizilerini (\n \t \r \b \\ \") tarama sırasında
// gerçek kontrol byte'larına çözer (bkz. tokenizer.cpp ~292) — yani
// örneğin "a\nb" kaynağı için Token::token alanı gerçek bir 0x0A byte'ı
// taşır. std::quoted yalnız `"` ve `\` kaçışını ele alır, kontrol
// karakterlerini DEĞİŞTİRMEZ; bu yüzden çok satırlı bir string/identifier
// token'ı önceden birden fazla fiziksel satıra bölünüyordu — tek-satır
// sözleşmesini kırıyordu. Bu fonksiyon kontrol karakterlerini açık,
// tersine çevrilebilir escape dizilerine çevirir ve tırnak içine alır.
inline std::string quoteTokenField(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\x%02x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
    return out;
}

inline int cmdTokens(const CliArgs& args) {
    std::string source = readSource(args);
    if (source.empty()) return 1;

    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source, inputFilePath(args));

    std::cout << "Tokenler (" << tokens.size() << " adet):\n";
    for (auto* t : tokens) {
        // SourceLocation::column is derived from the UTF-8 source byte
        // offset; it is not a Unicode code-point or visual-terminal column.
        const int byteLength = t->end - t->start;
        std::cout << "  [" << t->gettype() << "] " << quoteTokenField(t->token)
                  << " file=" << quoteTokenField(inputFilePath(args))
                  << " byteOffset=" << t->start
                  << " byteLength=" << byteLength
                  << " line=" << t->loc.line
                  << " column=" << t->loc.column << "\n";
    }

    for (auto* t : tokens) delete t;
    return 0;
}

#endif // SAQUT_CLI_TOKENS
