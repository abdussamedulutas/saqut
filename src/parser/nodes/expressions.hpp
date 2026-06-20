#ifndef SAQUT_AST_EXPR_EXT
#define SAQUT_AST_EXPR_EXT

#include "parser/ast_node.hpp"

class PostfixNode : public ExpressionNode {
public:
    ASTNode*  operand  = nullptr;
    TokenType Operator;
    PostfixNode();
    ~PostfixNode() override { delete operand; }
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class CallExpressionNode : public ExpressionNode {
public:
    ASTNode* callee = nullptr;
    std::vector<ASTNode*> arguments;
    CallExpressionNode();
    ~CallExpressionNode() override { delete callee; for (auto* a : arguments) delete a; }
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class MemberAccessNode : public ExpressionNode {
public:
    ASTNode*   object = nullptr;
    std::string member;
    bool       arrow = false;
    MemberAccessNode();
    ~MemberAccessNode() override { delete object; }
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class IndexExpressionNode : public ExpressionNode {
public:
    ASTNode* object = nullptr;
    ASTNode* index  = nullptr;
    IndexExpressionNode();
    ~IndexExpressionNode() override { delete object; delete index; }
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class ArrayLiteralNode : public ExpressionNode {
public:
    std::vector<ASTNode*> elements;
    ArrayLiteralNode();
    ~ArrayLiteralNode() override { for (auto* e : elements) delete e; }
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

// ADR-026: expr as TargetType[?]
class CastExpressionNode : public ExpressionNode {
public:
    ASTNode*    operand        = nullptr;
    std::string targetTypeName;     // "int", "float", "bool", "string"
    bool        targetNullable = false; // as int? → true
    CastExpressionNode();
    ~CastExpressionNode() override { delete operand; }
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

#endif
