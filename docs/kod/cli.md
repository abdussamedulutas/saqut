# Komut Satırı Arayüzü (`src/cli/` + `src/cli/commands/`)

## Sorumluluk

saQut derleyicisinin tüm CLI komutlarını kaydeder, argümanları ayrıştırır
ve ilgili pipeline'ı çalıştırır. Her komut kendi header'ında inline fonksiyon
olarak tanımlanır ve `cli.hpp` tarafından include edilir.

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `cli.hpp` | `CliDispatcher`, `CliCommand` — komut kaydı ve dağıtımı. |
| `args.hpp` | `CliArgs` — komut satırı argümanlarını ayrıştırma. |
| `commands/run.hpp` | `cmdRun` — pipeline'ı baştan sona çalıştırır (yükle→parse→symbol→typecheck→opt→ir→vm). |
| `commands/check.hpp` | `cmdCheck` — yalnızca semantik analiz, hataları raporlar. |
| `commands/tokens.hpp` | `cmdTokens` — token listesini gösterir. |
| `commands/ast.hpp` | `cmdAst` — AST'yi JSON olarak gösterir (öncesi/sonrası optimizasyon). |
| `commands/symbols.hpp` | `cmdSymbols` — sembol tablosunu gösterir. |
| `commands/ir.hpp` | `cmdIr` — IR instruction'larını gösterir. |
| `commands/exec.hpp` | `cmdExec` — IR göster + çalıştır. |
| `commands/bench.hpp` | `cmdBench` — performans testlerini çalıştırır. |
| `commands/lsp.hpp` | `cmdLSP` — Language Server Protocol modunda başlatır. |
| `commands/dap.hpp` | `cmdDAP` — Debug Adapter Protocol modunda başlatır. |

## Ana tipler

```
CliCommand
  ├─ name        : string
  ├─ description : string
  ├─ hidden      : bool (alias'lar için)
  └─ execute(CliArgs&) → int (0 = başarılı)

CliDispatcher
  ├─ registerCommand(cmd) — komut ekle
  ├─ find(name) → CliCommand*
  ├─ run(argc, argv) → int — ana giriş
  └─ printHelp()

CliArgs
  ├─ has(name) → bool
  ├─ get(name) → string
  ├─ getInt(name, default) → int
  └─ positional → vector<string>
```

## Pipeline komutları

```
cmdRun:  ModuleLoader → SymbolCollector → TypeChecker → StructuralValidator
         → Optimizasyon → IRGenerator → Interpreter
cmdCheck: ModuleLoader → SymbolCollector → TypeChecker → StructuralValidator
cmdTokens: Tokenizer::scan() → token listesi
cmdAst:   ModuleLoader → SymbolCollector → AST JSON (önce/sonra optimizasyon)
cmdIR:    ModuleLoader → ... → IRGenerator → IR dump
cmdExec:  ModuleLoader → ... → IRGenerator → IR dump + Interpreter
cmdBench: Bench altyapısı
cmdLSP:   LSPHandler → stdio üzerinden LSP
cmdDAP:   DAPHandler → stdio üzerinden DAP
```

## Tasarım kararları

- **Her komut kendi header'ında**: Lazy include — yalnızca kullanılan komutun
  bağımlılıkları derlenir. `cli.hpp` tüm komutları include eder.
- **CliCommand fonksiyonel**: `std::function<int(const CliArgs&)>` ile esnek
  çağrı. Yeni komut eklemek: header oluştur + `registerCommand()`.
- **Main.cpp'de kayıt**: `registerCommand(cmdRun)` vb. Her komut bir satır.
- **Hidden komutlar**: `hidden=true` olan komutlar yardımda görünmez (alias'lar
  veya iç komutlar için).
