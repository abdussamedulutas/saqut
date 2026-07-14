# ADR-035 — Capability Modeli (--allow-fs/net/sys, A+B enforcement)

İlgili: #76, #87, #90, #91. Önceki: ADR-034 (FFI declaration modeli).

## Bağlam

FFI seam'i (ADR-034) host fonksiyonları için sözdizimsel/dispatch altyapısını
kurdu, ama dış dünyaya açılan fonksiyonların (dosya I/O, sistem çağrıları)
kontrolsüz kullanılmasını engelleyen bir mekanizma yoktu. #76'da tartışılan
tasarım burada uygulamaya kondu: varsayılan her şey **kapalı**, kullanıcı
`--allow-fs` / `--allow-net` / `--allow-sys` ile açıkça izin vermeli.

## Karar

- **Kategori:** üç capability — `fs`, `net`, `sys` (#76'nın kendi önerisi).
  `date::now()` da `sys` kategorisine girer (zaman non-deterministik/dış-durum
  okuyan bir kaynaktır).
- **Bildirim seviyesi:** `ffi ... requires <cap>;` (ADR-034'te zaten ayrılmış
  sözdizimsel yer) → `Symbol::requiredCap` → `Instruction::requiredCap` →
  runtime kontrol. Tek kaynaktan (root.sqt) üç katmana akar.
- **A+B enforcement (#76'nın kendi önerisi):**
  - **A — derleme zamanı:** `SymbolCollector::resolveFfiImport`, import
    anında `allowedCaps_` kümesine bakar; eksikse `E_CAP_MISSING` derleme
    hatası (import-gating zaten var olan mekanizmayla aynı noktada).
  - **B — runtime backstop:** `Interpreter`, CALLHOST `__ffi__` dispatch'inde
    `instr.requiredCap`'i `caps_`'e karşı kontrol eder; eksikse yakalanabilir
    `E_CAP_MISSING` Error fırlatır (ADR-025 uyumlu — `try/catch` ile
    yakalanabilir). B, A olmadan da (örn. `caps::drop` sonrası) tek başına
    yeterli — runtime her zaman son sözü söyler.
- **`caps::drop`/`caps::has` (#91):** capability'siz — düşürmek her zaman
  serbest, yükseltme YOK (geri ekleme fonksiyonu yok). VM state'ine
  (`caps_`) doğrudan erişim gerektirdiğinden `__ffi__` genel yolundan değil,
  `HostContext` enjeksiyonuyla (`HostContext::caps` pointer'ı) çalışır —
  `host_functions.hpp`'deki `HostFn::impl` imzası bu yüzden `HostContext&`
  parametresi aldı (fs/sys'in de VM state'ine ihtiyacı var: `readBytes`/
  `writeBytes` heap, `sys::args()` CLI argümanları).
- **Statik analiz (`saqut ir --capabilities`):** IR'deki tüm `requiredCap`
  alanlarını toplar, kullanılan kategori kümesini raporlar. **`caps::drop`'tan
  ETKİLENMEZ** — bu üst-sınır (programın ihtiyaç DUYABİLECEĞİ capability'ler)
  raporudur, runtime'da gerçekten kullanılıp kullanılmadığını değil.
- **Golden test altyapısı:** `--allow-fs` gibi bayrak gerektiren testler için
  `BASE.flags` dosyası eklendi (satır başına bir CLI bayrağı); hem CMake
  keşif mekanizmasına hem `tests/run.sh`'a işlendi.

## Reddedilenler

- **Yol bazlı kısıt** (`--allow-fs-read=/tmp/*`): v1'de hepsi-ya-hiçbir-şey;
  #76'da v2 olarak işaretlenmiş, bilinçli olarak ertelendi.
- **Manifest/JSON dosyası** (`saqut.json`): CLI bayrağı birincil; #76'da
  opsiyonel sonraki-adım olarak bırakıldı.
