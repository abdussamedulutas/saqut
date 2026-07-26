# SQ-090-EXIT-CODE-CONTRACT — Validation Contract

**Revision: Amendment 02**
**Başmimar incelemesinden sonra coder öncesi revize edildi.**
**Önceki içerik ayrı accepted contract değildir; bu güncel dosyalar bağlayıcıdır.**

**Görev kimliği:** SQ-090-EXIT-CODE-CONTRACT
**Hedef sürüm:** 0.9.0
**Rol:** İzole Hafif Muhalif Testçi (coder'dan tamamen ayrı, bağımsız oturum
— coder'ın implementation-report'unu, akıl yürütmesini veya diff'ini
görmeden çalışır)
**Kabul edilmiş karar:** `tasks/SQ-090-CLI-VM-CONTRACT/decision.md` §3.1
**İlgili implementation contract:** `tasks/SQ-090-EXIT-CODE-CONTRACT/implementation-contract.md`
(testçi bunu **davranış hedefi** olarak okuyabilir — hangi exit kodunun
hangi girdide beklendiğini buradan alır; ama coder'ın raporunu veya diff
açıklamasını oracle olarak kullanmaz, yalnız kendi black-box ölçümüne
güvenir)

Bu contract, coder henüz çalışmadan **önce yazılmıştır** — test planı bu
belgeyle dondurulur. Testçi, coder'ın implementation-report.md'sini
**okumadan önce** kendi ölçümünü tamamlar; kendi ölçümü ile coder'ın raporu
çelişirse coder'ın raporunu değil kendi ölçümünü esas alır.

---

## 1. Rol sınırı ve izolasyon (bağlayıcı)

- Testçi `src/` altındaki C++ implementasyonunu (değiştirilmiş veya
  değiştirilmemiş) **okumaz.**
- Testçi coder'ın implementation-report.md'sini, commit mesajını veya diff'ini
  test planını dondurmadan **görmez.** (Test planı zaten bu contract'la
  donduruldu — testçi bu contract'ı okuduktan sonra doğrudan black-box
  ölçüme geçer.)
- Testçi expected çıktıyı mevcut/değişmiş implementasyona **uydurmaz.**
- Testçi kaynağı veya tracked test'i **düzeltmez.**
- Sonuç yalnız **GÖZLENDİ / GÖZLENMEDİ / BLOCKED** olarak sınıflandırılır —
  PASS/FAIL dili kullanılmaz (bu hâlâ AGENTS.md §4 DoD'nin "Test Edildi"
  aşamasına geçiş kararını testçi değil ürün sahibi/mimar verir).
- Build/test bu görevde **çalıştırılır** (validation-only ölçüm amacıyla,
  önceki SQ-090-CLI-BASELINE turlarındaki gibi) — ama production source veya
  tracked test **değiştirilmez.**

---

## 2. Fresh build provenance — ZORUNLU

```
BUILD_DIR=$(mktemp -d /tmp/saqut-sq090-exitcode-validation-XXXXXX)
cmake -S /home/saqut/Masaüstü/saqutcompiler -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR"
```

- Repo kökündeki `build/` **kullanılmaz.**
- Coder'ın kendi `/tmp/saqut-sq090-exitcode-build-*` dizini **yeniden
  kullanılmaz** — testçi kendi bağımsız fresh build'ini yapar.
- Kaydedilecekler: `evidence/00-provenance.md` (branch, HEAD, `git status
  --short`, toolchain, binary yolu, SHA-256, timestamp).
- **Coder raporuna güvenmeden binary hash doğrulaması:** testçi, binary'nin
  gerçekten bu HEAD'den ve bu oturumda derlendiğini kendi
  `sha256sum`/`stat`/`git rev-parse HEAD` çıktısıyla kanıtlar; coder'ın
  "değiştirdim" iddiasını doğrudan kabul etmez.
- `src/cli/exit_codes.hpp`, `src/cli/commands/run.hpp`,
  `src/cli/commands/check.hpp`, `src/cli/commands/ir.hpp` dosyalarının HEAD'e
  göre gerçekten değişip değişmediği `git diff --stat HEAD -- <path>` ile
  kaydedilir (bu, "coder gerçekten bir şey değiştirdi mi" sorusunun black-box
  ön-kontrolüdür — kaynağı okumak değil, yalnız değişiklik varlığını görmek).
- Ayrıca aşağıdaki iki komut da provenance'a kaydedilir:

  ```
  git diff --stat HEAD -- src/cli/exit_codes.hpp src/cli/commands/run.hpp \
      src/cli/commands/check.hpp src/cli/commands/ir.hpp
  git status --short -- src/cli/exit_codes.hpp src/cli/commands/run.hpp \
      src/cli/commands/check.hpp src/cli/commands/ir.hpp
  ```

  Gerekçe: `git diff` yeni **untracked** `exit_codes.hpp` dosyasını tek
  başına göstermez (untracked dosyalar `git diff`'te görünmez); `git status
  --short` bunu tamamlar ve untracked yeni dosyanın varlığını da kanıtlar.

---

## 3. Fixture'lar

Testçi kendi izole `/tmp` dizinine şu kaynakları hazırlar/kopyalar (kaynak
her biri için belirtilmiştir):

**FX-SYNTAX** — gerçek, tracked syntax-error kaynağı (repo kökünden
salt-okunur kopyalanır, `tests/` değiştirilmez):
```
tests/lsp/fixtures/syntax_error_recovery.sqt
```
(İçerik: `int broken() { ) return 0; }` + `topla`/`main` — E901 üretir,
`)` token'ında.)

**FX-SEMANTIC** — exact, tracked semantic-error kaynağı (testçi seçim
yapmaz):
```
tests/golden/numeric/longint_narrowing.sqt
```
Beklenen diagnostic dayanağı: `tests/golden/numeric/longint_narrowing.compile_error`.

**FX-RUNTIME** — exact, tracked runtime-error kaynağı (testçi seçim yapmaz,
alternatif aramaz):
```
tests/golden/arithmetic/mod_by_zero.sqt
```
Beklenen stderr dayanağı: `tests/golden/arithmetic/mod_by_zero.runtime_error`.

Eksik `main` fixture'ı (boş dosya veya `main`'siz kaynak) bu task'ta exit 70
için **kullanılmaz.** Kabul edilmiş decision.md §3.2'ye göre eksik/geçersiz
`main`, gelecekte `SQ-090-RUN-ENTRYPOINT` tarafından exit 65 üretecek şekilde
ele alınmalıdır; bugünkü geçici 70 davranışı **bilinen bir ürün sözleşmesi
sapmasıdır**, bu task'ın doğru runtime sınıflandırması veya kabul kriteri
değildir. Testçi eksik `main` senaryosunu **ölçmez.**

**FX-VALID-0 / FX-VALID-1 / FX-VALID-255** — üç ayrı geçerli program:
```
int main() { return 0; }
int main() { return 1; }
int main() { return 255; }
```
(Sözdizimi `examples/merhaba.sqt`/`examples/fibonacci.sqt` ile teyitli
biçimdedir — bu contract'ı yazan oturum bunu zaten SQ-090-CLI-BASELINE
amendment-01/02 kanıtından bilir; testçi yine de kendi kopyasını
`examples/fibonacci.sqt` ile karşılaştırarak salt-okunur teyit eder.)

**FX-VALID-CHECK-IR** — geçerli, hatasız bir kaynak (`examples/fibonacci.sqt`
doğrudan kullanılabilir, kopyalamaya gerek yok, salt okunur çağrılır) —
`check`/`ir` başarı yolunu (exit 0, değişmedi) ölçmek için.

Her fixture içeriği hash'i `evidence/01-fixtures-manifest.txt`'e kaydedilir.

---

## 4. Komut matrisi — ZORUNLU

| # | Komut | Fixture | Beklenen davranış hedefi (implementation-contract §3'ten) |
|---|---|---|---|
| 1 | `run` | FX-SYNTAX | exit 65 |
| 2 | `check` | FX-SYNTAX | exit 65 |
| 3 | `ir` | FX-SYNTAX | exit 65 |
| 4 | `run` | FX-SEMANTIC | exit 65 |
| 5 | `check` | FX-SEMANTIC | exit 65 |
| 6 | `ir` | FX-SEMANTIC | exit 65 |
| 7 | `run` | FX-RUNTIME (`mod_by_zero.sqt`) | exit 70 |
| 8 | `run` | FX-VALID-0 | exit 0 |
| 9 | `run` | FX-VALID-1 | exit 1 |
| 10 | `run` | FX-VALID-255 | exit 255 |
| 11 | `check` | FX-VALID-CHECK-IR | exit 0 |
| 12 | `ir` | FX-VALID-CHECK-IR | exit 0 |
| 13 | `exec '1 + 1'` | (kaynak yok, inline ifade) | Amendment 01 baseline'ıyla karşılaştırılır (§8b) |

"Beklenen davranış hedefi" sütunu testçiye **ne ölçeceğini** söyler; testçi
bunu doğru varsaymaz, yalnız ölçer ve gözlenen değeri kaydeder. Beklenenle
gözlenen farklıysa bu **GÖZLENMEDİ** olarak raporlanır, ölçüm hatası veya
kabul kriterinin karşılanmadığı anlamına gelir — testçi bunu "düzeltmeye"
çalışmaz.

Her komut için ayrı ham dosyalar:
```
evidence/cmd-NN-<label>.command.txt
evidence/cmd-NN-<label>.stdout.bin
evidence/cmd-NN-<label>.stderr.bin
evidence/cmd-NN-<label>.exit.txt
```

---

## 5. `stdout`/`stderr` içeriğinin değişmediğinin kontrolü

Implementation-contract §3, yalnız exit kodunun değiştiğini, `stdout`/
`stderr` **içeriğinin** değişmediğini iddia ediyor. Testçi bunu doğrulamak
için, mümkünse `tasks/SQ-090-CLI-BASELINE/evidence/amendment-02/cmd-01`
(run, FX-SYNTAX ile aynı fixture) ve `cmd-02` (check) ham stdout/stderr
içeriğiyle **byte-diff** karşılaştırması yapar.

**Absolute path normalizasyonu — zorunlu ön adım.** Amendment 02 baseline'ı
ve bu validation'ın yeni ölçümü, kaynağa **farklı absolute fixture
yollarından** erişir (farklı `/tmp` izole dizinleri veya farklı repo
checkout yolları). Bu nedenle **doğrudan byte-diff yapılmaz.** Önce her iki
ham çıktıda yalnız fixture'ın exact absolute path'i `%FIXTURE%` ile
değiştirilir (örn. `sed "s|$ABSOLUTE_FIXTURE_PATH|%FIXTURE%|g"`); ardından
normalize edilmiş iki çıktı byte-diff edilir:

```
sed "s|$OLD_FIXTURE_PATH|%FIXTURE%|g" \
    tasks/SQ-090-CLI-BASELINE/evidence/amendment-02/cmd-01-*.stderr.bin \
    > /tmp/baseline.normalized.stderr
sed "s|$NEW_FIXTURE_PATH|%FIXTURE%|g" \
    evidence/cmd-01-run-syntax.stderr.bin \
    > /tmp/new.normalized.stderr
diff /tmp/baseline.normalized.stderr /tmp/new.normalized.stderr
```

**Normalize edilemeyenler** (bunlardan herhangi biri farklıysa gerçek metin
farkı sayılır, normalizasyonla gizlenmez):

- diagnostic code/message;
- line/column/offset;
- JSON anahtar veya değerleri;
- whitespace ve satır sonları;
- stdout/stderr ayrımı.

Ham, normalize edilmemiş çıktılar `evidence/` altında **ayrıca** (normalize
edilmiş kopyanın yanında) korunur — normalizasyon yalnız karşılaştırma
adımı içindir, ham kanıt kaybolmaz.

Fark yalnız fixture path'inden kaynaklanmıyorsa ve gerçek metin farkı varsa,
bu **ayrı bir GÖZLENDİ/GÖZLENMEDİ satırı** olarak raporlanır ("exit kodu
değişti ama stdout/stderr içeriği de değişti — implementation-contract §3
ihlali" veya "içerik korundu"). Baseline dosyası bulunamaz/uyumsuzsa bu
adım **BLOCKED** olarak işaretlenir, tahmin edilmez.

---

## 6. Determinizm — sınırlı

En az `run` + FX-SYNTAX ve `run` + FX-RUNTIME kombinasyonları 3'er kez
koşulur. Yalnız şu biçimde raporlanır (genel "deterministiktir" hükmü
yasak):

> "Seçilen `<komut> <fixture>` kombinasyonu, bu ortamda üç tekrar boyunca
> byte-identical sonuç üretti."

Her koşunun stdout/stderr SHA-256'sı ve exit kodu `evidence/02-determinism.md`'ye
kaydedilir; boş code block kabul edilmez.

---

## 7. Mevcut tracked test regresyon komutu

Testçi kendi fresh build dizininde, exact olarak:

```
ctest --test-dir "$BUILD_DIR" -R '^golden_' --output-on-failure
```

`tests/run.sh` bu task'ta **çalıştırılmaz.** Gerekçe: script satır 9'da
`SAQUT="$ROOT/build/saqut"` **hardcode** edilmiştir — dışarıdan verilen
`SAQUT` environment değişkenini kullanmaz, izole `/tmp` fresh binary ile
çalıştırılamaz. Bu script'i izole build dizinine karşı çalıştırmaya çalışmak
yalnız repo kökündeki stale binary'yi test eder ve bu task'ın kanıt
zincirini geçersiz kılar. Script bu task'ta değiştirilmez; eksiklik ayrı
`SQ-090-CLI-TEST-HARNESS` task'ının kapsamına kaydedilir.

Bu komutun çıktısı — geçen/kalan test sayısı, `FATAL_ERROR` varsa tam metni
— `evidence/03-tracked-regression.txt`'e ham olarak kaydedilir.

**Bu adımın amacı** decision.md §10 task 8'in ("test harness genelleştirmesi")
henüz yapılmamış olması nedeniyle mevcut testlerin **kırılıp kırılmadığını**
görmektir — yeni test yazmak veya mevcut testi genişletmek bu contract'ın
kapsamında değildir.

**8 adet `.compile_error` fixture'ının** (`cmake/run_golden_error.cmake`
altında) ayrı ayrı hangi exit kodunu ürettiği not edilir (yalnız bilgi
amaçlı — bu script `EXIT_CODE EQUAL 0` mı diye baktığından `1→65`
değişiminden etkilenmemesi **beklenir**; testçi bunu doğrular, varsayımla
geçmez).

---

## 8. Regression: `ast`/`symbols`/`exec` dokunulmadı mı?

Implementation-contract INV-6 gereği bu üç komutun davranışı **değişmemeli.**
Testçi, `tasks/SQ-090-CLI-BASELINE/evidence/amendment-02/cmd-03`
(`ast`, FX-SYNTAX ile) ve `cmd-06` (`symbols`) ham kanıtlarıyla aynı
fixture üzerinde `ast`/`symbols`'ü kendi binary'sinde tekrar çalıştırıp
**exit + stdout + stderr** birebir karşılaştırır. Bu karşılaştırmada da §5'te
tanımlanan **aynı fixture-path normalizasyonu** uygulanır (absolute path
`%FIXTURE%` ile değiştirilir, ardından normalize edilmiş çıktılar
karşılaştırılır) — diagnostic code/message, line/column/offset, JSON
alanları, whitespace ve stdout/stderr ayrımı normalize edilmez. Fark varsa
bu **GÖZLENDİ — beklenmeyen regresyon** olarak raporlanır ve PASS/FAIL
değil ayrı bir uyarı satırı olarak öne çıkarılır (implementation-contract'ın
izin vermediği bir değişiklik anlamına gelir).

### 8b. `exec` dar kontrolü

INV-6 metni `exec`'i de kapsadığı için (implementation-contract §5, INV-6),
validation da `exec` için dar bir kontrol içerir: `saqut exec '1 + 1'`
çalıştırılır; `stdout`/`stderr`/exit, mevcut Amendment 01 baseline'ıyla
(`tasks/SQ-090-CLI-BASELINE/evidence/amendment-01/` altındaki ilgili `exec`
kaydı) karşılaştırılır. Baseline kaydı bulunamazsa bu satır **BLOCKED**
olarak işaretlenir, tahmin edilmez.

---

## 9. Kabul kriterleri sınıflandırması (K-1 kısmi, K-3 kısmi, K-4 kısmi, K-7, task-local RUNTIME-1, K-6 yalnız kapsam dışı ürün kriteri)

decision.md §7'deki K-1, K-3, K-4, K-7 kriterlerinin **bu dar task
kapsamındaki alt kümesi**, task-local `RUNTIME-1` kriteri ve kapsam dışı
`K-6`'nın durumu için GÖZLENDİ/GÖZLENMEDİ/BLOCKED:

| Kriter | İçerik | Bu task kapsamında mı |
|---|---|---|
| K-1 (kısmi) | syntax hatalı dosya için `run`/`check`/`ir` exit 65 | Evet — §4 satır 1-3 |
| K-3 (kısmi) | semantic hatalı dosya için aynı | Evet — §4 satır 4-6 |
| K-4 (kısmi) | geçerli program için `check`/`ir` exit 0 | Evet — §4 satır 11-12 |
| K-7 | `main` dönüşü process status'u | Evet — §4 satır 8-10 |
| RUNTIME-1 | `tests/golden/arithmetic/mod_by_zero.sqt` `run` ile exit 70 üretir ve mevcut runtime stderr içeriği korunur | Evet — task-local kriterdir, decision.md K-6 **değildir** — §4 satır 7 |
| K-6 | `main` içermeyen/geçersiz entrypoint'li kaynak `run` ile structured diagnostic + exit 65 üretir; ham VM exception görünmez | **Bu task kapsamı dışında ve ölçülmez.** `SQ-090-RUN-ENTRYPOINT` kapsamıdır. |

K-6'yı runtime exit 70 veya stderr biçimiyle ilişkilendiren hiçbir cümle bu
contract'ta kullanılmaz — K-6 ve RUNTIME-1 ayrı, birbirine karıştırılmayan
kriterlerdir.

Her satır için ayrı GÖZLENDİ/GÖZLENMEDİ/BLOCKED + evidence referansı rapora
yazılır.

---

## 10. Evidence düzeni

```
tasks/SQ-090-EXIT-CODE-CONTRACT/evidence/
  00-provenance.md
  01-fixtures-manifest.txt
  02-determinism.md
  03-tracked-regression.txt
  cmd-NN-<label>.command.txt / .stdout.bin / .stderr.bin / .exit.txt
```

Binary, dependency veya geniş build çıktısı buraya kopyalanmaz.

---

## 11. Rapor — ZORUNLU tek çıktı

```
tasks/SQ-090-EXIT-CODE-CONTRACT/validation-report.md
```

Zorunlu bölümler:

1. Provenance (§2, binary hash doğrulaması dahil, coder iddiasına
   güvenmeden)
2. Değişen dosyaların `git diff --stat` kanıtı (yalnız §7'deki 4 dosyanın
   değişip değişmediği — içerik değil, değişiklik varlığı)
3. Fixture matrisi (FX-SYNTAX, FX-SEMANTIC, FX-RUNTIME, FX-VALID-*,
   exact fixture yolları ve SHA-256 değerleri)
4. Komut matrisi sonuçları (§4 tablosu, gözlenen vs. beklenen, evidence
   referanslı)
5. stdout/stderr içerik korunması kontrolü (§5)
6. Determinizm kanıtı (§6, hash'lerle)
7. Tracked test regresyon sonucu (§7, `ctest --test-dir "$BUILD_DIR" -R
   '^golden_' --output-on-failure` ham çıktı özet; `tests/run.sh`
   çalıştırılmadı, gerekçe §7'de)
8. `ast`/`symbols` dokunulmadı kontrolü (§8)
9. K-1/K-3/K-4/K-7, RUNTIME-1 ve kapsam dışı K-6 teyidi (§9)
10. Final `git status --short` (görev sonu, production source/tracked test
    dışında hiçbir şeyin beklenmedik biçimde değişmediğinin teyidi —
    placeholder kullanılmaz, ham çıktı yapıştırılır)
11. DoD durumu teyidi: bu validation raporu tek başına DoD'yi "Test
    Edildi"ya taşımaz — o karar ürün sahibi/mimara aittir; bu rapor yalnız
    kanıt sağlar

---

## 12. Durma koşulları

- Fresh build başarısız olursa — sonraki adımlara geçilmez, BLOCKED
  raporlanır.
- `git diff --stat HEAD -- src/cli/exit_codes.hpp src/cli/commands/run.hpp
  src/cli/commands/check.hpp src/cli/commands/ir.hpp` **hiçbir değişiklik
  göstermezse** — coder henüz çalışmamış demektir; testçi durur, "coder
  değişikliği bulunamadı" diye BLOCKED yazar, tahminle devam etmez.
- `git status --short`, izin verilen 4 dosya dışında `src/` altında başka bir
  değişiklik gösterirse — bu implementation-contract §7 ihlali potansiyel
  bir bulgudur; testçi düzeltmez, bunu **ayrı bir uyarı** olarak rapora
  yazar ve PM'e/mimar'a döner.
- FX-SEMANTIC veya FX-RUNTIME için uygun bir fixture bulunamazsa — o satır
  BLOCKED olarak işaretlenir, tahmini fixture icat edilmez.
- Beklenmeyen crash/segfault — `timeout 10s` uygulanır, olduğu gibi
  kaydedilir.
