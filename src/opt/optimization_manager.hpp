// ============================================================================
// saQut — Optimizasyon Yöneticisi (ADR-007, ADR-009)
//
// 1. AST'yi klonlar (orijinal dokunulmaz).
// 2. Etkin pass'leri fixpoint döngüsüyle çalıştırır.
// 3. Optimize edilmiş klon sahipliğini döndürür (caller delete eder).
//
// Fixpoint garantisi: her pass yalnızca küçülten dönüşümler yapar
// (katlama: n düğüm → 1 düğüm; DCE: düğüm siler). Büyüten pass
// (inlining vb.) eklenirse maxFixpointRounds tavanı zorunludur (ADR-009).
// ============================================================================

#ifndef SAQUT_OPT_MANAGER
#define SAQUT_OPT_MANAGER

#include <memory>
#include <vector>
#include "core/config.hpp"
#include "opt/ast_clone.hpp"
#include "opt/optimization_pass.hpp"
#include "opt/constant_folding.hpp"
#include "opt/dead_code_elim.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "symbol/symbol_table.hpp"

class OptimizationManager {
public:
    explicit OptimizationManager(const CompilerConfig& cfg, DiagnosticEngine& diag) {
        if (cfg.optConstantFolding)
            passes_.push_back(std::make_unique<ConstantFoldingPass>(diag));
        if (cfg.optDeadCodeElim)
            passes_.push_back(std::make_unique<DeadCodeElimPass>());
        maxRounds_ = cfg.maxFixpointRounds;
    }

    // optimize: AST'yi klonlar ve optimize edilmiş kopyayı döndürür.
    // Dönen pointer caller'a aittir (delete edilmeli).
    ASTNode* optimize(ASTNode* root, SymbolTable* table) {
        ASTNode* clone = deepClone(root);

        for (int round = 0; round < maxRounds_; ++round) {
            bool anyChange = false;
            for (auto& pass : passes_)
                if (pass->run(clone, table)) anyChange = true;
            if (!anyChange) break;
        }

        return clone;
    }

private:
    std::vector<std::unique_ptr<OptimizationPass>> passes_;
    int maxRounds_ = 10;
};

#endif // SAQUT_OPT_MANAGER
