#ifndef SAQUT_AST_LITERAL
#define SAQUT_AST_LITERAL

#include "parser/ast_node.hpp"

class LiteralNode : public ASTNode {
public:
    Token*       lexerToken  = nullptr;
    ParserToken  parserToken;

    LiteralType literalType  = LiteralType::INTEGER;
    int         literalBase  = 10;
    bool        isFloatValue = false;

    LiteralNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

#endif
