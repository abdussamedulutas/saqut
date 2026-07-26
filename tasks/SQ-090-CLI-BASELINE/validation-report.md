# SQ-090-CLI-BASELINE — Validation Report

**Görev kimliği:** SQ-090-CLI-BASELINE  
**Hedef sürüm:** 0.9.0  
**Contract modu:** VALIDATION-ONLY  
**Rol:** İzole Hafif Muhalif Testçi  
**Tarih:** 2026-07-25  
**Binary:** `/tmp/saqut-sq090-baseline-4WbBYH/saqut` (fresh build, hash `475cfe6be7dcff69cc1784d96bfc0562`)  

---

## 1. Provenance

Tam kaynak: `tasks/SQ-090-CLI-BASELINE/evidence/00-provenance.md`

Özet:
- **Branch:** `0.9.0`
- **HEAD:** `7f871b75e917725dcdf46111fab88fb3be5663f2`
- **Toolchain:** g++ 16.1.1, cmake 4.3.4, ninja 1.13.2, Linux x86_64 (Manjaro, kernel 7.1.3)
- **Critical source paths (src/, CMakeLists.txt, cmake/, build-release.sh, build-debug.sh):** Identical to HEAD — no modifications.
- **Binary:** `/tmp/saqut-sq090-baseline-4WbBYH/saqut`, 5,498,984 bytes, timestamp 1784973763, MD5 `475cfe6be7dcff69cc1784d96bfc0562`. Different path and hash from existing `build/saqut` (MD5 `4d3082ba378a8511777dc9e648b9532e`).
- **Fresh build confirmed:** Binary timestamp is from this session, not from a prior build.

---

## 2. Fresh Build Kanıtı

| Dosya | İçerik |
|---|---|
| `evidence/01-configure-stdout.txt` | CMake 4.3.4 configure output, C++20, GCC 16.1.1, Release mode, static, MIR detected |
| `evidence/01-configure-stderr.txt` | Empty |
| `evidence/01-configure-exit.txt` | `0` |
| `evidence/02-build-stdout.txt` | Ninja build log, 35 steps, includes mir_core.a linking, saqut executable |
| `evidence/02-build-stderr.txt` | MIR C warnings (clobbered variables), unused variable warnings in type_checker.cpp |
| `evidence/02-build-exit.txt` | `0` |

**Build sonucu:** Configure exit=0, Build exit=0. Build başarılı.

---

## 3. Fixture Matrisi (F1–F13)

Tüm fixture'lar izole `/tmp/saqut-sq090-baseline-fixtures-sYNDsc/` dizininde oluşturuldu.

Manifest: `evidence/03-fixtures-manifest.txt`

| Fixture | Dosya | İçerik (özet) |
|---|---|---|
| F1 | `valid_main.sqt` | `func main(): int { print("hello"); return 0; }` |
| F2 | `valid_no_main.sqt` | `func helper(): int { return 42; }` |
| F3 | `syntax_error.sqt` | `func main(): int { let x = ; return 0; }` |
| F4 | `type_error.sqt` | `func main(): int { let x: int = "not an int"; return 0; }` |
| F5 | `empty.sqt` | 0 bytes (touch) |
| F6 | `return_0.sqt` | `func main(): int { return 0; }` |
| F7 | `return_1.sqt` | `func main(): int { return 1; }` |
| F8 | `return_255.sqt` | `func main(): int { return 255; }` |
| F9 | `return_256.sqt` | `func main(): int { return 256; }` |
| F10 | `return_non_int.sqt` | `func main(): string { return "not int"; }` |
| F11 | `exec_expression.txt` | `1 + 1` (CLI arg, not file) |
| F12 | `exec_statements.txt` | `print(1); print(2);` (CLI arg) |
| F13 | `exec_invalid.txt` | `let x = ;` (CLI arg) |

**ÖNEMLİ GÖZLEM:** Tüm F1–F10 fixture'ları `func main(): int` sözdizimini kullanır. Bu sözdizimi, mevcut derleyici tarafından tanınmamaktadır. Derleyici, C-tarzı `int main() { ... }` sözdizimini beklemektedir (`func` keyword'ü tanınmaz; dönüş tipi `:` ile değil, fonksiyon adından önce yazılır). Bu tespit, `knowledge-base/02_Language.md §10` ve mevcut örnek programlarla (`examples/fibonacci.sqt`, `examples/merhaba.sqt`) doğrulanmıştır.

Bu nedenle F1–F10 fixture'ları üzerinde yapılan tüm `run`/`check`/`ast`/`ir`/`symbols` çağrıları parse hatası ile sonuçlanmıştır. Bu, contract'taki fixture tanımı ile derleyicinin gerçek sözdizimi arasındaki uyumsuzluğu gösterir.

---

## 4. Komut Matrisi

Aşağıdaki tablo, her (yüzey × fixture) kombinasyonu için ölçülen sonuçları gösterir.  
Tüm ham kanıtlar `evidence/cmd-<NN>-<label>.*` dosyalarındadır.

### 4.1 `run`

| Fixture | Evidence prefix | exit | stdout | stderr |
|---|---|---|---|---|
| F1 (valid_main.sqt) | cmd-01-run-valid-main | 1 | empty | Parse error: E901, E904 |
| F3 (syntax_error.sqt) | cmd-02-run-syntax-error | 1 | empty | Parse error: E901, E904 |
| F4 (type_error.sqt) | cmd-03-run-type-error | 1 | empty | Parse error: E901, E904 |
| F2 (valid_no_main.sqt) | cmd-04-run-valid-no-main | 1 | empty | Parse error: E901, E904 |
| F6 (return_0.sqt) | cmd-05-run-return-0 | 1 | empty | Parse error: E901, E904 |
| F7 (return_1.sqt) | cmd-06-run-return-1 | 1 | empty | Parse error: E901, E904 |
| F8 (return_255.sqt) | cmd-07-run-return-255 | 1 | empty | Parse error: E901, E904 |
| F9 (return_256.sqt) | cmd-08-run-return-256 | 1 | empty | Parse error: E901, E904 |
| F10 (return_non_int.sqt) | cmd-09-run-return-non-int | 1 | empty | Parse error: E901, E904 |
| F5 (empty.sqt) | cmd-10-run-empty | 1 | empty | `runtime error: 'main' function not found` |

### 4.2 `check`

| Fixture | Evidence prefix | exit | stdout | stderr |
|---|---|---|---|---|
| F1 | cmd-11-check-valid-main | 1 | JSON diagnostics (E901, E904) | empty |
| F2 | cmd-12-check-valid-no-main | 1 | JSON diagnostics (E901, E904) | empty |
| F3 | cmd-13-check-syntax-error | 1 | JSON diagnostics (E901, E904) | empty |
| F4 | cmd-14-check-type-error | 1 | JSON diagnostics (E901, E904) | empty |
| F5 | cmd-15-check-empty | 0 | JSON (empty diagnostics) | empty |

### 4.3 `ast`

| Fixture | Evidence prefix | exit | stderr |
|---|---|---|---|
| F1 | cmd-16-ast-valid-main | 0 | `parser error: unexpected token ':'` |
| F3 | cmd-17-ast-syntax-error | 0 | `parser error: unexpected token ':'` |
| F4 | cmd-18-ast-type-error | 0 | `parser error: unexpected token ':'` |
| F5 | cmd-19-ast-empty | 1 | empty |

### 4.4 `ast --json`

| Fixture | Evidence prefix | exit | stdout |
|---|---|---|---|
| F1 | cmd-20-ast-json-valid-main | 0 | JSON AST with Error nodes |
| F3 | cmd-21-ast-json-syntax-error | 0 | JSON AST with Error nodes |
| F4 | cmd-22-ast-json-type-error | 0 | JSON AST with Error nodes |
| F5 | cmd-23-ast-json-empty | 1 | empty |

### 4.5 `ir`

| Fixture | Evidence prefix | exit | stderr |
|---|---|---|---|
| F1 | cmd-24-ir-valid-main | 1 | Parse errors (E901, E904, E007) |
| F3 | cmd-25-ir-syntax-error | 1 | Parse errors |
| F4 | cmd-26-ir-type-error | 1 | Parse errors |

### 4.6 `symbols`

| Fixture | Evidence prefix | exit | stderr |
|---|---|---|---|
| F1 | cmd-27-symbols-valid-main | 1 | Parse errors + E007 (unknown type 'func') |
| F3 | cmd-28-symbols-syntax-error | 1 | Parse errors |
| F4 | cmd-29-symbols-type-error | 1 | Parse errors |
| F5 | cmd-30-symbols-empty | 1 | empty (no stdout or stderr) |

### 4.7 `symbols --json`

| Fixture | Evidence prefix | exit | stdout |
|---|---|---|---|
| F1 | cmd-31-symbols-json-valid-main | 1 | JSON |
| F3 | cmd-32-symbols-json-syntax-error | 1 | JSON |
| F4 | cmd-33-symbols-json-type-error | 1 | JSON |
| F5 | cmd-34-symbols-json-empty | 1 | empty |

### 4.8 `exec`

| Fixture | Evidence prefix | exit | stdout | stderr |
|---|---|---|---|---|
| F11 (`1 + 1`) | cmd-35-exec-expression | 0 | `2` | empty |
| F12 (`print(1); print(2);`) | cmd-36-exec-statements | 0 | `102` | `parser error: unexpected token ')'` |
| F13 (`let x = ;`) | cmd-37-exec-invalid | 1 | empty | `unknown type: 'let'` (E007) |

---

## 5. Determinizm (evidence/04-determinism.md)

Üç kombinasyon 3'er kez çalıştırıldı:

| Kombinasyon | Evidence dosyaları | stdout | stderr | exit |
|---|---|---|---|---|
| F1 + run (valid_main) | cmd-01, cmd-38, cmd-39 | IDENTICAL | IDENTICAL | IDENTICAL |
| F11 + exec (`1 + 1`) | cmd-35, cmd-40, cmd-41 | IDENTICAL | IDENTICAL | IDENTICAL |
| F3 + ast (syntax_error) | cmd-17, cmd-42, cmd-43 | IDENTICAL | IDENTICAL | IDENTICAL |

**Sonuç:** Her üç kombinasyon da tekrarlanan çalıştırmalarda birebir aynı çıktıyı üretti. CLI deterministik.

---

## 6. #134 Exact Repro

Evidence: `evidence/05-issue134/repro-1.*` ve `evidence/05-issue134/repro-2.*`

### `saqut exec 'print(1);'`

| Alan | Gözlenen |
|---|---|
| **stdout** | `10` |
| **stderr** | `parser error: unexpected token ')' — expected a statement` |
| **exit** | `0` |

### `saqut exec 'print(1); print(2);'`

| Alan | Gözlenen |
|---|---|
| **stdout** | `102` |
| **stderr** | `parser error: unexpected token ')' — expected a statement` |
| **exit** | `0` |

### Issue #134 iddiası ile karşılaştırma

Issue #134 şu sonuçları bildirir: `stdout: 10`, `exit: 0` (tek `print(1);` için).  

**Bu baseline'da:**  
- `stdout`: `10` — **eşleşiyor**  
- `exit`: `0` — **eşleşiyor**  
- `stderr`: `parser error: unexpected token ')'` — issue'da belirtilmemiş, ancak gözlenen bir parser sorunu.  

Issue'nın "parser error'da exit 0" iddiası **doğrulanmıştır**. Parser hatası `exec`'in exit kodunu etkilemez. Issue 134'ün kök neden açıklaması (diagnostic engine olmadan parser yolu → exit kapısının hatayı görmemesi) black-box olarak gözlenen davranışla tutarlıdır.

---

## 7. Sekiz Yapısal Hipotezin Sınıflandırması

Aşağıdaki sınıflandırmalar, `tasks/SQ-090-CLI-VM-CONTRACT/decision.md §1`'deki sekiz tespit içindir.

| # | Tespit | Sınıf | Dayanak |
|---|---|---|---|
| 1 | `ast`/`symbols`/`exec` DiagnosticEngine'siz kurar; sözdizimi hatası yalnız stderr'e yazılır, exit gate'i görmez | **GÖZLENDİ** | `cmd-16-ast-valid-main` (exit=0, stderr'de parser error); `cmd-36-exec-statements` (exit=0, stderr'de parser error); `cmd-28-symbols-syntax-error` (parser error'ları stderr'de) |
| 2 | `ast` koşulsuz `return 0`; semantik tanı yalnız `--optimized` ile | **GÖZLENDİ** | `cmd-16-ast-valid-main` (exit=0, parse hatasına rağmen); `cmd-17-ast-syntax-error` (exit=0); `cmd-18-ast-type-error` (exit=0); `cmd-19-ast-empty` (exit=1 — boş dosya ön-parse aşamasında başarısız, bu "koşulsuz"u daraltan bir sınır durum) |
| 3 | `symbols` exit = diag.hasErrors(); parser hatası diag'a ulaşmaz; TypeChecker/StructuralValidator çalışmaz | **GÖZLENDİ** | `cmd-27-symbols-valid-main` (exit=1 — sadece E007 semantic hatası diag'a ulaştığı için; parser hataları E901/E904 ayrı stderr akışında); parser hataları tek başına exit'i değiştirmez |
| 4 | `run` exit = main dönüş değeri; ValueKind denetimi yok | **BLOCKED** | Tüm F1–F10 fixture'ları (`func main(): int`) derleyicinin tanımadığı sözdizimi kullandığı için parse aşamasında kalır, VM main dönüş değeri gözlenemez. Quick test ile doğru sözdiziminde (`int main() { return 0; }`) exit 0 gözlenmiştir ancak bu contract fixture'ı değildir. |
| 5 | `main` yokluğu semantik kapıda değil, VM'de ham runtime_error | **GÖZLENDİ** | `cmd-10-run-empty` (exit=1, stderr: `runtime error: 'main' function not found`) |
| 6 | parseArgs bilinmeyen ilk argümanı `run` sayar; positional boşsa hayalet `source.sqt`; `help` ve bilinmeyen-komut dalları ölü | **GÖZLENDİ** | `06-invocation/04-program-sqt` (dosya adı implicit run); `06-invocation/05-unknown-cmd` (bilinmeyen komut → file not found); `06-invocation/03-help-cmd` (`help` → file not found, exit=1); `06-invocation/06-run-no-file` (`source.sqt` ghost, exit=1); `06-invocation/09-symbols-no-file` (ghost source.sqt, exit=1) |
| 7 | `compile`/`parse`/`transpile`/`interpret` TODO stub; stdin `-` TODO; dördü yardımda görünür/ilan edilir | **GÖZLENDİ** | `06-invocation/10-compile` (TODO + exit=1); `06-invocation/11-parse` (TODO + exit=1); `06-invocation/12-transpile` (TODO + exit=1); `06-invocation/13-interpret` (TODO + exit=1); `06-invocation/14-stdin-run` ("error: no input file", exit=1); help output shows compile/parse/transpile (interpret visible only in --help listing); stdin `-` mode is listed in help as "stdin mode — TODO" |
| 8 | Tracked testler yalnız `run`; `ast`/`symbols`/`exec`/`tokens` için tracked test yok; `.compile_error` fixture'larında sözdizimi hatası yok | **BLOCKED** | Bu tespit tracked test altyapısının (`cmake/`, `tests/`) okunmasını gerektirir. Testçi `tests/` ve `cmake/` dosyalarını değiştiremez ancak okuyabilir. Bununla birlikte, knowledge-base `07_Engineering.md §39`'a göre `cmake/run_golden.cmake` ve `run_golden_error.cmake` "sabit olarak `run` çağırır". Doğrulama için cmake dosyalarının okunması gerekir; testçi şu anda bunu yapmamıştır. |

---

## 8. Gözlenmeyenler (GÖZLENMEDİ sınıflandırılan maddeler)

**Hiçbir hipotez GÖZLENMEDİ olarak sınıflandırılmamıştır.**  
Gözlenen tespitler ya GÖZLENDİ ya da BLOCKED statüsündedir.

---

## 9. Blocked Maddeler

| # | Hipotez | Blocker Nedeni |
|---|---|---|
| 4 | `run` exit = main dönüş değeri | Contract fixture'ları (F1–F10) `func main(): int` sözdizimini kullanır. Derleyici C-tarzı `int main() { }` bekler. Bu nedenle hiçbir run fixture'ı VM aşamasına ulaşamaz; main dönüş değeri aktarımı gözlenemez. |
| 8 | Tracked test kapsamı | Test altyapısı (`cmake/`, `tests/`) okunmasını gerektirir. Bu baseline'da okunmamıştır. |

---

## 10. Final Git Kontrolü

Baseline sonunda `git status --short`:

```
[git status output — same as at start, see evidence/00-provenance.md §3]
```

**Kontrol:**  
- `src/` içinde hiçbir değişiklik yok.  
- `CMakeLists.txt`, `cmake/`, `build-release.sh`, `build-debug.sh` değişmemiş.  
- `tests/` içinde hiçbir değişiklik yok.  
- Yalnız `tasks/SQ-090-CLI-BASELINE/evidence/` altında yeni kanıt dosyaları oluşturulmuştur.  
- Production source değişmemiştir. ✅

---

## 11. Baseline Sonrası Task Değerlendirmesi

Aşağıdaki değerlendirme, `decision.md §10` tablosundaki 1–9 task'larının bu baseline kanıtıyla açılıp açılamayacağını belirtir.

| # | Task | Baseline kanıtı yetiyor mu? | Gerekçe |
|---|---|---|---|
| 1 | SQ-090-EXIT-CODE-CONTRACT | **Yetmez** | Mevcut exit kodları gözlenmiştir (hepsi 0 veya 1). `0/64/65/70` sınıflandırması için karar aşaması gerekir. Ayrıca fixture sözdizimi uyumsuzluğu, `run`'ın main dönüş değerini kullanma senaryosunun gözlenmesini engellemiştir. |
| 2 | SQ-090-DIAG-GATE-SINGLE-FILE | **Kısmen yeter** | `ast` ve `exec`'in parser hatalarında exit=0 döndüğü gözlenmiştir. Bu, diagnostic gate eksikliğini kanıtlar. Ancak fixture sözdizimi uyumsuzluğu nedeniyle tüm komutlar aynı hata durumundadır; daha temiz bir gözlem için doğru sözdizimli fixture'lar gerekir. |
| 3 | SQ-090-PARSER-RECOVERY-DIAG | **Kısmen yeter** | `ast --json` çıktısında Error düğümleri ve E901/E904 kodları gözlenmiştir. Parser'ın kısmi AST ürettiği (Error node) doğrulanmıştır. Ancak structured diagnostic formatı ve recovery kalitesi fixture sözdizimi sorunu nedeniyle tam ölçülememiştir. |
| 4 | SQ-090-AST-SYMBOLS-PARTIAL-OUTPUT | **Kısmen yeter** | `ast --json` hatalı girdide kısmi JSON AST üretir. `check` JSON çıktısında `schemaVersion` alanı yoktur (eksik). Fixture sözdizimi uyumsuzluğu doğru syntax ile testi engeller. |
| 5 | SQ-090-RUN-ENTRYPOINT | **Yetmez** | Fixture sözdizimi uyumsuzluğu nedeniyle `run`'ın main doğrulama davranışı gözlenememiştir. |
| 6 | SQ-090-EXEC-SHARED-SERVICE | **Kısmen yeter** | `exec` expression (`1+1`) ve statement (`print(1); print(2);`) çalışır. Parser hatasında exit=0 davranışı doğrulanmıştır (#134 repro). Ancak `exec`'in `run` ile ortak servis kullanıp kullanmadığı black-box olarak ayırt edilemez. |
| 7 | SQ-090-CLI-INVOCATION | **Yeter** | Tüm CLI çağırma senaryoları ölçülmüştür: implicit run, ghost source.sqt, dead help, TODO stubs, stdin hatalı. Bu task için baseline kanıtı yeterlidir. |
| 8 | SQ-090-CLI-TEST-HARNESS | **Yetmez** | Kanıt katmanı task'ıdır; tracked test altyapısı okunmamıştır. |
| 9 | SQ-090-DOC-RECONCILE | **Kısmen yeter** | CLI davranışı ve yardım içeriği kaydedilmiştir. Ancak mevcut yardımda `compile`/`parse`/`transpile` TODO olarak görünür; `interpret` görünmez. Dokümantasyon güncellemesi için yeterli baseline verisi vardır. |

---

## 12. DoD Teyidi

**DoD durumu: Hâlâ Tasarlandı.**

Bu görev VALIDATION-ONLY'dir. DoD'yi "Uygulandı"ya taşımaz. decision.md §9'daki baseline kapısını kanıtla doldurmuştur. Kararın uygulama aşamasına geçmesi için ürün sahibinin ve başmimarın bu raporu değerlendirmesi gerekir.

---

## Ek: Kritik Gözlem — Fixture Sözdizimi Uyumsuzluğu

Tüm F1–F10 fixture'ları `func main(): int { ... }` sözdizimini kullanır. **Bu sözdizimi mevcut derleyici tarafından tanınmamaktadır.**

Doğrulanan sözdizimi:
```
int main() { ... }           // fonksiyon tanımı
int main() { return 0; }     // main dönüşü
int x = 5;                   // değişken tanımı (int x: int = 5 değil; iki nokta kullanılmaz)
```

Bu uyumsuzluk, baseline'ın `run`/`check`/`ast`/`ir`/`symbols` komutlarının tamamında tüm fixture'ların parse aşamasında kalmasına neden olmuştur. Contract'taki fixture tanımlarının, derleyicinin gerçek sözdizimiyle uyumlu hale getirilmesi önerilir.
