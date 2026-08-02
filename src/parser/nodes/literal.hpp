// ============================================================================
// saQut Compiler — LiteralNode (Sabit Değer Düğümü)
// ============================================================================
//
// DİZİN:   src/parser/nodes/literal.hpp
// KATMAN:  Katman 3 — Sayı, string, bool, null sabitleri
//
// AMAÇ:
//   Kaynak koddaki sabit değerleri temsil eder: tamsayı, float, string,
//   boolean ve null. LiteralType ile değerin hangi türde olduğu belirtilir.
//
// ============================================================================

#ifndef SAQUT_AST_LITERAL
#define SAQUT_AST_LITERAL

#include "parser/ast_node.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

static_assert(sizeof(long long) == sizeof(std::int64_t),
              "SaQut integer literal bridge requires a 64-bit long long");

inline std::uint64_t parseUnsignedIntegerLiteral(const std::string& raw, int base) {
    std::string s = raw;
    if (s.size() >= 2 && s[0] == '0') {
        char p = s[1];
        if (base == 16 && (p == 'x' || p == 'X'))
            s = s.substr(2);
        else if (base == 2 && (p == 'b' || p == 'B'))
            s = s.substr(2);
    }
    if (s.empty())
        s = "0";

    std::size_t pos = 0;
    const unsigned long long value = std::stoull(s, &pos, base);
    if (pos != s.size())
        throw std::invalid_argument("invalid integer literal");
    return static_cast<std::uint64_t>(value);
}

// Hex/binary literals are bit patterns when they are used as longint values.
// Keep this conversion explicit instead of relying on implementation-defined
// unsigned-to-signed conversion.
inline std::int64_t signedIntegerBits(std::uint64_t bits) {
    constexpr std::uint64_t signBit = UINT64_C(0x8000000000000000);
    if ((bits & signBit) == 0)
        return static_cast<std::int64_t>(bits);

    // -1 - (~bits) is the two's-complement signed interpretation of bits.
    return -1 - static_cast<std::int64_t>(~bits);
}

inline long long parseIntegerLiteral(const std::string& raw, int base) {
    if (base == 16 || base == 2)
        return static_cast<long long>(signedIntegerBits(parseUnsignedIntegerLiteral(raw, base)));

    std::string s = raw;
    if (s.size() >= 2 && s[0] == '0') {
        char p = s[1];
        if (base == 16 && (p == 'x' || p == 'X'))
            s = s.substr(2);
        else if (base == 2 && (p == 'b' || p == 'B'))
            s = s.substr(2);
    }
    if (s.empty())
        s = "0";
    size_t pos = 0;
    return std::stoll(s, &pos, base);
}

class LiteralNode : public ExpressionNode {
public:
    Token* lexerToken = nullptr;
    ParserToken parserToken;

    LiteralType literalType = LiteralType::INTEGER;
    int literalBase = 10;
    bool isFloatValue = false;

    // Sabit katlama (constant folding) tarafından üretilen sentetik literal.
    // parserToken.token yerine bu değer kullanılır.
    bool hasDirectValue = false;
    int directIntValue = 0;

    LiteralNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

#endif
