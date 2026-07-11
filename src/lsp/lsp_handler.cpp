#include "lsp/lsp_handler.hpp"
#include "symbol/symbol_table.hpp"
#include "symbol/symbol.hpp"
#include "core/type.hpp"
#include "lsp/uri.hpp"
#include "lsp/position.hpp"
#include "builtin/builtin_methods.hpp"
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
// Completion (Faz 4 — token/sembol tabanlı)
// ─────────────────────────────────────────────────────────────────────────────
//
// Faz 4'te string-hack yardımcıları (wordPrefix/lineUpToCursor/wordBefore)
// kaldırıldı. Yerine:
//   - İmleç öncesi bağlam token dizisinden çıkarılıyor (`.`, `::`, zincir)
//   - Zincir çözümü: findSymbolAt + structLayouts yürüyüşüyle a.b.c.
//   - Scope filtrelemesi: yalnızca imlecin bulunduğu fonksiyonun lokalleri
//     + globaller önerilir; başka fonksiyonun lokali ASLA önerilmez
//   - Builtin metodlar BuiltinMethodRegistry'den üretilir
//     (src/builtin/builtin_methods.hpp — tek doğruluk kaynağı)

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

// ── AST yardımcıları (scope filtrelemesi için) ──────────────────────────────

// Bir AST alt ağacındaki en büyük offset'i döndürür (düğümün bittiği yaklaşık konum).
static int astMaxOffset(ASTNode* node) {
    int max = node->loc.offset;
    for (auto* child : node->getChildren()) {
        int cm = astMaxOffset(child);
        if (cm > max) max = cm;
    }
    return max;
}

// Verilen offset'i kapsayan FunctionDecl AST düğümünü bulur.
// Bulamazsa nullptr (imleç global scope'ta).
static ASTNode* findEnclosingFunction(ASTNode* node, int offset) {
    if (!node) return nullptr;
    for (auto* child : node->getChildren()) {
        if (child->kind == ASTKind::FunctionDecl && child->loc.isValid()) {
            int end = astMaxOffset(child);
            if (offset >= child->loc.offset && offset <= end) return child;
        }
        ASTNode* found = findEnclosingFunction(child, offset);
        if (found) return found;
    }
    return nullptr;
}

// ── Scope filtrelemesi ──────────────────────────────────────────────────────

// İmlecin bulunduğu kapsamdan görünür sembolleri döndürür.
// Kural:
//   - Global semboller (scope->parent == nullptr) her zaman görünür
//   - Tanım konumu geçersiz (builtin) her zaman görünür
//   - Import edilmiş semboller her zaman görünür
//   - Lokal semboller yalnızca imleçle aynı fonksiyonun içindeyse görünür
//   - Field'lar hariç (onlar sadece . zinciriyle gelir)
static std::vector<Symbol*> visibleSymbols(DocumentState& state, int byteOffset) {
    ASTNode* func = state.ast ? findEnclosingFunction(state.ast, byteOffset) : nullptr;
    int funcStart = func ? func->loc.offset : 0;
    int funcEnd   = func ? astMaxOffset(func) : 0;

    std::vector<Symbol*> result;
    for (Symbol* sym : state.symbolTable.allSymbols()) {
        if (sym->kind == SymbolKind::Field) continue;

        // Global scope'ta tanımlanmış semboller her zaman görünür
        if (sym->scope && sym->scope->parent == nullptr) {
            result.push_back(sym);
            continue;
        }

        // Tanım konumu yok (builtin/yerleşik) — her zaman görünür
        if (!sym->definitionLoc.isValid()) {
            result.push_back(sym);
            continue;
        }

        // Başka dosyadan import edilmiş — her zaman görünür
        if (sym->definitionLoc.filePath != state.filePath) {
            result.push_back(sym);
            continue;
        }

        // Lokal sembol: yalnızca imleçle aynı fonksiyon içindeyse
        if (func && sym->definitionLoc.offset >= funcStart &&
                     sym->definitionLoc.offset <= funcEnd) {
            result.push_back(sym);
            continue;
        }

        // İmleç global scope'ta (hiçbir fonksiyonun içinde değil) — tüm lokaller görünür
        if (!func) {
            result.push_back(sym);
        }
    }
    return result;
}

// ── Zincir çözümü (a.b.c. için) ─────────────────────────────────────────────

// Bir tanımlayıcı zincirini (`chain`) çözerek sonundaki Type'ı döndürür.
// Örn: ["p", "adres", "sehir"] → p'nin tipinden başla, adres alanının tipine
// geç, sehir alanının tipini döndür. Herhangi bir adım başarısız olursa
// Type::error() döner.
static Type resolveChainType(DocumentState& state, const std::vector<std::string>& chain) {
    if (chain.empty()) return Type::error();

    // İlk tanımlayıcıyı sembol tablosunda bul
    Symbol* sym = nullptr;
    for (Symbol* s : state.symbolTable.allSymbols()) {
        if (s->name == chain[0] && s->kind != SymbolKind::Field) {
            sym = s;
            break;
        }
    }
    if (!sym) return Type::error();

    Type current = sym->type;

    for (size_t i = 1; i < chain.size(); ++i) {
        // Nullable / array wrapper'ları soy
        Type t = current;
        while (t.isArray() && t.elementType) t = *t.elementType;
        if (!t.isStruct()) return Type::error();

        auto it = state.symbolTable.structLayouts.find(t.structName);
        if (it == state.symbolTable.structLayouts.end()) return Type::error();

        bool found = false;
        for (auto& [fn, ft] : it->second) {
            if (fn == chain[i]) { current = ft; found = true; break; }
        }
        if (!found) return Type::error();
    }
    return current;
}

// ── Token-tabanlı bağlam çıkarma ────────────────────────────────────────────

struct CompletionCtx {
    enum Kind { Normal, Dot, Scope };
    Kind kind = Normal;
    std::vector<std::string> chain;  // Dot: zincirdeki tanımlayıcılar (sıralı)
    std::string target;              // Scope: :: solundaki ifade
    std::string prefix;              // Normal: kısmî tanımlayıcı metni
};

// İmleç öncesi token dizisinden completion bağlamını çıkarır.
// byteOffset: imlecin 0-tabanlı byte offset'i (positionEncoding dönüşümü sonrası).
static CompletionCtx analyzeContext(DocumentState& state, int byteOffset) {
    CompletionCtx ctx;
    const auto& toks = state.tokens;
    if (toks.empty()) return ctx;

    // İmleçten ÖNCE biten token'ları topla (binary search ile imleç konumunu bul)
    auto it = std::upper_bound(toks.begin(), toks.end(), byteOffset,
        [](int off, Token* t) { return off < t->start; });

    // İmleçten önce biten token'ları geriye doğru topla (en yakın en başta)
    std::vector<Token*> left;
    auto rit = it;
    while (rit != toks.begin()) {
        --rit;
        Token* tok = *rit;
        if (tok->end > byteOffset) continue; // imlecin ÖTESİNE taşan token'ı atla
        left.push_back(tok);
        if (left.size() >= 20) break;
    }

    if (left.empty()) return ctx;
    Token* nearest = left[0];

    // ── "." zinciri: a.b.c.| ──────────────────────────────────────────────
    if (nearest->token == ".") {
        ctx.kind = CompletionCtx::Dot;
        size_t i = 1;
        while (i < left.size()) {
            if (left[i]->gettype() == "identifier") {
                ctx.chain.push_back(left[i]->token);
                ++i;
                if (i < left.size() && left[i]->token == ".") {
                    ++i;
                } else {
                    break;
                }
            } else {
                break;
            }
        }
        std::reverse(ctx.chain.begin(), ctx.chain.end());
        return ctx;
    }

    // ── "::" scope çağrısı: expr::| ───────────────────────────────────────
    if (nearest->token == "::") {
        ctx.kind = CompletionCtx::Scope;
        if (left.size() > 1 && left[1]->gettype() == "identifier") {
            ctx.target = left[1]->token;
        }
        return ctx;
    }

    // ── Normal prefix: identif| veya foo| ─────────────────────────────────
    if (nearest->gettype() == "identifier") {
        if (byteOffset >= nearest->start && byteOffset <= nearest->end) {
            ctx.prefix = nearest->token.substr(0, byteOffset - nearest->start);
        } else if (nearest->end <= byteOffset) {
            // İmleç tanımlayıcının hemen sonrasında (boşluk olabilir)
            ctx.prefix = nearest->token;
        }
        return ctx;
    }

    return ctx;
}

// ── Builtin metod listesi (BuiltinMethodRegistry'den) ───────────────────────

// BuiltinMethodRegistry'deki bir metodu LSP CompletionItem'a dönüştürür.
static nlohmann::json builtinMethodItem(const BuiltinMethod* m) {
    // insertText: methodAdı(arg1, arg2) — receiver (params[0]) hariç
    std::string insertText = m->name + "(";
    for (size_t i = 1; i < m->params.size(); ++i) {
        if (i > 1) insertText += ", ";
        insertText += "${" + std::to_string(i) + "}";
    }
    insertText += ")";

    // detail: (argTipleri) → dönüşTipi
    std::string detail = "(";
    for (size_t i = 1; i < m->params.size(); ++i) {
        if (i > 1) detail += ", ";
        switch (m->params[i].kind) {
            case ParamKind::Fixed:     detail += m->params[i].fixedType.toString(); break;
            case ParamKind::ElemType:  detail += "T";   break;
            case ParamKind::ElemArray: detail += "T[]"; break;
            case ParamKind::StringVal: detail += "string"; break;
        }
    }
    detail += ") → ";
    switch (m->ret.kind) {
        case ReturnKind::Fixed:     detail += m->ret.fixedType.toString(); break;
        case ReturnKind::ElemType:  detail += "T";   break;
        case ReturnKind::ElemArray: detail += "T[]"; break;
    }

    return {
        {"label",            m->name},
        {"kind",             2},  // Method
        {"detail",           detail},
        {"insertText",       insertText},
        {"insertTextFormat", 2},  // Snippet
    };
}

// Verilen tip için BuiltinMethodRegistry'deki uygun metodları döndürür.
// Kategori filtrelemesi: Array metodları yalnızca array receiver için,
// StringVal metodları yalnızca string için, StructVal metodları yalnızca struct için.
static nlohmann::json builtinMethodsForType(const Type& receiverType, const std::string& typeName) {
    nlohmann::json items = nlohmann::json::array();
    const auto& reg = BuiltinMethodRegistry::instance();

    bool isReceiverArray = receiverType.isArray();
    bool isString        = receiverType.isString();
    bool isStruct        = receiverType.isStruct();

    // Struct array elemanı için de struct kabul et
    if (isReceiverArray && receiverType.elementType && receiverType.elementType->isStruct())
        isStruct = true;
    // Tip adı büyük harfle başlıyorsa struct kabul et
    if (!typeName.empty() && std::isupper(static_cast<unsigned char>(typeName[0])))
        isStruct = true;

    // Belirli bir tip grubuna girmeyen skaler tipler için (int, float, bool vb.)
    // hiçbir builtin metod göstermiyoruz — registry'de bunlara ait metod yok.
    if (!isReceiverArray && !isString && !isStruct)
        return items;

    // Uygun kategorilere göre filtrele
    for (int i = 0; i < reg.count(); ++i) {
        const BuiltinMethod* m = reg.byId(i);
        if (!m) continue;

        bool include = false;
        switch (m->category) {
            case MethodCategory::Array:
                include = isReceiverArray;
                break;
            case MethodCategory::StringVal:
                include = isString;
                break;
            case MethodCategory::StructVal:
                include = isStruct;
                break;
        }
        if (include) items.push_back(builtinMethodItem(m));
    }
    return items;
}

// ── handleCompletion (Faz 4 — yeniden yazıldı) ──────────────────────────────

nlohmann::json LspHandler::handleCompletion(const nlohmann::json& id,
                                             const nlohmann::json& params) {
    std::string uri  = params["textDocument"]["uri"].get<std::string>();
    int         line = params["position"]["line"].get<int>();
    int         ch   = params["position"]["character"].get<int>();

    DocumentState* state = store_.get(uri);
    if (!state) return JsonRpc::makeResponse(id, nlohmann::json::array());

    // İmleci byte offset'e çevir (positionEncoding dönüşümü)
    int byteCol  = toByteColumn(state->content, line, ch);
    int byteOff  = lspLineStartOffset(state->content, line) + (byteCol - 1);
    nlohmann::json items = nlohmann::json::array();

    // Token dizisinden bağlam çıkar
    CompletionCtx ctx = analyzeContext(*state, byteOff);

    // ── "." zinciri: a.b.c.| → alan tamamlama ──────────────────────────────
    if (ctx.kind == CompletionCtx::Dot) {
        Type targetType;
        if (ctx.chain.empty()) {
            // Sadece ".|" — öncesinde tanımlayıcı yok, yapabileceğimiz bir şey yok
            return JsonRpc::makeResponse(id, items);
        }

        // Zinciri çöz
        targetType = resolveChainType(*state, ctx.chain);
        if (targetType.isError()) return JsonRpc::makeResponse(id, items);

        // Nullable / array wrapper'ları soy
        while (targetType.isArray() && targetType.elementType)
            targetType = *targetType.elementType;
        if (!targetType.isStruct()) return JsonRpc::makeResponse(id, items);

        // Struct alanlarını listele
        auto it = state->symbolTable.structLayouts.find(targetType.structName);
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

    // ── "::" scope: expr::| → builtin metod tamamlama ─────────────────────
    if (ctx.kind == CompletionCtx::Scope) {
        if (ctx.target.empty()) return JsonRpc::makeResponse(id, items);

        // Sembolü bul
        Symbol* objSym = nullptr;
        for (Symbol* s : state->symbolTable.allSymbols()) {
            if (s->name == ctx.target) { objSym = s; break; }
        }

        // Tip adına (struct/enum) :: koymak anlamsız — boş dön
        if (objSym && (objSym->kind == SymbolKind::Struct ||
                       objSym->kind == SymbolKind::Enum))
            return JsonRpc::makeResponse(id, items);

        if (objSym) {
            items = builtinMethodsForType(objSym->type, objSym->name);
        } else {
            // Tanımlayıcı çözülemedi — tip adı olabilir ("int", "string", ...)
            Type t = Type::fromName(ctx.target);
            if (!t.isError()) {
                items = builtinMethodsForType(t, ctx.target);
            }
        }
        return JsonRpc::makeResponse(id, items);
    }

    // ── Normal prefix tamamlama (scope filtreli) ──────────────────────────
    std::string prefix = ctx.prefix;

    // Scope-filtreli semboller
    for (Symbol* sym : visibleSymbols(*state, byteOff)) {
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

    // Anahtar kelimeler
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
