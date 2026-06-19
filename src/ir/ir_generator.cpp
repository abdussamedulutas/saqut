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

IRProgram IRGenerator::generate(ASTNode* programNode, SymbolTable& /*symbolTable*/) {
    IRProgram program;

    // Üst düzey global değişken bildirimlerini topla (#38)
    std::vector<ASTNode*> globalVars;
    for (ASTNode* child : programNode->getChildren()) {
        if (child->kind == ASTKind::VariableDecl)
            globalVars.push_back(child);
    }

    for (ASTNode* child : programNode->getChildren()) {
        if (child->kind == ASTKind::FunctionDecl) {
            nameToSlot_.clear();
            nextSlot_ = 0;

            auto* fnDecl = (FunctionDeclNode*)child;
            IRFunction irFn(fnDecl->name, (int)fnDecl->params.size());
            program.addFunction(std::move(irFn));
            currentFunction_ = program.findFunction(fnDecl->name);

            // Global değişkenleri main'in başına ekle (Seçenek B)
            if (fnDecl->name == "main") {
                for (ASTNode* gv : globalVars)
                    generateStatement(gv);
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

    // ── Değişken bildirimi: int x = <ifade> ──────────────────────────────
    case ASTKind::VariableDecl: {
        auto* vd = (VariableDeclNode*)node;

        // Bu değişken için yeni bir slot ayır
        int varSlot = freshSlot();
        registerVariable(vd->name, varSlot);

        if (vd->initExpr) {
            // Başlatma ifadesini üret, sonucu bir slotta al
            int initSlot = generateExpression(vd->initExpr);

            if (initSlot != varSlot) {
                // Sonuç başka bir slotta, değişkenin slotuna kopyala
                emitLoadSlot(varSlot, initSlot);
            }
            // initSlot == varSlot: LOAD_CONST doğrudan varSlot'a yazıldı, kopya gerekmez
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
                int value = 0;
                if (lit->hasDirectValue)
                    value = lit->directIntValue;
                else if (lit->parserToken.token)
                    value = std::stoi(lit->parserToken.token->token);
                emitLoadConst(slot, value);
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
            case LiteralType::FLOAT:
                throw std::runtime_error(
                    "IR üretim hatası: float literal şu an VM tarafından desteklenmiyor. "
                    "Tam sayı kullanın veya float desteği eklenene kadar bekleyin.");
            case LiteralType::BOŞ:
                throw std::runtime_error(
                    "IR üretim hatası: null literal şu an VM tarafından desteklenmiyor.");
        }
        return slot;
    }

    // ── Değişken ismi: n, first, second ... ──────────────────────────────
    // Bu değişkenin değeri zaten bir slotta. O slotu döndür.
    case ASTKind::Identifier: {
        auto* id = (IdentifierNode*)node;
        std::string name = id->parserToken.token ? id->parserToken.token->token : "";

        // Önce builtin mi? (print gibi) — identifier olarak gelen builtin fonksiyon
        // çağrıları CallExpression içinde yakalanıyor, burada sadece değişken kalır
        return lookupVariable(name);
    }

    // ── İkili ifade: x + y, x = y, x < y ... ────────────────────────────
    case ASTKind::BinaryExpression: {
        auto* bin = (BinaryExpressionNode*)node;

        // Atama operatörleri: x = expr, x += expr ...
        // Sol taraf bir değişken, sağ taraf hesaplanır ve o değişkene yazılır.
        if (bin->Operator == TokenType::EQUAL) {
            // Sağ tarafı hesapla
            int rhsSlot = generateExpression(bin->Right);

            // Sol taraf değişkenin slotunu bul
            auto* lhsId = (IdentifierNode*)bin->Left;
            std::string varName = lhsId->parserToken.token->token;
            int varSlot = lookupVariable(varName);

            // Sonucu değişkenin slotuna kopyala
            if (rhsSlot != varSlot) {
                emitLoadSlot(varSlot, rhsSlot);
            }
            return varSlot;
        }

        // Birleşik atama: += -= *= /= %=
        // x OP= y  ≡  x = x OP y
        if (bin->Operator == TokenType::PLUS_EQUAL    ||
            bin->Operator == TokenType::MINUS_EQUAL   ||
            bin->Operator == TokenType::STAR_EQUAL    ||
            bin->Operator == TokenType::SLASH_EQUAL   ||
            bin->Operator == TokenType::PERCENT_EQUAL) {

            auto* lhsId = (IdentifierNode*)bin->Left;
            std::string varName = lhsId->parserToken.token->token;
            int varSlot = lookupVariable(varName);
            int rhsSlot = generateExpression(bin->Right);

            Opcode arithOp = Opcode::ADD;
            if      (bin->Operator == TokenType::MINUS_EQUAL)   arithOp = Opcode::SUB;
            else if (bin->Operator == TokenType::STAR_EQUAL)    arithOp = Opcode::MUL;
            else if (bin->Operator == TokenType::SLASH_EQUAL)   arithOp = Opcode::DIV;
            else if (bin->Operator == TokenType::PERCENT_EQUAL) arithOp = Opcode::MOD;

            int resultSlot = freshSlot();
            emitBinaryOp(arithOp, resultSlot, varSlot, rhsSlot);
            emitLoadSlot(varSlot, resultSlot);
            return varSlot;
        }

        // Unary prefix: Left = nullptr (ör: -x, !x)
        if (!bin->Left) {
            int operandSlot = generateExpression(bin->Right);
            int resultSlot  = freshSlot();

            if (bin->Operator == TokenType::MINUS) {
                int zeroSlot = freshSlot();
                emitLoadConst(zeroSlot, 0);
                emitBinaryOp(Opcode::SUB, resultSlot, zeroSlot, operandSlot);
            } else if (bin->Operator == TokenType::BANG) {
                Instruction ins(Opcode::NOT_UNARY);
                ins.dest = resultSlot; ins.src = operandSlot;
                currentFunction_->instructions.push_back(std::move(ins));
            } else if (bin->Operator == TokenType::TILDE) {
                Instruction ins(Opcode::BIT_NOT);
                ins.dest = resultSlot; ins.src = operandSlot;
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
            // Bitsel
            case TokenType::AMPERSAND:     return generateBinaryArithmetic(Opcode::BIT_AND, bin->Left, bin->Right);
            case TokenType::PIPE:          return generateBinaryArithmetic(Opcode::BIT_OR,  bin->Left, bin->Right);
            case TokenType::LSHIFT:        return generateBinaryArithmetic(Opcode::BIT_SHL, bin->Left, bin->Right);
            case TokenType::RSHIFT:        return generateBinaryArithmetic(Opcode::BIT_SHR, bin->Left, bin->Right);
            // Mantıksal: kısa devre dallanmasıyla üretilir (ADR-008)
            case TokenType::AMPERSAND_AMPERSAND: {
                int slotA  = generateExpression(bin->Left);
                int result = freshSlot();
                emitLoadConst(result, 0);
                int skipB  = emitJumpIfFalse(slotA);
                int slotB  = generateExpression(bin->Right);
                emitLoadSlot(result, slotB);
                patchJump(skipB);
                return result;
            }
            case TokenType::PIPE_PIPE: {
                int slotA  = generateExpression(bin->Left);
                int result = freshSlot();
                emitLoadConst(result, 1);
                int skipB  = emitJumpIfTrue(slotA);
                int slotB  = generateExpression(bin->Right);
                emitLoadSlot(result, slotB);
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
    emitBinaryOp(opcode, destSlot, leftSlot, rightSlot);
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
