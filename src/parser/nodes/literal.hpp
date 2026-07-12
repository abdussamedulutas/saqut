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

class LiteralNode : public ExpressionNode {
public:
    Token*       lexerToken  = nullptr;
    ParserToken  parserToken;

    LiteralType literalType  = LiteralType::INTEGER;
    int         literalBase  = 10;
    bool        isFloatValue = false;

    // Sabit katlama (constant folding) tarafından üretilen sentetik literal.
    // parserToken.token yerine bu değer kullanılır.
    bool hasDirectValue  = false;
    int  directIntValue  = 0;

    LiteralNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

#endif
