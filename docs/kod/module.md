# Modül Sistemi (`src/module/`)

## Sorumluluk

`import` bildirimlerini izleyerek tüm bağımlı dosyaları yükler, parse eder
ve tek bir `ModuleGraph` yapısında toplar. BFS benzeri bir yaklaşımla
çalışır; döngüsel bağımlılıklar sessizce atlanır. LSP için `SourceOverlay`
seam'i ile editör buffer'ından okuma desteği sunar.

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `module_graph.hpp` | `ModuleUnit` (tek dosyanın parse edilmiş hali: ast+tokens) ve `ModuleGraph` (tüm birimlerin düz listesi). |
| `module_loader.hpp` | `ModuleLoader` — `load(entryPath)` ile bağımlılık zincirini çözer, dosyaları parse eder. |
| `module_loader.cpp` | Gerçekleme: `loadUnit()` tek dosyayı yükler/parse eder, `seen_` ile döngü koruması. |

## Ana tipler

```
ModuleUnit
  ├─ filePath : string — canonical mutlak yol
  ├─ moduleId : int — ModuleRegistry ID
  ├─ ast      : ASTNode* — ProgramNode (sahiplik burada)
  └─ tokens   : vector<Token*> — bellek yönetimi için

ModuleGraph
  ├─ units    : vector<ModuleUnit> — units[0] = giriş dosyası
  ├─ ~ModuleGraph() — ast + token'ları temizler
  └─ move-only (kopyalama yasak)

ModuleLoader
  ├─ registry_ : ModuleRegistry&
  ├─ diag_     : DiagnosticEngine&
  ├─ overlay_  : SourceOverlay (opsiyonel) — LSP buffer okuma
  ├─ seen_     : unordered_set<string> — yüklenen/yüklenen dosyalar
  ├─ load(entryPath) → ModuleGraph
  └─ resolvePath(importerPath, rawPath) → canonical yol
```

## Diğer modüllerle temas

| Modül | İlişki |
|-------|--------|
| Parser | `loadUnit()` içinde `Tokenizer::scan()` + `Parser::parse()` çağırır. |
| Symbol | `SymbolCollector::collectModuleGraph()` ile 3 geçişli sembol toplama. |
| CLI | `cmdRun()`/`cmdCheck()`/`cmdLSP()` pipeline'ın ilk adımı olarak `ModuleLoader::load()` çağırır. |
| LSP | `SourceOverlay` seam'i sayesinde disk yerine editör buffer'ından okur. |
| Core | `ModuleRegistry` ile modül ID yönetimi. |

## Tasarım kararları

- **Döngüsel bağımlılık**: `seen_` seti ile korunur; döngü sessizce atlanır
  (hata üretilmez). ADR-031: döngü tespiti TODO.
- **SourceOverlay seam**: LSP'nin diskte olmayan buffer'larını yüklemek için.
  `loadUnit` önce overlay'e sorar, false dönerse diske düşer.
- **Sahiplik**: ModuleGraph AST ve token'ların sahibidir; yıkıcıda temizler.
  Move-only (kopyalama yasak).
- **Sıra bağımsızlığı**: units[0] giriş dosyasıdır, diğerlerinin sırası
  garanti edilmez. SymbolCollector 3 geçişle sıra bağımsızlığını sağlar.
