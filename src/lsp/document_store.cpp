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

static std::string uriToPath(const std::string& uri) {
    std::string s = uri;
    if (s.rfind("file://", 0) == 0)
        s = s.substr(7);
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = std::isdigit(s[i+1]) ? s[i+1]-'0' : std::tolower(s[i+1])-'a'+10;
            int lo = std::isdigit(s[i+2]) ? s[i+2]-'0' : std::tolower(s[i+2])-'a'+10;
            out += static_cast<char>(hi * 16 + lo);
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

void DocumentStore::runPipeline(DocumentState& state) {
    state.diagnostics = DiagnosticEngine{};
    state.symbolTable = SymbolTable{};

    std::string filePath = uriToPath(state.uri);

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
