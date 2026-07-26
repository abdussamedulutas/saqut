# SQ-090-EXIT-CODE-CONTRACT — Implementation Report

**Görev kimliği:** SQ-090-EXIT-CODE-CONTRACT
**Hedef sürüm:** 0.9.0
**Rol:** Hafif Model Uygulayıcı
**Uygulanan contract:** `tasks/SQ-090-EXIT-CODE-CONTRACT/implementation-contract.md` (Revision: Amendment 02)

Bu rapor yalnız implementation-contract.md'yi birebir uyguladığımı belgeler.
Validation-contract.md, önceki tester raporları veya başka ajanların
reasoning/özetleri okunmadı; bağımsız testçi ayrı bir oturumda çalışacak.

---

## 1. Provenance

**Başlangıç:**
- `git rev-parse HEAD`: `7f871b75e917725dcdf46111fab88fb3be5663f2` (dal `0.9.0`)
- `git status --short`: contract'ta belirtilen dirty worktree (kullanıcıya ait,
  `.claude/` silinen agent dosyaları, `wiki/`, `screenshots/`, değiştirilmiş
  ADR'ler, yeni `AGENTS.md`/`knowledge-base/`/`docs/`/`prompts/`/`tasks/`)
  görev başlangıcında değişmeden gözlendi — bunlara dokunulmadı.
- İzin verilen 4 dosyanın başlangıç durumu (`run.hpp`, `check.hpp`, `ir.hpp`)
  contract'ın §2 "Kaynak baseline" bölümündeki satır referanslarıyla
  birebir uyuştuğu doğrudan okuyarak teyit edildi (tüm `return 1;`/
  `exitCode = 1;` satırları contract'ın belirttiği tam konumlarda bulundu).

**Sonuç (bu rapor yazılırken):**
- `git rev-parse HEAD`: değişmedi, hâlâ `7f871b75e917725dcdf46111fab88fb3be5663f2`
  (commit yapılmadı).
- Değiştirilen dosyalar: aşağıda §2.

---

## 2. Değiştirilen exact dosyalar ve diff özeti

- `src/cli/exit_codes.hpp` — **yeni oluşturuldu**, içerik contract §4.1 ile
  birebir aynı (`kSuccess=0`, `kUsageError=64`, `kDataError=65`,
  `kSoftwareError=70`).
- `src/cli/commands/run.hpp` — `#include "cli/exit_codes.hpp"` eklendi;
  3 adet `return 1;` (module load / symbol collect / typecheck hata dalları)
  → `return saqut::exit_code::kDataError;`; catch bloğundaki
  `exitCode = 1;` → `exitCode = saqut::exit_code::kSoftwareError;`.
  `filePath.empty()` dalı (satır 28) ve `--jit` yolundaki `return 1;`
  (satır ~130) **dokunulmadı**.
- `src/cli/commands/check.hpp` — `#include` eklendi; tek `return
  diag.hasErrors() ? 1 : 0;` → `return diag.hasErrors() ?
  saqut::exit_code::kDataError : saqut::exit_code::kSuccess;`.
  `filePath.empty()` dalı dokunulmadı.
- `src/cli/commands/ir.hpp` — `#include` eklendi; tek `return 1;`
  (diagnostic hata dalı) → `return saqut::exit_code::kDataError;`.
  `filePath.empty()` dalı ve başarı yolundaki `return 0;` (dump sonrası)
  **dokunulmadı**.

`git diff --stat HEAD -- src/cli/exit_codes.hpp src/cli/commands/run.hpp src/cli/commands/check.hpp src/cli/commands/ir.hpp`:
```
 src/cli/commands/check.hpp | 3 ++-
 src/cli/commands/ir.hpp    | 3 ++-
 src/cli/commands/run.hpp   | 9 +++++----
 3 files changed, 9 insertions(+), 6 deletions(-)
```
(`exit_codes.hpp` untracked yeni dosya olduğu için `--stat`'te görünmez;
varlığı `git status --short` ile aşağıda §7'de teyit edilmiştir.)

Tam diff (3 tracked dosya):
```diff
diff --git c/src/cli/commands/check.hpp w/src/cli/commands/check.hpp
index 1b98bc4..e6fe1bb 100644
--- c/src/cli/commands/check.hpp
+++ w/src/cli/commands/check.hpp
@@ -9,6 +9,7 @@
 
 #include <iostream>
 #include "cli/args.hpp"
+#include "cli/exit_codes.hpp"
 #include "module/module_loader.hpp"
 #include "symbol/symbol_table.hpp"
 #include "symbol/symbol_collector.hpp"
@@ -41,7 +42,7 @@ inline int cmdCheck(const CliArgs& args) {
     out["diagnostics"] = diag.toJsonObj();
     std::cout << (args.compact ? out.dump() : out.dump(2)) << "\n";
 
-    return diag.hasErrors() ? 1 : 0;
+    return diag.hasErrors() ? saqut::exit_code::kDataError : saqut::exit_code::kSuccess;
 }
 
 #endif // SAQUT_CLI_CHECK
diff --git c/src/cli/commands/ir.hpp w/src/cli/commands/ir.hpp
index 6a373dc..9532209 100644
--- c/src/cli/commands/ir.hpp
+++ w/src/cli/commands/ir.hpp
@@ -8,6 +8,7 @@
 #include <iostream>
 #include <set>
 #include "cli/args.hpp"
+#include "cli/exit_codes.hpp"
 #include "core/capability.hpp"
 #include "module/module_loader.hpp"
 #include "symbol/symbol_table.hpp"
@@ -37,7 +38,7 @@ inline int cmdIr(const CliArgs& args) {
 
     if (diag.hasErrors()) {
         diag.printAll(std::cerr);
-        return 1;
+        return saqut::exit_code::kDataError;
     }
 
     if (args.optimized) {
diff --git c/src/cli/commands/run.hpp w/src/cli/commands/run.hpp
index d1ee9ca..414f043 100644
--- c/src/cli/commands/run.hpp
+++ w/src/cli/commands/run.hpp
@@ -9,6 +9,7 @@
 
 #include <iostream>
 #include "cli/args.hpp"
+#include "cli/exit_codes.hpp"
 #include "module/module_loader.hpp"
 #include "symbol/symbol_table.hpp"
 #include "symbol/symbol_collector.hpp"
@@ -47,7 +48,7 @@ inline int cmdRun(const CliArgs& args) {
 
     if (diag.hasErrors()) {
         diag.printAll(std::cerr);
-        return 1;
+        return saqut::exit_code::kDataError;
     }
 
     // ── Aşama 2: 3-geçiş sembol toplama + import doğrulama ───────────────
@@ -58,7 +59,7 @@ inline int cmdRun(const CliArgs& args) {
     if (diag.hasErrors()) {
         std::cerr << "compilation errors, cannot run program:\n";
         diag.printAll(std::cerr);
-        return 1;
+        return saqut::exit_code::kDataError;
     }
 
     // ── Aşama 3: Tip denetimi + yapısal doğrulama ─────────────────────────
@@ -70,7 +71,7 @@ inline int cmdRun(const CliArgs& args) {
     if (diag.hasErrors()) {
         std::cerr << "compilation errors, cannot run program:\n";
         diag.printAll(std::cerr);
-        return 1;
+        return saqut::exit_code::kDataError;
     }
 
     // ── Aşama 4 (opsiyonel): Optimizasyon ────────────────────────────────
@@ -151,7 +152,7 @@ inline int cmdRun(const CliArgs& args) {
                       << " live=" << vm.heapAllocCount() << "\n";
     } catch (const std::exception& e) {
         std::cerr << "runtime error: " << e.what() << "\n";
-        exitCode = 1;
+        exitCode = saqut::exit_code::kSoftwareError;
     }
 
     if (args.profile) stageTimer.printReport(std::cerr);
```

---

## 3. Invariant gözlemleri (kendi build'im — "Test Edildi" iddiası yapılmaz)

Kullanılan kök: `REPO_ROOT=/home/saqut/Masaüstü/saqutcompiler`
Build dizini: `BUILD_DIR=/tmp/saqut-sq090-exitcode-build-6sOvpj`

**INV-1 (syntax hatası, run/check/ir).**
Fixture: `$REPO_ROOT/tests/lsp/fixtures/syntax_error_recovery.sqt`
- `run`: exit **65** (gözlendi). stderr: `.../syntax_error_recovery.sqt:2:5: error [E901]: unexpected token ')' — expected a statement` / `— 1 error(s), 0 warning(s)`.
- `check`: exit **65** (gözlendi). stdout JSON diagnostic alanı dolu (`diagnostics.diagnostics[...]`).
- `ir`: exit **65** (gözlendi). stderr aynı E901 metni.
Kendi gözlemim: karşılandı.

**INV-2 (semantic hata — `longint_narrowing.sqt`).**
Fixture: `$REPO_ROOT/tests/golden/numeric/longint_narrowing.sqt`
- `run`: exit **65** (gözlendi). stderr: `compilation errors, cannot run program:` + `...longint_narrowing.sqt:4:5: error [E003]: 'i': longint → int requires explicit cast (...)`.
- `check`: exit **65** (gözlendi).
- `ir`: exit **65** (gözlendi). stderr aynı E003 metni.
Kendi gözlemim: karşılandı.

**INV-3 (runtime error, yalnız `run` — `mod_by_zero.sqt`).**
Fixture: `$REPO_ROOT/tests/golden/arithmetic/mod_by_zero.sqt`
- `run`: exit **70** (gözlendi). stderr: `runtime error: sıfıra bölme (mod)`.
- Beklenen dayanak `tests/golden/arithmetic/mod_by_zero.runtime_error` regex
  içeriği (`sıfıra bölme \(mod\)`) ile eşleşiyor.
Kendi gözlemim: karşılandı. Eksik `main` senaryosu bu invariant'ın parçası
olarak **kullanılmadı** (contract §5 INV-3 gereği).

**INV-4 (main status aktarımı korunur).**
- `int main() { return 0; }` → exit **0** (gözlendi)
- `int main() { return 1; }` → exit **1** (gözlendi)
- `int main() { return 255; }` → exit **255** (gözlendi)
Kendi gözlemim: karşılandı, regresyon yok — `exitCode = vm.run();` satırına
dokunulmadı.

**INV-5 (başarı yolu değişmez — `examples/fibonacci.sqt`).**
- `check`: exit **0** (gözlendi)
- `ir`: exit **0** (gözlendi), IR dump stdout'ta üretildi
Kendi gözlemim: karşılandı.

**INV-6 (`ast`/`symbols`/`exec` dokunulmadı).**
`ast.hpp`, `symbols.hpp`, `exec.hpp` dosyalarına hiç dokunulmadı — bu
otomatik olarak sağlanır (bkz. §7 dosya dokunma kanıtı, bu dosyalar
git status'ta hiç görünmüyor). Ayrıca çalıştırıp doğrulama yapılmadı
(contract gereği zorunlu değil).

---

## 4. Build komutları (izole `/tmp`, exact)

```
BUILD_DIR=$(mktemp -d /tmp/saqut-sq090-exitcode-build-XXXXXX)
# → /tmp/saqut-sq090-exitcode-build-6sOvpj
cmake -S "/home/saqut/Masaüstü/saqutcompiler" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR"
```
- `cmake` configure exit: 0
- `cmake --build` exit: 0 (yalnız önceden var olan, bu task'ın dokunmadığı
  dosyalardan kaynaklanan derleyici uyarıları var — `mir.c` clobbered
  değişken uyarıları, `type_checker.cpp` kullanılmayan değişken uyarıları;
  hata yok)
- Repo kökündeki `build/` **kullanılmadı**.

---

## 5. Hedefli komutların stdout/stderr/exit sonuçları (özet tablo)

| # | Komut | Fixture | exit | stdout (özet) | stderr (özet) |
|---|---|---|---|---|---|
| 1 | `run` | syntax_error_recovery.sqt | 65 | (boş) | E901, 1 error |
| 2 | `check` | syntax_error_recovery.sqt | 65 | JSON diagnostics dolu | (boş) |
| 3 | `ir` | syntax_error_recovery.sqt | 65 | (boş) | E901, 1 error |
| 4 | `run` | longint_narrowing.sqt | 65 | (boş) | "compilation errors..." + E003 |
| 5 | `check` | longint_narrowing.sqt | 65 | JSON diagnostics dolu | (boş) |
| 6 | `ir` | longint_narrowing.sqt | 65 | (boş) | E003 |
| 7 | `run` | mod_by_zero.sqt | 70 | (boş) | "runtime error: sıfıra bölme (mod)" |
| 8 | `run` | `int main(){return 0;}` | 0 | (boş) | (boş) |
| 9 | `run` | `int main(){return 1;}` | 1 | (boş) | (boş) |
| 10 | `run` | `int main(){return 255;}` | 255 | (boş) | (boş) |
| 11 | `check` | examples/fibonacci.sqt | 0 | JSON, diagnostics boş | (boş) |
| 12 | `ir` | examples/fibonacci.sqt | 0 | IR dump | (boş) |

Ham dosyalar `$BUILD_DIR/evidence/{01..12}-*.{command,stdout,stderr,exit}.txt`
altında saklandı (bu dosyalar `/tmp` altındadır, repo'ya commit edilmedi).

---

## 6. Golden ctest sonucu

```
ctest --test-dir "$BUILD_DIR" -R '^golden_' --output-on-failure
```
Sonuç: **100% tests passed, 0 tests failed out of 85** (Total Test time: 0.68 sec).
`.compile_error` fixture'ları dahil hiçbir golden test kırılmadı — bu,
`cmake/run_golden_error.cmake`'in yalnız `EXIT_CODE EQUAL 0` kontrolü
yaptığı (spesifik `1` değerine bağlı olmadığı) değerlendirmesiyle tutarlı.

`tests/run.sh` **çalıştırılmadı** — Başmimar açıklaması ve contract §9b
gereği: script `SAQUT="$ROOT/build/saqut"` hardcode eder, izole `/tmp`
fresh binary ile çalıştırılamaz, stale binary kanıtı üretir.

---

## 7. Kapsam dışı source dosyalarına dokunulmadığının kanıtı

```
$ git status --short -- src/
 M src/cli/commands/check.hpp
 M src/cli/commands/ir.hpp
 M src/cli/commands/run.hpp
?? src/cli/exit_codes.hpp
```

`src/` altında bu 4 dosya dışında **hiçbir değişiklik yok**. `ast.hpp`,
`symbols.hpp`, `exec.hpp`, `args.hpp`, `cli.hpp`, `main.cpp` ve tüm diğer
kaynak dosyaları listede görünmüyor — dokunulmadı.

Görev dışı, kullanıcıya ait dirty worktree değişiklikleri
(`.gitignore`, `CLAUDE.md`, ADR'ler, `docs/`, vb.) görev başlangıcındaki
haliyle korunmuştur; bu rapor bunlara dokunmadı.

`tests/`, `cmake/` altında hiçbir değişiklik yok (kontrol edilmedi çünkü
bu dosyalara hiç yazma işlemi yapılmadı).

---

## 8. Blocker ve bilinen geçici eksik-main sapması

**Blocker: yok.** Contract §12'deki hiçbir durma koşulu tetiklenmedi.

**Bilinen geçici ürün sözleşmesi sapması (kabul kriteri değil):**
Bu task'ın `run.hpp`'deki tek `catch (const std::exception&)` bloğunu 70'e
taşıması nedeniyle, eksik `main` fonksiyonu içeren bir kaynak da (örn.
`runtime error: 'main' function not found`) geçici olarak exit **70**
üretecektir. Bu **test edilmedi ve fixture olarak kullanılmadı** — kabul
edilmiş decision.md §3.2'ye göre eksik/geçersiz `main`'in gelecekte
`SQ-090-RUN-ENTRYPOINT` tarafından structured diagnostic + exit **65**
üretmesi beklenir. Bu, bu task'ın doğru runtime sınıflandırması veya kabul
kriteri değildir; ayrı task'ın kapsamındadır.

---

## 9. AGENTS.md §10 çalışma sonu raporu

1. **Rol ve görev kimliği**
   Hafif Model Uygulayıcı — SQ-090-EXIT-CODE-CONTRACT (Amendment 02) uygulaması.

2. **İncelenen kanıt**
   `AGENTS.md` (tam), implementation-contract.md (Amendment 02, tam),
   `src/cli/commands/run.hpp`/`check.hpp`/`ir.hpp` (tam dosya, uygulama
   öncesi ve sonrası), `tests/golden/arithmetic/mod_by_zero.runtime_error`,
   `tests/golden/numeric/longint_narrowing.compile_error`.

3. **Yapılan değişiklik veya karar**
   `src/cli/exit_codes.hpp` yeni oluşturuldu (contract §4.1 ile birebir).
   `run.hpp`: 3 diagnostic-gated `return 1;` → `kDataError` (65); catch
   bloğundaki `exitCode = 1;` → `kSoftwareError` (70). `check.hpp`: hata
   dalı `1` → `kDataError`, başarı dalı `0` → `kSuccess` (davranış aynı).
   `ir.hpp`: diagnostic hata dalı `1` → `kDataError`; başarı yolundaki
   `return 0;` dokunulmadı. `filePath.empty()` dalları ve `--jit` yolundaki
   `return 1;` hiçbirinde değiştirilmedi.

4. **Çalıştırılan komutlar**
   - `git status --short`, `git rev-parse HEAD` (başlangıç ve sonuç)
   - `cmake -S ... -B /tmp/saqut-sq090-exitcode-build-6sOvpj -G Ninja -DCMAKE_BUILD_TYPE=Release`
   - `cmake --build /tmp/saqut-sq090-exitcode-build-6sOvpj`
   - 12 hedefli `saqut run/check/ir` çağrısı (§5 tablosu)
   - `ctest --test-dir "$BUILD_DIR" -R '^golden_' --output-on-failure`
   - `git diff --stat HEAD -- <4 dosya>`, `git status --short -- src/`
   - `tests/run.sh` **çalıştırılmadı** (Başmimar açıklaması + contract §9b).

5. **DoD durumu**
   Bu task için: **Uygulandı.** Kendi izole build'imde gözlemledim;
   bağımsız validation (testçi oturumu) bekliyor. "Test Edildi" veya
   "Release Edildi" ilan edilmedi.

6. **Kanıtlanmayanlar**
   Bağımsız testçinin fresh build'inde aynı sonuçların tekrarlanabilirliği;
   `tests/run.sh` üzerinden herhangi bir doğrulama (bilinçli olarak
   çalıştırılmadı); eksik `main` senaryosunun gerçek davranışı (bu task
   kapsamında ölçülmedi, yalnız bilgi olarak not edildi); commit sonrası
   repo kökü `build/` ile tekrar üretilebilirlik.

7. **Riskler ve regresyon yüzeyi**
   `--jit` yolu ve `filePath.empty()` dalları bilinçli olarak değişmedi —
   bunlar hâlâ `1` döner (kapsam dışı, ayrı task'ların konusu). Eksik
   `main` geçici olarak 70 döner (bilinen sapma, §8). 8 adet
   `.compile_error` fixture'ı `EXIT_CODE EQUAL 0` kontrolüne dayandığı için
   etkilenmedi (ctest kanıtıyla doğrulandı). `stdout`/`stderr` üretim
   satırlarına dokunulmadı — yalnız sayısal `return` değerleri değişti.

8. **Sonraki yetkili rol**
   İzole Hafif Muhalif Testçi — validation-contract.md'ye göre bağımsız
   fresh build ile black-box ölçüm yapar. Bu implementation-report'u
   test planını dondurmadan önce **okumamalıdır** (contract §1 izolasyon
   kuralı).
