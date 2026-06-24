# saQut LSP — Tasarım ve Uygulama Rehberi

> Bu belge, saQut derleyicisine Language Server Protocol (LSP) desteği eklemek
> için gereken mimariyi, karar gerekçelerini ve uygulama sırasını belgeler.
> Kodlamaya başlamadan önce okunması amaçlanmıştır.

---

## 1. LSP Nedir ve Ne Yapar?

**Language Server Protocol**, editör ile dil araçları arasında JSON-RPC üzerinden
konuşan bir standarttır (Microsoft, 2016). Editör yazan kişi her dil için ayrı
eklenti yazmak zorunda kalmaz; dili bilen taraf (saQut) sunucu (`saqut-lsp`)
olarak çalışır, editör onunla stdin/stdout üzerinden konuşur.

```
VS Code / Neovim / Zed / Emacs
        │  JSON-RPC (stdin / stdout)
        ▼
  saqut-lsp (C++ process)
        │
        ▼
  Mevcut derleyici pipeline'ı
  (Lexer → Parser → SymbolCollector → TypeChecker)
```

Editör eklentisi **dil-agnostik** bir istemci (client) görevi görür; asıl iş
`saqut-lsp` sürecindedir. Bu ayrım son derece önemlidir: eklentiyi küçük tutar
ve dil mantığını C++'ta merkezi bir yerde tutar.

---

## 2. IDE Eklentisi Gerekli mi?

**Kısa cevap:** VS Code için minimum bir eklenti şarttır — ama içi neredeyse boştur.

### Neden küçük kalabilir?

VS Code, `vscode-languageclient` paketini hazır sunar. Eklenti yalnızca şunu yapar:

```typescript
// extension.ts — bundan ibaret
import { LanguageClient } from 'vscode-languageclient/node';

const client = new LanguageClient('saQut', {
    command: 'saqut-lsp',   // PATH'teki binary
    args: []
}, {
    documentSelector: [{ scheme: 'file', language: 'sqt' }]
});
client.start();
```

Dil mantığı, hata mesajları, tamamlama listeleri, hover açıklamaları — bunların
**hiçbiri** eklentide değil `saqut-lsp`'dedir. Eklenti sadece köprüdür.

### Neovim / Zed / Emacs

Bu editörler konfigürasyonla doğrudan LSP sunucusuna bağlanabilir; ayrı eklenti
**gerekmez**.

```lua
-- Neovim (lazy.nvim + nvim-lspconfig)
require('lspconfig').saqut_lsp.setup({
    cmd = { 'saqut-lsp' },
    filetypes = { 'sqt' },
})
```

**Karar:** VS Code için minimal bir eklenti yazılır (`package.json` + `extension.ts`,
~50 satır). Diğer editörler için yalnızca bir konfigürasyon örneği dokümante edilir.

---

## 3. LSP Özellikleri — Öncelik Sırası

Önce dar dikey dilim, sonra çerçeve ilkesi burada da geçerlidir.

### Tier 0 — Temeller (olmadan anlamsız)

| Özellik | LSP Metodu | Açıklama |
|---------|-----------|----------|
| Dosya açma/kapatma | `textDocument/didOpen`, `didClose` | Belge yaşam döngüsü |
| Değişiklik bildirimi | `textDocument/didChange` | Artımlı veya tam güncelleme |
| Tanı yayımı | `textDocument/publishDiagnostics` | Kırmızı dalgalı çizgi |

### Tier 1 — Temel Akıllı Özellikler

| Özellik | LSP Metodu | Derleyici Altyapısı |
|---------|-----------|---------------------|
| Tanıma git | `textDocument/definition` | `Symbol.definitionLoc` (hazır) |
| Sembol vurgulama | `textDocument/documentHighlight` | `Symbol.references` (hazır) |
| Hover bilgisi | `textDocument/hover` | `Symbol.type` + `kind` (hazır) |
| Tüm referanslar | `textDocument/references` | `Symbol.references` (hazır) |

Sembol tablosu **şimdiden** bu verileri tutuyor. Tier 1, büyük ölçüde
mevcut yapıyı JSON-RPC'ye çevirmektir.

### Tier 2 — Geliştirici Konforu

| Özellik | LSP Metodu | Not |
|---------|-----------|-----|
| Otomatik tamamlama | `textDocument/completion` | Scope'taki semboller + struct alanları |
| İmza yardımı | `textDocument/signatureHelp` | Fonksiyon parametre listesi |
| Sembol yeniden adlandırma | `textDocument/rename` | Tüm referanslara uygula |
| Belge sembolleri | `textDocument/documentSymbol` | Dosya yapısı / outline |

### Tier 3 — İleri Düzey

| Özellik | LSP Metodu | Not |
|---------|-----------|-----|
| Workspace sembolleri | `workspace/symbol` | Çok-dosya arama |
| Code action | `textDocument/codeAction` | Hızlı düzeltmeler |
| Inlay hints | `textDocument/inlayHint` | Tip göstergeleri |
| Semantic tokens | `textDocument/semanticTokens` | Zengin renklendirme (bkz. §6) |

---

## 4. C++ Mimarisi — Ne Eklenir, Ne Değişir

### 4.1 Tek binary, ayrı mod: `saqut lsp`

LSP modu mevcut `saqut` binary'sine **ek bir komut olarak** eklenir.
`run`, `check`, `ir` gibi kayıtlı komutlardan biri olur:

```
saqut run   source.sqt   → tek seferlik derleme + çalıştırma
saqut lsp                → LSP sunucu moduna gir (stdin/stdout JSON-RPC döngüsü)
saqut dap                → DAP hata ayıklama moduna gir (ayrı belge)
```

**Neden tek binary?**
- Dağıtım basit: tek dosya indir, her şey çalışır.
- `src/lsp/` ve `src/dap/` ayrı dizinlerde olur; `main.cpp`'de sadece
  `cmdLsp` ve `cmdDap` fonksiyonları kayıt edilir — tıpkı `cmdRun` gibi.
- CMake tek `add_executable(saqut ...)` hedefi korur.

`saqut lsp` çalıştırıldığında editörden `initialize` isteği gelene kadar
bekler, sonra standart LSP yaşam döngüsüne girer.

### 4.2 Eklenmesi gereken bileşenler

```
src/lsp/
  lsp_server.hpp / .cpp     — JSON-RPC döngüsü (stdin/stdout okuma/yazma)
  lsp_handler.hpp / .cpp    — LSP metodlarını dispatche eder
  document_store.hpp / .cpp — Açık belgeleri ve son pipeline sonuçlarını tutar
  lsp_types.hpp             — LSP Position, Range, Location, Diagnostic yapıları
  json_rpc.hpp / .cpp       — JSON-RPC 2.0 parse/serialize (nlohmann/json üstünde)
```

### 4.3 Mevcut bileşenlerde değişiklik

| Bileşen | Değişiklik |
|---------|-----------|
| `SymbolTable` | Değişmez — `Symbol.definitionLoc` ve `.references` zaten var |
| `DiagnosticEngine` | `toLspDiagnostics()` metodu eklenir (LSP formatına çevirir) |
| `SourceLocation` | `toLspPosition()` yardımcı metodu (line/col → LSP 0-tabanlı) |
| `ModuleLoader` | Değişmez — kaynak string parametresiyle çalışacak overload eklenir |
| `Parser` | Değişmez |
| `TypeChecker` | Değişmez |

**Kritik not:** LSP, satır/sütun numaralarını **0-tabanlı** bekler.
`SourceLocation` şu an **1-tabanlı**. `toLspPosition()` sadece `line-1, col-1`
döndüren ince bir adapter olacak; orijinal yapı korunacak.

### 4.4 DocumentStore — Merkezi Durum

LSP sunucusunun kalbidir. Her açık dosya için şunu tutar:

```cpp
struct DocumentState {
    std::string           uri;
    std::string           content;       // en son gönderilen kaynak
    int                   version;       // LSP belge versiyonu
    ASTNode*              ast;           // son başarılı parse
    SymbolTable           symbolTable;
    DiagnosticEngine      diagnostics;
    std::vector<Token*>   tokens;        // semantic token için
};
```

Değişiklik geldiğinde (`didChange`): pipeline yeniden çalıştırılır, yeni
`DocumentState` üretilir, `publishDiagnostics` gönderilir.

---

## 5. Performans ve Bellek — Büyük Projelerde

### 5.1 Asıl sorun: Her tuş vuruşunda tam derleme

Naif yaklaşım: kullanıcı her karakter yazdığında tüm pipeline'ı çalıştır.
10.000 satırlık projede bu kabul edilemez gecikmeler üretir.

### 5.2 Çözüm: Katmanlı geciktirme + artımlı yeniden derleme

**Debounce (300-500 ms):** Kullanıcı yazmayı bıraktıktan 300 ms sonra pipeline
çalışır. Her tuş vuruşunda değil.

**Dosya bazlı önbellek:** `ModuleLoader` daha önce parse edilmiş ve değişmemiş
dosyaları yeniden parse etmez. İçerik hash'i (SHA-1 veya std::hash) ile
"kirli" bayrak tutulur.

```cpp
struct CachedModule {
    std::string        contentHash;
    ASTNode*           ast;
    SymbolTable        symbolTable;
    bool               isDirty = false;
};
```

Değişen dosya → sadece o dosya ve ona bağlı dosyalar yeniden derlenir.

**Bellek tavanı:** 500'den fazla açık dosya varsa LRU politikasıyla en eski
`DocumentState` silinir. saQut projeleri bu boyuta ulaşmadan çok önce modül
sistemi devreye girecek; modül başına bir dosya hedef senaryodur.

### 5.3 Thread modeli

LSP sunucusu tek thread'de çalışabilir (basitlik). Büyük projelerde:

```
Ana thread:  JSON-RPC okuma/yazma
Worker pool: Pipeline yeniden çalıştırma (std::async veya thread pool)
```

İlk versiyonda tek thread yeterlidir. Gecikme hissedilirse thread pool eklenir.

### 5.4 Bellek tüketimi tahmini

| Bileşen | Tahmini Bellek |
|---------|---------------|
| AST (1000 satır dosya) | ~2-5 MB |
| SymbolTable (aynı dosya) | ~500 KB |
| Token listesi | ~200 KB |
| 10 açık dosya toplamı | ~30-50 MB |

Bu sayılar modern geliştirici makinesi için önemsizdir. Sorun ancak
**binlerce dosyalı** projede belirir; şimdilik prematüre optimizasyon gerekmez.

---

## 6. Sözdizimi Renklendirme (Syntax Highlighting)

### 6.1 İki yaklaşım

**TextMate grameri (`.tmLanguage`)** — Regex tabanlı, statik. IDE'nin kendi
motoru işler, LSP sunucusu devrede değil. Kurulumu kolay, sınırlı.

**Semantic tokens (LSP Tier 3)** — Derleyici token tiplerine tip/modifier
bilgisi ekler; editör bunu alıp renklendirir. TypeChecker çalıştıktan sonra
üretilir — struct adını değişken adından ayırt edebilir.

**Öneri:** Önce TextMate grameri yaz (hızlı kazanım), sonra semantic tokens
ekle (doğru renklendirme).

### 6.2 TextMate grameri için gereken scope'lar

```json
{
  "scopeName": "source.sqt",
  "patterns": [
    { "name": "keyword.control.sqt",
      "match": "\\b(if|else|for|while|do|return|break|continue|switch|case|default)\\b" },
    { "name": "keyword.other.sqt",
      "match": "\\b(import|export|struct|enum|as)\\b" },
    { "name": "storage.type.sqt",
      "match": "\\b(int|float|double|bool|string|void|decimal)\\b" },
    { "name": "constant.language.sqt",
      "match": "\\b(true|false|null)\\b" },
    { "name": "string.quoted.double.sqt",
      "begin": "\"", "end": "\"" },
    { "name": "comment.line.double-slash.sqt",
      "match": "//.*$" }
  ]
}
```

### 6.3 Semantic tokens C++ entegrasyonu

`textDocument/semanticTokens/full` isteği geldiğinde:
1. `DocumentStore`'dan son başarılı `tokens` listesi alınır.
2. Her token için LSP `SemanticTokenType` belirlenir:

```cpp
// Token → LSP semantic token tipi
SemanticTokenType toSemanticType(const Symbol* sym) {
    if (!sym) return SemanticTokenType::Variable;
    switch (sym->kind) {
        case SymbolKind::Function:  return SemanticTokenType::Function;
        case SymbolKind::Struct:    return SemanticTokenType::Struct;
        case SymbolKind::Enum:      return SemanticTokenType::Enum;
        case SymbolKind::Variable:  return SemanticTokenType::Variable;
        case SymbolKind::Parameter: return SemanticTokenType::Parameter;
        default:                    return SemanticTokenType::Variable;
    }
}
```

3. Sıkıştırılmış delta-encoded format üretilir (LSP spec §3.16.6).

---

## 7. Hata Ayıklama (Debugging) Desteği

LSP **debugging'i kapsamaz** — bu DAP (Debug Adapter Protocol) işidir.
Ancak LSP ile yapılabilecek ve debugging'e yardımcı olan şeyler vardır:

### LSP kapsamında

- **Hover:** Değişkenin tipi, değeri (static analysis sınırları içinde).
- **Inlay hints:** Satır sonlarında tip göstergeleri — `x /*: int*/`.
- **Breakpoint için:** Kaynak satırını IR satırına eşleyen `lineTable`
  (IR'da zaten mevcut) DAP entegrasyonunu kolaylaştırır.

### DAP (Debug Adapter Protocol) — ayrı konu

DAP için `saqut-dap` ayrı bir binary gerektirir. VM'e adım adım yürütme,
breakpoint, değişken inceleme desteği eklenmesi gerekir. Bu saQut'un şu an
`HENÜZ YOK` listesindedir ve LSP'den bağımsız bir çalışmadır.

**Önkoşullar (DAP için):**
- IR `lineTable` tamamlanmış olmalı (kısmen mevcut)
- VM'e `step`, `stepOver`, `continue`, `breakAtLine` API'si eklenmeli
- `SymbolTable` runtime'da okunabilir olmalı (değişken değerleri için)

---

## 8. Uygulama Sırası (Önerilen)

```
Faz 1 — Temel altyapı
  ├── src/lsp/lsp_types.hpp          (Position, Range, Diagnostic yapıları)
  ├── src/lsp/json_rpc.hpp/.cpp      (stdin/stdout JSON-RPC döngüsü)
  ├── src/lsp/document_store.hpp/.cpp
  ├── src/lsp/lsp_server.hpp/.cpp    (initialize, shutdown, exit)
  └── DiagnosticEngine::toLspDiagnostics()
      → Sonuç: editörde kırmızı dalgalı çizgi çalışır

Faz 2 — Tier 1 özellikler
  ├── textDocument/definition        (Symbol.definitionLoc)
  ├── textDocument/references        (Symbol.references)
  ├── textDocument/hover             (tip + kind bilgisi)
  └── textDocument/documentHighlight
      → Sonuç: "tanıma git" ve "hover" çalışır

Faz 3 — VS Code eklentisi
  ├── package.json (language contribution, file extension .sqt)
  ├── syntaxes/sqt.tmLanguage.json   (TextMate grameri)
  └── extension.ts (~50 satır, LanguageClient başlatır)
      → Sonuç: VS Code'da renklendirme + temel akıllı özellikler

Faz 4 — Tier 2 özellikler
  ├── textDocument/completion
  ├── textDocument/signatureHelp
  └── textDocument/rename
```

---

## 9. Açık Kararlar

| Soru | Seçenekler | Önerilen |
|------|-----------|----------|
| LSP binary adı | `saqut-lsp` vs `saqut --lsp` | Ayrı binary |
| TextMate grameri nerede? | Eklenti repo'su vs ana repo | Ana repoda `editor/vscode/` |
| Artımlı parse granülaritesi | Fonksiyon bazlı vs dosya bazlı | Dosya bazlı (önce basit) |
| Thread modeli | Tek thread vs worker pool | Önce tek thread |
| JSON kütüphanesi | Mevcut nlohmann/json | Mevcut nlohmann/json (değiştirme) |

---

## 10. Bağlantılı Belgeler ve Issue'lar

- `docs/adr-frontend-analiz.md` — Derleyici pipeline mimarisi kararları
- `docs/lsp/` — Bu klasör; uygulama ilerledikçe alt belgeler eklenecek
- GitHub #91 — LSP Tier 0-4 katmanlı yetenek haritası
- `src/core/location.hpp` — `SourceLocation` (1-tabanlı → 0-tabanlıya dönüşüm notu)
- `src/symbol/symbol.hpp` — `Symbol.definitionLoc`, `Symbol.references`
- `src/diagnostic/diagnostic_engine.hpp` — LSP diagnostic dönüşümü için hedef

---

*Son güncelleme: 2026-06-24*
