// ============================================================================
// saQut — ModuleGraph (Çok Dosyalı Derleme Birimi)
// ============================================================================
//
// DİZİN:   src/module/module_graph.hpp
// KATMAN:  Modül Sistemi — Tüm bağımlı dosyaların parse edilmiş hali
//
// AMAÇ:
//   Bir .sqt dosyasının ve tüm import bağımlılıklarının parse edilmiş
//   AST + token'larını tutar. Move-only; sahiplik tek noktada.
//
// ============================================================================

#ifndef SAQUT_MODULE_GRAPH
#define SAQUT_MODULE_GRAPH

#include <string>
#include <vector>
#include "parser/ast_node.hpp"
#include "tokenizer/tokenizer.hpp"

// Tek bir .sqt dosyasının parse edilmiş durumu.
struct ModuleUnit {
    std::string         filePath;    // canonical mutlak yol
    int                 moduleId;    // ModuleRegistry ID
    ASTNode*            ast;         // sahiplik bu struct'ta
    std::vector<Token*> tokens;      // bellek yönetimi için (ast ile birlikte silinir)
};

// Tüm bağımlı modüllerin düz listesi.
// units[0] = giriş dosyası (main), geri kalanlar bağımlılıklar.
// Sıra garantisi yoktur — SymbolCollector 3 geçişle sıra bağımsızlığını sağlar.
struct ModuleGraph {
    std::vector<ModuleUnit> units;

    ~ModuleGraph() {
        for (auto& u : units) {
            delete u.ast;
            for (auto* t : u.tokens) delete t;
        }
    }

    // Kopyalamayı kapat — sahiplik sadece burada.
    ModuleGraph()                            = default;
    ModuleGraph(const ModuleGraph&)          = delete;
    ModuleGraph& operator=(const ModuleGraph&) = delete;
    ModuleGraph(ModuleGraph&&)               = default;
    ModuleGraph& operator=(ModuleGraph&&)    = default;
};

#endif // SAQUT_MODULE_GRAPH
