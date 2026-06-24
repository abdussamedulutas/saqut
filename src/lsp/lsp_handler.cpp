#include "lsp/lsp_handler.hpp"
#include "symbol/symbol_table.hpp"
#include "symbol/symbol.hpp"
#include "core/type.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// dispatch — gelen JSON-RPC mesajını yönlendir
// ─────────────────────────────────────────────────────────────────────────────

nlohmann::json LspHandler::dispatch(const nlohmann::json& msg) {
    if (msg.is_discarded() || !msg.contains("method")) return nullptr;

    std::string method = msg["method"].get<std::string>();
    nlohmann::json id  = msg.value("id", nlohmann::json(nullptr));
    nlohmann::json params = msg.value("params", nlohmann::json::object());

    if (method == "initialize")
        return handleInitialize(id, params);

    if (method == "initialized")
        return nullptr; // notification, cevap yok

    if (method == "shutdown") {
        shutdownRequested_ = true;
        return JsonRpc::makeResponse(id, nullptr);
    }

    if (method == "exit") {
        std::exit(shutdownRequested_ ? 0 : 1);
    }

    if (method == "textDocument/didOpen") {
        handleDidOpen(params);
        return nullptr;
    }
    if (method == "textDocument/didChange") {
        handleDidChange(params);
        return nullptr;
    }
    if (method == "textDocument/didClose") {
        handleDidClose(params);
        return nullptr;
    }

    if (method == "textDocument/definition")
        return handleDefinition(id, params);

    if (method == "textDocument/hover")
        return handleHover(id, params);

    if (method == "textDocument/references")
        return handleReferences(id, params);

    // Bilinmeyen metod — null döndür (notification) veya boş cevap
    if (!id.is_null())
        return JsonRpc::makeError(id, -32601, "Method not found: " + method);
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tier 0 — Zorunlu metodlar
// ─────────────────────────────────────────────────────────────────────────────

nlohmann::json LspHandler::handleInitialize(const nlohmann::json& id,
                                             const nlohmann::json& /*params*/) {
    nlohmann::json capabilities = {
        {"textDocumentSync", 1},
        {"definitionProvider",        true},
        {"referencesProvider",        true},
        {"hoverProvider",             true},
        {"documentHighlightProvider", true},
    };
    nlohmann::json result = {
        {"capabilities", capabilities},
        {"serverInfo",   {{"name", "saQut"}, {"version", "0.1.0"}}}
    };
    return JsonRpc::makeResponse(id, result);
}

void LspHandler::handleDidOpen(const nlohmann::json& params) {
    auto& doc = params["textDocument"];
    std::string uri     = doc["uri"].get<std::string>();
    std::string content = doc["text"].get<std::string>();
    int         version = doc.value("version", 0);

    DocumentState& state = store_.update(uri, content, version);
    publishDiagnostics(uri, state.diagnostics);
}

void LspHandler::handleDidChange(const nlohmann::json& params) {
    std::string uri     = params["textDocument"]["uri"].get<std::string>();
    int         version = params["textDocument"].value("version", 0);
    std::string content;

    if (params.contains("contentChanges") && !params["contentChanges"].empty()) {
        content = params["contentChanges"].back()["text"].get<std::string>();
    }

    DocumentState& state = store_.update(uri, content, version);
    publishDiagnostics(uri, state.diagnostics);
}

void LspHandler::handleDidClose(const nlohmann::json& params) {
    std::string uri = params["textDocument"]["uri"].get<std::string>();
    store_.close(uri);
    // Kapalı belgeden tanılamaları temizle
    auto notif = JsonRpc::makeNotification("textDocument/publishDiagnostics", {
        {"uri", uri}, {"diagnostics", nlohmann::json::array()}
    });
    JsonRpc::writeMessage(out_, notif);
}

void LspHandler::publishDiagnostics(const std::string& uri,
                                     const DiagnosticEngine& diag) {
    auto notif = JsonRpc::makeNotification("textDocument/publishDiagnostics", {
        {"uri",         uri},
        {"diagnostics", diag.toLspDiagnostics()}
    });
    JsonRpc::writeMessage(out_, notif);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tier 1 — Sembol bilgisi metodları
// ─────────────────────────────────────────────────────────────────────────────

Symbol* LspHandler::findSymbolAt(DocumentState& state, int line, int character) {
    // 0-tabanlı LSP konumunu 1-tabanlı SourceLocation'a çevir
    int srcLine = line + 1;
    int srcCol  = character + 1;

    for (Symbol* sym : state.symbolTable.allSymbols()) {
        if (sym->definitionLoc.line == srcLine &&
            sym->definitionLoc.column <= srcCol &&
            srcCol <= sym->definitionLoc.column + (int)sym->name.size()) {
            return sym;
        }
        for (const auto& ref : sym->references) {
            if (ref.line == srcLine &&
                ref.column <= srcCol &&
                srcCol <= ref.column + (int)sym->name.size()) {
                return sym;
            }
        }
    }
    return nullptr;
}

nlohmann::json LspHandler::handleDefinition(const nlohmann::json& id,
                                             const nlohmann::json& params) {
    std::string uri  = params["textDocument"]["uri"].get<std::string>();
    int         line = params["position"]["line"].get<int>();
    int         ch   = params["position"]["character"].get<int>();

    DocumentState* state = store_.get(uri);
    if (!state) return JsonRpc::makeResponse(id, nullptr);

    Symbol* sym = findSymbolAt(*state, line, ch);
    if (!sym || !sym->definitionLoc.isValid())
        return JsonRpc::makeResponse(id, nullptr);

    auto pos = sym->definitionLoc.toLspPosition();
    nlohmann::json result = {
        {"uri", uri},
        {"range", {
            {"start", {{"line", pos.line}, {"character", pos.character}}},
            {"end",   {{"line", pos.line}, {"character", pos.character + (int)sym->name.size()}}}
        }}
    };
    return JsonRpc::makeResponse(id, result);
}

nlohmann::json LspHandler::handleHover(const nlohmann::json& id,
                                        const nlohmann::json& params) {
    std::string uri  = params["textDocument"]["uri"].get<std::string>();
    int         line = params["position"]["line"].get<int>();
    int         ch   = params["position"]["character"].get<int>();

    DocumentState* state = store_.get(uri);
    if (!state) return JsonRpc::makeResponse(id, nullptr);

    Symbol* sym = findSymbolAt(*state, line, ch);
    if (!sym) return JsonRpc::makeResponse(id, nullptr);

    std::string kindStr = symbolKindName(sym->kind);
    std::string typeStr = sym->type.toString();
    std::string content = "**" + sym->name + "**: " + typeStr +
                          " (" + kindStr + ")";

    nlohmann::json result = {
        {"contents", {{"kind", "markdown"}, {"value", content}}}
    };
    return JsonRpc::makeResponse(id, result);
}

nlohmann::json LspHandler::handleReferences(const nlohmann::json& id,
                                             const nlohmann::json& params) {
    std::string uri  = params["textDocument"]["uri"].get<std::string>();
    int         line = params["position"]["line"].get<int>();
    int         ch   = params["position"]["character"].get<int>();

    DocumentState* state = store_.get(uri);
    if (!state) return JsonRpc::makeResponse(id, nullptr);

    Symbol* sym = findSymbolAt(*state, line, ch);
    if (!sym) return JsonRpc::makeResponse(id, nlohmann::json::array());

    bool includeDecl = params.value("context", nlohmann::json::object())
                             .value("includeDeclaration", false);

    nlohmann::json locs = nlohmann::json::array();

    if (includeDecl && sym->definitionLoc.isValid()) {
        auto pos = sym->definitionLoc.toLspPosition();
        locs.push_back({
            {"uri", uri},
            {"range", {
                {"start", {{"line", pos.line}, {"character", pos.character}}},
                {"end",   {{"line", pos.line}, {"character", pos.character + (int)sym->name.size()}}}
            }}
        });
    }

    for (const auto& ref : sym->references) {
        auto pos = ref.toLspPosition();
        locs.push_back({
            {"uri", uri},
            {"range", {
                {"start", {{"line", pos.line}, {"character", pos.character}}},
                {"end",   {{"line", pos.line}, {"character", pos.character + (int)sym->name.size()}}}
            }}
        });
    }

    return JsonRpc::makeResponse(id, locs);
}
