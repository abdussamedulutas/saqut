# SQ-090-EXIT-CODE-CONTRACT — Implementation Contract

**Revision: Amendment 02**
**Başmimar incelemesinden sonra coder öncesi revize edildi.**
**Önceki içerik ayrı accepted contract değildir; bu güncel dosyalar bağlayıcıdır.**

**Görev kimliği:** SQ-090-EXIT-CODE-CONTRACT
**Hedef sürüm:** 0.9.0
**Rol:** Hafif Model Uygulayıcı
**Kabul edilmiş karar:** `tasks/SQ-090-CLI-VM-CONTRACT/decision.md` §3.1
**Başmimar dispositionu (bu contract'ın doğrudan girdisi):** SQ-090 CLI
baseline kapısı kanıtla karşılanmıştır; yeni baseline amendment istenmez;
`int x = ;` normatif syntax hatasıdır ve silent-accept olarak gözlenmiştir
ama bu task'ın kapsamında değildir; bu task yalnız exit-code temelini ve
hâlihazırda diagnostic üreten yolları kapsar.
**Denetim revizyonu (bu contract yazılırken repo HEAD):** `7f871b75e917725dcdf46111fab88fb3be5663f2` (dal `0.9.0`)
**Başlangıç `git status --short`:** Dirty worktree kullanıcıya aittir (silinen
`.claude/` agent dosyaları, `wiki/`, `screenshots/`, değiştirilmiş ADR'ler,
yeni `AGENTS.md`/`knowledge-base/`/`docs/`/`prompts/`/`tasks/` — bunların
hiçbiri bu görevle ilgili değildir, dokunulmaz).

---

## 1. Problem ve kullanıcı etkisi

`tasks/SQ-090-CLI-BASELINE/validation-report.md` ve
`validation-report-amendment-01.md`/`-amendment-02.md`'de fresh binary ile
gözlenmiştir: saQut'ta sysexits.h tarzı merkezi bir exit-code sözleşmesi
yoktur. Fiili davranış ikili bir modeldir — her hata sınıfı (syntax, semantic,
runtime, kullanım hatası) ayrım gözetmeksizin `1` döner; yalnız `run` komutu
programın kendi `main` dönüş değerini exit code olarak taşır.

Somut kanıt (`evidence/amendment-02/cmd-01` ve `cmd-02`, F-LSP fixture,
`tests/lsp/fixtures/syntax_error_recovery.sqt` üzerinde):

- `run` → E901 syntax hatasında **exit=1** (structured diagnostic stderr'de var)
- `check` → E901 syntax hatasında **exit=1** (JSON diagnostic stdout'ta var)

Kullanıcıya/CI'ya etkisi: script'ler `1` exit kodundan syntax hatasını,
semantic hatasını, kullanım hatasını veya çökmeyi ayırt edemez. AGENTS.md §9
(0.9.0 önceliği: "normatif VM doğruluğu") ve decision.md §3.1'in kabul
ettiği `0/64/65/70` sınıflandırması, bu ayrımı stderr diagnostic'i yerine
(kısmen) exit koduna da taşımayı amaçlar — decision.md §3.1'in son cümlesi
gereği ayrımın **birincil taşıyıcısı hâlâ stderr diagnostic'idir**; exit kodu
yalnız kaba sınıf bilgisini taşır.

---

## 2. Kaynak baseline (exact kanıt, satır referanslı)

Bu contract'ı yazan oturum `src/cli/commands/run.hpp`, `check.hpp`, `ir.hpp`
dosyalarını doğrudan okumuştur (salt okunur inceleme, AGENTS.md §2 gereği).

### `src/cli/commands/run.hpp`

- Satır 28: `if (filePath.empty()) { std::cerr << "error: no input file\n"; return 1; }` — **DEĞİŞMEZ** (kapsam dışı, bkz. §6).
- Satır 48-51: `if (diag.hasErrors()) { diag.printAll(std::cerr); return 1; }` (module load hatası) — **65 olacak.**
- Satır 58-62: aynı desen (symbol collect hatası) — **65 olacak.**
- Satır 70-74: aynı desen (typecheck/structural hatası) — **65 olacak.**
- Satır 113-131: `--jit` deneysel yolu, desteklenmeyen opcode'da `return 1;` (satır 130) — **DEĞİŞMEZ** (kapsam dışı, JIT `[EXPERIMENTAL]`, ADR-042 §3).
- Satır 134-155: VM yolu. `exitCode = vm.run();` (satır 145) — **DEĞİŞMEZ** (main status aktarımı, korunması zorunlu INV-4 ve decision.md K-7).
- Satır 152-155: `catch (const std::exception& e) { std::cerr << "runtime error: " << e.what() << "\n"; exitCode = 1; }` — **70 olacak.**

### `src/cli/commands/check.hpp`

- Satır 23: `if (filePath.empty()) return 1;` — **DEĞİŞMEZ** (kapsam dışı).
- Satır 44: `return diag.hasErrors() ? 1 : 0;` — hata dalı **65 olacak**; başarı dalı `0` olarak **kalır** (zaten decision.md §3.1 ile uyumlu, "non-run başarı = 0").

### `src/cli/commands/ir.hpp`

- Satır 25: `if (filePath.empty()) return 1;` — **DEĞİŞMEZ** (kapsam dışı).
- Satır 38-41: `if (diag.hasErrors()) { diag.printAll(std::cerr); return 1; }` — **65 olacak.**
- Satır 70-72 (`program.dump(); return 0;`) — **DEĞİŞMEZ.**

Merkezi bir exit-code sabiti/enum **yoktur** (grep ile doğrulanmıştır: kodda
64/65/70 sysexits değeri hiçbir yerde geçmez). `DiagnosticEngine::hasErrors()`
(`src/diagnostic/diagnostic_engine.hpp`) exit code üretmez, yalnız
`errorCount() > 0` döner; her komut kendi `1`/`0` kararını verir.

---

## 2b. `ast`, `symbols`, `exec` — açık dışlama

`evidence/amendment-02/cmd-03` ve `cmd-06` (F-LSP üzerinde `ast`/`symbols`)
E901 varken **exit=0** döndüklerini göstermiştir — yani bu iki komutun
bugün **hiç** diagnostic-gated bir exit yolu **yoktur** (decision.md §1(1),
§1(2), §1(3) tespitleriyle tutarlı). Başmimar dispositionu ve decision.md §10
task sırası (`SQ-090-DIAG-GATE-SINGLE-FILE`, ayrı task) gereği bu task
`ast.hpp`, `symbols.hpp`, `exec.hpp` dosyalarına **dokunmaz.**

---

## 3. Kullanıcı etkisi (davranış sözleşmesi hedefi)

| Komut | Girdi sınıfı | Eski exit | Yeni exit |
|---|---|---|---|
| `run` | syntax/semantic hata (`diag.hasErrors()`) | 1 | **65** |
| `run` | yakalanan runtime exception (`catch`) | 1 | **70** |
| `run` | geçerli program, `main` normal döner | `main` değeri (0..255) | **değişmez** |
| `check` | syntax/semantic hata (`diag.hasErrors()`) | 1 | **65** |
| `check` | geçerli/hatasız kaynak | 0 | **değişmez (0)** |
| `ir` | syntax/semantic hata (`diag.hasErrors()`) | 1 | **65** |
| `ir` | geçerli kaynak, dump üretilir | 0 | **değişmez (0)** |

`stdout`/`stderr` **içeriği bu task kapsamında değiştirilmez** — yalnız
`return` ifadelerindeki sayısal değer değişir. `diag.printAll(std::cerr)`,
`out["diagnostics"] = diag.toJsonObj()` gibi hiçbir çıktı üretim satırı
dokunulmaz.

---

## 4. Kapsam içi

1. `src/cli/exit_codes.hpp` adında **yeni** bir başlık dosyası oluşturulur.
   Tek içeriği (yorum hariç) şudur:

   ```cpp
   #ifndef SAQUT_CLI_EXIT_CODES
   #define SAQUT_CLI_EXIT_CODES

   namespace saqut::exit_code {
   constexpr int kSuccess = 0;
   constexpr int kUsageError = 64;
   constexpr int kDataError = 65;
   constexpr int kSoftwareError = 70;
   }  // namespace saqut::exit_code

   #endif  // SAQUT_CLI_EXIT_CODES
   ```

   `kUsageError` bu task'ta hiçbir çağrı sitesinde **kullanılmaz** (kapsam
   dışı, bkz. §6) — yalnız decision.md §3.1'deki dört sınıfın tamamının tek
   yerde tanımlı olması için sabit tanımlanır. Kullanılmayan sabit derleme
   uyarısı üretmez (namespace-scope `constexpr`, `-Wunused` tetiklemez).

2. `src/cli/commands/run.hpp`: `#include "cli/exit_codes.hpp"` eklenir.
   Satır 48-51, 58-62, 70-74'teki üç `return 1;` → `return
   saqut::exit_code::kDataError;`. Satır 152-155'teki `exitCode = 1;` →
   `exitCode = saqut::exit_code::kSoftwareError;`.

3. `src/cli/commands/check.hpp`: `#include "cli/exit_codes.hpp"` eklenir.
   Satır 44: `return diag.hasErrors() ? 1 : 0;` →
   `return diag.hasErrors() ? saqut::exit_code::kDataError :
   saqut::exit_code::kSuccess;`.

4. `src/cli/commands/ir.hpp`: `#include "cli/exit_codes.hpp"` eklenir.
   Satır 40: `return 1;` → `return saqut::exit_code::kDataError;`.
   Satır 72'deki `return 0;` **değişmez** — bu task yalnız hata kodlarını
   değiştirir, ir başarı yolundaki `return 0;` bu contract kapsamında
   dokunulmadan kalır. Coder'a bırakılmış bir seçim değildir.

### 4b. Exit 70 kapsamının sınırı (bağlayıcı açıklama)

Bu task, saQut CLI'ının bütün internal-error mimarisini çözmez. Yalnız
`run.hpp` içindeki mevcut `catch (const std::exception&)` dalını (satır
152-155) `kSoftwareError` (70) döndürecek şekilde değiştirir. `check.hpp`
veya `ir.hpp`'ye yeni bir `try`/`catch` bloğu **eklenmez** — bu iki komutta
zaten yakalanan bir runtime exception yolu yoktur ve bu task bunu icat
etmez.

---

## 5. İmplementation invariant'ları (Given/When/Then)

**INV-1 (K-1 karşılığı, run/check/ir syntax hatası).**
Given: `E901` üreten sözdizimi hatalı bir kaynak (örn.
`tests/lsp/fixtures/syntax_error_recovery.sqt`).
When: `saqut run <dosya>`, `saqut check <dosya>`, `saqut ir <dosya>`
çağrılır.
Then: üçü de exit **65** döner; `stdout`/`stderr` içeriği (diagnostic metni,
JSON diagnostic alanı) değişmeden kalır.

**INV-2 (semantic hata).**
Given: `tests/golden/numeric/longint_narrowing.sqt` (diagnostic dayanağı:
`tests/golden/numeric/longint_narrowing.compile_error`).
When: `run`/`check`/`ir` çağrılır.
Then: üçü de exit **65** döner.

**INV-3 (runtime error, yalnız `run`).**
Given: `tests/golden/arithmetic/mod_by_zero.sqt` — VM çalışması sırasında
yakalanan gerçek bir `std::exception` (division/modulo-by-zero runtime
exception) üreten, tracked, geçerli bir kaynak (beklenen stderr dayanağı:
`tests/golden/arithmetic/mod_by_zero.runtime_error`).
When: `saqut run tests/golden/arithmetic/mod_by_zero.sqt` çağrılır.
Then: exit **70** döner; mevcut `stderr` içeriği (`"runtime error: " +
e.what()` metni) değişmeden korunur.

Eksik `main` (`runtime error: 'main' function not found`) bu invariant'ın
**parçası değildir.** Kabul edilmiş decision.md §3.2'ye göre eksik/geçersiz
`main`, gelecekte `SQ-090-RUN-ENTRYPOINT` tarafından structured diagnostic +
exit **65** üretmelidir; bu task'ın catch-all bloğu (§4.2, satır 152-155)
eksik `main`'i de geçici olarak 70'e düşürür, ama bu **bilinen geçici bir
ürün sözleşmesi sapmasıdır**, doğru runtime sınıflandırması veya kabul
kriteri değildir — kapsamı `SQ-090-RUN-ENTRYPOINT`'e aittir, bu task'ta
düzeltilmez ve fixture olarak kullanılmaz.

**INV-4 (main status aktarımı korunur — regresyon değil, invariant).**
Given: `int main() { return 0; }`, `int main() { return 1; }`,
`int main() { return 255; }` (üç ayrı geçerli program).
When: `saqut run <dosya>` çağrılır.
Then: exit sırasıyla **0**, **1**, **255** olur — bu task hiçbir satırı
değiştirmediği için bu davranış **regresyonsuz** korunur.

**INV-5 (başarı yolu değişmez).**
Given: geçerli, hatasız bir kaynak.
When: `check`, `ir` çağrılır.
Then: ikisi de exit **0** döner (değişmedi).

**INV-6 (`ast`/`symbols`/`exec` dokunulmadı).**
Given: herhangi bir kaynak.
When: `ast`, `ast --json`, `symbols`, `symbols --json`, `exec` çağrılır.
Then: bu task öncesi/sonrası **bit-bit aynı** davranış (bu dosyalar
değiştirilmediği için otomatik sağlanır; coder ekstra doğrulama yapmaz,
yalnız bu dosyalara dokunmadığını teyit eder).

---

## 6. Kapsam dışı ve kesin yasaklar (tekrar, bağlayıcı)

- `ast.hpp`, `symbols.hpp`, `exec.hpp` parser/diagnostic yollarını düzeltme.
- `main` varlık, tip veya `0..255` aralık doğrulaması ekleme.
- F3 (`int x = ;`) silent-accept'i düzeltme — bu task'ın kapsamı dışında,
  ayrı bir mimari karar gerektirir.
- Parser recovery davranışını değiştirme.
- CLI invocation/help/stub/örtük-run davranışını (`args.hpp`, `cli.hpp`,
  `main.cpp`) değiştirme — `filePath.empty()` dallarındaki `return 1;`
  ifadeleri dahil (bunlar `kUsageError`'a **taşınmaz**, mevcut haliyle kalır;
  bu, decision.md §10'daki ayrı `SQ-090-CLI-INVOCATION` task'ının kapsamıdır).
- JSON schema veya `schemaVersion` ekleme.
- Yeni public CLI komutu/bayrağı ekleme.
- Refactor, formatlama veya dependency değişikliği (`exit_codes.hpp` dışında
  yeni dosya oluşturulmaz).
- Test expected'ını mevcut implementasyona uydurma — tracked test dosyaları
  (`tests/`, `cmake/`) bu task'ta **hiç değiştirilmez.**
- `--jit` deneysel yolundaki `return 1;` (run.hpp satır 130) değiştirme.

---

## 7. İzin verilen dosyalar (exact, wildcard yok)

- `src/cli/exit_codes.hpp` (yeni oluşturulur)
- `src/cli/commands/run.hpp` (yalnız §4.2'de belirtilen satırlar)
- `src/cli/commands/check.hpp` (yalnız §4.3'te belirtilen satır)
- `src/cli/commands/ir.hpp` — yalnız §4.4'teki include ve error return değişikliği; başarı yolundaki return 0 değişmez

Başka hiçbir dosya değiştirilmez. Yeni bir dosyaya ihtiyaç varsa (yukarıdaki
dört dosya dışında) coder durur ve `BLOCKED` raporu verir.

---

## 8. Okunması zorunlu dosyalar

- `src/cli/commands/run.hpp`, `check.hpp`, `ir.hpp` (tam dosya)
- `src/diagnostic/diagnostic_engine.hpp` (yalnız `hasErrors()`/`errorCount()`
  imzası için)
- `tasks/SQ-090-CLI-VM-CONTRACT/decision.md` §3.1
- Bu implementation-contract'ın tamamı

---

## 9. Coder'ın çalıştıracağı minimum kontroller

Coder, kendi izole `/tmp` build dizininde (repo kökündeki `build/`
**kullanılmaz**):

```
BUILD_DIR=$(mktemp -d /tmp/saqut-sq090-exitcode-build-XXXXXX)
cmake -S /home/saqut/Masaüstü/saqutcompiler -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR"
```

Build başarılı olduktan sonra, coder aşağıdaki **minimum kontrol matrisini**
kendi build'inde çalıştırır (bu resmi validation değildir, testçi bağımsız
olarak tekrar ölçecektir; coder'ın kendi gözlemi bağımsız validation
**sayılmaz**). Her komut için `stdout`, `stderr` ve exit kodu **ayrı ayrı**
kaydedilir:

| # | Kontrol | Komut(lar) |
|---|---|---|
Kullanılacak kök:

```
REPO_ROOT=/home/saqut/Masaüstü/saqutcompiler
```

| # | Kontrol | Komut(lar) |
|---|---|---|
| 1 | syntax hatası | `run`/`check`/`ir` üçü de `$REPO_ROOT/tests/lsp/fixtures/syntax_error_recovery.sqt` ile |
| 2 | semantic hatası (`longint_narrowing`) | `run`/`check`/`ir` üçü de `$REPO_ROOT/tests/golden/numeric/longint_narrowing.sqt` ile |
| 3 | runtime hatası (`mod_by_zero`) | `run` yalnız, `$REPO_ROOT/tests/golden/arithmetic/mod_by_zero.sqt` ile |
| 4 | `main` dönüş aktarımı | `run` üç ayrı geçerli program ile: `int main() { return 0; }`, `int main() { return 1; }`, `int main() { return 255; }` |
| 5 | geçerli program başarı yolu | `check`/`ir`, `$REPO_ROOT/examples/fibonacci.sqt` ile |
| 6 | tracked regresyon | `ctest --test-dir "$BUILD_DIR" -R '^golden_' --output-on-failure` |

`tests/run.sh` bu matrise **dahil edilmez** — bkz. §9b.

```
"$BUILD_DIR/saqut" run "$REPO_ROOT/tests/lsp/fixtures/syntax_error_recovery.sqt"; echo "exit=$?"
"$BUILD_DIR/saqut" check "$REPO_ROOT/tests/lsp/fixtures/syntax_error_recovery.sqt"; echo "exit=$?"
"$BUILD_DIR/saqut" ir "$REPO_ROOT/tests/lsp/fixtures/syntax_error_recovery.sqt"; echo "exit=$?"
```

(gerçek kaynak yolu `$REPO_ROOT/tests/lsp/fixtures/syntax_error_recovery.sqt`
— kopyalamaya gerek yok, salt okunur çağrılır.)

### 9b. `tests/run.sh` bu task'ta çalıştırılmaz (gerekçe)

`tests/run.sh` satır 9'da `SAQUT="$ROOT/build/saqut"` **hardcode** edilmiştir
— dışarıdan verilen `SAQUT` environment değişkenini kullanmaz, izole `/tmp`
fresh binary ile çalıştırılamaz. Bu script'i coder'ın veya testçinin izole
build dizinine karşı çalıştırması yalnız stale (repo kökündeki eski)
binary'nin test edilmesine yol açar, bu da bu task'ın kanıt zincirini
geçersiz kılar. Script bu task'ta **değiştirilmez**; eksiklik ayrı
`SQ-090-CLI-TEST-HARNESS` task'ının kapsamına kaydedilir. Tracked regresyon
kanıtı yalnız `ctest --test-dir "$BUILD_DIR" -R '^golden_' --output-on-failure`
komutuyla sağlanır.

Bu, `cmake/run_golden.cmake`/`run_golden_error.cmake` tabanlı testlerin
coder'ın kendi build'inde hâlâ geçtiğini gösterir — **resmi Test Edildi
kanıtı değildir**, yalnız coder'ın kendi hatasını erken yakalaması içindir.
Tracked test dosyaları değiştirilmez; yalnız çalıştırılır.

---

## 10. Regresyon riski (coder'ın bilmesi gereken, salt bilgi)

- `cmake/run_golden_error.cmake` yalnız `EXIT_CODE EQUAL 0` mı diye bakar
  (satır 22-26) — spesifik `1` değeri kontrol edilmez. 8 adet
  `.compile_error` fixture'ı bu nedenle exit `1→65` değişiminden
  **etkilenmez.**
- `tests/run.sh`, `check`/`run` çağrılarını hep `if cmd; then ...` veya
  `$?` üzerinden **sıfır/sıfır-değil** ayrımıyla kontrol eder (satır 86-93,
  104-111, 138-150) — spesifik `1` değerine bağlı hiçbir assert yoktur. Bu
  değişiklikten **etkilenmesi beklenmez.** Ancak §9b gereği bu script bu
  task'ta coder tarafından **çalıştırılmaz** — bu madde yalnız salt bilgi
  amaçlıdır.
- Bu değerlendirme coder'ın kendi build'inde `ctest`
  çalıştırmasının **yerine geçmez** — yalnız beklenen sonucu açıklar.

---

## 11. Implementation report (`tasks/SQ-090-EXIT-CODE-CONTRACT/implementation-report.md`) zorunlu alanları

1. Değiştirilen exact dosyalar ve satır aralıkları (diff özeti)
2. Her invariant (INV-1..INV-6) için: coder'ın kendi build'inde gözlemlediği
   sonuç (komut, çıktı, exit) — **"Test Edildi" iddiası yapılmaz**, yalnız
   "kendi gözlemim" olarak işaretlenir
3. Çalıştırılan build komutları (exact, izole `/tmp` dizini yolu dahil)
4. `ctest --test-dir "$BUILD_DIR" -R '^golden_' --output-on-failure` sonucu (geçti/geçmedi, kaç test)
5. Dokunulmayan dosyaların listesi (§7 dışındaki her şeyin değişmediğinin
   `git status --short` ile teyidi)
6. Karşılaşılan blocker (varsa)
7. AGENTS.md §10 formatında çalışma sonu raporu

Coder kendi işini **"Test Edildi"** veya **"Release Edildi"** olarak
işaretleyemez (AGENTS.md §6). Yalnız "Uygulandı, kendi build'imde
gözlemlendi, bağımsız validation bekliyor" diyebilir.

---

## 12. Durma koşulları

- Merkezi exit tanımı yeni public API veya yeni CLI bayrağı gerektirirse.
- `main` değer aktarımını (satır 134-159 civarındaki VM çağrı zinciri)
  korumak, önerilen değişiklikle çelişirse (yani `exitCode = vm.run();`
  satırına dokunmadan INV-4'ü sağlamak mümkün görünmezse).
- Runtime error ile compiler-internal error ayrımı, `run.hpp`'nin tek
  `catch (const std::exception&)` bloğu ötesinde yeni bir ayrım mimarisi
  gerektirirse (örn. `check`/`ir`'e de try/catch eklenmesi gerektiği
  düşünülürse — bu task'ta **eklenmez**, gerekiyorsa durulur).
- İzin verilen dosyalar (§7) dışında bir dosyanın değişmesi gerektiği
  ortaya çıkarsa.
- `.compile_error` veya `tests/run.sh` testlerinden biri coder'ın kendi
  build'inde spesifik `1` değerine bağlıymış gibi davranıp kırılırsa (§10'daki
  değerlendirmeyle çelişirse) — coder testi "düzeltmez", durur ve raporlar.

Bu durumlardan biri tetiklenirse coder değişikliği geri almaz (henüz commit
yoksa zaten kirli değişiklik kod incelemesi için kalır), `BLOCKED` yazar ve
PM'e döner.
