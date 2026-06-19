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
    ~IfStatementNode() override { delete condition; delete thenBranch; delete elseBranch; }
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class WhileStatementNode : public StatementNode {
public:
    ASTNode* condition = nullptr;
    ASTNode* body      = nullptr;
    WhileStatementNode();
    ~WhileStatementNode() override { delete condition; delete body; }
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
    ~ForStatementNode() override { delete init; delete condition; delete update; delete body; }
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class DoWhileStatementNode : public StatementNode {
public:
    ASTNode* condition = nullptr;
    ASTNode* body      = nullptr;
    DoWhileStatementNode();
    ~DoWhileStatementNode() override { delete body; delete condition; }
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class ReturnStatementNode : public StatementNode {
public:
    ASTNode* value = nullptr;
    ReturnStatementNode();
    ~ReturnStatementNode() override { delete value; }
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
    ~ExpressionStatementNode() override { delete expression; }
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

#endif
