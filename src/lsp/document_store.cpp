#include "lsp/document_store.hpp"
#include "module/module_loader.hpp"
#include "symbol/symbol_collector.hpp"
#include "semantic/type_checker.hpp"
#include "semantic/structural_validator.hpp"

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
    state.symbolTable = SymbolTable{};

    // URI'den dosya yolu çıkar (file:///... → /...)
    std::string filePath = state.uri;
    if (filePath.rfind("file://", 0) == 0)
        filePath = filePath.substr(7);

    ModuleRegistry registry;
    ModuleGraph    graph = ModuleLoader(registry, state.diagnostics)
                               .load(filePath);

    if (state.diagnostics.hasErrors()) return;

    SymbolCollector(state.symbolTable, state.diagnostics)
        .collectModuleGraph(graph);

    if (state.diagnostics.hasErrors()) return;

    for (auto& unit : graph.units)
        TypeChecker(state.symbolTable, state.diagnostics).check(unit.ast);
    for (auto& unit : graph.units)
        StructuralValidator(state.diagnostics).validate(unit.ast);

    // AST sahipliğini DocumentState'e aktar
    if (!graph.units.empty()) {
        state.ast        = graph.units[0].ast;
        graph.units[0].ast = nullptr;
    }
}
