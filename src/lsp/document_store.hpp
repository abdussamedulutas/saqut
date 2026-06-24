#ifndef SAQUT_LSP_DOCUMENT_STORE
#define SAQUT_LSP_DOCUMENT_STORE

#include <string>
#include <unordered_map>
#include <memory>
#include "parser/ast_node.hpp"
#include "symbol/symbol_table.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "core/module_registry.hpp"

struct DocumentState {
    std::string      uri;
    std::string      content;
    int              version = 0;
    ASTNode*         ast     = nullptr;
    SymbolTable      symbolTable;
    DiagnosticEngine diagnostics;

    ~DocumentState() { delete ast; }
    DocumentState() = default;
    DocumentState(const DocumentState&) = delete;
    DocumentState& operator=(const DocumentState&) = delete;
};

class DocumentStore {
public:
    DocumentState& update(const std::string& uri,
                          const std::string& content, int version);
    DocumentState* get(const std::string& uri);
    void           close(const std::string& uri);

private:
    void runPipeline(DocumentState& state);

    std::unordered_map<std::string, std::unique_ptr<DocumentState>> store_;
};

#endif // SAQUT_LSP_DOCUMENT_STORE
