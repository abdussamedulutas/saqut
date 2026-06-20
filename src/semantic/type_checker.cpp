#include "semantic/type_checker.hpp"
#include "parser/nodes/program.hpp"
#include "parser/nodes/declarations.hpp"
#include "parser/nodes/statements.hpp"
#include "parser/nodes/expressions.hpp"
#include "parser/nodes/binary_expr.hpp"
#include "parser/nodes/identifier.hpp"
#include "parser/nodes/literal.hpp"
#include <cmath>
#include <climits>

// ─────────────────────────────────────────────────────────────────────────────
// Yardımcılar
// ─────────────────────────────────────────────────────────────────────────────

int TypeChecker::numericRank(const Type& t) {
    if (!t.isPrimitive()) return -1;
    switch (t.prim) {
        case PrimitiveKind::Int:    return 0;
        case PrimitiveKind::Float:  return 1;
        case PrimitiveKind::Double: return 2;
        default: return -1;
    }
}

// ADR-021: "a != null" / "a == null" kalıbını ayrıştır
// Dönüş: {varName, isNotNull}  — varName boşsa kalıp tanınmadı.
std::pair<std::string, bool> TypeChecker::extractNullCheck(ASTNode* cond) {
    if (!cond || cond->kind != ASTKind::BinaryExpression) return {"", false};
    auto* bin = (BinaryExpressionNode*)cond;
    bool isNE = (bin->Operator == TokenType::BANG_EQUAL);
    bool isEE = (bin->Operator == TokenType::EQUAL_EQUAL);
    if (!isNE && !isEE) return {"", false};

    // Hangi taraf null literal?
    auto isNullLit = [](ASTNode* n) -> bool {
        if (!n || n->kind != ASTKind::Literal) return false;
        return ((LiteralNode*)n)->literalType == LiteralType::BOŞ;
    };
    auto identName = [](ASTNode* n) -> std::string {
        if (!n || n->kind != ASTKind::Identifier) return "";
        auto* id = (IdentifierNode*)n;
        return id->parserToken.token ? id->parserToken.token->token : "";
    };

    std::string var;
    if (isNullLit(bin->Right)) var = identName(bin->Left);
    else if (isNullLit(bin->Left)) var = identName(bin->Right);
    if (var.empty()) return {"", false};
    return {var, isNE}; // isNE=true → "a != null"; false → "a == null"
}

// ADR-021: guard pattern — bu statement her zaman çıkış yapıyor mu?
bool TypeChecker::alwaysExits(ASTNode* stmt) {
    if (!stmt) return false;
    switch (stmt->kind) {
        case ASTKind::ReturnStatement:
        case ASTKind::ThrowStatement:
        case ASTKind::BreakStatement:
        case ASTKind::ContinueStatement:
            return true;
        case ASTKind::Block: {
            auto& ch = stmt->getChildren();
            return !ch.empty() && alwaysExits(ch.back());
        }
        default: return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// check — giriş noktası
// ─────────────────────────────────────────────────────────────────────────────

void TypeChecker::check(ASTNode* program) {
    if (!program) return;
    for (ASTNode* child : program->getChildren()) {
        switch (child->kind) {
            case ASTKind::FunctionDecl: checkFunction(child); break;
            case ASTKind::VariableDecl: checkStmt(child);     break;
            default: break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// checkFunction
// ─────────────────────────────────────────────────────────────────────────────

void TypeChecker::checkFunction(ASTNode* fnNode) {
    auto* fn = (FunctionDeclNode*)fnNode;
    inFunction_        = true;
    currentReturnType_ = Type::fromName(fn->returnType);
    if (currentReturnType_.isError() && fn->returnType != "void")
        currentReturnType_ = Type::Void(); // bilinmeyen dönüş tipi → void gibi davran

    auto& ch = fn->getChildren();
    if (!ch.empty()) checkStmt(ch[0]); // body Block

    inFunction_ = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// checkAssign — atama uyumu + uyarı/hata raporlama
// ─────────────────────────────────────────────────────────────────────────────

bool TypeChecker::checkAssign(const Type& target, const Type& src,
                               bool srcIsLiteral,
                               const SourceLocation& loc,
                               const std::string& ctx) {
    if (target.isError() || src.isError()) return true; // önceki hata, sessiz geç

    // ADR-021: null literal ataması
    if (src.isNullLiteral()) {
        if (target.nullable) return true;  // T? ← null → OK
        diag_.report("E003", loc,
            "'" + ctx + "': null non-null tipine (" + target.toString() + ") atanamaz");
        return false;
    }

    // ADR-021: nullable uyumu
    // T? ← T  → OK (widening: non-null, nullable'a gider)
    // T  ← T? → E  (narrowing: nullable, non-null'a gidemez; narrowing gerekli)
    if (src.nullable && !target.nullable && src.equalsBase(target)) {
        diag_.report("E003", loc,
            "'" + ctx + "': " + src.toString() +
            " nullable tipi non-null " + target.toString() + " tipine atanamaz"
            " (if ile null kontrolü yapın)");
        return false;
    }
    // T? ← T → OK (equalsBase eşleşiyorsa, nullable farkı widening)
    if (!src.nullable && target.nullable && src.equalsBase(target)) return true;

    if (target.equals(src)) return true;

    int tRank = numericRank(target);
    int sRank = numericRank(src);

    if (tRank >= 0 && sRank >= 0) {
        if (tRank > sRank) {
            if (srcIsLiteral) return true;
            diag_.report("W004", loc,
                "'" + ctx + "': " + src.toString() +
                " → " + target.toString() + " örtük genişletme");
            return true;
        } else {
            diag_.report("E003", loc,
                "'" + ctx + "': " + src.toString() +
                " → " + target.toString() + " daraltma (veri kaybı)");
            return false;
        }
    }

    diag_.report("E003", loc,
        "'" + ctx + "': " + src.toString() +
        " tipi " + target.toString() + " tipine atanamaz");
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// checkStmt
// ─────────────────────────────────────────────────────────────────────────────

void TypeChecker::checkStmt(ASTNode* node) {
    if (!node) return;

    switch (node->kind) {

    case ASTKind::Block: {
        // ADR-021: guard/sıralı narrowing — if (a == null) return; → sonrasında a non-null
        std::vector<std::string> guardNarrowed; // bu blokta guard'la daraltılanlar
        for (ASTNode* child : node->getChildren()) {
            checkStmt(child);
            // guard kontrolü: if (a == null) { return/throw/break/continue; }
            if (child->kind == ASTKind::IfStatement) {
                auto* ifn = (IfStatementNode*)child;
                if (!ifn->elseBranch && ifn->thenBranch && alwaysExits(ifn->thenBranch)) {
                    auto [var, isNotNull] = extractNullCheck(ifn->condition);
                    if (!var.empty() && !isNotNull) { // "a == null" → guard
                        narrowedNonNull_.insert(var);
                        guardNarrowed.push_back(var);
                    }
                }
            }
        }
        for (auto& v : guardNarrowed) narrowedNonNull_.erase(v);
        break;
    }

    case ASTKind::VariableDecl: {
        auto* vd = (VariableDeclNode*)node;
        Type targetType = Type::fromName(vd->varType);
        if (targetType.isError() && table_.structLayouts.count(vd->varType))
            targetType = Type::structType(vd->varType);
        if (vd->initExpr) {
            Type srcType = checkExpr(vd->initExpr, targetType);
            bool isLit   = vd->initExpr->kind == ASTKind::Literal;
            checkAssign(targetType, srcType, isLit, vd->loc, vd->name);
        }
        // sibling VariableDecl'ler (int a, b;)
        for (ASTNode* sib : vd->getChildren()) {
            if (sib->kind == ASTKind::VariableDecl) checkStmt(sib);
        }
        break;
    }

    case ASTKind::ExpressionStatement: {
        auto* es = (ExpressionStatementNode*)node;
        if (es->expression) checkExpr(es->expression);
        break;
    }

    case ASTKind::ReturnStatement: {
        auto* rs = (ReturnStatementNode*)node;
        if (!rs->value) {
            if (inFunction_ && !currentReturnType_.isVoid())
                diag_.report("E006", rs->loc,
                    "Değersiz return; fonksiyon " +
                    currentReturnType_.toString() + " döndürmeli");
            break;
        }
        Type valType = checkExpr(rs->value, currentReturnType_);
        bool isLit   = rs->value->kind == ASTKind::Literal;
        checkAssign(currentReturnType_, valType, isLit, rs->loc, "return");
        break;
    }

    case ASTKind::IfStatement: {
        auto* ifn = (IfStatementNode*)node;
        if (ifn->condition) checkExpr(ifn->condition);

        // ADR-021: nested narrowing — if (a != null) { a non-null } else { a null }
        auto [narrowVar, isNotNull] = extractNullCheck(ifn->condition);

        if (!narrowVar.empty() && isNotNull) // "a != null" → then'de non-null
            narrowedNonNull_.insert(narrowVar);
        if (ifn->thenBranch) checkStmt(ifn->thenBranch);
        if (!narrowVar.empty() && isNotNull)
            narrowedNonNull_.erase(narrowVar);

        if (!narrowVar.empty() && !isNotNull) // "a == null" → else'de non-null
            narrowedNonNull_.insert(narrowVar);
        if (ifn->elseBranch) checkStmt(ifn->elseBranch);
        if (!narrowVar.empty() && !isNotNull)
            narrowedNonNull_.erase(narrowVar);

        break;
    }

    case ASTKind::WhileStatement: {
        auto* ws = (WhileStatementNode*)node;
        if (ws->condition) checkExpr(ws->condition);
        if (ws->body)      checkStmt(ws->body);
        break;
    }

    case ASTKind::ForStatement: {
        auto* fs = (ForStatementNode*)node;
        if (fs->init) {
            if (fs->init->kind == ASTKind::VariableDecl) checkStmt(fs->init);
            else checkExpr(fs->init);
        }
        if (fs->condition) checkExpr(fs->condition);
        if (fs->update)    checkExpr(fs->update);
        if (fs->body)      checkStmt(fs->body);
        break;
    }

    case ASTKind::DoWhileStatement: {
        auto* dw = (DoWhileStatementNode*)node;
        if (dw->body)      checkStmt(dw->body);
        if (dw->condition) checkExpr(dw->condition);
        break;
    }

    case ASTKind::BreakStatement:
    case ASTKind::ContinueStatement:
        break; // yapısal doğrulama StructuralValidator'ın işi

    // ADR-025: try { body } catch (Error e) { handler }
    case ASTKind::TryStatement: {
        auto* ts = (TryStatementNode*)node;
        if (ts->body)    checkStmt(ts->body);
        if (ts->handler) checkStmt(ts->handler);
        break;
    }

    // ADR-025: throw <ifade>; — unchecked, herhangi bir değer atılabilir
    case ASTKind::ThrowStatement: {
        auto* th = (ThrowStatementNode*)node;
        if (th->value) checkExpr(th->value);
        break;
    }

    // ADR-027: switch (expr) { case v1, v2: ... default: ... }
    case ASTKind::SwitchStatement: {
        auto* sw = (SwitchStatementNode*)node;
        if (!sw->subject) break;

        Type subjectType = checkExpr(sw->subject);
        // nullable T? → base tip ile karşılaştırma yapılır; case null: izin verilir
        Type baseType = subjectType;
        baseType.nullable = false;

        // Geçerli switch tipleri: int, float, bool, string (aggregate değil)
        bool subjectOk = baseType.isPrimitive() || baseType.isString()
                         || baseType.isVoid(); // void = bilinmeyen, hata zaten raporlandı
        if (!subjectOk && !baseType.isError()) {
            diag_.report("E003", sw->subject->loc,
                "switch konusu '" + subjectType.toString() +
                "' tipi desteklenmiyor (int/float/bool/string bekleniyor)");
        }

        for (auto& clause : sw->cases) {
            if (clause.isDefault) {
                for (auto* s : clause.body) checkStmt(s);
                continue;
            }
            for (auto* val : clause.values) {
                if (!val) continue;
                // case null: yalnızca nullable subject ile geçerli
                bool isNullLit = (val->kind == ASTKind::Literal &&
                                  ((LiteralNode*)val)->literalType == LiteralType::BOŞ);
                if (isNullLit) {
                    if (!subjectType.nullable)
                        diag_.report("E003", val->loc,
                            "case null: yalnızca nullable (T?) switch konusuyla kullanılabilir");
                    continue;
                }

                Type caseType = checkExpr(val, baseType);
                // Tip homojenliği: case değeri konuyla aynı base tipte olmalı
                if (!caseType.isError() && !baseType.isError() &&
                    !baseType.isVoid() && !caseType.equalsBase(baseType)) {
                    diag_.report("E003", val->loc,
                        "case değeri '" + caseType.toString() +
                        "' switch konusu tipiyle (" + baseType.toString() + ") uyumsuz");
                }

                // ADR-027: float case → tam-temsil edilemeyen literal uyarısı
                if (baseType.isPrimitive() &&
                    (baseType.prim == PrimitiveKind::Float ||
                     baseType.prim == PrimitiveKind::Double) &&
                    val->kind == ASTKind::Literal) {
                    auto* lit = (LiteralNode*)val;
                    if (lit->literalType == LiteralType::FLOAT && lit->lexerToken) {
                        const std::string& raw = lit->lexerToken->token;
                        // Tam-temsil kontrolü: 10^m paydasını 5^m'ye bölebilir miyiz?
                        // Kesirli basamakları bul, son sıfırları temizle
                        size_t dotPos = raw.find('.');
                        if (dotPos != std::string::npos) {
                            std::string frac = raw.substr(dotPos + 1);
                            // Üstel kısım varsa at (e/E sonrası) — o zaman genelde tam
                            size_t ePos = frac.find_first_of("eE");
                            if (ePos != std::string::npos) frac = frac.substr(0, ePos);
                            while (!frac.empty() && frac.back() == '0') frac.pop_back();
                            if (!frac.empty()) {
                                // Payda = 10^m; tam temsil için pay 5^m'ye bölünebilmeli
                                int m = (int)frac.size();
                                // Tüm basamakları tamsayı olarak al
                                std::string allDigits = raw.substr(0, dotPos) + frac;
                                while (allDigits.size() > 1 && allDigits[0] == '0')
                                    allDigits = allDigits.substr(1);
                                long long num = 0;
                                bool overflow = false;
                                for (char ch : allDigits) {
                                    if (num > (LLONG_MAX - (ch-'0')) / 10) { overflow = true; break; }
                                    num = num * 10 + (ch - '0');
                                }
                                long long fivePow = 1;
                                for (int i = 0; i < m && !overflow; i++) {
                                    if (fivePow > LLONG_MAX / 5) { overflow = true; break; }
                                    fivePow *= 5;
                                }
                                bool exact = !overflow && (num % fivePow == 0);
                                if (!exact)
                                    diag_.report("W005", val->loc,
                                        "case " + raw + ": bu float değeri IEEE 754'te tam temsil "
                                        "edilemez; karşılaştırma beklendik sonuç vermeyebilir");
                            }
                        }
                    }
                }
            }
            for (auto* s : clause.body) checkStmt(s);
        }
        break;
    }

    default:
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// checkExpr — tip çıkarımı + resolvedType ataması
// ─────────────────────────────────────────────────────────────────────────────

Type TypeChecker::checkExpr(ASTNode* node, const Type& expected) {
    if (!node) return Type::error();

    Type result = Type::error();

    switch (node->kind) {

    // ── Literal ────────────────────────────────────────────────────────────
    case ASTKind::Literal: {
        auto* lit = (LiteralNode*)node;
        int expRank = numericRank(expected);

        switch (lit->literalType) {
            case LiteralType::INTEGER:
                // Bağlam daha geniş sayısal tip ise literal o tip olarak tiplenir.
                if (expRank > 0) result = expected; // float veya double bekleniyor
                else             result = Type::Int();
                break;
            case LiteralType::FLOAT:
                // float literal → double bağlamında double olur; int bağlamında E003.
                if (!expected.isError() && expected.equals(Type::Double()))
                    result = Type::Double();
                else if (!expected.isError() && numericRank(expected) == 0) {
                    // int bekleniyor ama float literal: E003
                    diag_.report("E003", lit->loc,
                        "Float literal int bağlamında kullanılamaz (veri kaybı)");
                    result = Type::error();
                } else {
                    result = Type::Float();
                }
                break;
            case LiteralType::BOOLEAN: result = Type::Bool();   break;
            case LiteralType::STRING:  result = Type::String(); break;
            case LiteralType::BOŞ:
                // null literal: bağlam nullable ise o tip, değilse Void+nullable (null sentinel)
                if (!expected.isError() && expected.nullable)
                    result = expected;
                else
                    result = Type::Void().asNullable(); // null sentinel
                break;
            default: result = Type::error(); break;
        }
        break;
    }

    // ── Identifier ─────────────────────────────────────────────────────────
    case ASTKind::Identifier: {
        auto* id = (IdentifierNode*)node;
        result = id->resolvedSymbol ? id->resolvedSymbol->type : Type::error();
        // ADR-021: narrowing — bu değişken null kontrolünden geçtiyse non-null say
        if (result.nullable && id->parserToken.token) {
            std::string name = id->parserToken.token->token;
            if (narrowedNonNull_.count(name))
                result = result.asNonNull();
        }
        break;
    }

    // ── BinaryExpression ───────────────────────────────────────────────────
    case ASTKind::BinaryExpression: {
        auto* bin = (BinaryExpressionNode*)node;

        // Atama operatörleri
        if (bin->Operator == TokenType::EQUAL       ||
            bin->Operator == TokenType::PLUS_EQUAL  ||
            bin->Operator == TokenType::MINUS_EQUAL ||
            bin->Operator == TokenType::STAR_EQUAL  ||
            bin->Operator == TokenType::SLASH_EQUAL ||
            bin->Operator == TokenType::PERCENT_EQUAL) {
            Type leftType  = checkExpr(bin->Left);
            Type rightType = checkExpr(bin->Right, leftType);
            bool isLit     = bin->Right && bin->Right->kind == ASTKind::Literal;
            checkAssign(leftType, rightType, isLit, bin->loc, "atama");
            result = leftType;
            break;
        }

        // Unary (Left = nullptr): -, +, !, ~
        if (!bin->Left) {
            Type rightType = checkExpr(bin->Right);
            if (bin->Operator == TokenType::BANG) {
                result = Type::Bool();
            } else {
                result = rightType.isNumeric() ? rightType : Type::error();
                if (result.isError() && !rightType.isError())
                    diag_.report("E003", bin->loc, "Sayısal olmayan operand");
            }
            break;
        }

        Type leftType  = checkExpr(bin->Left);

        // ADR-021: && kısa-devre sağ taraf narrowing — "a != null && a.field"
        if (bin->Operator == TokenType::AMPERSAND_AMPERSAND) {
            auto [narrowVar, isNotNull] = extractNullCheck(bin->Left);
            if (!narrowVar.empty() && isNotNull)
                narrowedNonNull_.insert(narrowVar);
            checkExpr(bin->Right);
            if (!narrowVar.empty() && isNotNull)
                narrowedNonNull_.erase(narrowVar);
            result = Type::Bool();
            break;
        }

        Type rightType = checkExpr(bin->Right);

        // Mantıksal (||)
        if (bin->Operator == TokenType::PIPE_PIPE) {
            result = Type::Bool();
            break;
        }

        // Eşitlik karşılaştırması: string dahil herhangi tiple çalışır
        if (bin->Operator == TokenType::EQUAL_EQUAL ||
            bin->Operator == TokenType::BANG_EQUAL) {
            result = Type::Bool();
            break;
        }

        // Sıralama karşılaştırması: YALNIZCA sayısal tipler
        if (bin->Operator == TokenType::LESS         ||
            bin->Operator == TokenType::LESS_EQUAL   ||
            bin->Operator == TokenType::GREATER      ||
            bin->Operator == TokenType::GREATER_EQUAL) {
            if (leftType.isError() || rightType.isError()) {
                result = Type::error(); // önceki hata, sessiz geç
            } else if (leftType.isNumeric() && rightType.isNumeric()) {
                result = Type::Bool();
            } else {
                diag_.report("E003", bin->loc,
                    "Sıralama operatörü yalnızca sayısal tiplerle kullanılabilir: " +
                    leftType.toString() +
                    " — string için yalnızca == ve != kullanın");
                result = Type::error();
            }
            break;
        }

        // ADR-021: katı operand kuralı — non-null bağlamda nullable operand yasak
        // (eşitlik / null karşılaştırmaları için geçerli değil)
        if (!leftType.isError() && !rightType.isError() &&
            (leftType.nullable || rightType.nullable)) {
            diag_.report("E003", bin->loc,
                "Nullable operand: '" + leftType.toString() + "' ve '" +
                rightType.toString() + "' — null kontrolü yapın veya daraltın");
            result = Type::error();
            break;
        }

        // String birleştirme: yalnızca + operatörü (ADR-024)
        if (bin->Operator == TokenType::PLUS &&
            leftType.isString() && rightType.isString()) {
            result = Type::String();
            break;
        }

        // Aritmetik: +, -, *, /, %
        int lRank = numericRank(leftType);
        int rRank = numericRank(rightType);

        if (lRank >= 0 && rRank >= 0) {
            // Aynı tip veya otomatik genişletme; sonuç daha geniş tip.
            result = (lRank >= rRank) ? leftType : rightType;
        } else if (!leftType.isError() && !rightType.isError()) {
            diag_.report("E003", bin->loc,
                "Aritmetik operatör sayısal olmayan tip: " +
                leftType.toString() + " ve " + rightType.toString());
            result = Type::error();
        } else {
            result = Type::error();
        }
        break;
    }

    // ── Call ───────────────────────────────────────────────────────────────
    case ASTKind::Call: {
        auto* call = (CallExpressionNode*)node;
        Type calleeType = checkExpr(call->callee);

        if (!calleeType.isFunction()) {
            if (!calleeType.isError())
                diag_.report("E003", call->loc,
                    "Çağrılabilir değil: " + calleeType.toString());
            result = Type::error();
            // Argümanları yine de gez (cascade hatayı önle)
            for (auto* arg : call->arguments) checkExpr(arg);
            break;
        }

        // Argüman sayısı kontrolü — builtin print hariç (paramTypes boş = değişken arity)
        if (!calleeType.paramTypes.empty()) {
            size_t expected_count = calleeType.paramTypes.size();
            size_t got_count      = call->arguments.size();
            if (got_count != expected_count) {
                diag_.report("E008", call->loc,
                    std::to_string(expected_count) + " argüman bekleniyor, " +
                    std::to_string(got_count) + " verildi");
            }
        }

        // Argüman tiplerini kontrol et
        for (size_t i = 0; i < call->arguments.size(); ++i) {
            Type paramType = (i < calleeType.paramTypes.size())
                             ? calleeType.paramTypes[i]
                             : Type::error();
            Type argType = checkExpr(call->arguments[i], paramType);
            bool isLit   = call->arguments[i]->kind == ASTKind::Literal;
            if (!paramType.isError())
                checkAssign(paramType, argType, isLit,
                            call->arguments[i]->loc, "argüman");
        }

        result = calleeType.returnType ? *calleeType.returnType : Type::Void();
        break;
    }

    // ── Postfix ++/-- ──────────────────────────────────────────────────────
    case ASTKind::Postfix: {
        auto* pf = (PostfixNode*)node;
        Type opType = checkExpr(pf->operand);
        if (!opType.isNumeric() && !opType.isError())
            diag_.report("E003", pf->loc,
                "++ / -- sayısal olmayan tip: " + opType.toString());
        result = opType;
        break;
    }

    // ── MemberAccess / IndexExpression ─────────────────────────────────────
    case ASTKind::MemberAccess: {
        auto* ma = (MemberAccessNode*)node;
        Type objType = checkExpr(ma->object);
        // ADR-021: nullable nesne üstünde doğrudan alan erişimi yasak
        if (objType.nullable) {
            diag_.report("E003", node->loc,
                "Nullable tip '" + objType.toString() + "' üstünde doğrudan erişim"
                " — if ile null kontrolü yapın");
            result = Type::error();
            break;
        }
        if (objType.isStruct()) {
            result = table_.getFieldType(objType.structName, ma->member);
            if (result.isError())
                diag_.report("E001", node->loc,
                             "'" + objType.structName + "' struct'ında '" + ma->member + "' alanı yok");
        } else {
            result = Type::error();
        }
        break;
    }
    case ASTKind::IndexExpression: {
        auto* ie = (IndexExpressionNode*)node;
        Type objType = checkExpr(ie->object);
        if (ie->index) checkExpr(ie->index);
        // array eleman tipi
        if (objType.isArray() && objType.elementType)
            result = *objType.elementType;
        else
            result = Type::Int(); // varsayılan (tip çıkarımı tam değil)
        break;
    }

    default:
        result = Type::error();
        break;
    }

    // resolvedType'a yaz (ExpressionNode'dan türeyen tüm node'lar için)
    if (auto* exprNode = dynamic_cast<ExpressionNode*>(node))
        exprNode->resolvedType = result;

    return result;
}
