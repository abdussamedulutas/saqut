# saQut 0.9.0 — Release Notu Taslağı (RG-9)

> Bu, #157 SQ-090-RELEASE-GATE'in RG-9 kriteri ("bilinen eksikler ve kapsam
> dışı maddeler release notunda dürüstçe listelenir") için hazırlanan
> taslaktır. Ürün sahibi onayı olmadan resmi release notu sayılmaz.

## Bu sürümde

- **VM normatif çalıştırma yolu.** `run/check/ast/ir/symbols` temel CLI
  yüzeyi, syntax/semantic/runtime hata sınıflarını merkezi bir sözleşmeyle
  (`exit 0/64/65/70`) ayırt eder (RG-7).
- **VM dogfood kanıtı.** Elle yazılmış, gerçek JSON parser, XML parser ve
  CPU-ağırlıklı (Eratosthenes kalburu + rekürsif Fibonacci + sort) programlar
  `tests/golden/dogfood/` altında tracked (RG-4).
- **Basit VM GC** varsayılan açık; çoklu collection ve canlı-kök korunumu
  `tests/golden/gc/liveness.sqt` ile ölçülmüş (RG-5).
- **Dört bilinen hata iddiası** (#134, #135, #136, #114) güncel binary ile
  yeniden doğrulandı: üçü (#134 exec parser hatası yutma, #135 statik
  string-indeksleme denetimi, #136 switch return-completeness) gerçek bug'lardı
  ve düzeltildi; #114 mimari borç olarak dispose edildi (aktif hata değil).
- **#170 düzeltildi.** Parser'daki `(`/`[`/`{` kapanış delimiter'ı bekleyen
  **25 site** (issue'nun bulduğu 19 + revalidasyonda bulunan 6 ek aynı-sınıf
  site: import listesi, ffi bildirimi, struct/enum gövdesi) artık eksik
  delimiter'da `E905` diagnostic'i basıyor — eskiden sessizce farklı bir
  programa yeniden yorumlanıp exit 0 ile "başarıyla" çalışıyordu (ADR-038
  ihlali). LSP'nin toleranslı kurtarma davranışı değişmedi, yalnız artık
  gerçek bir diagnostic kaydediyor. Aynı turda, `import { ... }` listesinde
  beklenmeyen bir token'da parser'ı sonsuz döngüye sokan bağımsız bir
  hang/DoS hatası da bulunup düzeltildi (PR #173).

## Bilinen eksikler (dürüst liste)

### Orta — bilinen, izole, ayrı issue'larda

- **#160**: `saqut ast --output <dosya>` JSON olmayan (varsayılan) modda
  dosyaya hiçbir şey yazmıyor (`ASTNode::log()` `std::cout`'a hardcode).
  `--json --output` ile çalışıyor.
- **#163**: `saqut exec` argümansız çağrıldığında `args.hpp`'nin global
  "source.sqt" fallback'i yüzünden anlamsız bir semantic hata veriyor,
  kendi usage mesajını hiç göstermiyor.
- **#165**: MIR JIT, `switch` bir fonksiyonun SON ifadesi olduğunda
  segfault veriyor. VM (normatif backend) etkilenmiyor; JIT zaten her yerde
  `[EXPERIMENTAL]` ve bu 0.9.0 kapısının şartı değil (AGENTS.md §9).
- **#168**: saQut fonksiyonları array tipini DÖNÜŞ TİPİ olarak alamıyor
  (`int[] f() {...}` parse hatası veriyor) — parametre/değişken tipi olarak
  sorunsuz.

### Mimari borç — 1.0.0/backlog'a önerilen

- **#114**: Sayısal biçimlendirme (float/double/float32 → string) VM ve
  JIT'te iki ayrı yerde elle senkron tutuluyor; VM'in sıcak yol
  karşılaştırmaları switch-exhaustiveness kalkanı olmayan if/else-if
  zinciri kullanıyor; constant folding pass'i tip genişliğini kendi başına
  varsayıyor. Şu an yanlış sonuç üretmiyor, yalnız yapısal risk.

### Kapsam dışı bırakılan (versiyon-dalı engelı, karar zaten verilmiş)

- **#144 (SQ-100-CHECK-JSONL), #145 (SQ-100-SYMBOLS-JSONL), #146
  (SQ-100-TOKEN-POSITIONS)**: kod tamamlandı, branch-local tam `ctest`
  kanıtıyla doğrulandı, ama hedef sürüm `1.0.0` ve bu proje sürüm dallarını
  birbirine merge etmiyor — `1.0.0` dalı ancak 0.9.0 release commit'inden
  açılabilir (#157 Amendment 01 kararı). Yani bu üç iş 0.9.0'ın KENDİSİNİN
  bitmesini bekliyor; 0.9.0 release edilip `1.0.0` dalı açılınca retarget +
  fresh validation ile devam eder.
- **saqut-docs#1 dilim B/C/D**: yukarıdaki üçüne bağımlı, aynı nedenle
  bekliyor.

### LSP/DAP preview matrisi — RG-8

0.9.0 için yeni LSP/DAP capability eklenmedi ve tam 1.0 protokol sözleşmesi
ilan edilmedi. Mevcut tracked preview alt kümesi ayrı matrise bağlandı:
`docs/v0.9-lsp-dap-preview-matrix.md`.

- LSP: 21 tracked senaryo.
- DAP: 10 tracked senaryo.
- Matrix, desteklenen/testlenen preview satırlarını, explicit unsupported
  yüzeyleri ve advertised-unverified boşlukları ayrı listeler.

Bilinen sınır korunur: Bu release note taslağı editor smoke, tam LSP/DAP v1
contractı veya yeni protocol capability vaadi değildir.

## Kapsam dışı (AGENTS.md §9, hatırlatma)

Stabil JIT, AOT, public concurrency, sandbox, WASM, record/replay,
production Redis/SQLite uyumluluğu, generic native library loading,
kapsamlı optimizer, 500+ self-hosted stdlib migrasyonu — bunların hiçbiri
0.9.0 veya 1.0.0'a gizlice eklenemez.

## MIR JIT durumu

Her yerde `[EXPERIMENTAL]`. #165 (switch-son-ifade segfault) dahil, JIT'in
bilinen hataları bu release'in başarı kriteri değildir — yalnız VM
normatif backend'dir.
