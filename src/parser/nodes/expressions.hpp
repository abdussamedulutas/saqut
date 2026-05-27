#ifndef SAQUT_AST_EXPR_EXT
#define SAQUT_AST_EXPR_EXT

#include "parser/ast_node.hpp"

class PostfixNode : public ASTNode {
public:
    ASTNode*  operand  = nullptr;
    TokenType Operator;
    PostfixNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class CallExpressionNode : public ASTNode {
public:
    ASTNode* callee = nullptr;
    std::vector<ASTNode*> arguments;
    CallExpressionNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class MemberAccessNode : public ASTNode {
public:
    ASTNode*   object = nullptr;
    std::string member;
    bool       arrow = false;
    MemberAccessNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class IndexExpressionNode : public ASTNode {
public:
    ASTNode* object = nullptr;
    ASTNode* index  = nullptr;
    IndexExpressionNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

#endif
