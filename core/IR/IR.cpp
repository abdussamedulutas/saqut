#include "../Parser/ASTNode.cpp"
#include "../Tokenizer.cpp"
#include <vector>
#include <variant>

#ifndef IR
#define IR

enum class OPCode {
    // İşlem
    mathadd,
    mathsub,
    mathdiv,
    mathmul,
    // Tanımalama
    declare
};

struct Param {
    bool isRegister; 
    std::variant<int,float> value;
};

struct IROpData {
    OPCode op;
    int targetReg;
    Param arg1;
    Param arg2;
    Param arg3;
};

struct Identifier
{
    int last = 0;
};


class CodeGenerator
{
    private:
          void * processNumber(NumberToken * num, const std::string& rawStr) {
            if (num->isFloat || num->hasEpsilon) {
                return new float(std::strtof(rawStr.c_str(), nullptr));
            } 
            else {
                return new int(std::strtol(rawStr.c_str(), nullptr, num->base));
            }
        }
    public:
        CodeGenerator()
        {

        }
        Identifier identifier;
        std::vector<IROpData> IROpDatas;
        int parse(ASTNode * ast)
        {
            switch (ast->kind)
            {
                case ASTKind::BinaryExpression:{
                    return this->parseBinaryExpression((BinaryExpressionNode *) ast);
                }
                case ASTKind::Literal:{
                    return this->parseLiteral((LiteralNode *) ast);
                }
                default: return 0;
            }
        };
        int parseBinaryExpression(BinaryExpressionNode * binaryAST)
        {
            OPCode op;
            switch (binaryAST->Operator)
            {
                case TokenType::STAR:{
                    op = OPCode::mathmul;
                    break;
                }
                case TokenType::PLUS:{
                    op = OPCode::mathadd;
                    break;
                }
                case TokenType::MINUS:{
                    op = OPCode::mathsub;
                    break;
                }
                case TokenType::SLASH:{
                    op = OPCode::mathdiv;
                    break;
                }
            }
            int left = this->parse(binaryAST->Left);
            int right = this->parse(binaryAST->Right);

            IROpDatas.push_back({
                op,
                ++identifier.last,
                {true, left},
                {true, right},
                {false, 0}
            });
            return identifier.last;
        }
        int parseLiteral(LiteralNode * binaryAST)
        {
            LiteralNode literal = *binaryAST;
            NumberToken * num = (NumberToken *) &literal.parserToken.token;


            if(num->isFloat)
            {
                float * _value = (float *) this->processNumber(num, num->token);
                IROpDatas.push_back({
                    OPCode::declare,
                    ++identifier.last,
                    {false, *_value},
                    {false, 0},
                    {false, 0}
                });
            }else{
                int * _value = (int *) this->processNumber(num, num->token);
                IROpDatas.push_back({
                    OPCode::declare,
                    ++identifier.last,
                    {false, *_value},
                    {false, 0},
                    {false, 0}
                });
            }
            return identifier.last;
        }
};

#endif