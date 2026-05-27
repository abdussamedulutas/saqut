#ifndef SAQUT_AST_IDENTIFIER
#define SAQUT_AST_IDENTIFIER

#include "parser/ast_node.hpp"

class IdentifierNode : public ASTNode {
public:
    Token*       lexerToken  = nullptr;
    ParserToken  parserToken;

    IdentifierNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

#endif
