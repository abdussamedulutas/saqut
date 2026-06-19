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
