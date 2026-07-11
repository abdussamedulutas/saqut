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
    // Faz 2 ("son iyi tablo"): SymbolTable unique_ptr tabanlı sahiplik kullandığı
    // için kopyalanamaz, yalnızca taşınabilir — bu yüzden ayrı bir
    // lastGoodSymbolTable alanı yerine symbolTable'ın KENDİSİ bu rolü üstlenir:
    // runPipeline yalnızca yeni bir tablo üretebildiğinde üzerine yazar (bkz.
    // document_store.cpp), modül hiç yüklenemediğinde dokunmadan bırakır.
    // TODO(faz-ileri): SymbolCollector bir gün gerçekten yarıda kesilebilir hale
    // gelirse (bugün mümkün değil — hep tamamlanır), gerçek bir "son iyi" anlık
    // görüntüsü için SymbolTable derin kopyalanabilir hale getirilmeli.
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
