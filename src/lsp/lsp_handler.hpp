#ifndef SAQUT_LSP_HANDLER
#define SAQUT_LSP_HANDLER

#include "vendor/nlohmann/json.hpp"
#include "lsp/document_store.hpp"
#include "lsp/json_rpc.hpp"
#include <ostream>
#include <string>

class LspHandler {
public:
    explicit LspHandler(std::ostream& out) : out_(out) {}

    nlohmann::json dispatch(const nlohmann::json& msg);

private:
    std::ostream& out_;
    DocumentStore store_;
    bool          shutdownRequested_ = false;

    nlohmann::json handleInitialize(const nlohmann::json& id,
                                    const nlohmann::json& params);
    void handleDidOpen(const nlohmann::json& params);
    void handleDidChange(const nlohmann::json& params);
    void handleDidClose(const nlohmann::json& params);
    nlohmann::json handleDefinition(const nlohmann::json& id,
                                    const nlohmann::json& params);
    nlohmann::json handleHover(const nlohmann::json& id,
                               const nlohmann::json& params);
    nlohmann::json handleReferences(const nlohmann::json& id,
                                    const nlohmann::json& params);
    nlohmann::json handleDocumentSymbol(const nlohmann::json& id,
                                        const nlohmann::json& params);
    nlohmann::json handleDocumentHighlight(const nlohmann::json& id,
                                           const nlohmann::json& params);
    nlohmann::json handleCompletion(const nlohmann::json& id,
                                    const nlohmann::json& params);

    void publishDiagnostics(const std::string& uri,
                            const DiagnosticEngine& diag);

    // Verilen (0-tabanlı) satır/sütun pozisyonundaki sembolü bul
    Symbol* findSymbolAt(DocumentState& state, int line, int character);
};

#endif // SAQUT_LSP_HANDLER
