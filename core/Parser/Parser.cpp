#include <iostream>
#include <string>
#include <stdlib.h>
#include <unordered_map>
#include <string_view>
#include "../Tokenizer.cpp"
#include "../Tools.cpp"
#include <cstdint>
#include <string_view>
#include "./ParserToken.cpp"
#include "./ASTNode.cpp"


#ifndef PARSER
#define PARSER


class Parser {
    private:
        ASTNode astroot;
    public:
        TokenList tokens;
        void parse(TokenList tokens);
        int current = 0;
        ParserToken currentToken();
        void nextToken();
        ParserToken lookehead(uint32_t);
        ParserToken parseToken(Token);
        ParserToken getToken(int);
        void primaryExpression();
        ASTNode * volumeExpression(uint16_t precedence);
        ASTNode * volumeNullDominatorExpression();
        ASTNode * volumeLeftDominatorExpression(ASTNode * left);
};


ParserToken Parser::parseToken(Token token){
    ParserToken pToken;
    pToken.token = token;
    
    if(token.gettype() == "string")
    {
        pToken.type = TokenType::STRING;
    }
    else if(token.gettype() == "number")
    {
        pToken.type = TokenType::NUMBER;
    }
    else if(token.gettype() == "operator")
    {
        pToken.type = OPERATOR_MAP.find(token.token)->second;
    }
    else if(token.gettype() == "delimiter")
    {
        pToken.type = OPERATOR_MAP.find(token.token)->second;
    }
    else if(token.gettype() == "keyword")
    {
        pToken.type = KEYWORD_MAP.find(token.token)->second;
    }
    else if(token.gettype() == "identifier")
    {
        pToken.type = KEYWORD_MAP.find(token.token)->second;
    }

    return pToken;
}


ParserToken Parser::getToken(int offset){
    if(this->tokens.size() - 1 < this->current + offset)
    {
        ParserToken pToken;
        pToken.type = TokenType::SVR_VOID;
        return pToken;
    }
    return this->parseToken(this->tokens[this->current + offset]);
}

void Parser::nextToken(){
    if(this->tokens.size() <= this->current + 1)
    {
        this->current++;
    }
}

ParserToken Parser::lookehead(uint32_t forward){
    return this->getToken(this->current + forward);
}

ParserToken Parser::currentToken(){
    return this->getToken(this->current);
}

void Parser::parse(TokenList tokens){
    this->tokens = tokens;
    this->primaryExpression();
}

void Parser::primaryExpression()
{
    auto currentToken = this->currentToken();

    if(
        currentToken.is({
            TokenType::NUMBER,
            TokenType::PLUS_PLUS,
            TokenType::MINUS_MINUS,
            TokenType::PLUS,
            TokenType::MINUS,
            TokenType::BANG,
            TokenType::TILDE,
        })
    )
    {
        this->volumeExpression(0);
    }
}

ASTNode * Parser::volumeExpression(uint16_t precedence)
{
    if (this->currentToken().type == TokenType::SVR_VOID)
    {
        return nullptr;
    }

    ASTNode* left = this->volumeNullDominatorExpression();

    while(1)
    {
        auto nextToken = this->lookehead(+1);
        if(precedence < nextToken.getPowerOperator())
        {
            this->nextToken();
            left = this->volumeLeftDominatorExpression(left);
        }else{
            break;  
        }
    }
    return left;
}



ASTNode * Parser::volumeNullDominatorExpression()
{
    auto currentToken = this->currentToken();
    
    if (currentToken.type == TokenType::SVR_VOID) {
        // Hata: "Beklenmedik dosya sonu, bir değer bekleniyordu!"
        return nullptr; 
    }
    

    if(currentToken.is({
        TokenType::PLUS_PLUS,
        TokenType::MINUS_MINUS,
        TokenType::PLUS,
        TokenType::MINUS,
        TokenType::BANG,
        TokenType::TILDE,
    })) {
        this->nextToken();
        ASTNode * right = this->volumeExpression(currentToken.getPowerOperator());
        BinaryExpressionNode * binNode = new BinaryExpressionNode();
        binNode->Right = right;
        binNode->Left = nullptr;
        binNode->Operator = currentToken.type;
        return binNode;
    };

    if(currentToken.is({
        TokenType::NUMBER
    })) {
        LiteralNode * lNode = new LiteralNode();
        lNode->lexerToken = currentToken.token;
        lNode->parserToken = currentToken;
        return lNode;
    }

    return nullptr;
}



ASTNode * Parser::volumeLeftDominatorExpression(ASTNode * left)
{
    auto currentToken = this->currentToken();
    uint16_t precedence = currentToken.getPowerOperator();
    this->nextToken();
    auto right = this->volumeExpression(precedence);

    BinaryExpressionNode * binNode = new BinaryExpressionNode();
    binNode->Right = right;
    binNode->Left = left;
    binNode->Operator = currentToken.type;
    return binNode;
}






































 


































#endif