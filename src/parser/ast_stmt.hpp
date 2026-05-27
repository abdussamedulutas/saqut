// ============================================================================
// saQut Compiler — AST Deyim Düğümleri
// ============================================================================
//
// DİZİN:   src/parser/ast_stmt.hpp
// İÇERİK:  Block, If, While, For, DoWhile,
//          Return, Break, Continue, ExpressionStatement
//
// ============================================================================

#ifndef SAQUT_AST_STMT
#define SAQUT_AST_STMT

#include <iostream>
#include <sstream>
#include <string>
#include "parser/ast_node.hpp"
#include "parser/ast_json.hpp"
class BlockNode : public ASTNode {
public:
    BlockNode() { kind = ASTKind::Block; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "Block\n";
        for (auto* c : getChildren()) c->log(indent + 2);
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"Block\",\n"
           << in << "  \"children\": [\n"
           << childrenToJson(this, depth + 3)
           << in << "  ]\n"
           << in << "}";
        return ss.str();
    }
};

class IfStatementNode : public ASTNode {
public:
    ASTNode* condition  = nullptr;
    ASTNode* thenBranch = nullptr;
    ASTNode* elseBranch = nullptr;

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

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"IfStatement\"";
        if (condition) {
            ss << ",\n" << in << "  \"condition\":\n"
               << condition->toJson(depth + 2);
        }
        if (thenBranch) {
            ss << ",\n" << in << "  \"then\":\n"
               << thenBranch->toJson(depth + 2);
        }
        if (elseBranch) {
            ss << ",\n" << in << "  \"else\":\n"
               << elseBranch->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
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

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"WhileStatement\"";
        if (condition) {
            ss << ",\n" << in << "  \"condition\":\n"
               << condition->toJson(depth + 2);
        }
        if (body) {
            ss << ",\n" << in << "  \"body\":\n"
               << body->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};

class ForStatementNode : public ASTNode {
public:
    ASTNode* init      = nullptr;
    ASTNode* condition = nullptr;
    ASTNode* update    = nullptr;
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

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"ForStatement\"";
        if (init) {
            ss << ",\n" << in << "  \"init\":\n"
               << init->toJson(depth + 2);
        }
        if (condition) {
            ss << ",\n" << in << "  \"condition\":\n"
               << condition->toJson(depth + 2);
        }
        if (update) {
            ss << ",\n" << in << "  \"update\":\n"
               << update->toJson(depth + 2);
        }
        if (body) {
            ss << ",\n" << in << "  \"body\":\n"
               << body->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
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

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"DoWhileStatement\"";
        if (body) {
            ss << ",\n" << in << "  \"body\":\n"
               << body->toJson(depth + 2);
        }
        if (condition) {
            ss << ",\n" << in << "  \"condition\":\n"
               << condition->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};

// ============================================================================
// ReturnStatementNode — return [ifade]
// ============================================================================

class ReturnStatementNode : public ASTNode {
public:
    ASTNode* value = nullptr;

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

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"ReturnStatement\"";
        if (value) {
            ss << ",\n" << in << "  \"value\":\n"
               << value->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};

// ============================================================================
// BreakStatementNode — break
// ============================================================================

class BreakStatementNode : public ASTNode {
public:
    BreakStatementNode() { kind = ASTKind::BreakStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "BreakStatement\n";
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        return in + "{\n" + in + "  \"kind\": \"BreakStatement\"\n" + in + "}";
    }
};

// ============================================================================
// ContinueStatementNode — continue
// ============================================================================

class ContinueStatementNode : public ASTNode {
public:
    ContinueStatementNode() { kind = ASTKind::ContinueStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "ContinueStatement\n";
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        return in + "{\n" + in + "  \"kind\": \"ContinueStatement\"\n" + in + "}";
    }
};

// ============================================================================
// ExpressionStatementNode — İfadeyi Statement Olarak Sarma
// ============================================================================

class ExpressionStatementNode : public ASTNode {
public:
    ASTNode* expression = nullptr;

    ExpressionStatementNode() { kind = ASTKind::ExpressionStatement; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "ExpressionStatement\n";
        if (expression) expression->log(indent + 2);
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"ExpressionStatement\",\n"
           << in << "  \"location\": " << loc.toJson() << "";
        if (expression) {
            ss << ",\n" << in << "  \"expression\":\n"
               << expression->toJson(depth + 2);
        }
        ss << "\n" << in << "}";
        return ss.str();
    }
};


// ============================================================================
// CallExpressionNode — Fonksiyon Çağrısı f(a, b, ...)
// ============================================================================

#endif // SAQUT_AST_STMT
