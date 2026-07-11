#include "lsp/lsp_handler.hpp"
#include "symbol/symbol_table.hpp"
#include "symbol/symbol.hpp"
#include "core/type.hpp"
#include "lsp/uri.hpp"
#include "lsp/position.hpp"
#include <algorithm>
#include <map>

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

    if (method == "textDocument/documentSymbol")
        return handleDocumentSymbol(id, params);

    if (method == "textDocument/documentHighlight")
        return handleDocumentHighlight(id, params);

    if (method == "textDocument/completion")
        return handleCompletion(id, params);

    // Bilinmeyen metod — null döndür (notification) veya boş cevap
    if (!id.is_null())
        return JsonRpc::makeError(id, -32601, "Method not found: " + method);
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tier 0 — Zorunlu metodlar
// ─────────────────────────────────────────────────────────────────────────────

nlohmann::json LspHandler::handleInitialize(const nlohmann::json& id,
                                             const nlohmann::json& params) {
    // Faz 3: konum birimi anlaşması. LSP varsayılanı UTF-16'dır; istemci
    // general.positionEncodings'te "utf-8" listeliyorsa onu seçiyoruz —
    // SourceLocation.column zaten byte/UTF-8 code unit saydığı için bu
    // durumda hiç dönüşüm gerekmez (src/lsp/position.hpp devre dışı kalır).
    positionEncoding_ = "utf-16";
    if (params.contains("capabilities") && params["capabilities"].contains("general")) {
        auto& general = params["capabilities"]["general"];
        if (general.contains("positionEncodings") && general["positionEncodings"].is_array()) {
            for (auto& enc : general["positionEncodings"]) {
                if (enc.is_string() && enc.get<std::string>() == "utf-8") {
                    positionEncoding_ = "utf-8";
                    break;
                }
            }
        }
    }

    nlohmann::json capabilities = {
        {"textDocumentSync", 1},
        {"positionEncoding", positionEncoding_},
        {"definitionProvider",        true},
        {"referencesProvider",        true},
        {"hoverProvider",             true},
        {"documentSymbolProvider",    true},
        {"documentHighlightProvider", true},
        {"completionProvider", {
            {"triggerCharacters", nlohmann::json::array({":", "."})}
        }},
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
    publishDiagnosticsGrouped(state);
}

void LspHandler::handleDidChange(const nlohmann::json& params) {
    std::string uri     = params["textDocument"]["uri"].get<std::string>();
    int         version = params["textDocument"].value("version", 0);
    std::string content;

    if (params.contains("contentChanges") && !params["contentChanges"].empty()) {
        content = params["contentChanges"].back()["text"].get<std::string>();
    }

    DocumentState& state = store_.update(uri, content, version);
    publishDiagnosticsGrouped(state);
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

// Faz 3: state.diagnostics artık modül grafiğindeki TÜM dosyalardan gelen
// tanıları içerebilir (bir import edilen modülün hatası da burada olabilir).
// Eskiden hepsi sorgulanan `uri`'ye basılıyordu (kök neden #4 — import edilen
// modülün hatası ana dosyada görünüyordu). Şimdi loc.filePath'e göre gruplayıp
// her dosya için ayrı publishDiagnostics gönderiyoruz. std::map (sıralı) —
// bildirim SIRASI testte önemli, unordered_map olsaydı çalıştırmalar arası
// deterministik olmazdı.
void LspHandler::publishDiagnosticsGrouped(DocumentState& state) {
    std::map<std::string, nlohmann::json> byFile;
    byFile[state.filePath] = nlohmann::json::array(); // sorgulanan dosya her zaman bir bildirim alır (stale temizliği)

    for (const auto& d : state.diagnostics.all()) {
        // Konumsuz tanılar (ör. E_MODULE_NOT_FOUND — SourceLocation{} boş
        // filePath'le gelir) sorgulanan dosyaya düşer; eski davranışla aynı.
        std::string fp = d.loc.filePath.empty() ? state.filePath : d.loc.filePath;
        std::string content = (fp == state.filePath) ? state.content : store_.contentForPath(fp);
        LspPosition pos = toLspPos(content, d.loc);

        nlohmann::json item;
        item["range"] = {
            {"start", {{"line", pos.line}, {"character", pos.character}}},
            {"end",   {{"line", pos.line}, {"character", pos.character + d.tokenLength}}}
        };
        item["severity"] = (d.level == DiagLevel::Error) ? 1 : 2;
        item["code"]     = d.code;
        item["message"]  = d.hint.empty() ? d.message : d.message + "\n" + d.hint;
        item["source"]   = "saQut";

        byFile[fp].push_back(item);
    }

    for (auto& [fp, diagsJson] : byFile) {
        auto notif = JsonRpc::makeNotification("textDocument/publishDiagnostics", {
            {"uri",         store_.uriForPath(fp)},
            {"diagnostics", diagsJson}
        });
        JsonRpc::writeMessage(out_, notif);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Faz 3 — pozisyon dönüşümü yardımcıları
// ─────────────────────────────────────────────────────────────────────────────

int LspHandler::toByteColumn(const std::string& content, int line, int character) const {
    if (positionEncoding_ == "utf-8") return character + 1; // zaten byte birimi
    return lspToByteCol(content, line, character);
}

LspPosition LspHandler::toLspPos(const std::string& content, const SourceLocation& loc) const {
    if (!loc.isValid()) return {0, 0};
    if (positionEncoding_ == "utf-8" || content.empty())
        return loc.toLspPosition(); // byte==utf-8-birim; içerik yoksa en iyi çaba
    return { loc.line - 1, byteColToLsp(content, loc.line - 1, loc.column) };
}

std::string LspHandler::contentForLoc(DocumentState& state, const SourceLocation& loc) const {
    if (loc.filePath == state.filePath) return state.content;
    return store_.contentForPath(loc.filePath);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tier 1 — Sembol bilgisi metodları
// ─────────────────────────────────────────────────────────────────────────────

// Faz 3: token binary search + (offset→Symbol*) indeksi (kök neden #3).
// 1) İstemci pozisyonunu (utf-16 veya utf-8) bu belgenin byte offset'ine çevir.
// 2) O offset'i kapsayan token'ı state.tokens'ta binary search ile bul.
// 3) Token identifier değilse (keyword/operator/...) sembol yok.
// 4) Token'ın başlangıç offset'i state.symbolByOffset'te varsa, scope-doğru
//    çözülmüş sembolü döndür (runPipeline'da SymbolCollector'ın resolvedSymbol
//    ataması sırasında toplanan referans/definition offsetlerinden kurulur).
Symbol* LspHandler::findSymbolAt(DocumentState& state, int line, int character) {
    int byteCol = toByteColumn(state.content, line, character);
    int offset  = lspLineStartOffset(state.content, line) + (byteCol - 1);

    const auto& toks = state.tokens;
    auto it = std::upper_bound(toks.begin(), toks.end(), offset,
        [](int off, Token* t) { return off < t->start; });
    if (it == toks.begin()) return nullptr;
    Token* tok = *std::prev(it);
    if (offset < tok->start || offset >= tok->end) return nullptr;
    if (tok->gettype() != "identifier") return nullptr;

    auto found = state.symbolByOffset.find(tok->start);
    return (found != state.symbolByOffset.end()) ? found->second : nullptr;
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

    // Faz 3: tanım sorgulanan dosyada olmayabilir (import edilen sembol) —
    // artık HER ZAMAN sorgulanan URI değil, sym->definitionLoc.filePath'in
    // gerçek URI'si döner (kök neden #4).
    std::string targetContent = contentForLoc(*state, sym->definitionLoc);
    LspPosition pos = toLspPos(targetContent, sym->definitionLoc);
    nlohmann::json result = {
        {"uri", store_.uriForPath(sym->definitionLoc.filePath)},
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

    std::string content;
    if (sym->kind == SymbolKind::Function && sym->type.isFunction()) {
        // "int gcd(int a, int b)"
        std::string ret = sym->type.returnType ? sym->type.returnType->toString() : "void";
        std::string sig = ret + " " + sym->name + "(";
        for (size_t i = 0; i < sym->type.paramTypes.size(); ++i) {
            if (i > 0) sig += ", ";
            sig += sym->type.paramTypes[i].toString();
            if (i < sym->paramNames.size())
                sig += " " + sym->paramNames[i];
        }
        sig += ")";
        content = "```sqt\n" + sig + "\n```";
    } else if (sym->kind == SymbolKind::Struct) {
        content = "```sqt\nstruct " + sym->name + "\n```";
    } else if (sym->kind == SymbolKind::Enum) {
        content = "```sqt\nenum " + sym->name + "\n```";
    } else {
        // değişken / parametre / alan
        std::string typeStr = sym->type.toString();
        content = "```sqt\n" + typeStr + " " + sym->name + "\n```";
    }

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

    // Faz 3: her konum KENDİ dosyasının URI'siyle döner — sym->references
    // farklı dosyalardan gelebilir (bu sembolü import eden başka bir modül),
    // eskiden hepsi sorgulanan `uri`'ye kopyalanıyordu (kök neden #4).
    auto addLoc = [&](const SourceLocation& loc) {
        if (!loc.isValid()) return;
        std::string content = contentForLoc(*state, loc);
        LspPosition pos = toLspPos(content, loc);
        locs.push_back({
            {"uri", store_.uriForPath(loc.filePath)},
            {"range", {
                {"start", {{"line", pos.line}, {"character", pos.character}}},
                {"end",   {{"line", pos.line}, {"character", pos.character + (int)sym->name.size()}}}
            }}
        });
    };

    if (includeDecl) addLoc(sym->definitionLoc);
    for (const auto& ref : sym->references) addLoc(ref);

    return JsonRpc::makeResponse(id, locs);
}

// LSP SymbolKind sayıları: Function=12, Variable=13, Struct=23, Enum=10, EnumMember=22, Field=8
static int lspSymbolKind(SymbolKind k) {
    switch (k) {
        case SymbolKind::Function:   return 12;
        case SymbolKind::Struct:     return 23;
        case SymbolKind::Enum:       return 10;
        case SymbolKind::EnumValue:  return 22;
        case SymbolKind::Field:      return 8;
        case SymbolKind::Variable:   return 13;
        case SymbolKind::Parameter:  return 13;
    }
    return 13;
}

nlohmann::json LspHandler::handleDocumentSymbol(const nlohmann::json& id,
                                                 const nlohmann::json& params) {
    std::string uri = params["textDocument"]["uri"].get<std::string>();
    DocumentState* state = store_.get(uri);
    if (!state) return JsonRpc::makeResponse(id, nlohmann::json::array());

    nlohmann::json symbols = nlohmann::json::array();

    for (Symbol* sym : state->symbolTable.allSymbols()) {
        // Parametre ve alan sembollerini gizle — gürültü yapar
        if (sym->kind == SymbolKind::Parameter) continue;
        if (sym->kind == SymbolKind::Field)     continue;
        if (!sym->definitionLoc.isValid())      continue;
        // Faz 3: symbolTable tüm modül grafiğini kapsar (import edilen
        // dosyaların sembolleri de içinde) — yalnızca BU belgeye ait olanları
        // listele (kök neden #4).
        if (sym->definitionLoc.filePath != state->filePath) continue;

        LspPosition pos = toLspPos(state->content, sym->definitionLoc);
        int  end = pos.character + static_cast<int>(sym->name.size());

        nlohmann::json range = {
            {"start", {{"line", pos.line}, {"character", pos.character}}},
            {"end",   {{"line", pos.line}, {"character", end}}}
        };

        symbols.push_back({
            {"name",            sym->name},
            {"kind",            lspSymbolKind(sym->kind)},
            {"range",           range},
            {"selectionRange",  range}
        });
    }

    return JsonRpc::makeResponse(id, symbols);
}

// HighlightKind: Text=1, Read=2, Write=3
nlohmann::json LspHandler::handleDocumentHighlight(const nlohmann::json& id,
                                                    const nlohmann::json& params) {
    std::string uri  = params["textDocument"]["uri"].get<std::string>();
    int         line = params["position"]["line"].get<int>();
    int         ch   = params["position"]["character"].get<int>();

    DocumentState* state = store_.get(uri);
    if (!state) return JsonRpc::makeResponse(id, nlohmann::json::array());

    Symbol* sym = findSymbolAt(*state, line, ch);
    if (!sym)   return JsonRpc::makeResponse(id, nlohmann::json::array());

    nlohmann::json highlights = nlohmann::json::array();

    auto makeHighlight = [&](const SourceLocation& loc, int kind) {
        // documentHighlight protokolde tek bir belgeye özeldir (uri alanı
        // yok) — başka dosyadaki referansları BURAYA sızdırmıyoruz (kök
        // neden #4'ün documentHighlight varyantı).
        if (!loc.isValid() || loc.filePath != state->filePath) return;
        LspPosition pos = toLspPos(state->content, loc);
        int  end = pos.character + static_cast<int>(sym->name.size());
        highlights.push_back({
            {"range", {
                {"start", {{"line", pos.line}, {"character", pos.character}}},
                {"end",   {{"line", pos.line}, {"character", end}}}
            }},
            {"kind", kind}
        });
    };

    if (sym->definitionLoc.isValid())
        makeHighlight(sym->definitionLoc, 3); // Write — tanım noktası

    for (const auto& ref : sym->references)
        makeHighlight(ref, 2); // Read — kullanım noktaları

    return JsonRpc::makeResponse(id, highlights);
}

// ─────────────────────────────────────────────────────────────────────────────
// Completion
// ─────────────────────────────────────────────────────────────────────────────

// Satırda imlecin solundaki tanımlayıcıyı döndürür (prefix)
static std::string wordPrefix(const std::string& content, int line, int ch) {
    int curLine = 0;
    size_t i = 0;
    while (i < content.size() && curLine < line) {
        if (content[i++] == '\n') ++curLine;
    }
    size_t lineStart = i;
    size_t end = lineStart + static_cast<size_t>(ch);
    if (end > content.size()) end = content.size();
    size_t start = end;
    while (start > lineStart &&
           (std::isalnum(static_cast<unsigned char>(content[start-1])) ||
            content[start-1] == '_')) {
        --start;
    }
    return content.substr(start, end - start);
}

// İmleç konumundaki satırı, imlece kadar döndürür
static std::string lineUpToCursor(const std::string& content, int line, int ch) {
    int curLine = 0;
    size_t i = 0;
    while (i < content.size() && curLine < line) {
        if (content[i++] == '\n') ++curLine;
    }
    size_t lineStart = i;
    size_t end = lineStart + static_cast<size_t>(ch);
    if (end > content.size()) end = content.size();
    return content.substr(lineStart, end - lineStart);
}

// Satır metninin sonundaki tanımlayıcıyı döndürür ("efsane." → "efsane")
static std::string wordBefore(const std::string& lineText, char delim1, char delim2 = 0) {
    if (lineText.empty()) return "";
    size_t end = lineText.size();
    // Sondaki delimiteri atla
    if (end > 0 && lineText[end-1] == delim1) --end;
    if (delim2 && end > 0 && lineText[end-1] == delim2) --end;
    size_t start = end;
    while (start > 0 && (std::isalnum(static_cast<unsigned char>(lineText[start-1]))
                         || lineText[start-1] == '_'))
        --start;
    return lineText.substr(start, end - start);
}

// CompletionItemKind: Function=3, Variable=6, Field=5, Struct=22, Enum=13, EnumMember=20, Keyword=14
static int completionKind(SymbolKind k) {
    switch (k) {
        case SymbolKind::Function:   return 3;
        case SymbolKind::Variable:   return 6;
        case SymbolKind::Parameter:  return 6;
        case SymbolKind::Field:      return 5;
        case SymbolKind::Struct:     return 22;
        case SymbolKind::Enum:       return 13;
        case SymbolKind::EnumValue:  return 20;
    }
    return 6;
}

nlohmann::json LspHandler::handleCompletion(const nlohmann::json& id,
                                             const nlohmann::json& params) {
    std::string uri  = params["textDocument"]["uri"].get<std::string>();
    int         line = params["position"]["line"].get<int>();
    int         ch   = params["position"]["character"].get<int>();

    DocumentState* state = store_.get(uri);
    if (!state) return JsonRpc::makeResponse(id, nlohmann::json::array());

    nlohmann::json items = nlohmann::json::array();
    std::string lineText = lineUpToCursor(state->content, line, ch);

    // ── "expr." → struct alan tamamlama ──────────────────────────────────────
    if (!lineText.empty() && lineText.back() == '.') {
        std::string objName = wordBefore(lineText, '.');

        // Sembol tablosunda bul, tipini al
        Symbol* objSym = nullptr;
        for (Symbol* s : state->symbolTable.allSymbols()) {
            if (s->name == objName) { objSym = s; break; }
        }

        if (objSym) {
            // Tip adı (SymbolKind::Struct) üzerinde alan tamamlama yapma
            if (objSym->kind == SymbolKind::Struct)
                return JsonRpc::makeResponse(id, items); // boş

            Type t = objSym->type;
            // Nullable wrapper'ı soy
            while (t.isArray() && t.elementType) t = *t.elementType;
            std::string sName = t.isStruct() ? t.structName : "";

            if (!sName.empty() && state->symbolTable.hasStruct(sName)) {
                auto it = state->symbolTable.structLayouts.find(sName);
                if (it != state->symbolTable.structLayouts.end()) {
                    for (auto& [fieldName, fieldType] : it->second) {
                        items.push_back({
                            {"label",  fieldName},
                            {"kind",   5},  // Field
                            {"detail", fieldType.toString()},
                        });
                    }
                }
                return JsonRpc::makeResponse(id, items);
            }
        }
        // struct değilse boş döndür — bilinmeyen nesneye alan önermiyoruz
        return JsonRpc::makeResponse(id, items);
    }

    // ── "expr::" → built-in method tamamlama ─────────────────────────────────
    if (lineText.size() >= 2 &&
        lineText[lineText.size()-1] == ':' &&
        lineText[lineText.size()-2] == ':') {

        std::string objName = wordBefore(lineText, ':', ':');

        // Sembolü bul
        Symbol* objSym = nullptr;
        for (Symbol* s : state->symbolTable.allSymbols()) {
            if (s->name == objName) { objSym = s; break; }
        }

        // Tip adına (struct/enum) :: koymak anlamsız — boş dön
        if (objSym && (objSym->kind == SymbolKind::Struct ||
                       objSym->kind == SymbolKind::Enum))
            return JsonRpc::makeResponse(id, items);

        // Değişkenin tipini belirle
        struct BM { const char* name; const char* sig; const char* detail; int mask; };
        // mask: 1=int/float, 2=string, 4=array, 8=struct, 16=genel
        static const BM builtins[] = {
            {"toStr",      "toStr()",               "() → string",                          1|2|8|16},
            {"toJson",     "toJson()",              "() → string",                              8|16},
            {"dump",       "dump()",                "() → string",                              8|16},
            {"abs",        "abs()",                 "() → T",                                      1},
            {"toInt",      "toInt()",               "() → int",                                  1|2},
            {"toFloat",    "toFloat()",             "() → float",                                1|2},
            {"len",        "len()",                 "() → int",                                  2|4},
            {"toUpper",    "toUpper()",             "() → string",                                 2},
            {"toLower",    "toLower()",             "() → string",                                 2},
            {"trim",       "trim()",                "() → string",                                 2},
            {"startsWith", "startsWith(prefix)",    "(prefix: string) → bool",                     2},
            {"endsWith",   "endsWith(suffix)",      "(suffix: string) → bool",                     2},
            {"indexOf",    "indexOf(s)",            "(s: string) → int",                           2},
            {"substr",     "substr(start, len)",    "(start: int, len: int) → string",             2},
            {"split",      "split(sep)",            "(sep: string) → string[]",                    2},
            {"replace",    "replace(from, to)",     "(from: string, to: string) → string",         2},
            {"contains",   "contains(value)",       "(value) → bool",                            2|4},
            {"push",       "push(value)",           "(value) → void",                              4},
            {"pop",        "pop()",                 "() → T",                                      4},
            {"keys",       "keys()",                "() → string[]",                               4},
            {"values",     "values()",              "() → T[]",                                    4},
        };

        // Hangi maskeler geçerli?
        int allowed = 16; // genel her zaman
        if (objSym) {
            Type t = objSym->type;
            if (t.isArray())  allowed |= 4;
            else if (t.isString()) allowed |= 2;
            else if (t.isStruct()) allowed |= 8;
            else if (t.isPrimitive()) allowed |= 1; // int/float/bool
        } else {
            allowed = 0xFF; // bilinmeyen → hepsini göster
        }

        for (auto& b : builtins) {
            if (!(b.mask & allowed)) continue;
            items.push_back({
                {"label",            b.name},
                {"kind",             2},
                {"detail",           b.detail},
                {"insertText",       b.sig},
                {"insertTextFormat", 2},
            });
        }
        return JsonRpc::makeResponse(id, items);
    }

    // ── Normal prefix tamamlama — semboller + anahtar kelimeler ──────────────
    std::string prefix = wordPrefix(state->content, line, ch);

    for (Symbol* sym : state->symbolTable.allSymbols()) {
        if (sym->kind == SymbolKind::Field) continue; // alanlar sadece . ile gelir
        if (!prefix.empty() && sym->name.rfind(prefix, 0) != 0) continue;

        std::string detail;
        if (sym->kind == SymbolKind::Function && sym->type.isFunction()) {
            std::string ret = sym->type.returnType ? sym->type.returnType->toString() : "void";
            detail = ret + " " + sym->name + "(";
            for (size_t i = 0; i < sym->type.paramTypes.size(); ++i) {
                if (i > 0) detail += ", ";
                detail += sym->type.paramTypes[i].toString();
                if (i < sym->paramNames.size()) detail += " " + sym->paramNames[i];
            }
            detail += ")";
        } else {
            detail = sym->type.toString();
        }

        items.push_back({
            {"label",  sym->name},
            {"kind",   completionKind(sym->kind)},
            {"detail", detail},
        });
    }

    static const std::vector<std::string> keywords = {
        "int","float","bool","string","void",
        "if","else","while","for","return",
        "true","false","null",
        "struct","enum","import","export",
        "break","continue","throw","try","catch",
        "switch","case","default","as"
    };
    for (const auto& kw : keywords) {
        if (!prefix.empty() && kw.rfind(prefix, 0) != 0) continue;
        items.push_back({{"label", kw}, {"kind", 14}});
    }

    return JsonRpc::makeResponse(id, items);
}
