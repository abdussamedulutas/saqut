#include "ir/ir_generator.hpp"
#include "tokenizer/token.hpp"
#include "parser/nodes/program.hpp"
#include "parser/nodes/declarations.hpp"
#include "parser/nodes/statements.hpp"
#include "parser/nodes/expressions.hpp"
#include "parser/nodes/binary_expr.hpp"
#include "parser/nodes/identifier.hpp"
#include "parser/nodes/literal.hpp"
#include <stdexcept>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// generate — Ana giriş noktası
// ─────────────────────────────────────────────────────────────────────────────

IRProgram IRGenerator::generate(ASTNode* programNode, SymbolTable& symbolTable) {
    IRProgram program;

    // 0. Geçiş: struct layout haritasını sembol tablosundan al
    structLayouts_ = symbolTable.structLayouts;

    // 1. Geçiş: modül-düzeyi VariableDecl'leri topla ve kayıt et
    // "Global" değil — bu dosyanın (modülün) kendi değişkenleri.
    std::vector<VariableDeclNode*> globalVars;
    for (ASTNode* child : programNode->getChildren()) {
        if (child->kind == ASTKind::VariableDecl) {
            auto* vd = (VariableDeclNode*)child;
            nameToGlobal_[vd->name] = globalCount_++;
            program.globalCount++;
            program.globalNames.push_back(vd->name);
            globalVars.push_back(vd);
        }
    }

    // 2. Geçiş: fonksiyonları üret
    for (ASTNode* child : programNode->getChildren()) {
        if (child->kind == ASTKind::FunctionDecl) {
            nameToSlot_.clear();
            nextSlot_ = 0;

            auto* fnDecl = (FunctionDeclNode*)child;
            IRFunction irFn(fnDecl->name, (int)fnDecl->params.size());
            program.addFunction(std::move(irFn));
            currentFunction_ = program.findFunction(fnDecl->name);

            // main'in başında global değişkenlerin init ifadelerini üret
            if (fnDecl->name == "main") {
                for (VariableDeclNode* gv : globalVars) {
                    if (gv->initExpr) {
                        int initSlot = generateExpression(gv->initExpr);
                        emitStoreGlobal(initSlot, nameToGlobal_[gv->name]);
                    }
                }
            }

            generateFunction(child);
            currentFunction_->slotCount = nextSlot_;
        }
    }

    return program;
}

// ─────────────────────────────────────────────────────────────────────────────
// generateFunction — Tek bir fonksiyonu IR'a çevirir
// ─────────────────────────────────────────────────────────────────────────────

void IRGenerator::generateFunction(ASTNode* functionDeclNode) {
    auto* fn = (FunctionDeclNode*)functionDeclNode;

    // Parametreler slot 0, 1, 2, ... sırasıyla alır.
    // Interpreter, CALL sırasında bu slotlara argümanları kopyalar.
    for (auto* param : fn->params) {
        int slot = freshSlot();
        registerVariable(param->name, slot);
    }

    // Fonksiyon gövdesi — children[0] her zaman BlockNode
    auto& children = fn->getChildren();
    if (!children.empty()) {
        generateStatement(children[0]);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// generateStatement — Deyim türlerine göre talimat üret
// ─────────────────────────────────────────────────────────────────────────────

void IRGenerator::generateStatement(ASTNode* node) {
    if (!node) return;

    switch (node->kind) {

    // ── Blok: içindeki her deyimi sırayla üret ───────────────────────────
    case ASTKind::Block: {
        for (ASTNode* child : node->getChildren()) {
            generateStatement(child);
        }
        break;
    }

    // ── Değişken bildirimi: int x = <ifade>  /  Point p; ────────────────
    case ASTKind::VariableDecl: {
        auto* vd = (VariableDeclNode*)node;

        // Bu değişken için yeni bir slot ayır
        int varSlot = freshSlot();
        registerVariable(vd->name, varSlot);

        if (vd->initExpr) {
            int initSlot = generateExpression(vd->initExpr);
            // float/double değişkenine int sabit atama → INT_TO_FLOAT
            bool targetIsFloat = (vd->varType == "float" || vd->varType == "double");
            bool srcIsInt = false;
            if (auto* e = dynamic_cast<ExpressionNode*>(vd->initExpr))
                srcIsInt = e->resolvedType.isPrimitive() && e->resolvedType.prim == PrimitiveKind::Int;
            if (targetIsFloat && srcIsInt) {
                int conv = freshSlot();
                emitIntToFloat(conv, initSlot);
                initSlot = conv;
            }
            if (initSlot != varSlot) emitLoadSlot(varSlot, initSlot);
        } else if (structLayouts_.count(vd->varType)) {
            // Struct değişkeni: init ifadesi yoksa boş StructObject oluştur
            int fc = getStructFieldCount(vd->varType);
            emitStructNew(varSlot, vd->varType, fc);
        }

        // Sibling VariableDecl'ler: int a, b; → children'da diğer VariableDecl'ler
        for (ASTNode* sib : node->getChildren()) {
            if (sib->kind == ASTKind::VariableDecl) {
                generateStatement(sib);
            }
        }
        break;
    }

    // ── return <ifade> ───────────────────────────────────────────────────
    case ASTKind::ReturnStatement: {
        auto* rs = (ReturnStatementNode*)node;
        int returnSlot = 0; // varsayılan: slot[0] (void fonksiyon / boş return)

        if (rs->value) {
            returnSlot = generateExpression(rs->value);
        }
        emitReturn(returnSlot);
        break;
    }

    // ── if (koşul) { ... } [else { ... }] ───────────────────────────────
    case ASTKind::IfStatement: {
        auto* ifn = (IfStatementNode*)node;

        // Koşulu hesapla
        int condSlot = generateExpression(ifn->condition);

        // "Koşul yanlışsa atla" → hedef henüz bilinmiyor, backpatch bekliyor
        int jumpToElse = emitJumpIfFalse(condSlot);

        // Then bloğu
        if (ifn->thenBranch) generateStatement(ifn->thenBranch);

        if (ifn->elseBranch) {
            // Then bitti, else'i atla (then içinde çalışanlar else'e girmemeli)
            int jumpOverElse = emitJumpUnconditional(-1);
            // Şimdi else'in başlangıç konumunu biliyoruz → jumpToElse'i doldur
            patchJump(jumpToElse);
            generateStatement(ifn->elseBranch);
            // Else bitti → jumpOverElse'i doldur
            patchJump(jumpOverElse);
        } else {
            // Else yok → jumpToElse doğrudan if sonrasına atlıyor
            patchJump(jumpToElse);
        }
        break;
    }

    // ── while (koşul) { gövde } ──────────────────────────────────────────
    case ASTKind::WhileStatement: {
        auto* ws = (WhileStatementNode*)node;

        int loopStart = currentInstrIndex();
        loopContextStack_.push_back({});

        int condSlot = generateExpression(ws->condition);
        int exitJump = emitJumpIfFalse(condSlot);

        if (ws->body) generateStatement(ws->body);

        // continue → LOOP_START (hedef baştan beri biliniyor)
        for (int idx : loopContextStack_.back().continueJumps)
            currentFunction_->instructions[idx].jumpTarget = loopStart;

        emitJumpUnconditional(loopStart);
        patchJump(exitJump); // OUT burası

        // break → OUT
        int outTarget = currentInstrIndex();
        for (int idx : loopContextStack_.back().breakJumps)
            currentFunction_->instructions[idx].jumpTarget = outTarget;

        loopContextStack_.pop_back();
        break;
    }

    // ── for (init; koşul; güncelleme) { gövde } ─────────────────────────
    //
    // IR yapısı (continue C_LABEL'a, break OUT'a atlar):
    //   [init]
    //   LOOP_START:
    //     [koşul] → JIF_FALSE OUT
    //     [gövde]
    //   C_LABEL:
    //     [güncelleme]
    //     JMP LOOP_START
    //   OUT:
    // ─────────────────────────────────────────────────────────────────────
    case ASTKind::ForStatement: {
        auto* fs = (ForStatementNode*)node;

        if (fs->init) generateStatement(fs->init);

        int loopStart = currentInstrIndex();
        loopContextStack_.push_back({});

        int condSlot = fs->condition ? generateExpression(fs->condition) : -1;
        int exitJump = (condSlot != -1) ? emitJumpIfFalse(condSlot) : -1;

        if (fs->body) generateStatement(fs->body);

        // C_LABEL: güncelleme başlangıcı — continue buraya atlar
        int cLabel = currentInstrIndex();
        for (int idx : loopContextStack_.back().continueJumps)
            currentFunction_->instructions[idx].jumpTarget = cLabel;

        if (fs->update) generateExpression(fs->update);

        emitJumpUnconditional(loopStart);

        if (exitJump != -1) patchJump(exitJump); // OUT burası

        // break → OUT
        int outTarget = currentInstrIndex();
        for (int idx : loopContextStack_.back().breakJumps)
            currentFunction_->instructions[idx].jumpTarget = outTarget;

        loopContextStack_.pop_back();
        break;
    }

    // ── do { gövde } while (koşul) ───────────────────────────────────────
    case ASTKind::DoWhileStatement: {
        auto* dw = (DoWhileStatementNode*)node;

        int loopStart = currentInstrIndex();
        loopContextStack_.push_back({});

        if (dw->body) generateStatement(dw->body);

        // COND_LABEL: koşul değerlendirmesi — continue buraya atlar
        int condLabel = currentInstrIndex();
        for (int idx : loopContextStack_.back().continueJumps)
            currentFunction_->instructions[idx].jumpTarget = condLabel;

        int condSlot = generateExpression(dw->condition);
        Instruction jit(Opcode::JIF_TRUE);
        jit.cond       = condSlot;
        jit.jumpTarget = loopStart;
        currentFunction_->instructions.push_back(std::move(jit));

        // break → OUT (JIF_TRUE'dan sonraki konum)
        int outTarget = currentInstrIndex();
        for (int idx : loopContextStack_.back().breakJumps)
            currentFunction_->instructions[idx].jumpTarget = outTarget;

        loopContextStack_.pop_back();
        break;
    }

    // ── İfade deyimi: bir ifadeyi değerlendirip sonucu at ────────────────
    // Örnek: print(x) çağrısı, veya x = 5 ataması
    case ASTKind::ExpressionStatement: {
        auto* es = (ExpressionStatementNode*)node;
        if (es->expression) {
            generateExpression(es->expression); // sonucu kullanmıyoruz
        }
        break;
    }

    case ASTKind::BreakStatement: {
        int jumpIdx = emitJumpUnconditional(-1);
        if (!loopContextStack_.empty())
            loopContextStack_.back().breakJumps.push_back(jumpIdx);
        break;
    }
    case ASTKind::ContinueStatement: {
        int jumpIdx = emitJumpUnconditional(-1);
        if (!loopContextStack_.empty())
            loopContextStack_.back().continueJumps.push_back(jumpIdx);
        break;
    }

    default:
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// generateExpression — İfadeyi IR'a çevirir, sonucu içeren slot'u döndürür
// ─────────────────────────────────────────────────────────────────────────────

int IRGenerator::generateExpression(ASTNode* node) {
    if (!node) return 0;

    switch (node->kind) {

    // ── Sabit değer: 42, 3.14, true ... ──────────────────────────────────
    case ASTKind::Literal: {
        auto* lit = (LiteralNode*)node;
        int slot = freshSlot();

        switch (lit->literalType) {
            case LiteralType::INTEGER: {
                // Float/double bağlamında tam sayı literali → LOAD_FLOAT (bağlama-göre tip, ADR-010)
                bool asFloat = lit->resolvedType.isPrimitive() &&
                               (lit->resolvedType.prim == PrimitiveKind::Float ||
                                lit->resolvedType.prim == PrimitiveKind::Double);
                if (asFloat) {
                    double val = 0.0;
                    if (lit->hasDirectValue) val = (double)lit->directIntValue;
                    else if (lit->parserToken.token) val = std::stod(lit->parserToken.token->token);
                    emitLoadFloat(slot, val);
                } else {
                    int value = 0;
                    if (lit->hasDirectValue) value = lit->directIntValue;
                    else if (lit->parserToken.token) value = std::stoi(lit->parserToken.token->token);
                    emitLoadConst(slot, value);
                }
                break;
            }
            case LiteralType::BOOLEAN: {
                int value = 0;
                if (lit->hasDirectValue)
                    value = lit->directIntValue ? 1 : 0;
                else
                    value = (lit->parserToken.token &&
                             lit->parserToken.token->token == "true") ? 1 : 0;
                emitLoadConst(slot, value);
                break;
            }
            case LiteralType::STRING: {
                // StringToken::context tırnak işaretleri olmadan içeriği tutar
                std::string content;
                if (auto* st = dynamic_cast<StringToken*>(lit->lexerToken))
                    content = st->context;
                else if (lit->parserToken.token) {
                    // Fallback: token'ın başındaki ve sonundaki " işaretlerini sıyır
                    std::string raw = lit->parserToken.token->token;
                    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"')
                        content = raw.substr(1, raw.size() - 2);
                    else
                        content = raw;
                }
                Instruction ins(Opcode::LOAD_STRING);
                ins.dest        = slot;
                ins.stringValue = std::move(content);
                currentFunction_->instructions.push_back(std::move(ins));
                break;
            }
            case LiteralType::FLOAT: {
                double val = 0.0;
                if (lit->parserToken.token)
                    val = std::stod(lit->parserToken.token->token);
                emitLoadFloat(slot, val);
                break;
            }
            case LiteralType::BOŞ:
                // null literal → Null kind Value (ADR-021)
                { Instruction ins(Opcode::LOAD_CONST); ins.dest = slot; ins.intValue = 0;
                  currentFunction_->instructions.push_back(std::move(ins)); } // placeholder; VM'de Null üretmeli
                break;
        }
        return slot;
    }

    // ── Değişken ismi: n, first, second ... ──────────────────────────────
    case ASTKind::Identifier: {
        auto* id = (IdentifierNode*)node;
        std::string name = id->parserToken.token ? id->parserToken.token->token : "";

        if (isGlobal(name)) {
            int tempSlot = freshSlot();
            emitLoadGlobal(tempSlot, getGlobalIndex(name));
            return tempSlot;
        }
        return lookupVariable(name);
    }

    // ── İkili ifade: x + y, x = y, x < y ... ────────────────────────────
    case ASTKind::BinaryExpression: {
        auto* bin = (BinaryExpressionNode*)node;

        // Atama operatörleri: x = expr  ve  a[i] = expr
        if (bin->Operator == TokenType::EQUAL) {
            int rhsSlot = generateExpression(bin->Right);

            // a[i] = val → ARRAY_SET
            if (bin->Left && bin->Left->kind == ASTKind::IndexExpression) {
                auto* idx = (IndexExpressionNode*)bin->Left;
                int arrSlot = generateExpression(idx->object);
                int idxSlot = generateExpression(idx->index);
                emitArraySet(arrSlot, idxSlot, rhsSlot);
                return rhsSlot;
            }

            // p.field = val → FIELD_SET
            if (bin->Left && bin->Left->kind == ASTKind::MemberAccess) {
                auto* ma = (MemberAccessNode*)bin->Left;
                int objSlot = generateExpression(ma->object);
                std::string structName;
                if (auto* exprObj = dynamic_cast<ExpressionNode*>(ma->object))
                    structName = exprObj->resolvedType.structName;
                int idx2 = getStructFieldIndex(structName, ma->member);
                if (idx2 >= 0) emitFieldSet(objSlot, idx2, rhsSlot);
                return rhsSlot;
            }

            auto* lhsId = (IdentifierNode*)bin->Left;
            std::string varName = lhsId->parserToken.token->token;

            if (isGlobal(varName)) {
                emitStoreGlobal(rhsSlot, getGlobalIndex(varName));
                return rhsSlot;
            }

            int varSlot = lookupVariable(varName);
            if (rhsSlot != varSlot) emitLoadSlot(varSlot, rhsSlot);
            return varSlot;
        }

        // Birleşik atama: += -= *= /= %= &= |= <<= >>=
        // x OP= y  ≡  x = x OP y
        if (bin->Operator == TokenType::PLUS_EQUAL    ||
            bin->Operator == TokenType::MINUS_EQUAL   ||
            bin->Operator == TokenType::STAR_EQUAL    ||
            bin->Operator == TokenType::SLASH_EQUAL   ||
            bin->Operator == TokenType::PERCENT_EQUAL ||
            bin->Operator == TokenType::AMPERSAND_EQUAL ||
            bin->Operator == TokenType::PIPE_EQUAL      ||
            bin->Operator == TokenType::LSHIFT_EQUAL    ||
            bin->Operator == TokenType::RSHIFT_EQUAL) {

            auto* lhsId = (IdentifierNode*)bin->Left;
            std::string varName = lhsId->parserToken.token->token;
            int rhsSlot = generateExpression(bin->Right);

            Opcode arithOp = Opcode::ADD;
            if      (bin->Operator == TokenType::MINUS_EQUAL)    arithOp = Opcode::SUB;
            else if (bin->Operator == TokenType::STAR_EQUAL)     arithOp = Opcode::MUL;
            else if (bin->Operator == TokenType::SLASH_EQUAL)    arithOp = Opcode::DIV;
            else if (bin->Operator == TokenType::PERCENT_EQUAL)  arithOp = Opcode::MOD;
            else if (bin->Operator == TokenType::AMPERSAND_EQUAL) arithOp = Opcode::BAND;
            else if (bin->Operator == TokenType::PIPE_EQUAL)     arithOp = Opcode::BOR;
            else if (bin->Operator == TokenType::LSHIFT_EQUAL)   arithOp = Opcode::SHL;
            else if (bin->Operator == TokenType::RSHIFT_EQUAL)   arithOp = Opcode::SHR;

            int resultSlot = freshSlot();

            if (isGlobal(varName)) {
                int currentSlot = freshSlot();
                emitLoadGlobal(currentSlot, getGlobalIndex(varName));
                emitBinaryOp(arithOp, resultSlot, currentSlot, rhsSlot);
                emitStoreGlobal(resultSlot, getGlobalIndex(varName));
                return resultSlot;
            }

            int varSlot = lookupVariable(varName);
            emitBinaryOp(arithOp, resultSlot, varSlot, rhsSlot);
            emitLoadSlot(varSlot, resultSlot);
            return varSlot;
        }

        // Unary prefix: Left = nullptr (ör: -x, !x)
        if (!bin->Left) {
            int operandSlot = generateExpression(bin->Right);
            int resultSlot  = freshSlot();

            if (bin->Operator == TokenType::MINUS) {
                // -x → 0 - x
                int zeroSlot = freshSlot();
                emitLoadConst(zeroSlot, 0);
                emitBinaryOp(Opcode::SUB, resultSlot, zeroSlot, operandSlot);
            } else if (bin->Operator == TokenType::BANG) {
                // !x → (x == 0): sıfırsa 1, değilse 0
                int zeroSlot = freshSlot();
                emitLoadConst(zeroSlot, 0);
                emitBinaryOp(Opcode::EQUAL_EQUAL, resultSlot, operandSlot, zeroSlot);
            } else if (bin->Operator == TokenType::TILDE) {
                // ~x — bitsel değil
                Instruction ins(Opcode::BNOT);
                ins.dest = resultSlot;
                ins.src  = operandSlot;
                currentFunction_->instructions.push_back(std::move(ins));
            } else {
                emitLoadSlot(resultSlot, operandSlot);
            }
            return resultSlot;
        }

        // Aritmetik operatörler
        switch (bin->Operator) {
            case TokenType::PLUS:    return generateBinaryArithmetic(Opcode::ADD,           bin->Left, bin->Right);
            case TokenType::MINUS:   return generateBinaryArithmetic(Opcode::SUB,           bin->Left, bin->Right);
            case TokenType::STAR:    return generateBinaryArithmetic(Opcode::MUL,           bin->Left, bin->Right);
            case TokenType::SLASH:   return generateBinaryArithmetic(Opcode::DIV,           bin->Left, bin->Right);
            case TokenType::PERCENT: return generateBinaryArithmetic(Opcode::MOD,           bin->Left, bin->Right);
            // Karşılaştırma operatörleri
            case TokenType::LESS:          return generateBinaryArithmetic(Opcode::LESS,          bin->Left, bin->Right);
            case TokenType::LESS_EQUAL:    return generateBinaryArithmetic(Opcode::LESS_EQUAL,    bin->Left, bin->Right);
            case TokenType::GREATER:       return generateBinaryArithmetic(Opcode::GREATER,       bin->Left, bin->Right);
            case TokenType::GREATER_EQUAL: return generateBinaryArithmetic(Opcode::GREATER_EQUAL, bin->Left, bin->Right);
            case TokenType::EQUAL_EQUAL:   return generateBinaryArithmetic(Opcode::EQUAL_EQUAL,   bin->Left, bin->Right);
            case TokenType::BANG_EQUAL:    return generateBinaryArithmetic(Opcode::NOT_EQUAL,     bin->Left, bin->Right);

            // Bitsel operatörler
            case TokenType::AMPERSAND: return generateBinaryArithmetic(Opcode::BAND, bin->Left, bin->Right);
            case TokenType::PIPE:      return generateBinaryArithmetic(Opcode::BOR,  bin->Left, bin->Right);
            case TokenType::LSHIFT:    return generateBinaryArithmetic(Opcode::SHL,  bin->Left, bin->Right);
            case TokenType::RSHIFT:    return generateBinaryArithmetic(Opcode::SHR,  bin->Left, bin->Right);

            // Mantıksal operatörler: kısa devre dallanmasıyla üretilir (ADR-008).
            // NOT: sıradan ikili işlem değil — b, a'nın değerine göre atlanabilir.
            case TokenType::AMPERSAND_AMPERSAND: {
                int slotA  = generateExpression(bin->Left);
                int result = freshSlot();
                emitLoadConst(result, 0);               // varsayılan: false
                int skipB  = emitJumpIfFalse(slotA);    // a false → b'yi atla
                int slotB  = generateExpression(bin->Right);
                emitLoadSlot(result, slotB);            // result = b
                patchJump(skipB);
                return result;
            }
            case TokenType::PIPE_PIPE: {
                int slotA  = generateExpression(bin->Left);
                int result = freshSlot();
                emitLoadConst(result, 1);               // varsayılan: true
                int skipB  = emitJumpIfTrue(slotA);     // a true → b'yi atla
                int slotB  = generateExpression(bin->Right);
                emitLoadSlot(result, slotB);            // result = b
                patchJump(skipB);
                return result;
            }

            default: {
                // Bilinmeyen operatör — boş slot döndür
                int slot = freshSlot();
                emitLoadConst(slot, 0);
                return slot;
            }
        }
    }

    // ── Fonksiyon çağrısı: fibonacci(n-1), print(x) ... ─────────────────
    case ASTKind::Call: {
        auto* call = (CallExpressionNode*)node;

        // Hangi fonksiyon çağrılıyor? Callee bir Identifier
        std::string fnName;
        bool        isBuiltin = false;

        if (call->callee && call->callee->kind == ASTKind::Identifier) {
            auto* calleeId = (IdentifierNode*)call->callee;
            if (calleeId->parserToken.token) {
                fnName = calleeId->parserToken.token->token;
            }
            // Builtin kontrolü: resolvedSymbol->isBuiltin
            if (calleeId->resolvedSymbol && calleeId->resolvedSymbol->isBuiltin) {
                isBuiltin = true;
            }
        }

        // Her argümanı hesapla, sonuçların slot numaralarını topla
        std::vector<int> argSlots;
        for (ASTNode* arg : call->arguments) {
            argSlots.push_back(generateExpression(arg));
        }

        if (isBuiltin) {
            // CALLHOST: host (C++) fonksiyonu çağır (print gibi), dönüş değeri yok
            Instruction ins(Opcode::CALLHOST);
            ins.functionName = fnName;
            ins.argSlots     = argSlots;
            currentFunction_->instructions.push_back(std::move(ins));
            return -1; // Dönüş değeri yok
        } else {
            // CALL: saQut fonksiyonu çağır, sonucu yeni slota yaz
            int destSlot = freshSlot();
            Instruction ins(Opcode::CALL);
            ins.dest         = destSlot;
            ins.functionName = fnName;
            ins.argSlots     = argSlots;
            currentFunction_->instructions.push_back(std::move(ins));
            return destSlot;
        }
    }

    // ── Postfix: i++, i-- ────────────────────────────────────────────────
    case ASTKind::Postfix: {
        auto* pf = (PostfixNode*)node;
        // Şu anki değeri döndür, sonra artır/azalt
        int operandSlot = generateExpression(pf->operand);
        int resultSlot  = freshSlot(); // dönüş değeri (artırmadan önceki)
        emitLoadSlot(resultSlot, operandSlot);

        int oneSlot  = freshSlot();
        emitLoadConst(oneSlot, 1);
        int newSlot = freshSlot();

        if (pf->Operator == TokenType::PLUS_PLUS) {
            emitBinaryOp(Opcode::ADD, newSlot, operandSlot, oneSlot);
        } else {
            emitBinaryOp(Opcode::SUB, newSlot, operandSlot, oneSlot);
        }
        emitLoadSlot(operandSlot, newSlot); // orijinal değişkeni güncelle
        return resultSlot;                  // artırmadan önceki değer
    }

    // ── Üye erişimi okuma: p.x ───────────────────────────────────────────
    case ASTKind::MemberAccess: {
        auto* ma     = (MemberAccessNode*)node;
        int objSlot  = generateExpression(ma->object);
        int destSlot = freshSlot();
        // Nesnenin struct adını resolvedType üstünden al (tip denetleyici yazdı)
        std::string structName;
        if (auto* exprObj = dynamic_cast<ExpressionNode*>(ma->object))
            structName = exprObj->resolvedType.structName;
        int idx = getStructFieldIndex(structName, ma->member);
        if (idx >= 0) emitFieldGet(destSlot, objSlot, idx);
        return destSlot;
    }
    case ASTKind::ArrayLiteral: {
        auto* al = (ArrayLiteralNode*)node;
        int arrSlot = freshSlot();
        emitArrayNew(arrSlot, (int)al->elements.size());
        for (int i = 0; i < (int)al->elements.size(); i++) {
            int idxSlot = freshSlot();
            emitLoadConst(idxSlot, i);
            int valSlot = generateExpression(al->elements[i]);
            emitArraySet(arrSlot, idxSlot, valSlot);
        }
        return arrSlot;
    }

    // ── Index erişimi okuma: a[i] ─────────────────────────────────────────
    case ASTKind::IndexExpression: {
        auto* idx = (IndexExpressionNode*)node;
        int arrSlot  = generateExpression(idx->object);
        int idxSlot  = generateExpression(idx->index);
        int destSlot = freshSlot();
        emitArrayGet(destSlot, arrSlot, idxSlot);
        return destSlot;
    }

    default:
        // Bilinmeyen ifade türü
        return freshSlot(); // boş slot (0 değeriyle)
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// generateBinaryArithmetic — İkili op için sol+sağ üret, talimat ekle
// ─────────────────────────────────────────────────────────────────────────────

int IRGenerator::generateBinaryArithmetic(Opcode opcode, ASTNode* leftNode, ASTNode* rightNode) {
    int leftSlot  = generateExpression(leftNode);
    int rightSlot = generateExpression(rightNode);
    int destSlot  = freshSlot();

    // Float tip kontrolü — resolvedType üstünden (tip denetleyici tarafından yazıldı)
    bool leftIsFloat  = false, rightIsFloat = false;
    if (auto* e = dynamic_cast<ExpressionNode*>(leftNode))
        leftIsFloat  = e->resolvedType.isPrimitive() &&
                       (e->resolvedType.prim == PrimitiveKind::Float ||
                        e->resolvedType.prim == PrimitiveKind::Double);
    if (auto* e = dynamic_cast<ExpressionNode*>(rightNode))
        rightIsFloat = e->resolvedType.isPrimitive() &&
                       (e->resolvedType.prim == PrimitiveKind::Float ||
                        e->resolvedType.prim == PrimitiveKind::Double);

    if (leftIsFloat || rightIsFloat) {
        // Int operandı float'a çevir
        if (!leftIsFloat) {
            int conv = freshSlot();
            emitIntToFloat(conv, leftSlot);
            leftSlot = conv;
        }
        if (!rightIsFloat) {
            int conv = freshSlot();
            emitIntToFloat(conv, rightSlot);
            rightSlot = conv;
        }
        // Float opcode eşleştirmesi
        Opcode floatOp = opcode;
        if      (opcode == Opcode::ADD) floatOp = Opcode::FADD;
        else if (opcode == Opcode::SUB) floatOp = Opcode::FSUB;
        else if (opcode == Opcode::MUL) floatOp = Opcode::FMUL;
        else if (opcode == Opcode::DIV) floatOp = Opcode::FDIV;
        // karşılaştırma opcodeları aynı kalır (LESS, GREATER, vb.)
        emitBinaryOp(floatOp, destSlot, leftSlot, rightSlot);
    } else {
        emitBinaryOp(opcode, destSlot, leftSlot, rightSlot);
    }
    return destSlot;
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot yönetimi
// ─────────────────────────────────────────────────────────────────────────────

int IRGenerator::freshSlot() {
    return nextSlot_++;
}

void IRGenerator::registerVariable(const std::string& name, int slot) {
    nameToSlot_[name] = slot;
}

int IRGenerator::lookupVariable(const std::string& name) {
    auto it = nameToSlot_.find(name);
    if (it == nameToSlot_.end()) {
        // Bu noktaya normalde gelinmemeli; sembol toplayıcı E001 üretmiş olur.
        // Yine de çökmemek için 0 döndür.
        return 0;
    }
    return it->second;
}

// ─────────────────────────────────────────────────────────────────────────────
// Talimat yazma yardımcıları
// ─────────────────────────────────────────────────────────────────────────────

void IRGenerator::emitLoadConst(int destSlot, int value) {
    Instruction ins(Opcode::LOAD_CONST);
    ins.dest     = destSlot;
    ins.intValue = value;
    currentFunction_->instructions.push_back(std::move(ins));
}

void IRGenerator::emitLoadSlot(int destSlot, int srcSlot) {
    Instruction ins(Opcode::LOAD_SLOT);
    ins.dest = destSlot;
    ins.src  = srcSlot;
    currentFunction_->instructions.push_back(std::move(ins));
}

void IRGenerator::emitLoadGlobal(int destSlot, int globalIndex) {
    Instruction ins(Opcode::LOAD_GLOBAL);
    ins.dest     = destSlot;
    ins.intValue = globalIndex;
    currentFunction_->instructions.push_back(std::move(ins));
}

void IRGenerator::emitStoreGlobal(int srcSlot, int globalIndex) {
    Instruction ins(Opcode::STORE_GLOBAL);
    ins.src      = srcSlot;
    ins.intValue = globalIndex;
    currentFunction_->instructions.push_back(std::move(ins));
}

void IRGenerator::emitLoadFloat(int destSlot, double value) {
    Instruction ins(Opcode::LOAD_FLOAT);
    ins.dest       = destSlot;
    ins.floatValue = value;
    currentFunction_->instructions.push_back(std::move(ins));
}

void IRGenerator::emitIntToFloat(int destSlot, int srcSlot) {
    Instruction ins(Opcode::INT_TO_FLOAT);
    ins.dest = destSlot;
    ins.src  = srcSlot;
    currentFunction_->instructions.push_back(std::move(ins));
}

void IRGenerator::emitStructNew(int destSlot, const std::string& structType, int fieldCount) {
    Instruction ins(Opcode::STRUCT_NEW);
    ins.dest         = destSlot;
    ins.intValue     = fieldCount;
    ins.functionName = structType; // struct tip adı
    currentFunction_->instructions.push_back(std::move(ins));
}

void IRGenerator::emitFieldGet(int destSlot, int objSlot, int fieldIdx) {
    Instruction ins(Opcode::FIELD_GET);
    ins.dest     = destSlot;
    ins.src      = objSlot;
    ins.intValue = fieldIdx;
    currentFunction_->instructions.push_back(std::move(ins));
}

void IRGenerator::emitFieldSet(int objSlot, int fieldIdx, int valSlot) {
    Instruction ins(Opcode::FIELD_SET);
    ins.dest     = objSlot;
    ins.intValue = fieldIdx;
    ins.right    = valSlot;
    currentFunction_->instructions.push_back(std::move(ins));
}

void IRGenerator::emitArrayNew(int destSlot, int capacity) {
    Instruction ins(Opcode::ARRAY_NEW);
    ins.dest     = destSlot;
    ins.intValue = capacity;
    currentFunction_->instructions.push_back(std::move(ins));
}

void IRGenerator::emitArrayGet(int destSlot, int arrSlot, int idxSlot) {
    Instruction ins(Opcode::ARRAY_GET);
    ins.dest  = destSlot;
    ins.left  = arrSlot;
    ins.right = idxSlot;
    currentFunction_->instructions.push_back(std::move(ins));
}

void IRGenerator::emitArraySet(int arrSlot, int idxSlot, int valSlot) {
    Instruction ins(Opcode::ARRAY_SET);
    ins.dest  = arrSlot;
    ins.left  = idxSlot;
    ins.right = valSlot;
    currentFunction_->instructions.push_back(std::move(ins));
}

void IRGenerator::emitArrayLen(int destSlot, int arrSlot) {
    Instruction ins(Opcode::ARRAY_LEN);
    ins.dest = destSlot;
    ins.src  = arrSlot;
    currentFunction_->instructions.push_back(std::move(ins));
}

int IRGenerator::getStructFieldIndex(const std::string& structType, const std::string& fieldName) const {
    auto it = structLayouts_.find(structType);
    if (it == structLayouts_.end()) return -1;
    for (int i = 0; i < (int)it->second.size(); i++)
        if (it->second[i].first == fieldName) return i;
    return -1;
}

int IRGenerator::getStructFieldCount(const std::string& structType) const {
    auto it = structLayouts_.find(structType);
    if (it == structLayouts_.end()) return 0;
    return (int)it->second.size();
}

bool IRGenerator::isGlobal(const std::string& name) const {
    return nameToGlobal_.count(name) > 0;
}

int IRGenerator::getGlobalIndex(const std::string& name) const {
    auto it = nameToGlobal_.find(name);
    return (it != nameToGlobal_.end()) ? it->second : -1;
}

void IRGenerator::emitBinaryOp(Opcode op, int destSlot, int leftSlot, int rightSlot) {
    Instruction ins(op);
    ins.dest  = destSlot;
    ins.left  = leftSlot;
    ins.right = rightSlot;
    currentFunction_->instructions.push_back(std::move(ins));
}

void IRGenerator::emitReturn(int srcSlot) {
    Instruction ins(Opcode::RETURN);
    ins.src = srcSlot;
    currentFunction_->instructions.push_back(std::move(ins));
}

int IRGenerator::emitJumpUnconditional(int targetInstrIndex) {
    Instruction ins(Opcode::JMP);
    ins.jumpTarget = targetInstrIndex;
    currentFunction_->instructions.push_back(std::move(ins));
    return (int)currentFunction_->instructions.size() - 1;
}

int IRGenerator::emitJumpIfFalse(int condSlot) {
    Instruction ins(Opcode::JIF_FALSE);
    ins.cond       = condSlot;
    ins.jumpTarget = -1; // henüz bilinmiyor — patchJump() bekliyor
    currentFunction_->instructions.push_back(std::move(ins));
    return (int)currentFunction_->instructions.size() - 1;
}

int IRGenerator::emitJumpIfTrue(int condSlot) {
    Instruction ins(Opcode::JIF_TRUE);
    ins.cond       = condSlot;
    ins.jumpTarget = -1;
    currentFunction_->instructions.push_back(std::move(ins));
    return (int)currentFunction_->instructions.size() - 1;
}

void IRGenerator::patchJump(int instrIndex) {
    // instrIndex'teki JMP veya JIF_FALSE'un hedefini şu anki konuma doldur
    currentFunction_->instructions[instrIndex].jumpTarget = currentInstrIndex();
}

int IRGenerator::currentInstrIndex() const {
    return (int)currentFunction_->instructions.size();
}
