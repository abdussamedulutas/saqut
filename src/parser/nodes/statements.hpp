#ifndef SAQUT_AST_STMT
#define SAQUT_AST_STMT

#include "parser/ast_node.hpp"

class BlockNode : public StatementNode {
public:
    BlockNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class IfStatementNode : public StatementNode {
public:
    ASTNode* condition  = nullptr;
    ASTNode* thenBranch = nullptr;
    ASTNode* elseBranch = nullptr;
    IfStatementNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class WhileStatementNode : public StatementNode {
public:
    ASTNode* condition = nullptr;
    ASTNode* body      = nullptr;
    WhileStatementNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class ForStatementNode : public StatementNode {
public:
    ASTNode* init      = nullptr;
    ASTNode* condition = nullptr;
    ASTNode* update    = nullptr;
    ASTNode* body      = nullptr;
    ForStatementNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class DoWhileStatementNode : public StatementNode {
public:
    ASTNode* condition = nullptr;
    ASTNode* body      = nullptr;
    DoWhileStatementNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class ReturnStatementNode : public StatementNode {
public:
    ASTNode* value = nullptr;
    ReturnStatementNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class BreakStatementNode : public StatementNode {
public:
    BreakStatementNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class ContinueStatementNode : public StatementNode {
public:
    ContinueStatementNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class ExpressionStatementNode : public StatementNode {
public:
    ASTNode* expression = nullptr;
    ExpressionStatementNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

#endif
