// ============================================================================
// saQut Compiler — Bildirim Düğümleri
// ============================================================================
//
// DİZİN:   src/parser/nodes/declarations.hpp
// KATMAN:  Katman 3 — FunctionDecl, StructDecl, EnumDecl, VariableDecl, ImportDecl
//
// AMAÇ:
//   Dilin bildirim yapılarını temsil eden AST düğümleri. Her biri bir
//   program öğesini (fonksiyon, struct, enum, değişken, import) tanımlar.
//
// ============================================================================

#ifndef SAQUT_AST_DECL
#define SAQUT_AST_DECL

#include "parser/ast_node.hpp"

class VariableDeclNode; // fwd — FunctionDeclNode::params için

class FunctionDeclNode : public ASTNode {
public:
    std::string name;
    std::string returnType;
    std::vector<VariableDeclNode*> params;
    bool isExported = false;
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
    bool isExported = false;
    StructDeclNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

struct EnumMember {
    std::string name;
    int         value = 0;
};

class EnumDeclNode : public ASTNode {
public:
    std::string              name;
    std::vector<EnumMember>  members;
    bool isExported = false;
    EnumDeclNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

// import {name1, name2} from "file.sqt";
class ImportDeclNode : public ASTNode {
public:
    std::vector<std::string> importedNames;  // {"add", "Vector"}
    std::string              sourcePath;     // "math.sqt" (ham, çözümlenmemiş)
    ImportDeclNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

#endif
