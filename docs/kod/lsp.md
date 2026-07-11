# Dil Sunucusu — LSP (`src/lsp/`)

## Sorumluluk

Language Server Protocol (LSP) uygulaması. stdio üzerinden JSON-RPC mesajları
alır, kaynak kodunu DocumentStore'da yönetir ve tamamlama, tanıma gitme,
hover, referanslar, documentSymbol, highlight, diagnostic hizmetleri sunar.
Faz 3'te konum birimi anlaşması (UTF-8/UTF-16), token binary search, cross-file
definition ve gruplanmış publishDiagnostics eklendi.

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `lsp_handler.hpp` / `.cpp` | `LspHandler` — LSP isteklerini dispatch eder, tüm handler'ları içerir. |
| `lsp_server.hpp` / `.cpp` | `LspServer` — stdio üzerinden JSON-RPC mesajlarını okur/yazar. |
| `document_store.hpp` / `.cpp` | `DocumentStore` — açık belgelerin buffer'larını yönetir, `runPipeline` ile overlay olarak derler. |
| `json_rpc.hpp` | JSON-RPC tel协议 yardımcıları (Content-Length header). |
| `lsp_types.hpp` | LSP tip tanımları (Location, CompletionItem, vs.). |
| `position.hpp` | UTF-16 ↔ byte dönüştürücüleri (Faz 3 positionEncoding anlaşması). |
| `uri.hpp` | `uriToPath` / `pathToUri` dönüşümleri. |

## Ana tipler

```
LspHandler
  ├─ out_                 : ostream&
  ├─ store_               : DocumentStore
  ├─ shutdownRequested_   : bool
  ├─ positionEncoding_    : string ("utf-16" | "utf-8")
  ├─ dispatch(msg) → json
  ├─ handleInitialize/Hover/Definition/References/DocumentSymbol/...
  ├─ findSymbolAt(state, line, character) → Symbol*
  ├─ toByteColumn / toLspPos — pozisyon dönüşümü
  ├─ publishDiagnosticsGrouped() — dosyaya göre gruplanmış tanı
  └─ contentForLoc(state, loc) → string

DocumentStore
  ├─ open(path, content) — belge açar
  ├─ change(path, newContent) — belge günceller
  ├─ close(path) — belge kapatır
  ├─ runPipeline() — tüm açık belgeleri overlay ile derler
  └─ contentForPath(path) → string — disk veya buffer

DocumentState
  ├─ ast / tokens / symbolTable / diagnostics
  ├─ tokens       : sorted vector (Faz 3: binary search)
  └─ symbolByOffset : unordered_map (Faz 3: offset→Symbol* indeksi)
```

## Desteklenen Özellikler (LSP)

- `textDocument/completion` — token tabanlı bağlam, struct zincir çözümü, scope filtrelemesi
- `textDocument/definition` — sembol tanımına git
- `textDocument/hover` — sembol tip bilgisi
- `textDocument/references` — tüm referanslar
- `textDocument/documentSymbol` — hiyerarşik sembol listesi
- `textDocument/documentHighlight` — aynı sembolün vurgulanması
- `textDocument/publishDiagnostics` — gruplanmış hata/uyarı bildirimi

## Tasarım kararları

- **SourceOverlay seam**: DocumentStore::runPipeline, ModuleLoader'a overlay
  sağlayarak disk yerine editör buffer'ını derler (kök neden #1).
- **Token binary search**: findSymbolAt() artık sembolleri lineer taramak
  yerine token binary search + offset→Symbol* indeksi kullanır (kök neden #3).
- **Gruplanmış diagnostics**: publishDiagnostics her dosya için ayrı gönderilir
  (kök neden #4). İmport edilen modülün hatası artık ana dosyada görünmez.
- **Position encoding anlaşması**: initialize'da istemci general.positionEncodings'e
  göre UTF-8 veya UTF-16 seçilir (kök neden #3/Faz 3).
- **Cross-file definition**: definition/references tanımın bulunduğu dosyanın
  URI'sini döndürür (+ `uriForPath` ile).
