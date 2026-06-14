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
