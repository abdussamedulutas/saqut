#include "semantic/structural_validator.hpp"
#include "parser/nodes/declarations.hpp"
#include "parser/nodes/statements.hpp"
#include "parser/nodes/expressions.hpp"

void StructuralValidator::validate(ASTNode* program) {
    if (!program) return;
    for (ASTNode* child : program->getChildren())
        walkDecl(child);
}

void StructuralValidator::walkDecl(ASTNode* node) {
    if (!node) return;
    if (node->kind == ASTKind::FunctionDecl) {
        auto* fn = (FunctionDeclNode*)node;
        inFunction_ = true;
        auto& ch = fn->getChildren();
        if (!ch.empty()) walkStmt(ch[0]);
        inFunction_ = false;
    }
    // VariableDecl / StructDecl: yapısal kural yok
}

void StructuralValidator::walkStmt(ASTNode* node) {
    if (!node) return;

    switch (node->kind) {

    case ASTKind::Block:
        for (ASTNode* child : node->getChildren()) walkStmt(child);
        break;

    case ASTKind::IfStatement: {
        auto* ifn = (IfStatementNode*)node;
        if (ifn->thenBranch) walkStmt(ifn->thenBranch);
        if (ifn->elseBranch) walkStmt(ifn->elseBranch);
        break;
    }

    case ASTKind::WhileStatement: {
        auto* ws = (WhileStatementNode*)node;
        loopDepth_++; pureLoopDepth_++;
        if (ws->body) walkStmt(ws->body);
        loopDepth_--; pureLoopDepth_--;
        break;
    }

    case ASTKind::DoWhileStatement: {
        auto* dw = (DoWhileStatementNode*)node;
        loopDepth_++; pureLoopDepth_++;
        if (dw->body) walkStmt(dw->body);
        loopDepth_--; pureLoopDepth_--;
        break;
    }

    case ASTKind::ForStatement: {
        auto* fs = (ForStatementNode*)node;
        loopDepth_++; pureLoopDepth_++;
        if (fs->init) walkStmt(fs->init);
        if (fs->body) walkStmt(fs->body);
        loopDepth_--; pureLoopDepth_--;
        break;
    }

    case ASTKind::SwitchStatement: {
        auto* sw = (SwitchStatementNode*)node;
        loopDepth_++; // break switch içinde geçerli
        for (auto& c : sw->cases)
            for (auto* s : c.body) walkStmt(s);
        loopDepth_--;
        break;
    }

    case ASTKind::BreakStatement:
        if (loopDepth_ == 0)
            diag_.report("E004", node->loc,
                "'break' döngü veya switch dışında kullanılamaz",
                "break yalnızca for, while, do-while veya switch içinde çalışır — bu ifadeyi bir döngü gövdesine taşıyın");
        break;

    case ASTKind::ContinueStatement:
        if (pureLoopDepth_ == 0)
            diag_.report("E004", node->loc,
                "'continue' döngü dışında kullanılamaz",
                "continue yalnızca for, while veya do-while içinde çalışır — bu ifadeyi bir döngü gövdesine taşıyın");
        break;

    case ASTKind::ReturnStatement:
        if (!inFunction_)
            diag_.report("E005", node->loc,
                "'return' fonksiyon dışında kullanılamaz",
                "return ifadesini bir fonksiyon gövdesine taşıyın: `func isim() : tip { return deger; }`");
        break;

    case ASTKind::VariableDecl: {
        // sibling'leri de gez
        for (ASTNode* sib : node->getChildren())
            if (sib->kind == ASTKind::VariableDecl) walkStmt(sib);
        break;
    }

    case ASTKind::ExpressionStatement:
        break; // ifade içinde return/break olamaz

    default:
        break;
    }
}
