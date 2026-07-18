// ============================================================================
// saQut Compiler — TypeChecker Gerçeklemesi
// ============================================================================
//
// DİZİN:   src/semantic/type_checker.cpp
// KATMAN:  Faz 3 — Tip denetimi ve tip çıkarımı
//
// AMAÇ:
//   AST'yi gezerek tip denetimi yapar ve her ifadeye resolvedType atar.
//   ADR-010 (gizli dönüşüm yok), ADR-021 (nullable), ADR-024 (string),
//   ADR-025 (hata yönetimi), ADR-026 (cast), ADR-027 (switch), ADR-028
//   (decimal) kurallarını uygular.
//
// ============================================================================

#include "semantic/type_checker.hpp"
#include "parser/nodes/program.hpp"
#include "parser/nodes/declarations.hpp"
#include "parser/nodes/statements.hpp"
#include "parser/nodes/expressions.hpp"
#include "parser/nodes/binary_expr.hpp"
#include "parser/nodes/identifier.hpp"
#include "parser/nodes/literal.hpp"
#include "builtin/builtin_methods.hpp"
#include <cmath>
#include <climits>

// ─────────────────────────────────────────────────────────────────────────────
// Yardımcılar
// ─────────────────────────────────────────────────────────────────────────────

// Hint mesajlarında kullanmak için bir ifade düğümünden kısa kaynak metin üretir.
static std::string nodeHintText(ASTNode* node) {
    if (!node) return "<expression>";
    if (node->kind == ASTKind::Literal) {
        auto* lit = static_cast<LiteralNode*>(node);
        if (lit->parserToken.token) return lit->parserToken.token->token;
        if (lit->hasDirectValue)    return std::to_string(lit->directIntValue);
        return "<literal>";
    }
    if (node->kind == ASTKind::Identifier) {
        auto* id = static_cast<IdentifierNode*>(node);
        if (id->parserToken.token) return id->parserToken.token->token;
        return "<identifier>";
    }
    if (node->kind == ASTKind::BinaryExpression) {
        auto* bin = static_cast<BinaryExpressionNode*>(node);
        auto it = OPERATOR_MAP_REV.find(bin->Operator);
        std::string op = (it != OPERATOR_MAP_REV.end()) ? std::string(it->second) : "?";
        if (!bin->Left) return op + nodeHintText(bin->Right);
        return nodeHintText(bin->Left) + " " + op + " " + nodeHintText(bin->Right);
    }
    return "<expression>";
}

int TypeChecker::numericRank(const Type& t) {
    if (!t.isPrimitive()) return -1;
    switch (t.prim) {
        case PrimitiveKind::Int:     return 0;
        case PrimitiveKind::Float:   return 1;
        case PrimitiveKind::Double:  return 2;
        case PrimitiveKind::Decimal: return 3;
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

// Non-void fonksiyon kontrolü: tüm akış yolları return veya throw ile bitiyor mu?
bool TypeChecker::pathAlwaysReturns(ASTNode* stmt) {
    if (!stmt) return false;
    switch (stmt->kind) {
        case ASTKind::ReturnStatement:
        case ASTKind::ThrowStatement:
            return true;
        case ASTKind::Block:
            // Sıralı yürütme: ilk garantili çıkışa kadar ilerle
            for (ASTNode* ch : stmt->getChildren())
                if (pathAlwaysReturns(ch)) return true;
            return false;
        case ASTKind::IfStatement: {
            auto* ifn = (IfStatementNode*)stmt;
            if (!ifn->elseBranch) return false; // else yok → if atlanabilir
            return pathAlwaysReturns(ifn->thenBranch)
                && pathAlwaysReturns(ifn->elseBranch);
        }
        default:
            return false; // döngü, atama, çağrı vb. → garanti yok
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
    if (currentReturnType_.isError()) {
        if (table_.structLayouts.count(fn->returnType))
            currentReturnType_ = Type::structType(fn->returnType);
        else {
            // #64 regresyonunun nullable-özel-tip hali: "Book?"/"Color?" gibi
            // nullable soneki önce çözülmeli (symbol_collector::typeFromName
            // ile aynı desen), yoksa taban tip struct/enum olsa da bulunamaz.
            std::function<Type(const std::string&)> resolveType = [&](const std::string& name) -> Type {
                if (!name.empty() && name.back() == '?') {
                    Type base = resolveType(name.substr(0, name.size() - 1));
                    if (!base.isError()) return base.asNullable();
                    return Type::error();
                }
                Type t = Type::fromName(name);
                if (!t.isError()) return t;
                if (table_.structLayouts.count(name)) return Type::structType(name);
                if (table_.isEnumName(name)) return Type::enumType(name);
                if (name.size() > 2 && name.substr(name.size() - 2) == "[]") {
                    Type elem = resolveType(name.substr(0, name.size() - 2));
                    if (!elem.isError()) return Type::array(elem);
                }
                return Type::error();
            };
            Type resolved = resolveType(fn->returnType);
            if (!resolved.isError())
                currentReturnType_ = resolved;
            else if (fn->returnType != "void")
                currentReturnType_ = Type::Void();
        }
    }

    auto& ch = fn->getChildren();
    if (!ch.empty()) checkStmt(ch[0]); // body Block

    // Non-void fonksiyonun tüm akış yolları return/throw ile bitmeli
    if (!currentReturnType_.isVoid()) {
        if (ch.empty() || !pathAlwaysReturns(ch[0])) {
            diag_.report("E006", fn->loc,
                "'" + fn->name + "' function must return " + fn->returnType +
                " but some paths have no return",
                "add `return <value>;` to all control flow paths");
        }
    }

    inFunction_ = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// checkAssign — atama uyumu + uyarı/hata raporlama
// ─────────────────────────────────────────────────────────────────────────────

bool TypeChecker::checkAssign(const Type& target, const Type& src,
                               bool srcIsLiteral,
                               const SourceLocation& loc,
                               const std::string& ctx,
                               const std::string& hintExpr) {
    if (target.isError() || src.isError()) return true; // önceki hata, sessiz geç

    // ADR-021: null literal assignment
    if (src.isNullLiteral()) {
        if (target.nullable) return true;  // T? ← null → OK
        diag_.report("E003", loc,
            "'" + ctx + "': cannot assign null to non-null type (" + target.toString() + ")",
            "make the type nullable: `" + target.toString() + "? " + ctx + " = null;`");
        return false;
    }

    // ADR-021: nullable compatibility
    // T? ← T  → OK (widening: non-null to nullable)
    // T  ← T? → E  (narrowing: nullable to non-null requires narrowing)
    if (src.nullable && !target.nullable && src.equalsBase(target)) {
        diag_.report("E003", loc,
            "'" + ctx + "': " + src.toString() +
            " nullable type cannot be assigned to non-null " + target.toString() +
            " (use if to check for null)",
            "if (" + ctx + " != null) { /* here " + ctx + " is non-null, safe to use */ }");
        return false;
    }
    // T? ← T → OK (equalsBase eşleşiyorsa, nullable farkı widening)
    if (!src.nullable && target.nullable && src.equalsBase(target)) return true;

    if (target.equals(src)) return true;

    // ADR-040: longint rank kulesi dışında izole (byte gibi) — int→longint
    // kayıpsız serbest (uyarısız), longint→int VE longint↔float/double/decimal
    // yalnızca açık `as` (numericRank longint içermez, burada elle ele alınır).
    if (target.isLongInt() || src.isLongInt()) {
        if (target.isLongInt() && src.isIntegral()) return true; // int/longint → longint
        const std::string castTarget = hintExpr.empty() ? ctx : hintExpr;
        diag_.report("E003", loc,
            "'" + ctx + "': " + src.toString() + " → " + target.toString() +
            " requires explicit cast (longint is isolated from the numeric rank tower)",
            "use explicit cast: `" + castTarget + " as " + target.toString() + "`");
        return false;
    }

    int tRank = numericRank(target);
    int sRank = numericRank(src);

    if (tRank >= 0 && sRank >= 0) {
        if (tRank > sRank) {
            if (srcIsLiteral) return true;
            const std::string castTarget = hintExpr.empty() ? ctx : hintExpr;
            diag_.report("W004", loc,
                "'" + ctx + "': " + src.toString() +
                " → " + target.toString() + " implicit widening",
                "use explicit cast: `" + castTarget + " as " + target.toString() + "` (to suppress this warning)");
            return true;
        } else {
            const std::string castTarget = hintExpr.empty() ? ctx : hintExpr;
            diag_.report("E003", loc,
                "'" + ctx + "': " + src.toString() +
                " → " + target.toString() + " narrowing conversion (possible data loss)",
                "use explicit cast: `" + castTarget + " as " + target.toString() + "` (data loss may occur)");
            return false;
        }
    }

    diag_.report("E003", loc,
        "'" + ctx + "': cannot assign " + src.toString() +
        " to " + target.toString(),
        "use explicit cast: `<expression> as " + target.toString() + "`");
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
        if (targetType.isError()) {
            // #64 regresyonunun nullable-özel-tip hali (bkz. checkFunction) —
            // "?" soneki önce çözülmeli, yoksa struct/enum taban tipi bulunamaz.
            std::function<Type(const std::string&)> resolveType = [&](const std::string& name) -> Type {
                if (!name.empty() && name.back() == '?') {
                    Type base = resolveType(name.substr(0, name.size() - 1));
                    if (!base.isError()) return base.asNullable();
                    return Type::error();
                }
                Type t = Type::fromName(name);
                if (!t.isError()) return t;
                if (table_.structLayouts.count(name)) return Type::structType(name);
                if (table_.isEnumName(name)) return Type::enumType(name);
                if (name.size() > 2 && name.substr(name.size() - 2) == "[]") {
                    Type elem = resolveType(name.substr(0, name.size() - 2));
                    if (!elem.isError()) return Type::array(elem);
                }
                return Type::error();
            };
            targetType = resolveType(vd->varType);
        }
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
                    "empty return; function must return " +
                    currentReturnType_.toString(),
                    "add a value to the return statement: `return <" + currentReturnType_.toString() + "_value>;`");
            break;
        }
        Type valType = checkExpr(rs->value, currentReturnType_);
        bool isLit   = rs->value->kind == ASTKind::Literal;
        checkAssign(currentReturnType_, valType, isLit, rs->loc, "return", nodeHintText(rs->value));
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

        // Geçerli switch tipleri: int, float, bool, string, enum (aggregate değil)
        bool subjectOk = baseType.isPrimitive() || baseType.isString()
                         || baseType.isEnum()
                         || baseType.isVoid(); // void = bilinmeyen, hata zaten raporlandı
        if (!subjectOk && !baseType.isError()) {
            diag_.report("E003", sw->subject->loc,
                "switch subject '" + subjectType.toString() +
                "' type not supported (expected int/float/bool/string/enum)",
                "switch only works with int, float, bool, string or enum values — use if-else for struct/array");
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
                            "case null: only valid with nullable (T?) switch subject",
                            "make the switch subject nullable: `" + subjectType.toString() + "? variable = ...;`");
                    continue;
                }

                Type caseType = checkExpr(val, baseType);
                // Type homogeneity: case value must be same base type as subject
                if (!caseType.isError() && !baseType.isError() &&
                    !baseType.isVoid() && !caseType.equalsBase(baseType)) {
                    diag_.report("E003", val->loc,
                        "case value '" + caseType.toString() +
                        "' incompatible with switch subject type (" + baseType.toString() + ")",
                        "all case values must be the same type as the switch subject (" + baseType.toString() + "). Use cast: `value as " + baseType.toString() + "`");
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
                                        "case " + raw + ": this float value cannot be exactly represented in IEEE 754; comparison may not yield expected result");
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
                // byte bağlamı (#86): 0-255 aralık denetimi — sessiz kırpma YOK.
                if (!expected.isError() && expected.isByte()) {
                    long long v = 0;
                    if (lit->hasDirectValue) v = lit->directIntValue;
                    else if (lit->parserToken.token) {
                        try { v = std::stoll(lit->parserToken.token->token); }
                        catch (...) { v = -1; } // taşma → aralık dışı say
                    }
                    if (v < 0 || v > 255) {
                        diag_.report("E003", lit->loc,
                            "integer literal " + (lit->parserToken.token
                                ? lit->parserToken.token->token : std::to_string(v))
                                + " is out of byte range (0-255)",
                            "byte holds 0-255; use int for larger values or a value in range");
                        result = Type::error();
                    } else {
                        result = Type::Byte();
                    }
                }
                // Bağlam daha geniş sayısal tip ise literal o tip olarak tiplenir (ADR-010/028).
                else if (!expected.isError() && expected.isDecimal()) result = Type::Decimal();
                // ADR-040: longint rank kulesi dışında (numericRank longint'i tanımaz,
                // burada elle ele alınır) — longint bağlamında literal longint tiplenir.
                else if (!expected.isError() && expected.isLongInt()) result = Type::LongInt();
                else if (expRank > 0) result = expected; // float veya double bekleniyor
                else                  result = Type::Int();
                break;
            case LiteralType::FLOAT:
                // float literal → decimal bağlamında decimal olur (ADR-028); int bağlamında E003.
                if (!expected.isError() && expected.isDecimal())
                    result = Type::Decimal();
                else if (!expected.isError() && expected.equals(Type::Double()))
                    result = Type::Double();
                else if (!expected.isError() && numericRank(expected) == 0) {
                    diag_.report("E003", lit->loc,
                        "float literal cannot be used in int context (data loss)",
                        "use an integer literal (e.g. 3 instead of 3.0) or change the variable type to float: `float variable = ...;`");
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
            checkAssign(leftType, rightType, isLit, bin->loc, "assignment");
            result = leftType;
            break;
        }

        // Unary (Left = nullptr): -, +, !, ~
        if (!bin->Left) {
            Type rightType = checkExpr(bin->Right);
            if (bin->Operator == TokenType::BANG) {
                result = Type::Bool();
            } else {
                // byte tekli işlemde int'e terfi eder (#86, C modeli).
                if (rightType.isByte())      result = Type::Int();
                else if (rightType.isNumeric()) result = rightType;
                else                         result = Type::error();
                if (result.isError() && !rightType.isError())
                    diag_.report("E003", bin->loc, "non-numeric operand",
                    "- (unary) only works on int or float values");
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

        // Comparison operators: ONLY numeric types
        if (bin->Operator == TokenType::LESS         ||
            bin->Operator == TokenType::LESS_EQUAL   ||
            bin->Operator == TokenType::GREATER      ||
            bin->Operator == TokenType::GREATER_EQUAL) {
            if (leftType.isError() || rightType.isError()) {
                result = Type::error(); // previous error, silent pass
            } else if (leftType.isNumeric() && rightType.isNumeric()) {
                result = Type::Bool();
            } else if (leftType.isDate() && rightType.isDate()) {
                // #88 (ADR-036): date karşılaştırılabilir ama numeric DEĞİL —
                // aritmetik (+, -, vb.) bu yoldan geçmez, yalnızca burada izinli.
                result = Type::Bool();
            } else {
                diag_.report("E003", bin->loc,
                    "comparison operator only works with numeric types: " +
                    leftType.toString() +
                    " — for string use only == and !=",
                    "for string comparison use == or !=; for numeric comparison use int/float");
                result = Type::error();
            }
            break;
        }

        // ADR-021: strict operand rule — nullable operands forbidden in non-null context
        // (not applicable for equality / null comparisons)
        if (!leftType.isError() && !rightType.isError() &&
            (leftType.nullable || rightType.nullable)) {
            diag_.report("E003", bin->loc,
                "nullable operand: '" + leftType.toString() + "' and '" +
                rightType.toString() + "' — check for null or narrow",
                "if (variable != null) { /* here it is non-null, safe to use */ } or define the variable as non-null type");
            result = Type::error();
            break;
        }

        // String birleştirme: yalnızca + operatörü (ADR-024)
        if (bin->Operator == TokenType::PLUS &&
            leftType.isString() && rightType.isString()) {
            result = Type::String();
            break;
        }

        // Arithmetic / bitwise: +, -, *, /, %, &, |, ^, <<, >>
        // ADR-010/#114: bir operand literal, diğeri tipli bir ifadeyse literal
        // diğer operandın tipine göre YENİDEN tiplenir (bağlama-göre tipleme
        // yalnızca literal başlı-başına değerlendirildiğinde değil, ikili
        // ifadenin İÇİNDE de geçerli olmalı) — aksi halde örn. `d + 0.2`
        // (d: double) sağdaki `0.2` bağlamsız Float() (32-bit) tiplenir, sonra
        // double'a genişletilir ve çift-yuvarlama precision farkı sızar.
        if (bin->Left && bin->Left->kind == ASTKind::Literal &&
            !rightType.isError() && rightType.isNumeric())
            leftType = checkExpr(bin->Left, rightType);
        if (bin->Right && bin->Right->kind == ASTKind::Literal &&
            !leftType.isError() && leftType.isNumeric())
            rightType = checkExpr(bin->Right, leftType);

        // byte C-modeli terfi (#86): byte operand int'e yükselir, sonuç asla
        // byte olmaz (byte + byte → int; byte & byte → int). numericRank byte
        // içermez, o yüzden burada elle int'e çeviriyoruz.
        Type lArith = leftType.isByte()  ? Type::Int() : leftType;
        Type rArith = rightType.isByte() ? Type::Int() : rightType;

        // ADR-040: longint rank kulesi dışında izole (numericRank longint'i
        // tanımaz) — int/longint karışımı serbest (sonuç longint), ama
        // float/double/decimal ile karışım yasak (açık `as` gerekir).
        if (lArith.isLongInt() || rArith.isLongInt()) {
            if (lArith.isIntegral() && rArith.isIntegral()) {
                result = Type::LongInt();
                break;
            } else if (!leftType.isError() && !rightType.isError()) {
                diag_.report("E003", bin->loc,
                    "arithmetic operator on longint mixed with " +
                    (lArith.isLongInt() ? rArith.toString() : lArith.toString()) +
                    ": explicit cast required (longint is isolated from the numeric rank tower)",
                    "use explicit cast: `variable as longint` or `variable as " +
                    (lArith.isLongInt() ? rArith.toString() : lArith.toString()) + "`");
                result = Type::error();
                break;
            } else {
                result = Type::error();
                break;
            }
        }

        int lRank = numericRank(lArith);
        int rRank = numericRank(rArith);

        if (lRank >= 0 && rRank >= 0) {
            // Same type or implicit widening; result is the wider type.
            result = (lRank >= rRank) ? lArith : rArith;
        } else if (!leftType.isError() && !rightType.isError()) {
            diag_.report("E003", bin->loc,
                "arithmetic operator on non-numeric type: " +
                leftType.toString() + " and " + rightType.toString(),
                "use int or float for arithmetic. Cast: `variable as int`");
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
                    "not callable: " + calleeType.toString(),
                    "only functions can be called. define with: `func name(parameters) : returnType { ... }`");
            result = Type::error();
            // Still walk arguments (cascade error prevention)
            for (auto* arg : call->arguments) checkExpr(arg);
            break;
        }

        // Argument count check — builtin print excepted (empty paramTypes = variable arity)
        if (!calleeType.paramTypes.empty()) {
            size_t expected_count = calleeType.paramTypes.size();
            size_t got_count      = call->arguments.size();
            if (got_count != expected_count) {
                diag_.report("E008", call->loc,
                    std::to_string(expected_count) + " argument(s) expected, " +
                    std::to_string(got_count) + " given",
                    got_count < expected_count
                        ? std::to_string(expected_count - got_count) + " argument(s) missing — check function definition and add missing arguments"
                        : std::to_string(got_count - expected_count) + " argument(s) extra — remove extra arguments");
            }
        }

        // Check argument types
        for (size_t i = 0; i < call->arguments.size(); ++i) {
            Type paramType = (i < calleeType.paramTypes.size())
                             ? calleeType.paramTypes[i]
                             : Type::error();
            Type argType = checkExpr(call->arguments[i], paramType);
            bool isLit   = call->arguments[i]->kind == ASTKind::Literal;
            if (!paramType.isError())
                checkAssign(paramType, argType, isLit,
                            call->arguments[i]->loc, "argument");
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
                "++ / -- on non-numeric type: " + opType.toString(),
                "++ and -- only work on int or float variables");
        result = opType;
        break;
    }

    // ── MemberAccess / IndexExpression ─────────────────────────────────────
    case ASTKind::MemberAccess: {
        auto* ma = (MemberAccessNode*)node;
        Type objType = checkExpr(ma->object);
        // ADR-021: direct field access on nullable object forbidden
        if (objType.nullable) {
            diag_.report("E003", node->loc,
                "direct access on nullable type '" + objType.toString() +
                "' — use if to check for null",
                "if (variable != null) { variable.field ... } or make the type non-null: `" + objType.toString().substr(0, objType.toString().size()-1) + " variable = ...;`");
            result = Type::error();
            break;
        }
        // Struct TİP ADI değişken gibi kullanılmış: Efsane.message → hata
        if (objType.isStruct() && ma->object->kind == ASTKind::Identifier) {
            auto* idNode = static_cast<IdentifierNode*>(ma->object);
            std::string idName = (idNode->parserToken.token)
                                 ? idNode->parserToken.token->token : "";
            Symbol* sym = idName.empty() ? nullptr : table_.resolve(idName);
            if (sym && sym->kind == SymbolKind::Struct) {
                diag_.report("E001", ma->object->loc,
                    "'" + idName + "' is a struct type, not a variable",
                    "declare an instance first: `" + idName + " myVar;` then use `myVar." + ma->member + "`",
                    (int)idName.size());
                result = Type::error();
                break;
            }
        }

        if (objType.isEnum()) {
            // Color.Red — enum üye erişimi
            if (!table_.hasEnumMember(objType.enumName, ma->member)) {
                diag_.report("E001", node->loc,
                    "'" + ma->member + "' is not a member of enum '" + objType.enumName + "'",
                    "check the '" + objType.enumName + "' enum definition",
                    (int)ma->member.size());
                result = Type::error();
            } else {
                result = Type::enumType(objType.enumName);
            }
        } else if (objType.isStruct()) {
            result = table_.getFieldType(objType.structName, ma->member);
            if (result.isError())
                diag_.report("E001", node->loc,
                             "field '" + ma->member + "' not found in struct '" + objType.structName + "'",
                             "check the '" + objType.structName + "' struct definition — use `saqut symbols <file>` to see available fields",
                             (int)ma->member.size());
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

    // ── ScopeCall: builtin metod çağrısı (ADR-033, #85) ────────────────────
    //
    // Üç yüzey sözdizimi aynı düğüme düşer:
    //   1. UFCS nokta çağrısı (birincil): arr.push(12), s.upper(), p.toJson()
    //      → dotCall=true, receiver arguments[0]; kategori receiver TİPİNDEN.
    //   2. Ad alanı (ikincil): array::push(arr,12), string::upper(s),
    //      struct::toJson(p) — sol taraf sabit kategori adı.
    //   3. ESKİ element-tipi sözdizimi: int::push(arr,12), Person::toJson(p)
    //      — W006 uyarısıyla çalışır, v0.7.0'da kaldırılacak.
    case ASTKind::ScopeCall: {
        auto* sc = (ScopeCallNode*)node;
        const auto& reg = BuiltinMethodRegistry::instance();

        // Önce tüm argümanları (dotCall'da receiver dahil) denetle
        std::vector<Type> argTypes;
        for (auto* arg : sc->arguments)
            argTypes.push_back(checkExpr(arg));

        Type recvType        = argTypes.empty() ? Type::error() : argTypes[0];
        bool isReceiverArray = recvType.isArray();

        // Tanı mesajlarında görünen çağrı adı
        std::string displayName = sc->dotCall
            ? "." + sc->methodName
            : sc->leftTypeName + "::" + sc->methodName;

        bool        isStruct   = false;
        Type        elemType   = Type::error();
        std::string lookupName = sc->leftTypeName; // reg.lookup'un sol adı

        if (sc->dotCall) {
            // ── 1. UFCS: kategori receiver tipinden ──────────────────────
            // Alan gölgeleme kuralı: receiver struct ve metod adı bir ALANSA
            // builtin'e hiç bakılmaz — alan kazanır (alanlar çağrılabilir
            // olmadığından bu bir hatadır, sessiz sürpriz değil).
            bool fieldShadows = false;
            if (recvType.isStruct()) {
                auto lay = table_.structLayouts.find(recvType.structName);
                if (lay != table_.structLayouts.end())
                    for (auto& [fn, ft] : lay->second)
                        if (fn == sc->methodName) { fieldShadows = true; break; }
            }
            if (fieldShadows) {
                diag_.report("E001", sc->loc,
                    "'" + sc->methodName + "' is a field of struct '" +
                    recvType.structName + "' and is not callable",
                    "the field shadows the builtin method (ADR-033) — "
                    "rename the field or use struct::" + sc->methodName + "(value)");
                result = Type::error();
                break;
            }
            if (isReceiverArray && recvType.elementType) {
                elemType   = *recvType.elementType;
                lookupName = "array";
                isStruct   = elemType.isStruct(); // struct-array: ar metodları geçerli
            } else if (recvType.isString()) {
                elemType   = Type::String();
                lookupName = "string";
            } else if (recvType.isStruct()) {
                elemType   = recvType;
                lookupName = recvType.structName;
                isStruct   = true;
            } else {
                if (!recvType.isError())
                    diag_.report("E001", sc->loc,
                        "type '" + recvType.toString() + "' has no builtin methods",
                        "dot-call works on array, string and struct values (ADR-033)");
                result = Type::error();
                break;
            }
        } else if (sc->leftTypeName == "array") {
            // ── 2. array:: ad alanı — element tipi receiver'dan türetilir ──
            if (!isReceiverArray) {
                diag_.report("E003", sc->loc,
                    "array::" + sc->methodName + " expects an array as first argument"
                    + (argTypes.empty() ? "" : ", got '" + recvType.toString() + "'"),
                    "example: array::push(arr, value)");
                result = Type::error();
                break;
            }
            elemType = recvType.elementType ? *recvType.elementType : Type::Int();
            isStruct = elemType.isStruct();
            // lookup'ta "array" sv/st dallarına düşmez, ar: bulunur
        } else if (sc->leftTypeName == "struct") {
            // ── 2. struct:: ad alanı ──────────────────────────────────────
            if (!recvType.isStruct()) {
                diag_.report("E003", sc->loc,
                    "struct::" + sc->methodName + " expects a struct as first argument"
                    + (argTypes.empty() ? "" : ", got '" + recvType.toString() + "'"),
                    "example: struct::toJson(value)");
                result = Type::error();
                break;
            }
            elemType = recvType;
            isStruct = true;
        } else {
            // ── 3. ESKİ sözdizimi: ElemTip::method / StructAd::method ─────
            elemType = BuiltinMethodRegistry::resolveElemType(sc->leftTypeName);
            if (elemType.isError()) {
                if (table_.hasStruct(sc->leftTypeName)) {
                    isStruct = true;
                    elemType = Type::structType(sc->leftTypeName);
                } else {
                    diag_.report("E001", sc->loc,
                        "unknown type '" + sc->leftTypeName + "' in scope call",
                        "use value.method(...) or the array::/string::/struct:: namespaces (ADR-033)");
                    result = Type::error();
                    break;
                }
            } else if (elemType.isStruct()) {
                isStruct = true;
            }
            // string::upper(s) YENİ modelde de geçerli (string ad alanı) —
            // uyarı yalnızca element-tipi kullanımına verilir: skaler/struct
            // sol ad, ya da string:: ile ARRAY metodu (string::push(sarr,x)).
            if (sc->leftTypeName != "string" || isReceiverArray) {
                std::string suggestion = isReceiverArray
                    ? "use value.method(...) or array::" + sc->methodName + "(value, ...)"
                    : "use value." + sc->methodName + "(...) or struct::" + sc->methodName + "(value)";
                diag_.report("W006", sc->loc,
                    "deprecated builtin call syntax '" + displayName + "' — "
                    "the element-type prefix will be removed in v0.7.0 (ADR-033)",
                    suggestion);
            }
        }

        const BuiltinMethod* bm = reg.lookup(lookupName, sc->methodName, isStruct, isReceiverArray);
        if (!bm) {
            // Hata mesajında hangi tiplerin bu metodu desteklediğini söyle
            std::string typeDesc = sc->dotCall ? recvType.toString() : sc->leftTypeName;
            diag_.report("E001", sc->loc,
                "'" + sc->methodName + "' is not a built-in method for type '" + typeDesc + "'",
                std::string("use one of: length, push, pop, insert, remove, slice, reverse, concat, contains, indexOf, clear")
                + (lookupName == "string"
                    ? " — or string methods: upper, lower, trim, split, substring, replace, repeat, charAt, indexOf, contains, startsWith, endsWith"
                    : "")
                + (isStruct ? " — or struct methods: toJson, dump" : ""));
            result = Type::error();
            break;
        }

        // Argüman sayısı kontrolü
        if (sc->arguments.size() != bm->params.size()) {
            diag_.report("E008", sc->loc,
                displayName + " expects " +
                std::to_string(bm->params.size()) + " argument(s), " +
                std::to_string(sc->arguments.size()) + " given"
                + (sc->dotCall ? " (receiver counts as the first argument)" : ""),
                "check the method signature");
            result = Type::error();
            break;
        }

        // Her argümanı kontrol et
        bool anyError = false;
        for (size_t i = 0; i < sc->arguments.size(); ++i) {
            Type argType = argTypes[i];
            const ParamRule& pr = bm->params[i];

            Type expectedType;
            switch (pr.kind) {
                case ParamKind::Fixed:     expectedType = pr.fixedType; break;
                case ParamKind::ElemType:  expectedType = elemType; break;
                case ParamKind::ElemArray: expectedType = Type::array(elemType); break;
                case ParamKind::StringVal: expectedType = Type::String(); break;
            }

            bool isLit = sc->arguments[i]->kind == ASTKind::Literal;
            // Dar-tipli literal argümanlar (byte gibi) bağlamla yeniden
            // tiplenir — argTypes başta bağlamsız hesaplandı, bu yüzden
            // `arr.push(250)` literali önce int oldu; beklenen tiple
            // yeniden değerlendir (aralık denetimi + doğru tip). (#86)
            if (isLit && !expectedType.isError())
                argType = checkExpr(sc->arguments[i], expectedType);
            if (!argType.isError() && !expectedType.isError()) {
                if (!checkAssign(expectedType, argType, isLit,
                                 sc->arguments[i]->loc,
                                 displayName + " arg " + std::to_string(i + 1)))
                    anyError = true;
            }
        }
        if (anyError) {
            result = Type::error();
            break;
        }

        // Dönüş tipini hesapla
        switch (bm->ret.kind) {
            case ReturnKind::Fixed:     result = bm->ret.fixedType; break;
            case ReturnKind::ElemType:  result = elemType; break;
            case ReturnKind::ElemArray: result = Type::array(elemType); break;
        }

        // builtinId'yi node'a yaz (IR codegen kullanır)
        sc->builtinId = bm->runtimeId;
        break;
    }

    // ── CastExpression: expr as TargetType[?]  (ADR-026) ──────────────────
    case ASTKind::CastExpression: {
        auto* cast = (CastExpressionNode*)node;
        Type srcType = checkExpr(cast->operand);

        // Source type: struct/array cast forbidden
        bool srcOk = srcType.isPrimitive() || srcType.isString() || srcType.isError();
        if (!srcOk) {
            diag_.report("E003", cast->loc,
                "'" + srcType.toString() + "' cannot be cast with 'as' "
                "(only int/float/decimal/bool/string)",
                "'as' operator only works between scalar types. For struct/array conversion write a conversion function");
            result = Type::error();
            break;
        }

        // Resolve target type
        Type targetBase = Type::fromName(cast->targetTypeName);
        if (targetBase.isError()) {
            diag_.report("E003", cast->loc,
                "unknown target type: '" + cast->targetTypeName + "'",
                "valid target types: int, float, bool, string (or nullable variants: int?, float?, ...)");
            result = Type::error();
            break;
        }
        if (!targetBase.isPrimitive() && !targetBase.isString()) {
            diag_.report("E003", cast->loc,
                "'" + cast->targetTypeName + "' cannot be target of 'as' cast"
                " (only int/float/bool/string)",
                "valid target types: int, float, bool, string. For struct/array conversion write a conversion function");
            result = Type::error();
            break;
        }

        // Conversion matrix — invalid combinations
        bool srcIsStr  = srcType.isString();
        bool tgtIsStr  = targetBase.isString();
        bool srcIsBool = srcType.isPrimitive() && srcType.prim == PrimitiveKind::Bool;
        bool tgtIsBool = targetBase.isPrimitive() && targetBase.prim == PrimitiveKind::Bool;

        // bool↔int/float forbidden (ADR-026: "keep forbidden from start for safety")
        bool srcIsNumeric = srcType.isPrimitive() && !srcIsBool;
        bool tgtIsNumeric = targetBase.isPrimitive() && !tgtIsBool;

        if (tgtIsBool && !srcIsBool && !srcType.isError()) {
            diag_.report("E003", cast->loc,
                "bool as target type not allowed with 'as'",
                "bool conversion not supported. For bool use explicit comparison: `value != 0`");
            result = Type::error();
            break;
        }
        if (srcIsBool && !tgtIsBool && !tgtIsStr) {
            diag_.report("E003", cast->loc,
                "bool can only be cast to string ('as string')",
                "use `value as string` — result will be \"true\" or \"false\"");
            result = Type::error();
            break;
        }

        // string→bool forbidden
        if (srcIsStr && tgtIsBool) {
            diag_.report("E003", cast->loc,
                "bool conversion from string not supported",
                "for bool use comparison: `value == \"true\"` or `value != \"\"`");
            result = Type::error();
            break;
        }

        // byte dönüşüm matrisi (#86, ADR-026 genişlemesi):
        //   byte → int    güvenli (byte VM'de int olarak taşınır)
        //   byte → string  güvenli (int gösterimi)
        //   int  → byte   fallible (0-255 dışı → Error/null)
        //   byte ↔ float/decimal YASAK — önce int'e geç
        //   string → byte YASAK — `text as int as byte`
        bool srcIsByte = srcType.isByte();
        bool tgtIsByte = targetBase.isByte();
        bool srcIsInt  = srcType.isPrimitive() && srcType.prim == PrimitiveKind::Int;
        bool tgtIsInt  = targetBase.isPrimitive() && targetBase.prim == PrimitiveKind::Int;
        if (srcIsByte && !(tgtIsInt || tgtIsStr || tgtIsByte)) {
            diag_.report("E003", cast->loc,
                "'byte' can only be cast to int or string",
                "for float/decimal go through int first: `value as int as float`");
            result = Type::error();
            break;
        }
        if (tgtIsByte && !(srcIsInt || srcIsByte)) {
            diag_.report("E003", cast->loc,
                "only 'int' can be cast to byte",
                "cast to int first: `value as int as byte`");
            result = Type::error();
            break;
        }

        // Hedef tip (nullable flag ile)
        result = targetBase;
        if (cast->targetNullable) result = result.asNullable();
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
