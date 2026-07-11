// ============================================================================
// saQut Compiler — BinaryExpressionNode (İkili İşlem Düğümü)
// ============================================================================
//
// DİZİN:   src/parser/nodes/binary_expr.hpp
// KATMAN:  Katman 3 — İkili operatör ifadeleri
//
// AMAÇ:
//   a + b, a == b, a && b gibi iki operandlı işlemleri temsil eder.
//   Operator alanı TokenType ile hangi işlem olduğunu belirtir.
//
// ============================================================================

#ifndef SAQUT_AST_BINARY_EXPR
#define SAQUT_AST_BINARY_EXPR

#include "parser/ast_node.hpp"

class BinaryExpressionNode : public ExpressionNode {
public:
    TokenType Operator;
    ASTNode*  Left  = nullptr;
    ASTNode*  Right = nullptr;

    BinaryExpressionNode();
    ~BinaryExpressionNode() override { delete Left; delete Right; }
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

#endif
