// ============================================================================
// saQut — Ölü Kod Eleme (Dead Code Elimination) Pass (ADR-009)
//
// Bir Block içinde ilk return/break/continue'dan sonra gelen deyimler
// erişilemez (unreachable) olarak işaretlenir ve silinir.
// ============================================================================

#ifndef SAQUT_OPT_DEAD_CODE_ELIM
#define SAQUT_OPT_DEAD_CODE_ELIM

#include <string>
#include <algorithm>
#include "opt/optimization_pass.hpp"
#include "parser/nodes/statements.hpp"
#include "parser/nodes/declarations.hpp"
#include "parser/nodes/expressions.hpp"
#include "parser/nodes/program.hpp"

class DeadCodeElimPass : public OptimizationPass {
public:
    bool run(ASTNode* root, SymbolTable*) override {
        changed_ = false;
        visit(root);
        return changed_;
    }

    const std::string& name() const override {
        static std::string n = "dead-code-elim";
        return n;
    }

private:
    bool changed_ = false;

    void visit(ASTNode* node) {
        if (!node) return;

        switch (node->kind) {

        // ── Block: ana DCE mantığı ───────────────────────────────────────────
        case ASTKind::Block: {
            auto* blk   = static_cast<BlockNode*>(node);
            auto& ch    = blk->getChildren();
            bool  term  = false;

            for (auto* child : ch) {
                if (term) {
                    // Bu deyim erişilemez
                    if (auto* sn = dynamic_cast<StatementNode*>(child)) {
                        if (sn->isReachable) {
                            sn->isReachable = false;
                            changed_ = true;
                        }
                    }
                }
                if (child->kind == ASTKind::ReturnStatement ||
                    child->kind == ASTKind::BreakStatement  ||
                    child->kind == ASTKind::ContinueStatement)
                    term = true;
            }

            // Erişilemez çocukları sil ve vektörden çıkar
            ch.erase(std::remove_if(ch.begin(), ch.end(),
                [](ASTNode* n) {
                    auto* sn = dynamic_cast<StatementNode*>(n);
                    return sn && !sn->isReachable;
                }), ch.end());

            // Alt bloklara da in
            for (auto* child : ch) visit(child);
            break;
        }

        // ── İf/While/For gövdelerine in ─────────────────────────────────────
        case ASTKind::IfStatement: {
            auto* ifs = static_cast<IfStatementNode*>(node);
            visit(ifs->thenBranch);
            visit(ifs->elseBranch);
            break;
        }

        case ASTKind::WhileStatement: {
            auto* ws = static_cast<WhileStatementNode*>(node);
            visit(ws->body);
            break;
        }

        case ASTKind::ForStatement: {
            auto* fs = static_cast<ForStatementNode*>(node);
            visit(fs->body);
            break;
        }

        case ASTKind::DoWhileStatement: {
            auto* dw = static_cast<DoWhileStatementNode*>(node);
            visit(dw->body);
            break;
        }

        // ── Üst düzey: fonksiyon gövdelerine in ─────────────────────────────
        case ASTKind::FunctionDecl:
        case ASTKind::Program:
        default:
            for (auto* ch : node->getChildren()) visit(ch);
            break;
        }
    }
};

#endif // SAQUT_OPT_DEAD_CODE_ELIM
