#ifndef SAQUT_AST_DECL
#define SAQUT_AST_DECL

#include "parser/ast_node.hpp"

class FunctionDeclNode : public ASTNode {
public:
    std::string name;
    std::string returnType;
    FunctionDeclNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class VariableDeclNode : public StatementNode {
public:
    std::string varType;
    std::string name;
    ASTNode*   initExpr = nullptr;
    VariableDeclNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class StructDeclNode : public ASTNode {
public:
    std::string name;
    StructDeclNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

#endif
