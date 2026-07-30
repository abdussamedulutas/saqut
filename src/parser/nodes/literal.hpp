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

#include <string>

inline long long parseIntegerLiteral(const std::string& raw, int base) {
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
