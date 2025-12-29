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
        virtual void log(int indent)
        {
            std::cout << "<Unknown>\n";
        }
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
        void log(int indent) override
        {
            std::cout
                << padRight(" ",indent)
                << "BinaryExpressionNode"
                << OPERATOR_MAP_STRREV.find(this->Operator)->second
                << "( "<< OPERATOR_MAP_REV.find(this->Operator)->second << " )"
                << "\n";
    
            this->Right->log(indent + 4);
            if(this->Left != nullptr) this->Left->log(indent + 4);
        }
        ASTNode* Right;
        TokenType Operator;
        ASTNode* Left;
};


class LiteralNode : public ASTNode
{
    protected:
        ASTKind kind = ASTKind::Literal;
        void log(int indent)
        {
            std::cout << padRight(" ",indent)  << "LiteralNode {" << this->lexerToken.token << "}\n";
        }
    public:
        Token lexerToken;
        ParserToken parserToken;
};

#endif