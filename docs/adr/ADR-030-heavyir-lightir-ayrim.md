# ADR-030 — heavyIR / lightIR Ayrımı

**Durum:** Kabul  
**Tarih:** 2026-06-25

## Karar

IR üreteci iki mod üretir:

- **heavyIR (varsayılan):** Tam meta-veri içerir — alan adları (`fieldNames`), fonksiyon
  adları, kaynak konum (`sourceLine`, `sourceCol`, `sourceFile`). `saqut ir` komutunda
  görüntülenir; LSP, DAP ve hata mesajları bu bilgiye dayanır.

- **lightIR (`--optimized` ile optimizasyon sonrası):** Sabit katlama ve DCE geçirmiş,
  gereksiz instruction'lar kaldırılmış IR. Yine de kaynak konum bilgisi taşır (stacktrace
  için). Optimizasyon **klon** üzerinde yapılır, orijinal heavyIR dokunulmaz (ADR-007).

## Motivasyon

Analiz araçları (AST dump, sembol tablosu, diagnostic) orijinal IR'ı referans alır.
Optimizasyon geçitleri klonla çalışarak analiz verilerini bozmaz.

## Uygulama

`src/opt/optimization_manager.cpp`: `OptimizationManager` AST klonunu alır; IR generator
klondan çalışır. `--optimized` bayrağı `src/cli/commands/run.hpp` ve `ir.hpp`'da işlenir.
