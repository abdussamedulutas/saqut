# SQ-090-EXIT-CODE-CONTRACT — Validation Report

**Revision:** Amendment 02
**Rol:** İzole Hafif Muhalif Testçi (coder'dan tamamen ayrı, bağımsız oturum)
**Tarih:** 2026-07-25
**Binary SHA-256:** `98c0acdedfb9eddac2c96c8b15aeadc37e977c725746c8bf8bd5a989580ee5a2`

---

## 1. Provenance

| Alan | Değer |
|---|---|
| Branch | `0.9.0` |
| HEAD | `7f871b75e917725dcdf46111fab88fb3be5663f2` |
| Build dizini | `/tmp/saqut-sq090-exitcode-validation-2DHSAm` |
| Binary yolu | `/tmp/saqut-sq090-exitcode-validation-2DHSAm/saqut` |
| Binary SHA-256 | `98c0acdedfb9eddac2c96c8b15aeadc37e977c725746c8bf8bd5a989580ee5a2` |
| Binary boyut | 5,498,984 bytes |
| Build tipi | Release |
| Derleyici | GCC 16.1.1 20260625 |
| CMake | 4.3.4 |
| Ninja | 1.13.2 |
| Timestamp | 1784982835 (epoch) |

Ayrıntı: `evidence/00-provenance.md`

---

## 2. Değişen dosyalar (`git diff --stat HEAD` — 4 izin verilen dosya)

```
src/cli/commands/check.hpp | 3 ++-
src/cli/commands/ir.hpp    | 3 ++-
src/cli/commands/run.hpp   | 9 +++++----
```

`exit_codes.hpp` **untracked** olarak görünür (yeni dosya):

```
?? src/cli/exit_codes.hpp
```

Toplam 3 dosyada değişiklik, 1 yeni dosya. Bu, implementation-contract §7'nin izin verdiği 4 dosyayla tam eşleşir.

**Kontrol: `src/` altında beklenmeyen başka değişiklik yok.** (Grep sonucu boş döndü — ek değişiklik bulunamadı.)

---

## 3. Fixture matrisi

| ID | Fixture | Yol | SHA-256 |
|---|---|---|---|
| FX-SYNTAX | syntax_error_recovery.sqt | `tests/lsp/fixtures/syntax_error_recovery.sqt` | `e3c2b87c95a96ac7607f3b7f0d90826335a12f0573eb472de81ab3f46620658f` |
| FX-SEMANTIC | longint_narrowing.sqt | `tests/golden/numeric/longint_narrowing.sqt` | `52107e4f5ab10e9a1866e08fbb8b39e4facf80bbf97072d4bada5b5e57320da3` |
| FX-RUNTIME | mod_by_zero.sqt | `tests/golden/arithmetic/mod_by_zero.sqt` | `ff84a666b6e6f2e50e52b00afee56b3f7f61f4f1ffd1bde79f9f51196725a87f` |
| FX-VALID-CHECK-IR | fibonacci.sqt | `examples/fibonacci.sqt` | `dcbad0670734f4b0ca9b5e8acc1293d4b6f2d40d32aa304f4e4235e7c5a5cd3f` |
| FX-VALID-0 | valid_return_0.sqt | `/tmp/saqut-fixtures-QpDn4D/valid_return_0.sqt` | `7a1f5b98cf232b8f459a41fc77a26e587b6c31e336e76b852eb8b5adbfd108c8` |
| FX-VALID-1 | valid_return_1.sqt | `/tmp/saqut-fixtures-QpDn4D/valid_return_1.sqt` | `c83e34cd1e25e8fefb5c028f7ef48946eb65b1ed811c6b2563a65d22bde5e8cb` |
| FX-VALID-255 | valid_return_255.sqt | `/tmp/saqut-fixtures-QpDn4D/valid_return_255.sqt` | `ea5b0be6d1b9ae786a2c6d1cf57a89e1989fa7c24b9ddd5db46d654e9a000a49` |

Tüm fixture'lar mevcut ve erişilebilir. Ayrıntı: `evidence/01-fixtures-manifest.txt`

---

## 4. Komut matrisi sonuçları

| # | Komut | Fixture | Beklenen | Gözlenen | Sonuç | Evidence |
|---|---|---|---|---|---|---|
| 1 | `run` | FX-SYNTAX | exit 65 | exit 65 | **GÖZLENDİ** | `cmd-01-run-syntax.exit.txt` |
| 2 | `check` | FX-SYNTAX | exit 65 | exit 65 | **GÖZLENDİ** | `cmd-02-check-syntax.exit.txt` |
| 3 | `ir` | FX-SYNTAX | exit 65 | exit 65 | **GÖZLENDİ** | `cmd-03-ir-syntax.exit.txt` |
| 4 | `run` | FX-SEMANTIC | exit 65 | exit 65 | **GÖZLENDİ** | `cmd-04-run-semantic.exit.txt` |
| 5 | `check` | FX-SEMANTIC | exit 65 | exit 65 | **GÖZLENDİ** | `cmd-05-check-semantic.exit.txt` |
| 6 | `ir` | FX-SEMANTIC | exit 65 | exit 65 | **GÖZLENDİ** | `cmd-06-ir-semantic.exit.txt` |
| 7 | `run` | FX-RUNTIME | exit 70 | exit 70 | **GÖZLENDİ** | `cmd-07-run-runtime.exit.txt` |
| 8 | `run` | FX-VALID-0 (return 0) | exit 0 | exit 0 | **GÖZLENDİ** | `cmd-08-run-valid0.exit.txt` |
| 9 | `run` | FX-VALID-1 (return 1) | exit 1 | exit 1 | **GÖZLENDİ** | `cmd-09-run-valid1.exit.txt` |
| 10 | `run` | FX-VALID-255 (return 255) | exit 255 | exit 255 | **GÖZLENDİ** | `cmd-10-run-valid255.exit.txt` |
| 11 | `check` | FX-VALID-CHECK-IR | exit 0 | exit 0 | **GÖZLENDİ** | `cmd-11-check-valid.exit.txt` |
| 12 | `ir` | FX-VALID-CHECK-IR | exit 0 | exit 0 | **GÖZLENDİ** | `cmd-12-ir-valid.exit.txt` |

**Tüm 12 komut beklenen exit kodunu üretti. Hiçbir sapma yok.**

---

## 5. stdout/stderr içerik korunması kontrolü (§5)

### 5a. CMD-01 (run + FX-SYNTAX) stderr

- Baseline: `tasks/SQ-090-CLI-BASELINE/evidence/amendment-02/cmd-01-run-lsp-syntax-error.stderr.bin`
- New: `evidence/cmd-01-run-syntax.stderr.bin`

Path normalizasyonu sonrası **byte-identical** (`diff exit=0`).

### 5b. CMD-02 (check + FX-SYNTAX) stdout

- Baseline: `tasks/SQ-090-CLI-BASELINE/evidence/amendment-02/cmd-02-check-lsp-syntax-error.stdout.bin`
- New: `evidence/cmd-02-check-syntax.stdout.bin`

Path normalizasyonu sonrası **byte-identical** (`diff exit=0`). Not: Baseline'da her iki `"file"` alanı absolute path içerirken, yeni çıktıda `"file"` alanlarından biri absolute diğeri relative idi. Her ikisi de `%FIXTURE%` ile normalize edildi ve fark kalmadı.

**Değerlendirme:** Implementation-contract §3'ün "stdout/stderr içeriği değişmedi" iddiası **GÖZLENDİ** — fark yalnız fixture path'inden kaynaklanıyor, gerçek metin (diagnostic code, message, line/column/offset) tamamen aynı.

---

## 6. Determinizm kanıtı (§6)

### `run` + FX-SYNTAX (3 tekrar)

| Run | Exit | stdout SHA-256 | stderr SHA-256 |
|---|---|---|---|
| 1 | 65 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | `0ce6eac9b3e08cf3328e280cd27957a578e03c413da4cc582590314fe6b8cc44` |
| 2 | 65 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | `0ce6eac9b3e08cf3328e280cd27957a578e03c413da4cc582590314fe6b8cc44` |
| 3 | 65 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | `0ce6eac9b3e08cf3328e280cd27957a578e03c413da4cc582590314fe6b8cc44` |

### `run` + FX-RUNTIME (3 tekrar)

| Run | Exit | stdout SHA-256 | stderr SHA-256 |
|---|---|---|---|
| 1 | 70 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | `0e834bd0ecb7a2d93eb703a037f70056aa764f1b346cd557b96db9a1c8e52685` |
| 2 | 70 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | `0e834bd0ecb7a2d93eb703a037f70056aa764f1b346cd557b96db9a1c8e52685` |
| 3 | 70 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | `0e834bd0ecb7a2d93eb703a037f70056aa764f1b346cd557b96db9a1c8e52685` |

**Sonuç:** "Seçilen `run` + `FX-SYNTAX` ve `run` + `FX-RUNTIME` kombinasyonları, bu ortamda üç tekrar boyunca byte-identical sonuç üretti."

Ayrıntı: `evidence/02-determinism.md`

---

## 7. Tracked test regresyon sonucu (§7)

```
ctest --test-dir /tmp/saqut-sq090-exitcode-validation-2DHSAm -R '^golden_' --output-on-failure
```

**85/85 tests passed, 0 tests failed. Süre: 0.68 sn.**

Tüm golden testleri geçti. `tests/run.sh` bu task'ta çalıştırılmadı (gerekçe: §7'de belirtildiği gibi script satır 9'da `SAQUT="$ROOT/build/saqut"` hardcode edilmiştir).

### `.compile_error` fixture exit kodları (bilgi amaçlı)

8 adet `.compile_error` fixture'ının her biri `run` ile exit **65** üretir:

| Fixture | exit |
|---|---|
| `longint_narrowing.sqt` | 65 |
| `nullable_assign_error.sqt` | 65 |
| `nullable_operand_error.sqt` | 65 |
| `now_no_permission.sqt` | 65 |
| `fs/no_permission.sqt` | 65 |
| `math/ffi_not_imported.sqt` | 65 |
| `string/ordering_error.sqt` | 65 |
| `sys/no_permission.sqt` | 65 |

Bu, `run_golden_error.cmake`'ın beklentisiyle uyumludur (exit 1 → 65 değişimi bu testleri etkilemez).

Ayrıntı: `evidence/03-tracked-regression.txt`

---

## 8. `ast`/`symbols` dokunulmadı kontrolü (§8)

### ast (CMD-13, FX-SYNTAX ile)

| Ölçüm | Baseline (amendment-02) | Yeni | Fark |
|---|---|---|---|
| Exit | 0 | 0 | Yok |
| stderr (byte) | `parser error: unexpected token ')' — expected a statement` | Aynı | **Identical** |
| stdout | 1654 bytes | 1654 bytes | **Byte-identical** (diff exit=0) |

### symbols (CMD-14, FX-SYNTAX ile)

| Ölçüm | Baseline (amendment-02) | Yeni | Fark |
|---|---|---|---|
| Exit | 0 | 0 | Yok |
| stderr | `parser error: unexpected token ')' — expected a statement` | Aynı | **Identical** |
| stdout | Path absolute (`/tmp/...`) | Path relative (`tests/lsp/...`) | **Path normalization sonrası byte-identical** |

### exec '1 + 1' (§8b, CMD-15)

| Ölçüm | Baseline (amendment-01) | Yeni | Fark |
|---|---|---|---|
| Exit | 0 | 0 | Yok |
| stdout | `2` (1 byte) | `2` (1 byte) | **Byte-identical** |
| stderr | (boş) | (boş) | **Identical** |

**Değerlendirme:** INV-6 ihlali yok. `ast`, `symbols` ve `exec` davranışı değişmemiştir — **GÖZLENDİ.**

---

## 9. K-1/K-3/K-4/K-7, RUNTIME-1 ve K-6 teyidi (§9)

| Kriter | İçerik | Bu task'ta kapsam | Sonuç | Evidence |
|---|---|---|---|---|
| **K-1 (kısmi)** | Syntax hatalı dosya → `run`/`check`/`ir` exit 65 | Evet (§4 satır 1-3) | **GÖZLENDİ** — her üç komut exit 65 üretti | cmd-01, cmd-02, cmd-03 |
| **K-3 (kısmi)** | Semantic hatalı dosya → aynı | Evet (§4 satır 4-6) | **GÖZLENDİ** — her üç komut exit 65 üretti | cmd-04, cmd-05, cmd-06 |
| **K-4 (kısmi)** | Geçerli program → `check`/`ir` exit 0 | Evet (§4 satır 11-12) | **GÖZLENDİ** — her iki komut exit 0 üretti | cmd-11, cmd-12 |
| **K-7** | `main` dönüşü process status'u | Evet (§4 satır 8-10) | **GÖZLENDİ** — 0→0, 1→1, 255→255 | cmd-08, cmd-09, cmd-10 |
| **RUNTIME-1** | `mod_by_zero.sqt` `run` ile exit 70 + stderr korunur | Evet (task-local) | **GÖZLENDİ** — exit 70, stderr `runtime error: sıfıra bölme (mod)` önceki tracked testle tutarlı | cmd-07 |
| **K-6** | Eksik/geçersiz entrypoint → exit 65 structured diagnostic | **Bu task kapsamı dışı** | **ÖLÇÜLMEDİ** — `SQ-090-RUN-ENTRYPOINT` kapsamıdır | — |

---

## 10. Final `git status --short` (görev sonu)

Ham çıktı (değişiklik yok, yalnız dosya ekleme — evidence ve validation-report):

```
 M src/cli/commands/check.hpp
 M src/cli/commands/ir.hpp
 M src/cli/commands/run.hpp
?? src/cli/exit_codes.hpp
?? tasks/
```

Bu task kapsamında `tasks/SQ-090-EXIT-CODE-CONTRACT/evidence/` altına kanıt dosyaları ve `tasks/SQ-090-EXIT-CODE-CONTRACT/validation-report.md` yazılmıştır. Production source, tracked test, commit veya issue değiştirilmemiştir.

---

## 11. DoD durumu teyidi

**Bu validation raporu tek başına DoD'yi "Test Edildi"ya taşımaz.** Karar ürün sahibi/mimara aittir. Bu rapor yalnız kanıt sağlar.

---

## Zorunlu Çalışma Sonu Raporu (AGENTS.md §10)

### 1. Rol ve görev kimliği

**İzole Hafif Muhalif Testçi**, SQ-090-EXIT-CODE-CONTRACT, Amendment 02, hedef sürüm 0.9.0, VALIDATION-ONLY.

### 2. İncelenen kanıt

- `tasks/SQ-090-EXIT-CODE-CONTRACT/validation-contract.md` — tamamı okundu ve uygulandı.
- `tasks/SQ-090-CLI-BASELINE/evidence/amendment-02/` — cmd-01, cmd-02, cmd-03, cmd-06 ham çıktıları karşılaştırma için kullanıldı.
- `tasks/SQ-090-CLI-BASELINE/evidence/amendment-01/` — cmd-35 (exec '1 + 1') karşılaştırma için kullanıldı.
- Fixture'lar: `tests/lsp/fixtures/syntax_error_recovery.sqt`, `tests/golden/numeric/longint_narrowing.sqt`, `tests/golden/arithmetic/mod_by_zero.sqt`, `examples/fibonacci.sqt`.

### 3. Yapılan değişiklik veya karar

Hiçbir kod, test, doküman veya contract değişikliği yapılmamıştır. Yalnız:
- Bağımsız fresh Release Build üretildi (`/tmp/saqut-sq090-exitcode-validation-2DHSAm/`).
- 12 komutluk test matrisi çalıştırıldı.
- stdout/stderr path-normalize edilmiş karşılaştırmalar yapıldı.
- Determinizm testi yapıldı.
- Tracked regression testi (ctest golden_) çalıştırıldı.
- `ast`/`symbols`/`exec` regresyon kontrolü yapıldı.
- Kanıt dosyaları yazıldı: `evidence/` altında provenance, fixture manifest, determinism, regression, 15x4 ham çıktı dosyası.

### 4. Çalıştırılan komutlar

```
cmake -S /home/saqut/Masaüstü/saqutcompiler -B /tmp/saqut-sq090-exitcode-validation-2DHSAm -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/saqut-sq090-exitcode-validation-2DHSAm
ctest --test-dir /tmp/saqut-sq090-exitcode-validation-2DHSAm -R '^golden_' --output-on-failure

saqut run/check/ir file:<fixture> x 12
saqut ast/symbols file:<fixture> x 2
saqut exec '1 + 1' x 1
Determinism tekrarları: run+syntax x3, run+runtime x3
```

### 5. DoD durumu

- **Tasarlandı:** √ (önceden kabul edilmiş decision.md)
- **Uygulandı:** Bu validation raporunun ölçüm konusu — testçi görüş bildirmez.
- **Test Edildi:** Bu rapor kanıt sağlar; nihai karar ürün sahibi/mimara aittir.
- **Release Edildi:** Bu task kapsamında değil.

### 6. Kanıtlanmayanlar

- `tests/run.sh` çalıştırılmadı (§7 gerekçesi: hardcoded SAQUT yolu).
- K-6 (eksik `main` senaryosu) ölçülmedi — `SQ-090-RUN-ENTRYPOINT` kapsamı.
- JIT, AOT, LSP, DAP, MIR veya optimizer davranışı test edilmedi.
- İnteraktif olmayan CLI invocation varyasyonları test edilmedi (yalnız `file:` prefix ile çalışıldı).

### 7. Riskler ve regresyon yüzeyi

- **Tracked regression riski:** Düşük — 85/85 golden test geçti.
- **stdout/stderr içerik değişikliği:** Yok — path normalizasyonu sonrası baseline ile byte-identical.
- **`ast`/`symbols`/`exec` regresyonu:** Yok — tüm çıktılar baseline ile eşleşiyor.
- **`.compile_error` fixture'ları:** 8/8 exit 65 üretiyor — eski 1'den 65'e değişim CTest'e şeffaf.
- **Bilinen sapma:** `exit_codes.hpp` yeni bir dosya olduğu için `git diff` onu göstermez; `git status --short` ile varlığı kanıtlanmıştır.
- **Yapısal not:** İzin verilen 4 dosya dışında `src/` altında başka hiçbir production-source değişikliği yoktur.

### 8. Sonraki yetkili rol

**Ağır Şüpheci Başmimar** — bu raporu inceleyerek DoD geçiş kararını vermelidir.
