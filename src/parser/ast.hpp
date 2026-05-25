#ifndef SAQUT_AST
#define SAQUT_AST

#include <iostream>
#include <vector>
#include "parser/token.hpp"
#include "tools.hpp"

// ============================================================
// AST Node types
// ============================================================

enum class ASTKind {
    Program,
    FunctionDecl,
    Block,
    VariableDecl,
    BinaryExpression,
    UnaryExpression,
    Literal,
    Identifier,
    Postfix,
    IfStatement,
    ForStatement,
    WhileStatement,
    DoWhileStatement,
    ReturnStatement,
    BreakStatement,
    ContinueStatement,
    ExpressionStatement,
};

// ============================================================
// Base AST Node
// ============================================================

class ASTNode {
public:
    ASTKind kind;
    ASTNode* parent = nullptr;

    virtual void log(int indent = 0) {
        std::cout << "<Unknown>\n";
    }

    void addChild(ASTNode* child) {
        children.push_back(child);
        child->parent = this;
    }

    std::vector<ASTNode*>& getChildren() { return children; }

    virtual ~ASTNode() = default;

protected:
    std::vector<ASTNode*> children;
};

// ============================================================
// Program (root)
// ============================================================

class ProgramNode : public ASTNode {
public:
    ProgramNode() { kind = ASTKind::Program; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "Program\n";
        for (auto* c : getChildren())
            c->log(indent + 2);
    }
};

// ============================================================
// Function declaration
// ============================================================

class FunctionDeclNode : public ASTNode {
public:
    std::string name;
    std::string returnType;

    FunctionDeclNode() { kind = ASTKind::FunctionDecl; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent)
                  << "FunctionDecl " << returnType << " " << name << "()\n";
        for (auto* c : getChildren())
            c->log(indent + 2);
    }
};

// ============================================================
// Block { ... }
// ============================================================

class BlockNode : public ASTNode {
public:
    BlockNode() { kind = ASTKind::Block; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "Block\n";
        for (auto* c : getChildren())
            c->log(indent + 2);
    }
};

// ============================================================
// Variable declaration: type name [= expr]
// ============================================================

class VariableDeclNode : public ASTNode {
public:
    std::string varType;
    std::string name;
    ASTNode*   initExpr = nullptr;

    VariableDeclNode() { kind = ASTKind::VariableDecl; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent)
                  << "VariableDecl " << varType << " " << name;
        if (initExpr) {
            std::cout << " =\n";
            initExpr->log(indent + 4);
        } else {
            std::cout << "\n";
        }
    }
};

// ============================================================
// Expression nodes
// ============================================================

class BinaryExpressionNode : public ASTNode {
public:
    TokenType Operator;
    ASTNode*  Left  = nullptr;
    ASTNode*  Right = nullptr;

    BinaryExpressionNode() { kind = ASTKind::BinaryExpression; }

    void log(int indent = 0) override {
        auto it = OPERATOR_MAP_STRREV.find(Operator);
        std::string sym = (it != OPERATOR_MAP_STRREV.end()) ? std::string(it->second) : "?";
        std::string val;
        auto it2 = OPERATOR_MAP_REV.find(Operator);
        if (it2 != OPERATOR_MAP_REV.end()) val = std::string(it2->second);

        std::cout << padRight("", indent) << "BinaryExpr " << sym
                  << " (" << val << ")\n";
        if (Right) Right->log(indent + 2);
        if (Left)  Left->log(indent + 2);
    }
};

class LiteralNode : public ASTNode {
public:
    Token*       lexerToken  = nullptr;
    ParserToken  parserToken;

    LiteralNode() { kind = ASTKind::Literal; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent)
                  << "Literal {" << parserToken.token->token << "}\n";
    }
};

class IdentifierNode : public ASTNode {
public:
    Token*       lexerToken  = nullptr;
    ParserToken  parserToken;

    IdentifierNode() { kind = ASTKind::Identifier; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent)
                  << "Identifier {" << parserToken.token->token << "}\n";
    }
};

class PostfixNode : public ASTNode {
public:
    ASTNode*  operand  = nullptr;
    TokenType Operator;

    PostfixNode() { kind = ASTKind::Postfix; }

    void log(int indent = 0) override {
        auto it = OPERATOR_MAP_STRREV.find(Operator);
        std::string sym = (it != OPERATOR_MAP_STRREV.end()) ? std::string(it->second) : "?";

        std::cout << padRight("", indent) << "Postfix " << sym;
        auto it2 = OPERATOR_MAP_REV.find(Operator);
        if (it2 != OPERATOR_MAP_REV.end())
            std::cout << " (" << it2->second << ")";
        std::cout << "\n";
        if (operand) operand->log(indent + 2);
    }
};

// ============================================================
// Statement nodes
// ============================================================

class IfStatementNode : public ASTNode {
public:
    ASTNode* condition = nullptr;
    ASTNode* thenBranch = nullptr;   // BlockNode or single statement
    ASTNode* elseBranch = nullptr;   // optional

    IfStatementNode() { kind = ASTKind::IfStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "IfStatement\n";
        std::cout << padRight("", indent + 2) << "Condition:\n";
        if (condition) condition->log(indent + 4);
        std::cout << padRight("", indent + 2) << "Then:\n";
        if (thenBranch) thenBranch->log(indent + 4);
        if (elseBranch) {
            std::cout << padRight("", indent + 2) << "Else:\n";
            elseBranch->log(indent + 4);
        }
    }
};

class WhileStatementNode : public ASTNode {
public:
    ASTNode* condition = nullptr;
    ASTNode* body      = nullptr;

    WhileStatementNode() { kind = ASTKind::WhileStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "WhileStatement\n";
        std::cout << padRight("", indent + 2) << "Condition:\n";
        if (condition) condition->log(indent + 4);
        std::cout << padRight("", indent + 2) << "Body:\n";
        if (body) body->log(indent + 4);
    }
};

class ForStatementNode : public ASTNode {
public:
    ASTNode* init      = nullptr;  // VariableDecl or ExpressionStatement
    ASTNode* condition = nullptr;  // expression
    ASTNode* update    = nullptr;  // expression
    ASTNode* body      = nullptr;

    ForStatementNode() { kind = ASTKind::ForStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "ForStatement\n";
        if (init) {
            std::cout << padRight("", indent + 2) << "Init:\n";
            init->log(indent + 4);
        }
        if (condition) {
            std::cout << padRight("", indent + 2) << "Condition:\n";
            condition->log(indent + 4);
        }
        if (update) {
            std::cout << padRight("", indent + 2) << "Update:\n";
            update->log(indent + 4);
        }
        std::cout << padRight("", indent + 2) << "Body:\n";
        if (body) body->log(indent + 4);
    }
};

class DoWhileStatementNode : public ASTNode {
public:
    ASTNode* condition = nullptr;
    ASTNode* body      = nullptr;

    DoWhileStatementNode() { kind = ASTKind::DoWhileStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "DoWhileStatement\n";
        std::cout << padRight("", indent + 2) << "Body:\n";
        if (body) body->log(indent + 4);
        std::cout << padRight("", indent + 2) << "Condition:\n";
        if (condition) condition->log(indent + 4);
    }
};

class ReturnStatementNode : public ASTNode {
public:
    ASTNode* value = nullptr;  // optional

    ReturnStatementNode() { kind = ASTKind::ReturnStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "ReturnStatement";
        if (value) {
            std::cout << "\n";
            value->log(indent + 2);
        } else {
            std::cout << " (void)\n";
        }
    }
};

class BreakStatementNode : public ASTNode {
public:
    BreakStatementNode() { kind = ASTKind::BreakStatement; }
    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "BreakStatement\n";
    }
};

class ContinueStatementNode : public ASTNode {
public:
    ContinueStatementNode() { kind = ASTKind::ContinueStatement; }
    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "ContinueStatement\n";
    }
};

class ExpressionStatementNode : public ASTNode {
public:
    ASTNode* expression = nullptr;

    ExpressionStatementNode() { kind = ASTKind::ExpressionStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "ExpressionStatement\n";
        if (expression) expression->log(indent + 2);
    }
};

#endif
