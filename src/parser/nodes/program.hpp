// ============================================================================
// saQut Compiler — ProgramNode (Kök Düğüm)
// ============================================================================
//
// DİZİN:   src/parser/nodes/program.hpp
// KATMAN:  Katman 3 — AST'nin kök düğümü
//
// AMAÇ:
//   Tüm programı kapsayan kök AST düğümü. Children'ı bildirim düğümleridir
//   (FunctionDecl, StructDecl, VariableDecl, ImportDecl). Her .sqt dosyası
//   tek bir ProgramNode olarak temsil edilir.
//
// ============================================================================

#ifndef SAQUT_AST_PROGRAM
#define SAQUT_AST_PROGRAM

#include "parser/ast_node.hpp"

class ProgramNode : public ASTNode {
public:
    ProgramNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

#endif
