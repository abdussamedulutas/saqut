#include <unordered_map>
#include <string_view>

#include "../Tokenizer.cpp"
#include "./ParserToken.cpp"

#ifndef AST
#define AST


enum class ASTKind 
{
    BinaryExpression,
    Literal
};

class ASTNode
{
    private:
        std::vector<ASTNode *> childrens;
    public:
        ASTKind kind;
        ASTNode * parent;
    public:
        void addChild(ASTNode * children)
        {
            this->childrens.push_back(children);
        }
        void setParent(ASTNode * children)
        {
            this->parent = children;
        }
};

class BinaryExpressionNode : public ASTNode
{
    protected:
        ASTKind Kind = ASTKind::BinaryExpression;
    public:
        ASTNode* Right;
        TokenType Operator;
        ASTNode* Left;
};


class LiteralNode : public ASTNode
{
    protected:
        ASTKind kind = ASTKind::Literal;
    public:
        Token lexerToken;
        ParserToken parserToken;
};

#endif