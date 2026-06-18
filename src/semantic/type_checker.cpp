#include "semantic/type_checker.hpp"
#include "parser/nodes/program.hpp"
#include "parser/nodes/declarations.hpp"
#include "parser/nodes/statements.hpp"
#include "parser/nodes/expressions.hpp"
#include "parser/nodes/binary_expr.hpp"
#include "parser/nodes/identifier.hpp"
#include "parser/nodes/literal.hpp"

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
    if (target.equals(src))               return true;

    int tRank = numericRank(target);
    int sRank = numericRank(src);

    if (tRank >= 0 && sRank >= 0) {
        if (tRank > sRank) {
            // Genişletme (widening): int→float, int→double, float→double
            if (srcIsLiteral) return true; // literal bağlama-göre tiplenir, uyarısız
            diag_.report("W004", loc,
                "'" + ctx + "': " + src.toString() +
                " → " + target.toString() + " örtük genişletme");
            return true;
        } else {
            // Daraltma (narrowing): float→int, double→float, vb.
            diag_.report("E003", loc,
                "'" + ctx + "': " + src.toString() +
                " → " + target.toString() + " daraltma (veri kaybı)");
            return false;
        }
    }

    // Tamamen farklı tipler
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

    case ASTKind::Block:
        for (ASTNode* child : node->getChildren()) checkStmt(child);
        break;

    case ASTKind::VariableDecl: {
        auto* vd = (VariableDeclNode*)node;
        Type targetType = Type::fromName(vd->varType);
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
        if (ifn->condition)  checkExpr(ifn->condition);
        if (ifn->thenBranch) checkStmt(ifn->thenBranch);
        if (ifn->elseBranch) checkStmt(ifn->elseBranch);
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
            default:                   result = Type::error();  break;
        }
        break;
    }

    // ── Identifier ─────────────────────────────────────────────────────────
    case ASTKind::Identifier: {
        auto* id = (IdentifierNode*)node;
        result = id->resolvedSymbol ? id->resolvedSymbol->type : Type::error();
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
        Type rightType = checkExpr(bin->Right);

        // Mantıksal
        if (bin->Operator == TokenType::AMPERSAND_AMPERSAND ||
            bin->Operator == TokenType::PIPE_PIPE) {
            result = Type::Bool();
            break;
        }

        // Karşılaştırma
        if (bin->Operator == TokenType::EQUAL_EQUAL  ||
            bin->Operator == TokenType::BANG_EQUAL   ||
            bin->Operator == TokenType::LESS         ||
            bin->Operator == TokenType::LESS_EQUAL   ||
            bin->Operator == TokenType::GREATER      ||
            bin->Operator == TokenType::GREATER_EQUAL) {
            result = Type::Bool();
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
        checkExpr(ma->object);
        result = Type::error(); // TODO(faz3+): struct alan çözümü
        break;
    }
    case ASTKind::IndexExpression: {
        auto* ie = (IndexExpressionNode*)node;
        checkExpr(ie->object);
        if (ie->index) checkExpr(ie->index);
        result = Type::error(); // TODO(faz3+): array eleman tipi
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
