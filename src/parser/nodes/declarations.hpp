#ifndef SAQUT_AST_DECL
#define SAQUT_AST_DECL

#include "parser/ast_node.hpp"

class VariableDeclNode; // fwd — FunctionDeclNode::params için

class FunctionDeclNode : public ASTNode {
public:
    std::string name;
    std::string returnType;
    std::vector<VariableDeclNode*> params; // TODO(faz2): parametreler
    FunctionDeclNode();
    ~FunctionDeclNode() override;
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

class VariableDeclNode : public StatementNode {
public:
    std::string varType;
    std::string name;
    ASTNode*   initExpr = nullptr;
    VariableDeclNode();
    ~VariableDeclNode() override { delete initExpr; }
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
