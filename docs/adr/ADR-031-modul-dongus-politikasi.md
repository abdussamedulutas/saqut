# ADR-031 — Modül Döngüsü Tespiti Politikası

**Durum:** Uygulandı (issue #78, 2026-07-12)  
**Tarih:** 2026-06-25

## Karar

Döngüsel bağımlılık (`A → B → A`) tespit edildiğinde derleme hatası üretilir:

```
E_MODULE_CYCLE: circular module dependency detected: a.sqt -> b.sqt -> a.sqt
```

## Uygulama

`src/module/module_loader.cpp`: `seen_` seti tekrar yüklemeyi önlemeye devam eder
(elmas bağımlılıkta D bir kez yüklenir). Yanına eklenen `loadChain_` vektörü aktif
yükleme zincirini sıralı tutar; bir dosya kendi zincirinde tekrar görünürse
`E_MODULE_CYCLE` tanısı üretilir. Tanının konumu döngüyü kapatan `import`
bildiriminin `SourceLocation`'ıdır; mesaj döngü zincirini dosya adlarıyla gösterir.
Kendi kendini import eden dosya (`self.sqt → self.sqt`) da aynı yoldan yakalanır.

## Testler

- `tests/module/cycle_a.sqt` + `cycle_b.sqt` — karşılıklı import → `E_MODULE_CYCLE`
- `tests/module/self_import.sqt` — kendini import → `E_MODULE_CYCLE`
- `tests/golden/module/diamond.sqt` — elmas bağımlılık yanlış-pozitif üretmez
  (golden run testi, çıktı `23`)

Döngü testleri `tests/run.sh` "modül döngüsü" bölümünde `saqut check` exit kodu +
tanı kodu üzerinden doğrulanır.
