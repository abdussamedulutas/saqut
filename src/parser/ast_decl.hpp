// ============================================================================
// saQut Compiler — AST Deklarasyon Düğümleri
// ============================================================================
//
// DİZİN:   src/parser/ast_decl.hpp
// İÇERİK:  ProgramNode, FunctionDeclNode, VariableDeclNode, StructDeclNode
//
// ============================================================================

#ifndef SAQUT_AST_DECL
#define SAQUT_AST_DECL

#include <iostream>
#include <sstream>
#include <string>
#include "parser/ast_node.hpp"
#include "parser/ast_json.hpp"
class ProgramNode : public ASTNode {
public:
    ProgramNode() { kind = ASTKind::Program; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "Program\n";
        for (auto* c : getChildren()) c->log(indent + 2);
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"Program\",\n"
           << in << "  \"children\": [\n"
           << childrenToJson(this, depth + 3)
           << in << "  ]\n"
           << in << "}";
        return ss.str();
    }
};

class FunctionDeclNode : public ASTNode {
public:
    std::string name;
    std::string returnType;

    FunctionDeclNode() { kind = ASTKind::FunctionDecl; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent)
                  << "FunctionDecl " << returnType << " " << name << "()\n";
        for (auto* c : getChildren()) c->log(indent + 2);
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"FunctionDecl\",\n"
           << in << "  \"name\": \"" << jsonEscape(name) << "\",\n"
           << in << "  \"returnType\": \"" << jsonEscape(returnType) << "\",\n"
           << in << "  \"location\": " << loc.toJson() << ",\n"
           << in << "  \"children\": [\n"
           << childrenToJson(this, depth + 3)
           << in << "  ]\n"
           << in << "}";
        return ss.str();
    }
};

class VariableDeclNode : public ASTNode {
public:
    std::string varType;
    std::string name;
    ASTNode*   initExpr = nullptr;

    VariableDeclNode() { kind = ASTKind::VariableDecl; }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"VariableDecl\",\n"
           << in << "  \"name\": \"" << jsonEscape(name) << "\",\n"
           << in << "  \"varType\": \"" << jsonEscape(varType) << "\",\n"
           << in << "  \"location\": " << loc.toJson() << "";
        if (initExpr) {
            ss << ",\n" << in << "  \"initExpr\":\n"
               << initExpr->toJson(depth + 2);
        }
        // Çoklu değişken bildirimindeki kardeşler (int a, b, c;)
        if (!getChildren().empty()) {
            ss << ",\n" << in << "  \"declarators\": [\n";
            for (size_t i = 0; i < getChildren().size(); i++) {
                ss << ((VariableDeclNode*)getChildren()[i])->toJson(depth + 2);
                if (i + 1 < getChildren().size()) ss << ",";
                ss << "\n";
            }
            ss << in << "  ]";
        }
        ss << "\n" << in << "}";
        return ss.str();
    }

    void log(int indent = 0) override {
        std::cout << padRight("", indent)
                  << "VariableDecl " << varType << " " << name;
        if (initExpr) {
            std::cout << " =\n";
            initExpr->log(indent + 4);
        } else {
            std::cout << "\n";
        }
        // Kardeş değişkenleri de logla
        for (auto* child : getChildren()) {
            child->log(indent);
        }
    }
};

class StructDeclNode : public ASTNode {
public:
    std::string name;

    StructDeclNode() { kind = ASTKind::StructDecl; }

    void log(int indent = 0) override {
        std::cout << padRight("", indent) << "StructDecl " << name << "\n";
        for (auto* c : getChildren()) c->log(indent + 2);
    }

    std::string toJson(int depth = 0) override {
        std::string in = jsonIndent(depth);
        std::ostringstream ss;
        ss << in << "{\n"
           << in << "  \"kind\": \"StructDecl\",\n"
           << in << "  \"name\": \"" << jsonEscape(name) << "\",\n"
           << in << "  \"children\": [\n"
           << childrenToJson(this, depth + 3)
           << in << "  ]\n"
           << in << "}";
        return ss.str();
    }
};
#endif // SAQUT_AST_DECL
