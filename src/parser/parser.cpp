#include "parser/parser.hpp"
#include "parser/nodes/program.hpp"
#include "parser/nodes/binary_expr.hpp"
#include "parser/nodes/literal.hpp"
#include "parser/nodes/identifier.hpp"
#include "parser/nodes/expressions.hpp"
#include "parser/nodes/statements.hpp"
#include "parser/nodes/declarations.hpp"

// --------------------------------------------------------------------------
// parseToken: Ham Token'ı ParserToken'a dönüştür.
// --------------------------------------------------------------------------
ParserToken Parser::parseToken(Token* token) {
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

ParserToken Parser::getToken(int offset) {
    if ((int)tokens.size() - 1 < current + offset) {
        ParserToken pt;
        pt.type = TokenType::SVR_VOID;
        return pt;
    }
    return parseToken(tokens[current + offset]);
}

void Parser::nextToken() {
    if ((int)tokens.size() >= current + 1)
        current++;
}

ParserToken Parser::lookahead(uint32_t forward) {
    return getToken(forward);
}

ParserToken Parser::currentToken() {
    return getToken(0);
}

ASTNode* Parser::parse(TokenList toks) {
    tokens  = toks;
    current = 0;
    return parseProgram();
}

ASTNode* Parser::parseProgram() {
    ProgramNode* program = new ProgramNode();

    while (currentToken().type != TokenType::SVR_VOID) {
        int prevPos = current;
        ASTNode* decl = parseDeclaration();
        if (decl)
            program->addChild(decl);
        // İlerleme olmadıysa token atla — syntax hatasında sonsuz döngüyü önler
        if (current == prevPos)
            nextToken();
    }

    return program;
}

ASTNode* Parser::parseDeclaration() {
    auto ct = currentToken();

    if (ct.is({
        TokenType::KW_VOID, TokenType::KW_INT, TokenType::KW_FLOAT_TYPE,
        TokenType::KW_DOUBLE, TokenType::KW_DECIMAL, TokenType::KW_BOOL,
        TokenType::KW_CHAR, TokenType::KW_STRING_TYPE, TokenType::KW_AUTO
    })) {
        auto la1 = lookahead(1);
        auto la2 = lookahead(2);
        // int name(  → fonksiyon
        if (la1.type == TokenType::IDENTIFIER && la2.type == TokenType::LPAREN)
            return parseFunctionDecl();
        // int? name(  → nullable dönüş tipli fonksiyon (ADR-021)
        if (la1.type == TokenType::TERNARY) {
            auto la3 = lookahead(3);
            if (la2.type == TokenType::IDENTIFIER && la3.type == TokenType::LPAREN)
                return parseFunctionDecl();
        }
        return parseVariableDecl();
    }

    if (ct.type == TokenType::KW_STRUCT)
        return parseStructDecl();

    // Kullanıcı tanımlı tip adı (struct tipi) ile değişken/fonksiyon bildirimi
    if (ct.type == TokenType::IDENTIFIER) {
        auto la1 = lookahead(1);
        auto la2 = lookahead(2);
        if (la1.type == TokenType::IDENTIFIER && la2.type == TokenType::LPAREN)
            return parseFunctionDecl();
        if (la1.type == TokenType::TERNARY) {
            auto la3 = lookahead(3);
            if (la2.type == TokenType::IDENTIFIER && la3.type == TokenType::LPAREN)
                return parseFunctionDecl();
        }
        if (la1.type == TokenType::IDENTIFIER)
            return parseVariableDecl();
        // "TypeName[]...[] varName" — çok boyutlu struct/enum array bildirimi
        if (la1.type == TokenType::LBRACKET) {
            int off = 1;
            while (lookahead(off).type == TokenType::LBRACKET &&
                   lookahead(off + 1).type == TokenType::RBRACKET)
                off += 2;
            if (lookahead(off).type == TokenType::IDENTIFIER)
                return parseVariableDecl();
        }
    }

    return parseStatement();
}

ASTNode* Parser::parseExpression() {
    return parseExpression(0);
}

ASTNode* Parser::parseExpression(uint16_t precedence) {
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

ASTNode* Parser::parseNullDenotation() {
    auto ct = currentToken();

    if (ct.type == TokenType::SVR_VOID) {
        std::cerr << "parser error: unexpected end of file\n";
        return nullptr;
    }

    if (ct.type == TokenType::LPAREN) {
        nextToken();
        ASTNode* expr = parseExpression(0);
        if (currentToken().type == TokenType::RPAREN)
            nextToken();
        return expr;
    }

    // Array literal: [expr, expr, ...]
    if (ct.type == TokenType::LBRACKET) {
        nextToken();
        ArrayLiteralNode* arr = new ArrayLiteralNode();
        arr->loc = ct.token ? ct.token->loc : SourceLocation{};
        if (currentToken().type != TokenType::RBRACKET) {
            arr->elements.push_back(parseExpression(0));
            while (currentToken().type == TokenType::COMMA) {
                nextToken();
                arr->elements.push_back(parseExpression(0));
            }
        }
        if (currentToken().type == TokenType::RBRACKET)
            nextToken();
        return arr;
    }

    if (ct.is({
        TokenType::PLUS_PLUS, TokenType::MINUS_MINUS,
        TokenType::PLUS, TokenType::MINUS,
        TokenType::BANG, TokenType::TILDE
    })) {
        nextToken();
        ASTNode* right = parseExpression(ct.getPowerOperator());
        BinaryExpressionNode* bin = new BinaryExpressionNode();
        bin->loc    = ct.token ? ct.token->loc : SourceLocation{};
        bin->Right    = right;
        bin->Left     = nullptr;
        bin->Operator = ct.type;
        if (right) right->parent = bin;
        return bin;
    }

    if (ct.type == TokenType::NUMBER) {
        nextToken();
        LiteralNode* lit = new LiteralNode();
        lit->loc       = ct.token ? ct.token->loc : SourceLocation{};
        lit->lexerToken  = ct.token;
        lit->parserToken = ct;
        if (auto* nt = dynamic_cast<NumberToken*>(ct.token)) {
            lit->literalBase  = nt->base;
            lit->isFloatValue = nt->isFloat;
            lit->literalType  = nt->isFloat ? LiteralType::FLOAT : LiteralType::INTEGER;
        }
        return lit;
    }

    if (ct.type == TokenType::STRING) {
        nextToken();
        LiteralNode* lit = new LiteralNode();
        lit->literalType = LiteralType::STRING;
        lit->loc       = ct.token ? ct.token->loc : SourceLocation{};
        lit->lexerToken  = ct.token;
        lit->parserToken = ct;
        return lit;
    }

    if (ct.is({TokenType::KW_TRUE, TokenType::KW_FALSE, TokenType::KW_NULL})) {
        nextToken();
        LiteralNode* lit = new LiteralNode();
        if (ct.is({TokenType::KW_TRUE, TokenType::KW_FALSE}))
            lit->literalType = LiteralType::BOOLEAN;
        else
            lit->literalType = LiteralType::BOŞ;
        lit->loc       = ct.token ? ct.token->loc : SourceLocation{};
        lit->lexerToken  = ct.token;
        lit->parserToken = ct;
        return lit;
    }

    if (ct.type == TokenType::IDENTIFIER) {
        nextToken();
        IdentifierNode* id = new IdentifierNode();
        id->loc          = ct.token ? ct.token->loc : SourceLocation{};
        id->lexerToken     = ct.token;
        id->parserToken    = ct;
        return id;
    }

    return nullptr;
}

ASTNode* Parser::parseLeftDenotation(ASTNode* left) {
    auto ct = currentToken();

    if (ct.is({TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {
        nextToken();
        PostfixNode* pf = new PostfixNode();
        pf->loc     = ct.token ? ct.token->loc : SourceLocation{};
        pf->operand  = left;
        pf->Operator = ct.type;
        left->parent = pf;
        return pf;
    }

    if (ct.type == TokenType::LPAREN) {
        nextToken();
        CallExpressionNode* call = new CallExpressionNode();
        call->loc    = ct.token ? ct.token->loc : SourceLocation{};
        call->callee = left;
        left->parent = call;

        if (currentToken().type != TokenType::RPAREN) {
            call->arguments.push_back(parseExpression(0));
            while (currentToken().type == TokenType::COMMA) {
                nextToken();
                call->arguments.push_back(parseExpression(0));
            }
        }
        if (currentToken().type == TokenType::RPAREN)
            nextToken();
        return call;
    }

    if (ct.type == TokenType::LBRACKET) {
        nextToken();
        IndexExpressionNode* idx = new IndexExpressionNode();
        idx->loc     = ct.token ? ct.token->loc : SourceLocation{};
        idx->object = left;
        left->parent = idx;
        idx->index = parseExpression(0);
        if (currentToken().type == TokenType::RBRACKET)
            nextToken();
        return idx;
    }

    // ADR-026: as tip dönüşümü — expr as TargetType[?]
    if (ct.type == TokenType::KW_AS) {
        nextToken(); // tüket: as
        CastExpressionNode* cast = new CastExpressionNode();
        cast->loc     = ct.token ? ct.token->loc : SourceLocation{};
        cast->operand = left;
        if (left) left->parent = cast;

        // Hedef tip adını oku: int / float / bool / string / IDENTIFIER
        auto typeTok = currentToken();
        if (typeTok.is({TokenType::KW_INT, TokenType::KW_FLOAT_TYPE,
                        TokenType::KW_DOUBLE, TokenType::KW_DECIMAL,
                        TokenType::KW_BOOL, TokenType::KW_STRING_TYPE})) {
            // tip adını string olarak al
            cast->targetTypeName = typeTok.token ? typeTok.token->token : "";
            nextToken();
        } else if (typeTok.type == TokenType::IDENTIFIER && typeTok.token) {
            cast->targetTypeName = typeTok.token->token;
            nextToken();
        } else {
            std::cerr << "parser error: expected type name after 'as'\n";
            cast->targetTypeName = "int"; // error recovery
        }

        // Opsiyonel '?' — nullable hedef tip: as int?
        if (currentToken().type == TokenType::TERNARY) {
            cast->targetNullable = true;
            nextToken();
        }
        return cast;
    }

    if (ct.type == TokenType::DOT || ct.type == TokenType::ARROW) {
        bool arrow = (ct.type == TokenType::ARROW);
        nextToken();

        if (currentToken().type != TokenType::IDENTIFIER) {
            std::cerr << "parser error: expected member name\n";
            return left;
        }

        MemberAccessNode* ma = new MemberAccessNode();
        ma->loc     = ct.token ? ct.token->loc : SourceLocation{};
        ma->object = left;
        ma->member = currentToken().token->token;
        ma->arrow  = arrow;
        left->parent = ma;
        nextToken();
        return ma;
    }

    uint16_t prec = ct.getPowerOperator();
    nextToken();

    ASTNode* right = parseExpression(prec);

    BinaryExpressionNode* bin = new BinaryExpressionNode();
    bin->loc      = ct.token ? ct.token->loc : SourceLocation{};
    bin->Left     = left;
    bin->Right    = right;
    bin->Operator = ct.type;
    if (left)  left->parent  = bin;
    if (right) right->parent = bin;
    return bin;
}

ASTNode* Parser::parseFunctionDecl() {
    FunctionDeclNode* fn = new FunctionDeclNode();
    fn->loc = currentToken().token->loc;
    fn->returnType = currentToken().token->token;
    nextToken();

    // ADR-021: nullable dönüş tipi — int? f()
    if (currentToken().type == TokenType::TERNARY)
        { nextToken(); fn->returnType += "?"; }

    fn->name = currentToken().token->token;
    nextToken();

    if (currentToken().type == TokenType::LPAREN) {
        nextToken();
        while (currentToken().type != TokenType::RPAREN &&
               currentToken().type != TokenType::SVR_VOID) {
            auto typeTok = currentToken();
            bool isTypeKw = typeTok.is({
                TokenType::KW_VOID, TokenType::KW_INT, TokenType::KW_FLOAT_TYPE,
                TokenType::KW_DOUBLE, TokenType::KW_DECIMAL, TokenType::KW_BOOL,
                TokenType::KW_CHAR, TokenType::KW_STRING_TYPE, TokenType::KW_AUTO
            }) || typeTok.type == TokenType::IDENTIFIER;
            if (!isTypeKw || !typeTok.token) break;
            std::string paramType = typeTok.token->token;
            nextToken();
            // int[][] a — tip sonrasında [] boyutları
            while (currentToken().type == TokenType::LBRACKET) {
                nextToken();
                if (currentToken().type == TokenType::RBRACKET)
                    nextToken();
                paramType += "[]";
            }
            // ADR-021: nullable parametre — int? a
            if (currentToken().type == TokenType::TERNARY)
                { nextToken(); paramType += "?"; }
            if (currentToken().type != TokenType::IDENTIFIER || !currentToken().token) break;
            VariableDeclNode* param = new VariableDeclNode();
            param->loc = currentToken().token->loc;
            param->varType = paramType;
            param->name = currentToken().token->token;
            nextToken();
            fn->params.push_back(param);
            if (currentToken().type == TokenType::COMMA)
                nextToken();
        }
        if (currentToken().type == TokenType::RPAREN)
            nextToken();
    }

    if (currentToken().type == TokenType::LBRACE) {
        ASTNode* body = parseBlock();
        fn->addChild(body);
    }

    return fn;
}

ASTNode* Parser::parseStructDecl() {
    StructDeclNode* st = new StructDeclNode();
    st->loc = currentToken().token->loc;
    nextToken();
    if (currentToken().type == TokenType::IDENTIFIER) {
        st->name = currentToken().token->token;
        nextToken();
    }
    if (currentToken().type == TokenType::LBRACE) {
        nextToken();
        while (currentToken().type != TokenType::RBRACE && currentToken().type != TokenType::SVR_VOID) {
            ASTNode* field = parseDeclaration();
            if (field) st->addChild(field);
            else break;
        }
        if (currentToken().type == TokenType::RBRACE) nextToken();
    }
    if (currentToken().type == TokenType::SEMICOLON) nextToken();
    return st;
}

ASTNode* Parser::parseEnumDecl() {
    EnumDeclNode* en = new EnumDeclNode();
    en->loc = currentToken().token->loc;
    nextToken(); // 'enum' tüket
    if (currentToken().type == TokenType::IDENTIFIER) {
        en->name = currentToken().token->token;
        nextToken();
    }
    if (currentToken().type == TokenType::LBRACE) {
        nextToken();
        int nextVal = 0;
        while (currentToken().type != TokenType::RBRACE &&
               currentToken().type != TokenType::SVR_VOID) {
            if (currentToken().type != TokenType::IDENTIFIER) break;
            EnumMember m;
            m.name = currentToken().token->token;
            nextToken();
            // İsteğe bağlı açık değer: Red = 5
            if (currentToken().type == TokenType::EQUAL) {
                nextToken();
                if (currentToken().type == TokenType::NUMBER) {
                    m.value = std::stoi(currentToken().token->token);
                    nextToken();
                    nextVal = m.value + 1;
                }
            } else {
                m.value = nextVal++;
            }
            en->members.push_back(m);
            if (currentToken().type == TokenType::COMMA) nextToken();
        }
        if (currentToken().type == TokenType::RBRACE) nextToken();
    }
    if (currentToken().type == TokenType::SEMICOLON) nextToken();
    return en;
}

ASTNode* Parser::parseVariableDecl() {
    VariableDeclNode* vd = new VariableDeclNode();
    vd->loc = currentToken().token->loc;
    vd->varType = currentToken().token->token;
    nextToken();

    // Java/C# stili: int[][] x — tip adından hemen sonra [] boyutları gelir
    while (currentToken().type == TokenType::LBRACKET) {
        nextToken();
        if (currentToken().type == TokenType::RBRACKET)
            nextToken();
        vd->varType += "[]";
    }

    // ADR-021: nullable soneki — int? x
    if (currentToken().type == TokenType::TERNARY)
        { nextToken(); vd->varType += "?"; }

    if (currentToken().type != TokenType::IDENTIFIER) {
        std::cerr << "parser error: expected variable name\n";
        return vd;
    }

    vd->name = currentToken().token->token;
    nextToken();

    // C stili: int x[] — geriye dönük uyumluluk (postfix [])
    if (currentToken().type == TokenType::LBRACKET) {
        nextToken();
        while (currentToken().type != TokenType::RBRACKET &&
               currentToken().type != TokenType::SEMICOLON &&
               currentToken().type != TokenType::SVR_VOID)
            nextToken();
        if (currentToken().type == TokenType::RBRACKET)
            nextToken();
        if (vd->varType.back() != ']') vd->varType += "[]";
    }

    if (currentToken().type == TokenType::EQUAL) {
        nextToken();
        vd->initExpr = parseExpression();
    }

    while (currentToken().type == TokenType::COMMA) {
        nextToken();

        if (currentToken().type != TokenType::IDENTIFIER) {
            std::cerr << "parser error: expected variable name after ','\n";
            break;
        }

        VariableDeclNode* sibling = new VariableDeclNode();
        sibling->loc = currentToken().token->loc;
        sibling->varType = vd->varType;
        sibling->name = currentToken().token->token;
        nextToken();

        if (currentToken().type == TokenType::LBRACKET) {
            nextToken();
            while (currentToken().type != TokenType::RBRACKET &&
                   currentToken().type != TokenType::SEMICOLON &&
                   currentToken().type != TokenType::SVR_VOID)
                nextToken();
            if (currentToken().type == TokenType::RBRACKET)
                nextToken();
        }

        if (currentToken().type == TokenType::EQUAL) {
            nextToken();
            sibling->initExpr = parseExpression();
        }

        vd->addChild(sibling);
    }

    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();

    return vd;
}

ASTNode* Parser::parseStatement() {
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

    if (ct.type == TokenType::KW_TRY)
        return parseTryStatement();

    if (ct.type == TokenType::KW_THROW)
        return parseThrowStatement();

    if (ct.type == TokenType::KW_SWITCH)
        return parseSwitchStatement();

    if (ct.is({
        TokenType::KW_VOID, TokenType::KW_INT, TokenType::KW_FLOAT_TYPE,
        TokenType::KW_DOUBLE, TokenType::KW_DECIMAL, TokenType::KW_BOOL,
        TokenType::KW_CHAR, TokenType::KW_STRING_TYPE
    })) {
        return parseVariableDecl();
    }

    if (ct.type == TokenType::KW_STRUCT)
        return parseStructDecl();

    if (ct.type == TokenType::KW_ENUM)
        return parseEnumDecl();

    // Kullanıcı tanımlı struct tipiyle değişken bildirimi: Point p; veya Point p = ...;
    if (ct.type == TokenType::IDENTIFIER) {
        auto la1 = lookahead(1);
        auto la2 = lookahead(2);
        // "TypeName varName" → değişken bildirimi
        if (la1.type == TokenType::IDENTIFIER)
            return parseVariableDecl();
        // "TypeName? varName" (ADR-021 nullable struct) — la1=TERNARY, la2=IDENTIFIER
        if (la1.type == TokenType::TERNARY && la2.type == TokenType::IDENTIFIER)
            return parseVariableDecl();
        // "TypeName[]...[] varName" — çok boyutlu struct/enum array bildirimi
        if (la1.type == TokenType::LBRACKET) {
            int off = 1;
            while (lookahead(off).type == TokenType::LBRACKET &&
                   lookahead(off + 1).type == TokenType::RBRACKET)
                off += 2;
            if (lookahead(off).type == TokenType::IDENTIFIER)
                return parseVariableDecl();
        }
    }

    return parseExpressionStatement();
}

ASTNode* Parser::parseBlock() {
    BlockNode* block = new BlockNode();
    block->loc = currentToken().token ? currentToken().token->loc : SourceLocation{};

    if (currentToken().type == TokenType::LBRACE)
        nextToken();

    while (currentToken().type != TokenType::RBRACE &&
           currentToken().type != TokenType::SVR_VOID) {
        ASTNode* stmt = parseStatement();
        if (stmt)
            block->addChild(stmt);
        else
            break;
    }

    if (currentToken().type == TokenType::RBRACE)
        nextToken();

    return block;
}

ASTNode* Parser::parseIfStatement() {
    IfStatementNode* ifNode = new IfStatementNode();
    ifNode->loc = currentToken().token->loc;
    nextToken();

    if (currentToken().type == TokenType::LPAREN) {
        nextToken();
        ifNode->condition = parseExpression();
        if (currentToken().type == TokenType::RPAREN)
            nextToken();
    }

    ifNode->thenBranch = parseStatement();

    if (currentToken().type == TokenType::KW_ELSE) {
        nextToken();
        ifNode->elseBranch = parseStatement();
    }

    return ifNode;
}

ASTNode* Parser::parseWhileStatement() {
    WhileStatementNode* ws = new WhileStatementNode();
    ws->loc = currentToken().token->loc;
    nextToken();

    if (currentToken().type == TokenType::LPAREN) {
        nextToken();
        ws->condition = parseExpression();
        if (currentToken().type == TokenType::RPAREN)
            nextToken();
    }

    ws->body = parseStatement();
    return ws;
}

ASTNode* Parser::parseForStatement() {
    ForStatementNode* fs = new ForStatementNode();
    fs->loc = currentToken().token->loc;
    nextToken();

    if (currentToken().type == TokenType::LPAREN)
        nextToken();

    if (currentToken().type != TokenType::SEMICOLON)
        fs->init = parseStatement();
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();

    if (currentToken().type != TokenType::SEMICOLON)
        fs->condition = parseExpression();
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();

    if (currentToken().type != TokenType::RPAREN)
        fs->update = parseExpression();
    if (currentToken().type == TokenType::RPAREN)
        nextToken();

    fs->body = parseStatement();

    return fs;
}

ASTNode* Parser::parseDoWhileStatement() {
    DoWhileStatementNode* dw = new DoWhileStatementNode();
    dw->loc = currentToken().token->loc;
    nextToken();

    dw->body = parseStatement();

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

ASTNode* Parser::parseReturnStatement() {
    ReturnStatementNode* rs = new ReturnStatementNode();
    rs->loc = currentToken().token->loc;
    nextToken();

    if (currentToken().type != TokenType::SEMICOLON &&
        currentToken().type != TokenType::RBRACE) {
        rs->value = parseExpression();
    }

    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();

    return rs;
}

ASTNode* Parser::parseBreakStatement() {
    BreakStatementNode* bs = new BreakStatementNode();
    bs->loc = currentToken().token->loc;
    nextToken();
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();
    return bs;
}

ASTNode* Parser::parseContinueStatement() {
    ContinueStatementNode* cs = new ContinueStatementNode();
    cs->loc = currentToken().token->loc;
    nextToken();
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();
    return cs;
}

ASTNode* Parser::parseExpressionStatement() {
    ExpressionStatementNode* es = new ExpressionStatementNode();
    es->loc = currentToken().token ? currentToken().token->loc : SourceLocation{};
    es->expression = parseExpression();
    if (!es->expression) {
        while (currentToken().type != TokenType::SEMICOLON &&
               currentToken().type != TokenType::RBRACE &&
               currentToken().type != TokenType::SVR_VOID)
            nextToken();
        if (currentToken().type == TokenType::SEMICOLON)
            nextToken();
    }
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();

    return es;
}

// ADR-025: try { body } catch (Error catchVar) { handler }
ASTNode* Parser::parseTryStatement() {
    TryStatementNode* ts = new TryStatementNode();
    ts->loc = currentToken().token->loc;
    nextToken(); // tüket: try

    ts->body = parseBlock();

    // catch (Error e)
    if (currentToken().type == TokenType::KW_CATCH) {
        nextToken(); // tüket: catch
        if (currentToken().type == TokenType::LPAREN)
            nextToken(); // tüket: (
        // "Error" tip adını atla
        if (currentToken().type == TokenType::IDENTIFIER ||
            currentToken().type == TokenType::KW_STRING_TYPE)
            nextToken(); // tüket: Error (ya da herhangi bir tip adı)
        // catch değişken adını al
        if (currentToken().type == TokenType::IDENTIFIER && currentToken().token)
            ts->catchVar = currentToken().token->token;
        nextToken(); // tüket: değişken adı
        if (currentToken().type == TokenType::RPAREN)
            nextToken(); // tüket: )
        ts->handler = parseBlock();
    }

    return ts;
}

// ADR-027: switch (expr) { case v1, v2: stmts break; default: stmts }
// Fallthrough YOK — her case otomatik break'li.
ASTNode* Parser::parseSwitchStatement() {
    SwitchStatementNode* sw = new SwitchStatementNode();
    sw->loc = currentToken().token->loc;
    nextToken(); // tüket: switch

    // subject: switch (expr)
    if (currentToken().type == TokenType::LPAREN) {
        nextToken();
        sw->subject = parseExpression();
        if (currentToken().type == TokenType::RPAREN)
            nextToken();
    }

    if (currentToken().type != TokenType::LBRACE)
        return sw;
    nextToken(); // tüket: {

    while (currentToken().type != TokenType::RBRACE &&
           currentToken().type != TokenType::SVR_VOID) {

        auto ct = currentToken();

        if (ct.type == TokenType::KW_CASE) {
            nextToken(); // tüket: case
            CaseClause clause;

            // case değerlerini virgülle ayır: case 1, 2, 3:
            // COLON'ın önceliği 3 — parseExpression(3) ile ':'yi tüketmeyiz
            clause.values.push_back(parseExpression(3));
            while (currentToken().type == TokenType::COMMA) {
                nextToken();
                clause.values.push_back(parseExpression(3));
            }
            if (currentToken().type == TokenType::COLON)
                nextToken(); // tüket: :

            // Bu case'in body'si: case/default/} görene kadar
            while (currentToken().type != TokenType::KW_CASE &&
                   currentToken().type != TokenType::KW_DEFAULT &&
                   currentToken().type != TokenType::RBRACE &&
                   currentToken().type != TokenType::SVR_VOID) {
                ASTNode* stmt = parseStatement();
                if (stmt) clause.body.push_back(stmt);
                else break;
            }
            // Açık break varsa zaten tüketildi (parseStatement → parseBreakStatement)
            sw->cases.push_back(std::move(clause));

        } else if (ct.type == TokenType::KW_DEFAULT) {
            nextToken(); // tüket: default
            if (currentToken().type == TokenType::COLON)
                nextToken(); // tüket: :
            CaseClause clause;
            clause.isDefault = true;
            while (currentToken().type != TokenType::KW_CASE &&
                   currentToken().type != TokenType::KW_DEFAULT &&
                   currentToken().type != TokenType::RBRACE &&
                   currentToken().type != TokenType::SVR_VOID) {
                ASTNode* stmt = parseStatement();
                if (stmt) clause.body.push_back(stmt);
                else break;
            }
            sw->cases.push_back(std::move(clause));
        } else {
            // Beklenmedik token — atla
            nextToken();
        }
    }

    if (currentToken().type == TokenType::RBRACE)
        nextToken(); // tüket: }

    return sw;
}

// ADR-025: throw <ifade>;
ASTNode* Parser::parseThrowStatement() {
    ThrowStatementNode* th = new ThrowStatementNode();
    th->loc = currentToken().token->loc;
    nextToken(); // tüket: throw
    th->value = parseExpression();
    if (currentToken().type == TokenType::SEMICOLON)
        nextToken();
    return th;
}
