# saQut Araç Zinciri — Büyük Resim

> Bu belge; renklendirme, LSP ve debugging'in birlikte nasıl çalıştığını
> ve saQut'ta her birinin C++ tarafında neye dokunduğunu tek sayfada gösterir.
> Detay belgeler:
> - `docs/lsp/lsp-tasarim.md` — LSP mimarisi
> - `docs/dap/dap-tasarim.md` — Debugging mimarisi

---

## 1. Tek Binary, Üç Mod

```
saqut run   source.sqt    → derleme + çalıştırma  (şu an çalışıyor)
saqut lsp                 → LSP sunucu modu
saqut dap                 → DAP hata ayıklama modu
```

Her mod `main.cpp`'de kayıtlı bir komuttur. Ayrı binary yok.
`src/lsp/` ve `src/dap/` dizinleri bağımsız; kendi `cmdLsp`/`cmdDap`
fonksiyonlarını sunar, `main.cpp` bunları register eder.

---

## 2. Renklendirme — İki Katman

```
.sqt dosyası
    │
    ├── Katman 1: TextMate Grameri  (.tmLanguage.json)
    │       Editörün kendi motoru işler — saqut binary DEVREDEĞİL
    │       Regex tabanlı: anahtar kelimeler, string, yorum → renk
    │       Anında çalışır, çevrimdışı çalışır
    │       Kaynak: editor/vscode/syntaxes/sqt.tmLanguage.json
    │
    └── Katman 2: Semantic Tokens  (LSP üzerinden)
            saqut lsp pipeline'ı çalışır:
            Lexer → Parser → SymbolCollector → TypeChecker
            Her identifier → SymbolTable'dan kind bilgisi alınır
            (Function / Struct / Variable / Parameter / Enum)
            Delta-encode edilmiş token listesi editöre gönderilir
            Editör tema rengi uygular
            Sonuç: struct adı değişkenden ayrışır, parametre farklı renk alır
```

### TextMate vs Semantic Token — Pratik Fark

```sqt
Point p;        // TextMate: "Point" → identifier (gri)
                // Semantic: "Point" → struct (editörün struct rengi)

p.x = add(p);  // TextMate: "add" → identifier (gri)
                // Semantic: "add" → function (editörün function rengi)
```

**Uygulama sırası:** Önce TextMate grameri (birkaç saatlik iş, hemen kazanım),
sonra semantic tokens (TypeChecker pipeline'ı hazır olduğunda).

### C++ Tarafında Ne Değişir?

TextMate grameri için: **hiçbir şey** — tamamen editör tarafı, JSON dosyası.

Semantic tokens için yeni olan tek şey:

```cpp
// src/lsp/semantic_tokens.hpp
std::vector<uint32_t> buildSemanticTokens(
    const std::vector<Token*>& tokens,
    const SymbolTable& table);
// → LSP delta-encode formatında uint32 listesi döner
```

`SymbolTable` ve `Token` zaten mevcuttur. Bu fonksiyon onları birleştirir.

---

## 3. LSP — Akıllı Editör Özellikleri

```
Editör (VS Code / Neovim / ...)
    │  JSON-RPC  (stdin / stdout)
    ▼
saqut lsp  ←────────────────────────────────────────┐
    │                                                 │
    ├── DocumentStore                                 │
    │     Açık her dosya için tutar:                  │
    │     { kaynak, AST, SymbolTable, Diagnostics }   │
    │                                                 │
    ├── didChange → pipeline yeniden çalıştır         │
    │     Lexer → Parser → SymbolCollect → TypeCheck  │
    │     publishDiagnostics gönder                   │
    │                                                 │
    ├── definition    → Symbol.definitionLoc           │
    ├── references    → Symbol.references              │
    ├── hover         → Symbol.type + kind             │
    ├── completion    → scope'taki semboller           │
    └── semanticTokens → §2 Katman 2                  │
                                                       │
    Mevcut derleyici bileşenleri ────────────────────→─┘
    (Parser, SymbolCollector, TypeChecker — DEĞIŞMEZ)
```

**LSP'nin derleyiciye dokunduğu tek yer:**
`DiagnosticEngine`'e `toLspDiagnostics()` metodu eklenir.
`SourceLocation`'a `toLspPosition()` eklenir (1-tabanlı → 0-tabanlı).

---

## 4. Debugging (DAP) — Nasıl Çalışır?

LSP editörde akıllı özellik sağlar ama **programı çalıştırmaz**.
Debugging için Debug Adapter Protocol (DAP) gerekir — ayrı bir protokol.

```
Kullanıcı breakpoint koyar
    │
    ▼
Editör DAP istemcisi → saqut dap  (JSON üzerinden, genelde TCP/stdin)
    │
    ├── launch / attach isteği
    │     saqut dap → VM'i kontrollü başlatır
    │
    ├── setBreakpoints isteği
    │     saqut dap → breakpoint'leri VM'e kaydeder
    │     VM: her IR instruction'dan önce "bu satır breakpoint'te mi?" kontrol eder
    │
    ├── VM breakpoint'e çarpar → durur
    │     saqut dap → editöre "stopped" olayı gönderir
    │     Editör: kaynak satırı vurgular
    │
    ├── step / next / continue isteği
    │     saqut dap → VM'e "bir adım at" / "devam et" söyler
    │
    └── variables isteği
          saqut dap → VM'den mevcut frame'in slot'larını okur
          Slot adları: IR lineTable + SymbolTable üzerinden çözümlenir
          Editör: değişken panelini doldurur
```

### VM'e Eklenmesi Gerekenler

Şu an `Interpreter::run()` sonuna kadar koşar. DAP için:

```
Yeni VM durumları:
  Running → Paused (breakpoint/step)
  Paused  → Running (continue)
  Paused  → StepOver / StepIn / StepOut

VM'e eklenecek API:
  void  setBreakpoint(int lineNumber)
  void  clearBreakpoint(int lineNumber)
  void  stepOne()         // tek IR instruction
  void  stepOver()        // fonksiyon çağrısını geç
  Value readSlot(int n)   // değişken değerini oku
  int   currentLine()     // şu an hangi kaynak satırı
```

### IR lineTable — Kritik Önkoşul

Her IR instruction hangi kaynak satırından geldiğini bilmeli.
`IRFunction` büyük ölçüde bu bilgiyi zaten taşıyor; tam ve güvenilir
olması gerekiyor.

```
IR instruction index → kaynak satır/sütun
Bu eşleme IRGenerator'da üretilir, IRFunction.lineTable'da saklanır.
DAP bu tabloyu "editörde hangi satırı vurgulayayım?" için kullanır.
```

---

## 5. Üç Sistemin C++ Dizin Haritası

```
src/
  lsp/                    ← YENİ
    lsp_server.hpp/.cpp   — JSON-RPC döngüsü, initialize/shutdown
    lsp_handler.hpp/.cpp  — LSP metodlarını dispatche eder
    document_store.hpp/.cpp — Açık belge önbelleği
    lsp_types.hpp         — Position, Range, Location, Diagnostic
    semantic_tokens.hpp   — Token listesi → delta-encode

  dap/                    ← YENİ
    dap_server.hpp/.cpp   — DAP JSON-RPC döngüsü
    dap_handler.hpp/.cpp  — launch, breakpoints, step, variables
    dap_types.hpp         — DAP Breakpoint, StackFrame, Variable

  vm/
    interpreter.hpp/.cpp  — Mevcut; adım/breakpoint API eklenir
    call_frame.hpp        — Mevcut; değişmez

  ir/
    ir_function.hpp       — lineTable tamamlanır
    ir_generator.hpp/.cpp — Mevcut; değişmez

  diagnostic/
    diagnostic_engine.hpp — toLspDiagnostics() eklenir

  core/
    location.hpp          — toLspPosition() eklenir

  cli/commands/
    lsp.hpp               — cmdLsp fonksiyonu
    dap.hpp               — cmdDap fonksiyonu

editor/
  vscode/
    package.json          — Dil tanımı, .sqt ↔ saQut
    extension.ts          — LanguageClient başlatır (~50 satır)
    syntaxes/
      sqt.tmLanguage.json — TextMate grameri (Katman 1 renklendirme)
```

---

## 6. Bağımlılık ve Uygulama Sırası

```
Şu an hazır
  └── Lexer, Parser, SymbolCollector, TypeChecker, DiagnosticEngine
  └── Symbol.definitionLoc, Symbol.references (LSP Tier 1 için hazır)
  └── IR lineTable (kısmen — DAP için tamamlanmalı)

Adım 1 — TextMate grameri
  └── editor/vscode/syntaxes/sqt.tmLanguage.json
  └── Bağımlılık: hiçbir C++ değişikliği gerekmez
  └── Kazanım: editörde anında renklendirme

Adım 2 — LSP Tier 0: Tanılar
  └── src/lsp/ altyapısı + DiagnosticEngine adapter
  └── Kazanım: kırmızı dalgalı çizgi

Adım 3 — LSP Tier 1: Tanıma git / Hover
  └── definition, references, hover
  └── Kazanım: temel akıllı editör deneyimi

Adım 4 — VS Code eklentisi
  └── editor/vscode/ (TextMate + LanguageClient)
  └── Kazanım: VS Code'da tam deneyim

Adım 5 — IR lineTable tamamlama
  └── DAP önkoşulu — önce bu yapılmadan debugging olmaz

Adım 6 — DAP Tier 0: Breakpoint + Launch
  └── src/dap/ altyapısı + VM'e adım API'si
  └── Kazanım: editörde breakpoint ve durdurup devam

Adım 7 — DAP Tier 1: Değişken inceleme
  └── variables, stackTrace
  └── Kazanım: tam hata ayıklama deneyimi
```

---

## 7. Özet Tablo

| Sistem | Protokol | Mod | C++ değişimi | Önkoşul |
|--------|---------|-----|-------------|---------|
| Renklendirme (statik) | — (editör kendi) | — | Yok | Yok |
| Renklendirme (zengin) | LSP semantic tokens | `saqut lsp` | `semantic_tokens.hpp` | LSP Tier 0 |
| Akıllı özellikler | LSP JSON-RPC | `saqut lsp` | `src/lsp/` + 2 adapter | Mevcut pipeline |
| Hata ayıklama | DAP JSON-RPC | `saqut dap` | `src/dap/` + VM API | IR lineTable |

---

*Detay belgeler: `docs/lsp/lsp-tasarim.md` · `docs/dap/dap-tasarim.md`*
