#ifndef SAQUT_IR
#define SAQUT_IR

#include <vector>
#include <variant>
#include "parser/ast.hpp"

// ============================================================
// IR opcodes
// ============================================================

enum class OPCode {
    mathadd,
    mathsub,
    mathdiv,
    mathmul,
    declare
};

struct Param {
    bool isRegister;
    std::variant<int, float> value;
};

struct IROpData {
    OPCode op;
    int    targetReg;
    Param  arg1;
    Param  arg2;
    Param  arg3;
};

struct Identifier {
    int last = 0;
};

// ============================================================
// CodeGenerator: AST → IR
// ============================================================

class CodeGenerator {
private:
    void* processNumber(NumberToken* num, const std::string& rawStr) {
        if (num->isFloat || num->hasEpsilon)
            return new float(std::strtof(rawStr.c_str(), nullptr));
        return new int(std::strtol(rawStr.c_str(), nullptr, num->base));
    }

public:
    Identifier identifier;
    std::vector<IROpData> IROpDatas;

    int parse(ASTNode* ast) {
        switch (ast->kind) {
            case ASTKind::BinaryExpression:
                return parseBinaryExpr((BinaryExpressionNode*)ast);
            case ASTKind::Literal:
                return parseLiteral((LiteralNode*)ast);
            case ASTKind::Program:
                return parseProgram((ProgramNode*)ast);
            case ASTKind::ExpressionStatement:
                return parse(((ExpressionStatementNode*)ast)->expression);
            case ASTKind::FunctionDecl:
                return parseFunctionDecl((FunctionDeclNode*)ast);
            case ASTKind::Block:
                return parseBlock((BlockNode*)ast);
            case ASTKind::ReturnStatement:
                return parseReturn((ReturnStatementNode*)ast);
            case ASTKind::VariableDecl:
                return parseVariableDecl((VariableDeclNode*)ast);
            case ASTKind::IfStatement:
                return parseIf((IfStatementNode*)ast);
            case ASTKind::WhileStatement:
                return parseWhile((WhileStatementNode*)ast);
            default:
                return 0;
        }
    }

    int parseProgram(ProgramNode* prog) {
        for (auto* child : prog->getChildren())
            parse(child);
        return 0;
    }

    int parseFunctionDecl(FunctionDeclNode* fn) {
        for (auto* child : fn->getChildren())
            parse(child);
        return 0;
    }

    int parseBlock(BlockNode* block) {
        for (auto* child : block->getChildren())
            parse(child);
        return 0;
    }

    int parseReturn(ReturnStatementNode* ret) {
        if (ret->value)
            return parse(ret->value);
        return 0;
    }

    int parseVariableDecl(VariableDeclNode* vd) {
        if (vd->initExpr)
            return parse(vd->initExpr);
        return 0;
    }

    int parseIf(IfStatementNode* ifn) {
        parse(ifn->condition);
        parse(ifn->thenBranch);
        if (ifn->elseBranch)
            parse(ifn->elseBranch);
        return 0;
    }

    int parseWhile(WhileStatementNode* ws) {
        parse(ws->condition);
        parse(ws->body);
        return 0;
    }

    int parseBinaryExpr(BinaryExpressionNode* bin) {
        OPCode op;
        switch (bin->Operator) {
            case TokenType::STAR:  op = OPCode::mathmul; break;
            case TokenType::PLUS:  op = OPCode::mathadd; break;
            case TokenType::MINUS: op = OPCode::mathsub; break;
            case TokenType::SLASH: op = OPCode::mathdiv; break;
            default: return 0;
        }

        int left  = parse(bin->Left);
        int right = parse(bin->Right);

        IROpDatas.push_back({
            op,
            ++identifier.last,
            {true, left},
            {true, right},
            {false, 0}
        });
        return identifier.last;
    }

    int parseLiteral(LiteralNode* lit) {
        NumberToken* num = (NumberToken*)lit->parserToken.token;

        if (num->isFloat) {
            float* val = (float*)processNumber(num, num->token);
            IROpDatas.push_back({
                OPCode::declare,
                ++identifier.last,
                {false, *val},
                {false, 0},
                {false, 0}
            });
        } else {
            int* val = (int*)processNumber(num, num->token);
            IROpDatas.push_back({
                OPCode::declare,
                ++identifier.last,
                {false, *val},
                {false, 0},
                {false, 0}
            });
        }
        return identifier.last;
    }
};

#endif
