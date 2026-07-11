#include "lsp/document_store.hpp"
#include "lsp/uri.hpp"
#include "module/module_loader.hpp"
#include "symbol/symbol_collector.hpp"
#include "semantic/type_checker.hpp"
#include "semantic/structural_validator.hpp"
#include <filesystem>

namespace fs = std::filesystem;

DocumentState& DocumentStore::update(const std::string& uri,
                                      const std::string& content, int version) {
    auto it = store_.find(uri);
    if (it == store_.end()) {
        store_[uri] = std::make_unique<DocumentState>();
        it = store_.find(uri);
        it->second->uri = uri;
    }
    DocumentState& state = *it->second;
    state.content = content;
    state.version = version;
    delete state.ast;
    state.ast = nullptr;
    runPipeline(state);
    return state;
}

DocumentState* DocumentStore::get(const std::string& uri) {
    auto it = store_.find(uri);
    return (it != store_.end()) ? it->second.get() : nullptr;
}

void DocumentStore::close(const std::string& uri) {
    store_.erase(uri);
}

void DocumentStore::runPipeline(DocumentState& state) {
    state.diagnostics = DiagnosticEngine{};
    // NOT: state.symbolTable BURADA sıfırlanmaz. Faz 2 — parser artık panic-mode
    // recovery ile sözdizimi hatasında bile her zaman bir AST döndürür, bu yüzden
    // SymbolCollector normal şartlarda her turda çalışıp tabloyu yeniden kurar
    // (aşağıda). Tablo yalnızca modül hiç yüklenemediğinde (dosya bulunamadı vb.)
    // dokunulmadan kalır — LSP sorguları böylece bir önceki başarılı turun
    // ("son iyi") tablosuna düşmüş olur.

    std::string filePath = uriToPath(state.uri);

    // Overlay: derleme diski değil, açık olan editör buffer'larını görür.
    // Böylece A.sqt import ettiği B.sqt editörde açıksa, B'nin kaydedilmemiş
    // hali kullanılır (Faz 1, ADR: kaynak overlay).
    ModuleLoader::SourceOverlay overlay =
        [this](const std::string& path, std::string& out) -> bool {
            for (auto& [uri, docState] : store_) {
                std::string docPath = fs::weakly_canonical(uriToPath(uri)).string();
                if (docPath == path) {
                    out = docState->content;
                    return true;
                }
            }
            return false;
        };

    ModuleRegistry registry;
    ModuleGraph    graph = ModuleLoader(registry, state.diagnostics, overlay)
                               .load(filePath);

    // Faz 2: modül hiç yüklenemediyse (örn. dosya bulunamadı — overlay ve disk
    // ikisi de başarısız) toplanacak bir AST yok; sembol tablosu bir önceki
    // başarılı turdan kalan haliyle bırakılır ("son iyi tablo").
    if (graph.units.empty()) return;

    state.symbolTable = SymbolTable{};
    SymbolCollector(state.symbolTable, state.diagnostics)
        .collectModuleGraph(graph);

    // Faz 2: erken dönüş YOK. Parser artık sözdizimi hatalarında bile
    // (panic-mode recovery ile) tam bir AST döndürdüğü için, bir hata olsa
    // dahi hatanın DIŞINDAKİ fonksiyonlar için hover/definition/documentSymbol
    // çalışmaya devam etsin diye TypeChecker/StructuralValidator'a kadar iniyoruz.
    // Bu katmanlar ErrorNode'u (default: dalı) sessizce atlar.
    for (auto& unit : graph.units)
        TypeChecker(state.symbolTable, state.diagnostics).check(unit.ast);
    for (auto& unit : graph.units)
        StructuralValidator(state.diagnostics).validate(unit.ast);

    // AST sahipliğini DocumentState'e aktar
    state.ast          = graph.units[0].ast;
    graph.units[0].ast = nullptr;
}
