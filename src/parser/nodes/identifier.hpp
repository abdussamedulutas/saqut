#ifndef SAQUT_AST_IDENTIFIER
#define SAQUT_AST_IDENTIFIER

#include "parser/ast_node.hpp"

struct Symbol; // TODO(faz-2): sembol tablosu (Symbol) tanımlandığında bağlanacak

class IdentifierNode : public ExpressionNode {
public:
    Token*       lexerToken  = nullptr;
    ParserToken  parserToken;

    // TODO(faz-2): isim çözümlemede sembol tablosundaki tanıma bağlanır.
    Symbol* resolvedSymbol = nullptr;

    IdentifierNode();
    void log(int indent = 0) override;
    std::string toJson(int depth = 0) override;
};

#endif
