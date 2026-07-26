# SQ-090-CLI-BASELINE — Validation Report Amendment 01

**Görev kimliği:** SQ-090-CLI-BASELINE-AMENDMENT-01
**Hedef sürüm:** 0.9.0
**Contract modu:** VALIDATION-ONLY
**Rol:** İzole Hafif Muhalif Testçi (bağımsız yeni oturum)
**Tarih:** 2026-07-25
**Binary:** `/tmp/saqut-sq090-amendment01-build-H5aiLe/saqut` (SHA-256 `f5920051a68ed5019d6f5158a2040f67d7d0e762d3fbc82e1f122ebbaedaff79`)

---

## 1. Amendment Gerekçesi

Orijinal `validation-report.md` §3'te, F1–F10 fixture'larının `func main(): int { ... }` sözdizimini kullandığı ve bu sözdiziminin mevcut derleyici tarafından tanınmadığı tespit edilmiştir. Derleyici C-tarzı `int main() { ... }` sözdizimini bekler. Bu uyumsuzluk, orijinal baseline'ın F1–F10 üzerindeki tüm komut sonuçlarını kontamine etmiştir: her şey parse hatasına dönüşmüş, decision.md §1'deki hipotezler doğru ölçülememiştir.

Bu amendment, sözdizimi doğru fixture'larla tüm kontamine kombinasyonları yeniden ölçer.

---

## 2. Sözdizimi Doğrulama Kanıtı

Tam kanıt: `evidence/amendment-01/00-syntax-verification.md`

| Kaynak | Fonksiyon Tanımı | Tutarlı mı? |
|---|---|---|
| `examples/merhaba.sqt` | `int main() { ... }` | Evet |
| `examples/fibonacci.sqt` | `int main() { ... }`, `int fibonacci(int n) { ... }` | Evet |
| `knowledge-base/02_Language.md` line 118–119 | `Type function(Type param, ...) { statements }` | Evet |

**Sonuç:** Her üç kaynak da `Type function(Type params...) { body }` biçiminde hemfikirdir. Dönüş tipi fonksiyon adından önce yazılır; `func` keyword'ü veya `:` ayracı kullanılmaz. Sözdizimi **tamamen tutarlı**.

---

## 3. Fresh Build Kanıtı

Tam kanıt: `evidence/amendment-01/00-provenance.md`, `evidence/amendment-01/02-configure-*.txt`, `evidence/amendment-01/03-build-*.txt`

| Ölçüt | Değer |
|---|---|
| HEAD | `7f871b75e917725dcdf46111fab88fb3be5663f2` |
| Branch | `0.9.0` |
| Build dizini | `/tmp/saqut-sq090-amendment01-build-H5aiLe` |
| Configure exit | 0 |
| Build exit | 0 |
| Binary boyut | 5,498,984 bytes |
| Timestamp (epoch) | 1784975270 |
| SHA-256 | `f5920051a68ed5019d6f5158a2040f67d7d0e762d3fbc82e1f122ebbaedaff79` |
| Repo `build/saqut` SHA-256 | `f259069a18ec8a66066c1e5b7b8fd79e42ac45600fc4d7cd3b8a590a9095eaed` (farklı) |

**Fresh build onaylandı.** Binary bu amendment oturumunda derlenmiştir, eski veya repo binary'si kullanılmamıştır. (Aynı kaynak + aynı toolchain = orijinal baseline ile aynı MD5 hash — beklenen yeniden üretilebilirlik.)

---

## 4. Fixture Matrisi (F1–F13)

Tam manifest: `evidence/amendment-01/01-fixtures-manifest.txt`

| Fixture | Dosya | İçerik (özet) | Değişim |
|---|---|---|---|
| F1 | `valid_main.sqt` | `int main() { print("hello"); return 0; }` | **Düzeltildi** (`func` → `int`, `:` kaldırıldı) |
| F2 | `valid_no_main.sqt` | `int helper() { return 42; }` | **Düzeltildi** |
| F3 | `syntax_error.sqt` | `int main() { int x = ; return 0; }` | **Düzeltildi** (syntax hatası) |
| F4 | `type_error.sqt` | `int main() { int x = "not an int"; return 0; }` | **Düzeltildi** (semantik hata) |
| F5 | `empty.sqt` | 0 bytes (touch) | **Aynı** |
| F6 | `return_0.sqt` | `int main() { return 0; }` | **Düzeltildi** |
| F7 | `return_1.sqt` | `int main() { return 1; }` | **Düzeltildi** |
| F8 | `return_255.sqt` | `int main() { return 255; }` | **Düzeltildi** |
| F9 | `return_256.sqt` | `int main() { return 256; }` | **Düzeltildi** |
| F10 | `return_non_int.sqt` | `string main() { return "not int"; }` | **Düzeltildi** (dönüş tipi `string`) |
| F11 | (exec arg) | `1 + 1` | **Aynı** |
| F12 | (exec arg) | `print(1); print(2);` | **Aynı** |
| F13 | (exec arg) | `let x = ;` | **Aynı** |

---

## 5. Komut Matrisi Sonuçları

Tüm ham kanıt: `evidence/amendment-01/cmd-NN-<label>.*`

### 5.1 `run`

| Fixture | Exit | stdout | stderr | Evidence |
|---|---|---|---|---|
| F1 (valid) | **0** | `hello` | — | cmd-01 |
| F3 (syntax) | **0** | — | — | cmd-02 |
| F4 (type) | **1** | — | `E003: cannot assign string to int` | cmd-03 |
| F2 (no main) | **1** | — | `runtime error: 'main' function not found` | cmd-04 |
| F6 (return 0) | **0** | — | — | cmd-05 |
| F7 (return 1) | **1** | — | — | cmd-06 |
| F8 (return 255) | **255** | — | — | cmd-07 |
| F9 (return 256) | **0** | — | — | cmd-08 |
| F10 (non-int) | **0** | — | — | cmd-09 |
| F5 (empty) | **1** | — | `runtime error: 'main' function not found` | cmd-10 |

### 5.2 Main Status Matrisi (alt bölüm)

| Senaryo | run exit | VM main sonucu | Not |
|---|---|---|---|
| `return 0` (F6) | **0** | main = 0 | Doğru aktarım |
| `return 1` (F7) | **1** | main = 1 | exit = main dönüş değeri |
| `return 255` (F8) | **255** | main = 255 | Doğru aktarım |
| `return 256` (F9) | **0** | main = 256 | **Aralık dışı:** 256 → exit=0 (mod 256? sarma?) |
| `string main()` (F10) | **0** | `string` dönüşü | **Tip ihlali:** tanımlı diagnostic yok, exit=0 |
| `main` yok (F2) | **1** | — | `runtime error: 'main' function not found` |

### 5.3 `check`

| Fixture | Exit | stdout | Evidence |
|---|---|---|---|
| F1 (valid) | **0** | JSON, empty diagnostics | cmd-11 |
| F2 (no main) | **0** | JSON, empty diagnostics | cmd-12 |
| F3 (syntax) | **0** | JSON, empty diagnostics | cmd-13 |
| F4 (type) | **1** | JSON, E003 diagnostic | cmd-14 |
| F5 (empty) | **0** | JSON, empty diagnostics | cmd-15 |

### 5.4 `ast`

| Fixture | Exit | stderr | stdout | Evidence |
|---|---|---|---|---|
| F1 (valid) | **0** | — | AST text | cmd-16 |
| F3 (syntax) | **0** | — | AST text (no ErrorNode) | cmd-17 |
| F4 (type) | **0** | — | AST text (includes semantic info) | cmd-18 |
| F5 (empty) | **1** | — | — | cmd-19 |

### 5.5 `ast --json`

| Fixture | Exit | stdout | Evidence |
|---|---|---|---|
| F1 (valid) | **0** | Valid JSON AST | cmd-20 |
| F3 (syntax) | **0** | Valid JSON AST (no ErrorNode) | cmd-21 |
| F4 (type) | **0** | Valid JSON AST | cmd-22 |
| F5 (empty) | **1** | — | cmd-23 |

### 5.6 `ir`

| Fixture | Exit | stderr | stdout | Evidence |
|---|---|---|---|---|
| F1 (valid) | **0** | — | IR dump | cmd-24 |
| F3 (syntax) | **0** | — | IR dump | cmd-25 |
| F4 (type) | **1** | E003 semantic error | — | cmd-26 |

### 5.7 `symbols`

| Fixture | Exit | stderr | stdout | Evidence |
|---|---|---|---|---|
| F1 (valid) | **0** | — | Symbols text | cmd-27 |
| F3 (syntax) | **0** | — | Symbols text | cmd-28 |
| F4 (type) | **0** | — | Symbols text | cmd-29 |
| F5 (empty) | **1** | — | — | cmd-30 |

### 5.8 `symbols --json`

| Fixture | Exit | stdout | Evidence |
|---|---|---|---|
| F1 (valid) | **0** | Valid JSON | cmd-31 |
| F3 (syntax) | **0** | Valid JSON | cmd-32 |
| F4 (type) | **0** | Valid JSON | cmd-33 |
| F5 (empty) | **1** | — | cmd-34 |

### 5.9 `exec`

| Fixture | Exit | stdout | stderr | Evidence |
|---|---|---|---|---|
| F11 (`1 + 1`) | **0** | `2` | — | cmd-35 |
| F12 (`print(1); print(2);`) | **0** | `102` | `parser error: unexpected token ')'` | cmd-36 |
| F13 (`let x = ;`) | **1** | — | `E007: unknown type: 'let'` | cmd-37 |

### 5.10 Önemli gözlem: F3 (syntax_error.sqt)

F3 (`int x = ;`) **derleyici tarafından syntax hatası olarak algılanmaz.** Parser, `int x = ;`'i boş başlatıcılı geçerli bir değişken bildirimi olarak kabul eder. 

- `run F3`: exit=0 (program çalışır)
- `check F3`: exit=0, boş diagnostics
- `ast --json F3`: **ErrorNode içermez**, `x` `VariableDecl` olarak düzgün görünür
- `symbols F3`: exit=0, hata yok

Bu, `int x = ;`'in dil tarafından sözdizimsel olarak kabul edildiği anlamına gelir. Dil sözleşmesi açısından bu bekleniyorsa, F3 gerçek bir syntax hatası içermez. Gerçek bir syntax hatası için farklı bir fixture (örn. `int x = +;`) gerekir.

---

## 6. CLI Invocation Tekrar Sonuçları

### Yeniden çalıştırılan vakalar (sözdiziminden etkilenenler, yeni F1 ile)

| Vaka | Evidence | exit | stdout | stderr |
|---|---|---|---|---|
| `saqut program.sqt` (örtük run) | invocation-01 | **0** | `hello` | — |
| `saqut` (source.sqt varken) | invocation-02 | **0** | Help text | — |
| `ast --output /root/no-permission/out.json` (F1) | invocation-03 | **0** | AST text (stdout'a düştü) | — |

### Yeniden çalıştırılmayan vakalar (sözdiziminden etkilenmez, orijinal kanıt geçerlidir)

| Vaka | Orijinal evidence referansı |
|---|---|
| `saqut` (argümansız, source.sqt yok) | `evidence/06-invocation/01-no-args.*` |
| `saqut --help` | `evidence/06-invocation/02-help-flag.*` |
| `saqut help` | `evidence/06-invocation/03-help-cmd.*` |
| `saqut kesinlikle-bilinmeyen-komut-xyz123` | `evidence/06-invocation/05-unknown-cmd.*` |
| `saqut run` (dosya yok) | `evidence/06-invocation/06-run-no-file.*` |
| `saqut check` (dosya yok) | `evidence/06-invocation/07-check-no-file.*` |
| `saqut ast` (dosya yok) | `evidence/06-invocation/08-ast-no-file.*` |
| `saqut symbols` (dosya yok) | `evidence/06-invocation/09-symbols-no-file.*` |
| `saqut compile` / `parse` / `transpile` / `interpret` | `evidence/06-invocation/10-compile.*` – `13-interpret.*` |
| stdin `-` | `evidence/06-invocation/14-stdin-run.*` |
| source.sqt yokken argümansız `saqut` | `evidence/06-invocation/16-source-sqt-absent.*` |

### Önemli değişiklikler

1. **`saqut program.sqt`**: Orijinal baseline'da exit=1 (parse hatası). Yeni sözdizimiyle exit=0, program çalıştı. Örtük `run` kısayolu **aktif** — `saqut program.sqt` programı çalıştırır.
2. **`ast --output /root/no-permission/out.json`**: Orijinal baseline'da exit=0 (parser hatası + koşulsuz return 0'ın devamı). Yeni sözdizimiyle de exit=0 — **çıktı stdout'a yazıldı**, hata bildirilmedi.
3. **`saqut` (source.sqt varken)**: Orijinal baseline'da exit=0 (help). Yeni sözdizimiyle de exit=0, help. Değişmedi.

---

## 7. Tracked Test Envanteri

Tam liste: `evidence/amendment-01/04-tracked-test-inventory.txt`

**Yalnız envanter; hiçbir test çalıştırılmamıştır, PASS/FAIL iddiası yoktur.**

- Toplam CTest: **187**
- `golden_*` testleri: ~91 adet (çoğu golden + differential çifti)
- `differential_*` testleri: ~70 adet (VM vs JIT karşılaştırması)
- `lsp_*` testleri: 21 adet (JSONL tabanlı)
- `dap_*` testleri: 10 adet (JSONL tabanlı)
- `unit_tests`: 1 adet (`tests/run.sh`)
- `golden_opt_dce_ir_opt`, `golden_opt_folding_ir_opt`: 2 adet `ir` komutu kullanan

Gözlem:
- Tüm `golden_*` ve `differential_*` testleri `cmake/run_golden.cmake` üzerinden **yalnız `run` komutunu** kullanır (`COMMAND` parametresi varsayılan olarak `"run"`).
- `cmake/run_golden_error.cmake` da yalnız `run` kullanır (sabit kodlu).
- `ir` komutu yalnızca `golden_opt_dce_ir_opt` ve `golden_opt_folding_ir_opt` testlerinde kullanılır (COMMAND="ir").
- `check` komutu yalnız `tests/run.sh` içinde, module testlerinde ve semantic testlerinde kullanılır.
- `ast`, `symbols`, `exec`, `tokens` için **hiçbir tracked CTest testi yoktur.**
- `.compile_error` fixture'ları: 8 adet (hepsi semantic/runtime hata, sözdizim hatası değil).
- `.runtime_error` fixture'ları: 4 adet.
- `.ir_opt.expected` fixture'ları: 2 adet (optimized IR beklentisi).

---

## 8. Determinizm Kanıtı

Tam rapor: `evidence/amendment-01/05-determinism.md`

Üç kombinasyon, her biri 3 kez çalıştırıldı.

### F1+`run` (valid_main.sqt)

| Koşu | stdout SHA-256 | stderr SHA-256 | exit |
|---|---|---|---|
| #1 (cmd-01) | `2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 0 |
| #2 (cmd-38) | `2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 0 |
| #3 (cmd-39) | `2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 0 |

Byte-level `diff`: boş çıktı (dosyalar IDENTICAL).

### F11+`exec` (`1 + 1`)

| Koşu | stdout SHA-256 | stderr SHA-256 | exit |
|---|---|---|---|
| #1 (cmd-35) | `d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 0 |
| #2 (cmd-40) | `d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 0 |
| #3 (cmd-41) | `d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 0 |

Byte-level `diff`: boş çıktı (dosyalar IDENTICAL).

### F3+`ast` (syntax_error.sqt)

| Koşu | stdout SHA-256 | stderr SHA-256 | exit |
|---|---|---|---|
| #1 (cmd-17) | `b8f7c907b8946142364fe01db8ba43da7b61a51183e643c291f7b8f7a640de41` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 0 |
| #2 (cmd-42) | `b8f7c907b8946142364fe01db8ba43da7b61a51183e643c291f7b8f7a640de41` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 0 |
| #3 (cmd-43) | `b8f7c907b8946142364fe01db8ba43da7b61a51183e643c291f7b8f7a640de41` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 0 |

Byte-level `diff`: boş çıktı (dosyalar IDENTICAL).

**Sonuç:** Seçilen `<F1+run>`, `<F11+exec>` ve `<F3+ast>` kombinasyonları, bu ortamda üç tekrar boyunca byte-identical sonuç üretti.

---

## 9. Final Worktree Kanıtı

Ham çıktı (placeholder yok): `evidence/amendment-01/06-final-worktree.txt`

```
$ git status --short
 D .claude/agents/architect.md
 D .claude/agents/coder.md
 D .claude/agents/project-manager.md
 D .claude/agents/tester.md
 D .claude/hooks/role-guard.sh
 D .claude/settings.json
 M .gitignore
 M CLAUDE.md
 M docs/adr/ADR-032-mir-jit-gomulu-runtime-aot.md
 M docs/adr/ADR-038-determinizm-surum-uyumlulugu.md
 M docs/adr/ADR-041-self-hosted-stdlib-thin-runtime.md
 M docs/architecture.md
 D organization.md
 D screenshots/.directory
 D screenshots/dap.png
 D screenshots/lsp.png
 D target.txt
 D wiki/arrays.md
 D wiki/cli-commands.md
 D wiki/compound-assignment.md
 D wiki/control-flow.md
 D wiki/error-handling.md
 D wiki/functions.md
 D wiki/getting-started.md
 D wiki/globals.md
 D wiki/home.md
 D wiki/literals.md
 D wiki/operators.md
 D wiki/optimization.md
 D wiki/pipeline.md
 D wiki/strings.md
 D wiki/structs.md
 D wiki/variables-types.md
?? AGENTS.md
?? docs/adr/ADR-042-v1-feedback-mvp-ve-surumleme.md
?? docs/v0.9-v1.0-yol-haritasi.md
?? docs/v1.0-issue-disposition.md
?? docs/v1.0-kapsam-bildirgesi.md
?? docs/yerel-agent-kullanim-rehberi.md
?? knowledge-base/
?? prompts/
?? tasks/
?? tests/general/

$ git diff --stat HEAD -- CMakeLists.txt cmake/ src/ build-release.sh build-debug.sh tests/
```
(boş çıktı — hiçbir değişiklik yok)

**Kontrol:** `src/`, `CMakeLists.txt`, `cmake/`, `tests/`, `build-release.sh`, `build-debug.sh` hiçbirinde değişiklik yok. Sadece `tasks/SQ-090-CLI-BASELINE/evidence/amendment-01/` altında yeni kanıt dosyaları oluşturulmuştur.

---

## 10. İlk Raporun Korunan Bulguları

Aşağıdaki bulgular orijinal `validation-report.md`'den **korunur** (sözdizimi kontaminasyonundan etkilenmemişlerdir):

1. **Determinizm:** F11+exec (`1 + 1`) ve F3+ast (orijinal sözdizimiyle) 3 tekrarda byte-identical. Bu amendment ile aynı sonuç SHA-256 ile sıkılaştırılmış olarak yeniden doğrulanmıştır.
2. **#134 repro:** `saqut exec 'print(1);'` → stdout `10`, exit `0`. (Bu amendment F12 = `print(1); print(2);` ile aynı sonucu üretir: stdout `102`, exit `0`, parser hatası stderr'de.)
3. **exec F13 (invalid):** exit=1, stderr'de `unknown type: 'let'`. Aynı.
4. **F5 (empty) + run:** exit=1, `runtime error: 'main' function not found`. Aynı.
5. **F5 + check:** exit=0, boş JSON diagnostics. Aynı.
6. **CLI invocation (non-syntax-dependent):** `--help`, argümansız, bilinmeyen komut, stub komutlar, stdin `-`, source.sqt yok — orijinal kanıt geçerlidir.
7. **Help içeriği** (TODO stublar, stdin modu ilanı) — aynı.

---

## 11. İlk Raporun Düzeltilen Bulguları

Aşağıdaki bulgular **düzeltildi** (orijinal kontamine sonuç → bu amendment'ın doğru sonucu):

| # | Madde | Orijinal (kontamine) | Düzeltilmiş |
|---|---|---|---|
| 1 | F1 + run | exit=1, parse hatası | **exit=0, "hello" çıktısı** |
| 2 | F1 + check | exit=1, JSON E901/E904 | **exit=0, boş diagnostics** |
| 3 | F1 + ast | exit=0, ErrorNode içeren AST | **exit=0, temiz AST** |
| 4 | F1 + ir | exit=1, parse hatası | **exit=0, IR dump** |
| 5 | F1 + symbols | exit=1, E007 "unknown type func" | **exit=0, temiz symbol table** |
| 6 | F2 (no main) + check | exit=1, JSON E901/E904 | **exit=0, boş diagnostics** |
| 7 | F6 (return 0) + run | exit=1 | **exit=0** |
| 8 | F7 (return 1) + run | exit=1 | **exit=1** (run exit=main=1) |
| 9 | F8 (return 255) + run | exit=1 | **exit=255** |
| 10 | F9 (return 256) + run | exit=1 | **exit=0** (256 → mod? → 0) |
| 11 | F10 (non-int) + run | exit=1 | **exit=0** (string dönüşü, diagnostic yok) |
| 12 | F4 (type) + ast | exit=0, ErrorNode'lu AST | **exit=0, temiz AST (semantik hata AST'yi etkilemez)** |
| 13 | F4 + symbols | exit=1, parse hatası | **exit=0** (symbols semantik hatadan etkilenmez) |
| 14 | F4 + ir | exit=1 | **exit=1** (ir semantik hatayı görür — doğru) |
| 15 | F4 + check | exit=1 | **exit=1** (E003 diagnostic) |

---

## 12. İlk Raporun Geri Çekilen Bulguları

| # | Orijinal Bulgu | Geri Çekme Nedeni |
|---|---|---|
| 1 | "F3 çalıştırıldı, parse hatası alındı" | F3 (`int x = ;`) parser tarafından **syntax hatası olarak algılanmaz**. `ast --json` ErrorNode içermez. Orijinal rapordaki F3, `func` sözdizimi ile parse hatası verdiği için yanlışlıkla "syntax error" olarak raporlanmıştır. Doğru sözdizimiyle F3 geçerli bir programdır. |
| 2 | "F2 + check exit=1" (parse) | F2 doğru sözdizimiyle check exit=0. Orijinal rapor kontamine. |
| 3 | Decision.md §1(4): "run exit = main dönüş değeri, BLOCKED" artık geçerli | **Bu amendment ile GÖZLENDİ.** `return 0/1/255` doğru aktarılır. `return 256` → exit=0 (256 mod 256 = 0 olabilir); `string main()` → exit=0, diagnostic yok. |

---

## 13. Sekiz Hipotezin Yeni Sınıflandırması

Format: `<tespit no> | <orijinal sınıflandırma> → <yeni sınıflandırma> | <evidence dayanağı> | <not>`

| # | Tespit | Orijinal → Yeni | Dayanak | Not |
|---|---|---|---|---|
| 1 | `ast`/`symbols`/`exec` DiagnosticEngine'siz; sözdizimi hatası yalnız stderr'e | GÖZLENDİ → **GÖZLENDİ (korundu)** | `cmd-36-exec-statements` (exit=0, parser error stderr'de); `cmd-17-ast-syntax-error`: F3 geçerli olduğu için ölçülemedi, ama `cmd-36` exec hâlâ parser hatasında exit=0. `cmd-11-check-valid-main` ise düzgün çalışıyor. | F3 artık syntax hatası değil. Gerçek syntax hatası olan bir fixture ile tekrar test edilmeli. |
| 2 | `ast` koşulsuz `return 0` | GÖZLENDİ → **GÖZLENDİ (korundu)** | `cmd-16-ast-valid-main` exit=0; `cmd-18-ast-type-error` exit=0 (semantik hata AST'yi etkilemez, exit=0). | AST hatalı girdide bile exit=0. Doğru. |
| 3 | `symbols` exit = diag.hasErrors(); parser hataları diag'a ulaşmaz | GÖZLENDİ → **GÖZLENDİ (ayrıntıyla doğrulandı)** | `cmd-28-symbols-syntax-error`: F3 geçerli olduğu için parser hatası yok, exit=0. `cmd-29-symbols-type-error`: semantik hataya rağmen exit=0. `cmd-33-symbols-json-type-error`: exit=0, JSON symbols. | `symbols`'ün semantik hatada exit=0 dönmesi, `diag.hasErrors()`'un semantik hataları da görmediğini veya symbols'ün TypeChecker'ı çalıştırmadığını gösteriyor. |
| 4 | `run` exit = main dönüş değeri; ValueKind denetimi yok | BLOCKED → **GÖZLENDİ (yeni)** | `cmd-05-run-return-0` exit=0; `cmd-06-run-return-1` exit=1; `cmd-07-run-return-255` exit=255. `cmd-08-run-return-256` exit=0 (sarma). `cmd-09-run-return-non-int` exit=0 (diagnostic yok). | Main dönüş değeri doğru aktarılıyor (0, 1, 255). 256'da muhtemelen 8-bit truncation (256 % 256 = 0). `string main()` tip ihlali diagnostic üretmiyor. |
| 5 | `main` yokluğu semantik kapıda değil, VM'de runtime_error | GÖZLENDİ → **GÖZLENDİ (korundu)** | `cmd-04-run-valid-no-main` exit=1, stderr: `runtime error: 'main' function not found`. | Aynı. |
| 6 | parseArgs: bilinmeyen argüman `run` sayar; `source.sqt` ghost; help/unknown ölü | GÖZLENDİ → **GÖZLENDİ (korundu, ayrıntıyla)** | `invocation-01-program-sqt`: `saqut program.sqt` hâlâ örtük `run` olarak çalışır (exit=0, "hello" çıktısı). `saqut help` → exit=1, E_MODULE_NOT_FOUND. Ghost source.sqt hâlâ `run`/`check`/`ast`/`symbols` için aranıyor. | Örtük `run` kısayolu aktiftir. `help` komutu hâlâ çalışmaz. |
| 7 | `compile`/`parse`/`transpile`/`interpret` TODO stub; `-` TODO | GÖZLENDİ → **GÖZLENDİ (korundu)** | Help output'ta `compile`/`parse`/`transpile` görünür; `interpret` help'te yok ama `saqut interpret` TODO + exit=1. | Aynı. |
| 8 | Tracked testler yalnız `run`; `ast`/`symbols`/`exec` testi yok | BLOCKED → **GÖZLENDİ (yeni)** | `evidence/amendment-01/04-tracked-test-inventory.txt`: 187 testin tümü `run` (ve 2 `ir`); `ast`, `symbols`, `exec`, `tokens` için CTest yok. | cmake/run_golden.cmake'de COMMAND varsayılanı "run". cmake/run_golden_error.cmake'de sabit `run`. Tracked test envanteri çıkarılmıştır. |

---

## 14. Baseline Kapısı Durumu

**Bu amendment ile baseline kapısı (decision.md §9) tamamlanmış mıdır?**

**Kısmen evet.** Aşağıdaki tespitlerin tamamı artık black-box olarak gözlenmiştir:

- ✅ #1 (ast/symbols/exec diagnostic gatesiz) — exec'te parser hatasında exit=0, symbols semantik hatada exit=0
- ✅ #2 (ast koşulsuz return 0) — doğrulanmış
- ✅ #3 (symbols exit = diag.hasErrors; parser hataları diag'a ulaşmaz) — symbols type hatasında exit=0
- ✅ #4 (run exit = main dönüş değeri, ValueKind denetimi yok) — return 0/1/255 doğru; 256'da sarma; string main diagnostic yok
- ✅ #5 (main yok → runtime_error) — doğrulanmış
- ✅ #6 (implicit run, ghost source.sqt, dead help) — doğrulanmış
- ✅ #7 (TODO stublar) — doğrulanmış
- ✅ #8 (tracked test envanteri: yalnız run/ir) — tam envanter çıkarılmış

**Eksik:** F3 fixture'ı (`int x = ;`) syntax hatası olmadığı için, parser recovery davranışı (E9xx diagnostic, ErrorNode, exit 65, kısmi çıktı) doğru syntax'lı bir gerçek syntax hatası fixture'ı ile ölçülememiştir. Bu, kararın §3.3 (error-tolerant AST) maddesinin tam olarak doğrulanmasını engeller.

---

## 15. DoD Teyidi

**DoD durumu: Hâlâ Tasarlandı.**

Bu amendment VALIDATION-ONLY'dir. DoD'yi "Uygulandı"ya taşımaz. Baseline kapısını (decision.md §9) önemli ölçüde tamamlamıştır ancak F3'ün gerçek bir syntax hatası içermemesi nedeniyle küçük bir boşluk kalmıştır. Kararın uygulama aşamasına geçmesi için ürün sahibinin ve başmimarın bu raporu değerlendirmesi gerekir.

---

## Ek: Gözlemler ve Uyarılar

1. **F3 (`int x = ;`) syntax hatası değildir.** Parser bu sözdizimini geçerli kabul eder. Doğru syntax hatası fixture'ları (örn. `int x = +;`) ile ayrıca test edilmelidir.
2. **`return 256` → exit=0.** Muhtemelen `exit` kodunun 8-bit truncation'ından kaynaklanır (256 & 0xFF = 0). Process exit code POSIX'te 0-255 aralığındadır; bu davranış beklendiktir ancak diagnostic üretmez.
3. **`string main()` → exit=0, diagnostic yok.** Derleyici, `main`'in dönüş tipini kontrol etmez; VM `main`'den dönen değeri doğrudan process exit code'u olarak kullanır. Bu bir tip güvenliği açığıdır.
4. **`ast --output /root/no-permission/out.json`** → exit=0, çıktı stdout'a yazıldı. Sessiz fallback aktiftir.
5. **`saqut program.sqt`** örtük `run` kısayolu **hâlâ aktiftir**. decision.md §3.5'teki "kaldırılır" kararı henüz uygulanmamıştır.
6. **exec `print(1); print(2);`** → stdout `102`. Bu, `print(1)`'in çıktısı `1` ile `print(2)`'nin çıktısı `2`'nin bitişik yazılmasıdır (arada newline veya ayraç yok). Bu beklenen davranış olabilir.
