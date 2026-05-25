#ifndef SAQUT_PARSER
#define SAQUT_PARSER

#include <iostream>
#include <cstdint>
#include <string>
#include "parser/token.hpp"
#include "parser/ast.hpp"
#include "tools.hpp"

class Parser {
public:
    ASTNode* parse(TokenList tokens);

private:
    TokenList tokens;
    int current = 0;

    // Token navigation
    ParserToken currentToken();
    void        nextToken();
    ParserToken lookahead(uint32_t forward);
    ParserToken parseToken(Token* token);
    ParserToken getToken(int offset);

    // --- Top level ---
    ASTNode* parseProgram();

    // --- Declarations ---
    ASTNode* parseDeclaration();
    ASTNode* parseFunctionDecl();
    ASTNode* parseVariableDecl();

    // --- Statements ---
    ASTNode* parseStatement();
    ASTNode* parseBlock();
    ASTNode* parseIfStatement();
    ASTNode* parseWhileStatement();
    ASTNode* parseForStatement();
    ASTNode* parseDoWhileStatement();
    ASTNode* parseReturnStatement();
    ASTNode* parseBreakStatement();
    ASTNode* parseContinueStatement();
    ASTNode* parseExpressionStatement();

    // --- Expressions (Pratt parser) ---
    ASTNode* parseExpression();
    ASTNode* parseExpression(uint16_t precedence);
    ASTNode* parseNullDenotation();
    ASTNode* parseLeftDenotation(ASTNode* left);
};

// ============================================================
// Token helpers
// ============================================================

inline ParserToken Parser::parseToken(Token* token) {
    ParserToken pt;
    pt.token = token;

    std::string t = token->gettype();
    if (t == "string")
        pt.type = TokenType::STRING;
    else if (t == "number")
        pt.type = TokenType::NUMBER;
    else if (t == "operator")
        pt.type = OPERATOR_MAP.find(pt.token->token)->second;
    else if (t == "delimiter")
        pt.type = OPERATOR_MAP.find(pt.token->token)->second;
    else if (t == "keyword")
        pt.type = KEYWORD_MAP.find(pt.token->token)->second;
    else if (t == "identifier")
        pt.type = TokenType::IDENTIFIER;

    return pt;
}

inline ParserToken Parser::getToken(int offset) {
    if ((int)tokens.size() - 1 < current + offset) {
        ParserToken pt;
        pt.type = TokenType::SVR_VOID;
        return pt;
    }
    return parseToken(tokens[current + offset]);
}

inline void Parser::nextToken() {
    if ((int)tokens.size() >= current + 1)
        current++;
}

inline ParserToken Parser::lookahead(uint32_t forward) {
    return getToken(forward);
}

inline ParserToken Parser::currentToken() {
    return getToken(0);
}

// ============================================================
// Top level
// ============================================================

inline ASTNode* Parser::parse(TokenList toks) {
    tokens = toks;
    current = 0;
    return parseProgram();
}

inline ASTNode* Parser::parseProgram() {
    ProgramNode* program = new ProgramNode();

    while (currentToken().type != TokenType::SVR_VOID) {
        ASTNode* decl = parseDeclaration();
        if (decl)
            program->addChild(decl);
        else
            break;
    }

    return program;
}

// ============================================================
// Declarations
// ============================================================

inline ASTNode* Parser::parseDeclaration() {
    auto ct = currentToken();

    // Function declaration: type identifier ( ) { ... }
    if (ct.is({
        TokenType::KW_VOID, TokenType::KW_INT, TokenType::KW_FLOAT_TYPE,
        TokenType::KW_DOUBLE, TokenType::KW_BOOL, TokenType::KW_CHAR,
        TokenType::KW_STRING_TYPE, TokenType::KW_AUTO
    })) {
        // Check if next is identifier, then '(' → function
        auto la1 = lookahead(1);
        auto la2 = lookahead(2);
        if (la1.type == TokenType::IDENTIFIER && la2.type == TokenType::LPAREN)
            return parseFunctionDecl();
        // Otherwise variable declaration
        return parseVariableDecl();
    }

    // Standalone expression (for REPL / bare source.sqt)
    return parseStatement();
}

inline ASTNode* Parser::parseFunctionDecl() {
    FunctionDeclNode* fn = new FunctionDeclNode();
    fn->returnType = currentToken().token->token;  // e.g., "void", "int"
    nextToken();  // eat return type

    fn->name = currentToken().token->token;
    nextToken();  // eat name

    // Eat '(' ... ')'
    if (currentToken().type == TokenType::LPAREN) {
        nextToken();
        // Skip params for now
        while (currentToken().type != TokenType::RPAREN &&
               currentToken().type != TokenType::SVR_VOID)
            nextToken();
        if (currentToken().type == TokenType::RPAREN)
            nextToken();
    }

    // Parse body { ... }
    if (currentToken().type == TokenType::LBRACE) {
        ASTNode* body = parseBlock();
        fn->addChild(body);
    }

    return fn;
}

inline ASTNode* Parser::parseVariableDecl() {
    VariableDeclNode* vd = new VariableDeclNode();
    vd->varType = currentToken().token->token;  // e.g., "int", "float"
    nextToken();  // eat type

    if (currentToken().type != TokenType::IDENTIFIER) {
        std::cerr << "Parser hatası: değişken ismi bekleniyor\n";
        return vd;
    }

    vd->name = currentToken().token->token;
    nextToken();  // eat name

    // Optional initializer: = expression
    if (currentToken().type == TokenType::EQUAL) {
        nextToken();  // eat =
        vd->initExpr = parseExpression();
    }

    // Optional semicolon
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();

    return vd;
}

// ============================================================
// Statements
// ============================================================

inline ASTNode* Parser::parseStatement() {
    auto ct = currentToken();

    if (ct.type == TokenType::LBRACE)
        return parseBlock();

    if (ct.type == TokenType::KW_IF)
        return parseIfStatement();

    if (ct.type == TokenType::KW_WHILE)
        return parseWhileStatement();

    if (ct.type == TokenType::KW_FOR)
        return parseForStatement();

    if (ct.type == TokenType::KW_DO)
        return parseDoWhileStatement();

    if (ct.type == TokenType::KW_RETURN)
        return parseReturnStatement();

    if (ct.type == TokenType::KW_BREAK)
        return parseBreakStatement();

    if (ct.type == TokenType::KW_CONTINUE)
        return parseContinueStatement();

    // Variable declaration? (type identifier ...)
    if (ct.is({
        TokenType::KW_VOID, TokenType::KW_INT, TokenType::KW_FLOAT_TYPE,
        TokenType::KW_DOUBLE, TokenType::KW_BOOL, TokenType::KW_CHAR,
        TokenType::KW_STRING_TYPE
    })) {
        return parseVariableDecl();
    }

    // Default: expression statement
    return parseExpressionStatement();
}

inline ASTNode* Parser::parseBlock() {
    BlockNode* block = new BlockNode();

    if (currentToken().type == TokenType::LBRACE)
        nextToken();  // eat {

    while (currentToken().type != TokenType::RBRACE &&
           currentToken().type != TokenType::SVR_VOID) {
        ASTNode* stmt = parseStatement();
        if (stmt)
            block->addChild(stmt);
        else
            break;
    }

    if (currentToken().type == TokenType::RBRACE)
        nextToken();  // eat }

    return block;
}

inline ASTNode* Parser::parseIfStatement() {
    IfStatementNode* ifNode = new IfStatementNode();
    nextToken();  // eat 'if'

    // Condition: ( expression )
    if (currentToken().type == TokenType::LPAREN) {
        nextToken();
        ifNode->condition = parseExpression();
        if (currentToken().type == TokenType::RPAREN)
            nextToken();
    }

    // Then branch
    ifNode->thenBranch = parseStatement();

    // Optional else
    if (currentToken().type == TokenType::KW_ELSE) {
        nextToken();
        ifNode->elseBranch = parseStatement();
    }

    return ifNode;
}

inline ASTNode* Parser::parseWhileStatement() {
    WhileStatementNode* ws = new WhileStatementNode();
    nextToken();  // eat 'while'

    // Condition: ( expression )
    if (currentToken().type == TokenType::LPAREN) {
        nextToken();
        ws->condition = parseExpression();
        if (currentToken().type == TokenType::RPAREN)
            nextToken();
    }

    // Body
    ws->body = parseStatement();

    return ws;
}

inline ASTNode* Parser::parseForStatement() {
    ForStatementNode* fs = new ForStatementNode();
    nextToken();  // eat 'for'

    if (currentToken().type == TokenType::LPAREN)
        nextToken();  // eat (

    // Init
    if (currentToken().type != TokenType::SEMICOLON)
        fs->init = parseStatement();
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();

    // Condition
    if (currentToken().type != TokenType::SEMICOLON)
        fs->condition = parseExpression();
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();

    // Update
    if (currentToken().type != TokenType::RPAREN)
        fs->update = parseExpression();
    if (currentToken().type == TokenType::RPAREN)
        nextToken();

    // Body
    fs->body = parseStatement();

    return fs;
}

inline ASTNode* Parser::parseDoWhileStatement() {
    DoWhileStatementNode* dw = new DoWhileStatementNode();
    nextToken();  // eat 'do'

    // Body
    dw->body = parseStatement();

    // 'while' ( expression ) ;
    if (currentToken().type == TokenType::KW_WHILE) {
        nextToken();
        if (currentToken().type == TokenType::LPAREN) {
            nextToken();
            dw->condition = parseExpression();
            if (currentToken().type == TokenType::RPAREN)
                nextToken();
        }
        if (currentToken().type == TokenType::SEMICOLON)
            nextToken();
    }

    return dw;
}

inline ASTNode* Parser::parseReturnStatement() {
    ReturnStatementNode* rs = new ReturnStatementNode();
    nextToken();  // eat 'return'

    // Optional return value
    if (currentToken().type != TokenType::SEMICOLON &&
        currentToken().type != TokenType::RBRACE) {
        rs->value = parseExpression();
    }

    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();

    return rs;
}

inline ASTNode* Parser::parseBreakStatement() {
    BreakStatementNode* bs = new BreakStatementNode();
    nextToken();  // eat 'break'
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();
    return bs;
}

inline ASTNode* Parser::parseContinueStatement() {
    ContinueStatementNode* cs = new ContinueStatementNode();
    nextToken();  // eat 'continue'
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();
    return cs;
}

inline ASTNode* Parser::parseExpressionStatement() {
    ExpressionStatementNode* es = new ExpressionStatementNode();
    es->expression = parseExpression();

    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();

    return es;
}

// ============================================================
// Expressions — Pratt parser
// ============================================================

inline ASTNode* Parser::parseExpression() {
    return parseExpression(0);
}

inline ASTNode* Parser::parseExpression(uint16_t precedence) {
    if (currentToken().type == TokenType::SVR_VOID)
        return nullptr;

    ASTNode* left = parseNullDenotation();
    if (!left) return nullptr;

    while (true) {
        auto next = currentToken();
        if (next.type == TokenType::RPAREN ||
            next.type == TokenType::SEMICOLON ||
            next.type == TokenType::RBRACE ||
            next.type == TokenType::COMMA)
            break;

        if (precedence < next.getPowerOperator()) {
            left = parseLeftDenotation(left);
        } else {
            break;
        }
    }
    return left;
}

// Prefix / atoms — parse expressions that start with themselves
inline ASTNode* Parser::parseNullDenotation() {
    auto ct = currentToken();

    if (ct.type == TokenType::SVR_VOID) {
        std::cerr << "Parser hatası: beklenmeyen dosya sonu\n";
        return nullptr;
    }

    // Parenthesized expression
    if (ct.type == TokenType::LPAREN) {
        nextToken();
        ASTNode* expr = parseExpression(0);
        if (currentToken().type == TokenType::RPAREN)
            nextToken();
        return expr;
    }

    // Unary prefix: ++, --, +, -, !, ~
    if (ct.is({
        TokenType::PLUS_PLUS, TokenType::MINUS_MINUS,
        TokenType::PLUS, TokenType::MINUS,
        TokenType::BANG, TokenType::TILDE
    })) {
        nextToken();
        ASTNode* right = parseExpression(ct.getPowerOperator());
        BinaryExpressionNode* bin = new BinaryExpressionNode();
        bin->Right    = right;
        bin->Left     = nullptr;
        bin->Operator = ct.type;
        if (right) right->parent = bin;
        return bin;
    }

    // Numeric literal
    if (ct.type == TokenType::NUMBER) {
        nextToken();
        LiteralNode* lit = new LiteralNode();
        lit->lexerToken  = ct.token;
        lit->parserToken = ct;
        return lit;
    }

    // String literal
    if (ct.type == TokenType::STRING) {
        nextToken();
        LiteralNode* lit = new LiteralNode();
        lit->lexerToken  = ct.token;
        lit->parserToken = ct;
        return lit;
    }

    // Boolean / null literals
    if (ct.is({TokenType::KW_TRUE, TokenType::KW_FALSE, TokenType::KW_NULL})) {
        nextToken();
        LiteralNode* lit = new LiteralNode();
        lit->lexerToken  = ct.token;
        lit->parserToken = ct;
        return lit;
    }

    // Identifier
    if (ct.type == TokenType::IDENTIFIER) {
        nextToken();
        IdentifierNode* id = new IdentifierNode();
        id->lexerToken     = ct.token;
        id->parserToken    = ct;
        return id;
    }

    return nullptr;
}

// Infix / postfix — parse expressions that continue after a left operand
inline ASTNode* Parser::parseLeftDenotation(ASTNode* left) {
    auto ct = currentToken();

    // Postfix: ++, --
    if (ct.is({TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {
        nextToken();
        PostfixNode* pf = new PostfixNode();
        pf->operand  = left;
        pf->Operator = ct.type;
        left->parent = pf;
        return pf;
    }

    // Binary infix operators
    uint16_t prec = ct.getPowerOperator();
    nextToken();

    ASTNode* right = parseExpression(prec);

    BinaryExpressionNode* bin = new BinaryExpressionNode();
    bin->Left     = left;
    bin->Right    = right;
    bin->Operator = ct.type;
    if (left)  left->parent  = bin;
    if (right) right->parent = bin;
    return bin;
}

#endif
