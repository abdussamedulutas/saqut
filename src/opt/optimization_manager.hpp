// ============================================================================
// saQut — Optimizasyon Yöneticisi (ADR-007, ADR-009)
//
// İKİ KULLANIM YOLU:
//
// 1. runPassesInPlace(root, table)
//    Pass'leri verilen AST üstünde doğrudan çalıştırır — klon yok.
//    run / ir / transpile gibi "tek seferlik" komutlar bu yolu kullanır:
//    AST'nin tek versiyonu gerekiyor, orijinali saklamaya gerek yok.
//
// 2. optimize(root, table)         [sadece ast komutu için]
//    Önce deepClone, sonra runPassesInPlace. Orijinali dokunulmaz bırakır.
//    Çıktı: optimize edilmiş klon (caller delete eder).
//    ast komutunun "öncesi / sonrası" karşılaştırması için zorunlu.
//    Diğer komutlar bu yolu ÇAĞIRMAMALI — gereksiz klon maliyeti.
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

    // Pass'leri verilen AST üstünde yerinde çalıştırır — klon yok.
    // run / ir ve diğer tek-versiyon komutları bu yolu kullanır.
    void runPassesInPlace(ASTNode* root, SymbolTable* table) {
        for (int round = 0; round < maxRounds_; ++round) {
            bool anyChange = false;
            for (auto& pass : passes_)
                if (pass->run(root, table)) anyChange = true;
            if (!anyChange) break;
        }
    }

    // Önce deepClone, sonra runPassesInPlace. Orijinal dokunulmaz.
    // SADECE ast komutu kullanır — öncesi/sonrası karşılaştırması için.
    // Dönen pointer caller'a aittir (delete edilmeli).
    ASTNode* optimize(ASTNode* root, SymbolTable* table) {
        ASTNode* clone = deepClone(root);
        runPassesInPlace(clone, table);
        return clone;
    }

private:
    std::vector<std::unique_ptr<OptimizationPass>> passes_;
    int maxRounds_ = 10;
};

#endif // SAQUT_OPT_MANAGER
