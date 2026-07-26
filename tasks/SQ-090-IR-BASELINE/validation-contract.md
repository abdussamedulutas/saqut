# SQ-090-IR-BASELINE — Validation Contract (VALIDATION-ONLY)

**Revision:** Amendment 03 — Evidence-Completeness Revalidation

**Görev kimliği:** SQ-090-IR-BASELINE
**Hedef sürüm:** 0.9.0
**Contract modu:** VALIDATION-ONLY
**Rol:** İzole Hafif Muhalif Testçi (yeni, bağımsız oturum; coder/mimar
reasoning'i, production C++ kaynağı veya bu contract'ı yazan teslimat
yöneticisinin akıl yürütmesi görülmez/kullanılmaz)
**Kabul edilmiş karar / girdi:** Ürün sahibinin bu görev için verdiği doğrudan
talimat (SQ-090-IR-FINAL-FORM-AUDIT Amendment 01 kapsamının güncel binary
üzerindeki ölçüm kapısı) — otorite sıralamasında madde 1 (AGENTS.md §3).
**Amendment 01 gerekçesi:** Orijinal contract (a) mevcut çalışma ağacını
görmezden gelip HEAD'i fresh clone ile test ediyordu — bu, görevle doğrudan
ilgili aktif kaynak değişikliklerini (`src/cli/commands/run.hpp`,
`src/cli/commands/check.hpp`, `src/cli/commands/ir.hpp`,
`src/cli/exit_codes.hpp`) dışlıyordu; (b) IRB-2 instruction tanıma kuralı
opcode-adıyla-başlayan-satır sezgisine dayanıyordu ve doğrulanmamıştı; (c)
IRB-3 fixture'ı duplicate `main` + duplicate import binding üretip hedef
hipotezi izole etmiyordu; (d) IRB-5, `longint`/`float` surface sözdizimini
tracked kanıt aramadan belirsiz ilan ediyordu; (e) capability fixture'ı ve
gerekli `--allow-fs` bayrağı tanımsızdı; (f) IRB-8 ANSI arama yöntemi
(`xxd | grep '1b\['`) güvenilir değildi. Amendment 01 yalnız bu maddeleri
düzeltmişti.

**Amendment 02 gerekçesi:** Başmimarın son incelemesi şu kalan eksikleri
tespit etti: (a) başlangıç (§1.3) ve bitiş (§2/§Son kontrol)
karşılaştırmaları farklı yol kümeleri kullanıyordu (§1.3'te üç yol, §2'de
beş yol) — bu amendment ikisini de exact aynı beş yola sabitler; (b)
repository içi ve geçici fixture'lara göreli yol (`examples/fibonacci.sqt`,
`main.sqt`, `cap.sqt` vb.) ile referans veriliyordu — bu, tester'ın hangi
cwd'de çalıştığına göre farklı sonuç doğurabilir; artık her repository
fixture'ı `$REPO_ROOT` ile mutlak, her geçici fixture mutlak yol veya
explicit cwd ile referanslanır; (c) IRB-3'ün cwd'si tanımsızdı — artık
kesin olarak `$WORK/fixtures/irb3` olarak sabitlenir ve `.cmd` evidence
dosyaları cwd + argv'yi ayrı ayrı kaydeder; (d) IRB-9, `ctest -N`/`ctest -R`
komutlarını hangi dizinden çalıştıracağını ve regex'in yanlış eşleşmeye
açık olup olmadığını belirtmiyordu — artık `ctest --test-dir "$WORK/build"`
ve anchored (`^...$`) exact test adları zorunludur; (e) "END instruction"
ifadesi yanlıştı — END bir instruction değil, dump biçiminin bir
işaretçisidir; "END dump marker'ı" olarak düzeltildi. Amendment 02
ölçülmemiş hiçbir yeni davranış hipotezi eklemedi.

**Amendment 03 gerekçesi (Evidence-Completeness Revalidation):**
Amendment 02 sonrası yapılan ilk ölçüm denemesi (mevcut
`validation-report.md` + `evidence/`), Başmimar Günlüğü #003'te
(GitHub #101, yorum:
https://github.com/saqutlang/saqut/issues/101#issuecomment-5083670628)
şu kanıt-bütünlüğü eksiklikleriyle **provisional** ilan edildi ve
validation henüz kabul edilmedi:

- IRB-1/2/4/5/6/7/8/9 çağrılarının çoğunda exact `.cmd` kaydı yoktu
  (bu contract'ın kendi denetimi: mevcut `evidence/` altında 98 dosyadan
  hiçbiri `.cmd` uzantılı değil).
- IRB-5–9'un birçok çağrısında raw `.exit` kaydı yoktu (mevcut
  `evidence/` altında yalnız 14 `.exit` dosyası var); rapordaki exit
  iddiaları evidence dosyasıyla denetlenemiyor.
- Başlangıç beş-yol status/hash kaydı vardı, zorunlu bitiş karşılaştırması
  yoktu.
- IRB-3, `E_SYMBOL_NOT_IMPORTED` ile semantic kapıda durdu; bu sessiz IR
  kaybını önlüyor fakat "aynı adın çakışması doğru çözüldü" iddiasıyla
  aynı şey değildir — ayrı bir modül/name-resolution bulgusudur ve bu
  ayrım önceki raporda gizlenmişti.

Amendment 03 **davranış matrisini, fixture'ları, IRB-1–IRB-9
hipotezlerini ve GÖZLENDİ/GÖZLENMEDİ/BLOCKED sınıflandırma semantiğini
değiştirmez.** Yalnız kanıt bütünlüğü zorunluluklarını sağlamlaştırır ve
önceki denemenin nasıl ele alınacağını (silinmeden, geçerli kapanış kanıtı
sayılmadan, `revalidation-01/` altında yeniden ölçülerek) tanımlar.

**Denetim revizyonu (bu contract yazılırken repo HEAD):**
`7f871b75e917725dcdf46111fab88fb3be5663f2` (dal `0.9.0`).

Bu baseline, mevcut davranışı **değiştirmez**; yalnız ölçer.
`implementation-contract.md`, `implementation-report.md` veya coder prompt'u
üretilmez. Sonuçlar yalnız `GÖZLENDİ / GÖZLENMEDİ / BLOCKED` olarak
sınıflandırılır. PASS/FAIL veya DoD yükseltme dili kullanılmaz.

---

## R. Revalidation zorunlulukları (Amendment 03) — IRB-1–IRB-9'dan önce uygulanır

Bu bölüm, aşağıdaki IRB-1–IRB-9 hipotezlerine (bunlar değişmemiştir) ek
olarak, yeni izole testçinin **kanıt üretim disiplinini** bağlar.

### R.1 — Önceki deneme supersede edilir, silinmez

Mevcut `tasks/SQ-090-IR-BASELINE/validation-report.md` ve mevcut
`tasks/SQ-090-IR-BASELINE/evidence/` altındaki dosyalar bu görevin
**ilk ölçüm denemesidir**. Bunlar:

- silinmez, üzerine yazılmaz, taşınmaz;
- geçerli kapanış kanıtı **sayılmaz** (Başmimar Günlüğü #003 bunu
  provisional ilan etmiştir — GitHub #101,
  https://github.com/saqutlang/saqut/issues/101#issuecomment-5083670628);
- yeni testçi tarafından oracle, "beklenen değer" veya kısayol olarak
  kullanılmaz (bkz. R.10 — tüm ölçümler sıfırdan tekrarlanır).

### R.2 — Yeni kanıt konumu

Bu revalidasyonun **bütün** kanıtı tek bir dizin altında toplanır:

```
EVIDENCE_DIR="$REPO_ROOT/tasks/SQ-090-IR-BASELINE/evidence/revalidation-01"
```

(Bu, önceki amendment'lerdeki `$EVIDENCE_DIR` tanımının yerine geçer —
IRB-1–IRB-9 ve §1–§4'teki her `$EVIDENCE_DIR` referansı artık bu yeni,
`revalidation-01` alt dizinini gösterir.) Önceki denemenin
`evidence/` altındaki eski dosyaları (kök seviyesinde, `revalidation-01`
dışında duranlar) bu yeni dizine kopyalanmaz, karıştırılmaz.

### R.3 — Her executable komut için dörtlü kanıt

Configure ve build adımları dahil, çalıştırılan **her** executable komut
(precheck `check` çağrıları, `check`, `ir`, `run`, `run --profile`,
yönlendirme/redirect komutları, bütün `ctest` çağrıları, `cmake`
configure/build) için ayrı ve eksiksiz dört dosya üretilir:

```
<case>.cmd
<case>.stdout
<case>.stderr
<case>.exit
```

`<case>` her IRB/adım için benzersiz bir isimdir (örn.
`IRB-1-run1`, `IRB-3-single-main-check`, `01-configure`). Bu dörtlünün
herhangi biri eksikse o `<case>` için ölçüm **kanıtlanmamış** sayılır
(bkz. R.8).

### R.4 — `.cmd` dosyasının zorunlu alanları

Her `.cmd` dosyası en az şu üç alanı ayrı satırlarda, açıkça kaydeder:

```
cwd: <komutun çalıştırıldığı exact dizin>
argv: <tam, quoting korunmuş argüman listesi>
command: <tam çalıştırılan komut satırı>
```

Bu, önceki amendment'lerin IRB-3 için getirdiği cwd/argv kaydı
zorunluluğunun artık **bütün** `<case>`'lere genelleştirilmiş halidir.

### R.5 — Shell zinciri exit'i alt komutun yerine geçmez

IRB precheck, `check`, `ir`, `run`, `run --profile`, redirect (`>`/`2>`
içeren) ve bütün `ctest` çağrılarının her biri **kendi** `.exit` dosyasına
sahip olmalıdır. Bir shell zincirinin (`&&`, `;`, pipe) veya bir
wrapper script'in toplam/son exit kodu, zincirdeki bireysel alt
komutların exit kodlarının **yerine geçmez**. Örn. `cmd1 && cmd2`
şeklinde çalıştırılan bir çift varsa, `cmd1.exit` ve `cmd2.exit` ayrı
ayrı, her komutun kendi `$?` değeriyle kaydedilir — yalnız zincirin
son `$?`'si değil.

### R.6 — Beş yol için başlangıç/bitiş kanıtı ayrı ayrı üretilir

Önceki amendment'lerde tanımlanan exact aynı beş yol (`src/`,
`CMakeLists.txt`, `cmake/`, `tests/`, `examples/`) için başlangıçta ve
bitişte **ayrı kanıt dosyaları** üretilir (örn.
`revalidation-01/start-five-path-status.txt`,
`revalidation-01/start-five-path-diff-sha256.txt`,
`revalidation-01/end-five-path-status.txt`,
`revalidation-01/end-five-path-diff-sha256.txt`). Başlangıç ve bitiş
`status --short` çıktıları ile `diff HEAD ... | sha256sum` değerleri
**byte-identical** olmalıdır; değilse görev BLOCKED olur (bkz. R.8).

### R.7 — `manifest.tsv` zorunludur

`revalidation-01/manifest.tsv` üretilir. Her satır tab-separated olarak
en az şu alanları içerir:

```
irb_id<TAB>step<TAB>cwd<TAB>argv<TAB>cmd_path<TAB>stdout_path<TAB>stderr_path<TAB>exit_path<TAB>files_present
```

- `irb_id`: örn. `IRB-1`, `IRB-3`, `01-configure` (build/configure adımları
  için IRB dışı bir kimlik kullanılabilir).
- `step`: o IRB içindeki adımın kısa açıklaması (örn. `run1`,
  `single-main-check`).
- `cwd`, `argv`: R.4'teki `.cmd` içeriğiyle birebir tutarlı olmalı.
- `cmd_path`/`stdout_path`/`stderr_path`/`exit_path`: `revalidation-01/`
  köküne göre veya mutlak, exact dosya yolları.
- `files_present`: bu dört dosyanın **hepsinin** diskte var olup
  olmadığını gösteren `true`/`false` (kısmi "3/4 var" durumu `false`
  yazılır — hepsi ya vardır ya da satır `false`'dur).

Manifest, testin sonunda değil, **her adım tamamlandıkça** güncellenir;
sona bırakılıp tahminle doldurulmaz.

### R.8 — Eksik kanıt = görev bütünü BLOCKED

`manifest.tsv`'de `files_present = false` olan **tek bir satır** varsa,
veya R.6'daki başlangıç/bitiş beş-yol provenance'ı eşleşmiyorsa, **görevin
tamamı** `BLOCKED` olur — yalnız o tek IRB değil. Bu durumda testçi:

- hiçbir IRB için olumlu (`GÖZLENDİ`/`GÖZLENMEDİ`) kapanış hükmü veremez;
- eksik kanıtı tahminle tamamlamaz, "muhtemelen doğruydu" demez;
- `validation-report.md`'yi `BLOCKED — evidence tamamlanmadı` başlığıyla
  kapatır ve tam olarak hangi manifest satırlarının/hangi beş-yol
  karşılaştırmasının eksik/uyumsuz olduğunu listeler.

### R.9 — IRB-3 iki ayrı, birbirine gizlenmemiş sonuç taşır

IRB-3'ün mevcut hipotezi ve fixture'ı **değişmez**, ancak sonuç raporu
artık zorunlu olarak iki ayrı soruya iki ayrı cevap verir; ikinci soru
birincinin "başarı"sı içine gizlenmez:

1. **Semantic kapı IR üretimini engelledi mi?** (`check`/`ir` reddetti
   mi, exit code ve diagnostic var/yok — bu zaten IRB-3 adım 2–3'ün
   konusu.)
2. **Diagnostic gerçekten "aynı ad çakışması" (same-name collision) mı
   açıkladı, yoksa `E_SYMBOL_NOT_IMPORTED` gibi farklı bir
   name-resolution davranışı mı?** Tam diagnostic metni alıntılanır ve
   hangi kategoriye girdiği (same-name collision / başka bir
   name-resolution hatası / belirsiz) ham metne dayanarak ayrı bir
   satırda kaydedilir. "Semantic kapıda reddedildi, dolayısıyla
   çakışma doğru ele alınıyor" gibi bir çıkarım yasaktır — bu iki
   önerme bağımsız olarak raporlanır.

### R.10 — Tam yeniden ölçüm, eski çıktı kopyalanmaz

Yeni izole testçi, kendi **fresh out-of-tree Release build**'i ile
IRB-1–IRB-9'un **tamamını** sıfırdan yeniden çalıştırır. Önceki denemenin
(`evidence/` kök seviyesindeki) `.stdout`/`.stderr`/varsa `.exit`
dosyaları okunmaz, "muhtemelen hâlâ geçerlidir" varsayılmaz, yeni rapora
kopyalanmaz veya referans gösterilmez. Eski çıktıların exit değerleri
tahmin edilmez.

### R.11 — Tek rapor adı, supersession girişte açık

Tek geçerli rapor dosyası adı `validation-report.md` olarak kalır — yeni
bir `validation-report-amendment-03.md` gibi ayrı dosya açılmaz. Yeni
testçi bu dosyayı (mevcut ilk deneme denemesinin üzerine, R.1 gereği o
denemenin ham `evidence/` dosyalarını silmeden) revalidasyon sonucuyla
günceller ve raporun **girişinde** açıkça şunu belirtir: önceki deneme
neden supersede edildi (R.1'deki üç madde — `.cmd` eksikliği, `.exit`
eksikliği, eksik bitiş provenance'ı, IRB-3'ün gizlenmiş ikinci sorusu) ve
bu revalidasyonun hangi kanıt dizininde (`evidence/revalidation-01/`)
tutulduğu.

---

## 0. Kesin yasaklar

- `src/`, `CMakeLists.txt`, `cmake/`, `tests/`, `examples/`, issue veya git
  durumunu değiştirme (bu görev yalnız
  `tasks/SQ-090-IR-BASELINE/validation-report.md` ve
  `tasks/SQ-090-IR-BASELINE/evidence/` altına dosya ekler).
- Production C++ kaynağını okuma (yalnız CLI'nin gözlenebilir davranışı,
  `--help` çıktısı, `tests/golden/numeric/widths.sqt` gibi tracked fixture
  kanıtları ve bu contract'ın verdiği bilgi kullanılır).
- Coder veya mimar yorumunu/raporunu oracle olarak kullanma.
- Bu görevin kendi fresh out-of-tree build'i dışında bir binary ile ölçüm
  yapma (repository içindeki mevcut build dizini kullanılmaz).
- Beklenmeyen/anlaşılmayan bir çıktıyı "beklenen davranış" ilan etme —
  yalnız gözlemi kaydet.
- Fixture syntax'ını tahminle "doğru" kabul etme — §2'deki doğrulama adımı
  atlanmaz.
- 98 opcode'un tamamı için fixture üretme (IRB-5 yalnız 3 temsilî sınıf +
  1 kontrol grubu).
- `run` ile eksik-main davranışını (IRB-6) kabul kriteri sayma; bu
  SQ-090-RUN-ENTRYPOINT kapsamındadır.
- Dump formatını, opcode/flag kümesini, `--types`'ı veya ANSI davranışını
  düzeltme/değiştirme önerisi üretme — yalnız kaydet.
- Fixture'ları gerekli bayraklar olmadan çalıştırıp yanlışlıkla BLOCKED
  üretme (bkz. §6 — capability fixture'ı `--allow-fs` olmadan asla
  çalıştırılmaz).
- Bu görevin sonunda §Son kontrol'ü atlayarak raporu tamamlama.

---

## 1. Build ve provenance — ZORUNLU, her şeyden önce

Bu görev **fresh clone kullanmaz**. Mevcut çalışma ağacı HEAD'den farklıdır
ve bu farkın bir kısmı (`src/cli/commands/run.hpp`,
`src/cli/commands/check.hpp`, `src/cli/commands/ir.hpp`,
`src/cli/exit_codes.hpp`) görevle doğrudan ilgili aktif kaynak
değişiklikleridir. HEAD'i clone etmek güncel 0.9 kaynağını değil eski
kaynak durumunu test eder ve bu nedenle **yasaktır**.

1. `REPO_ROOT` = bu repository'nin mevcut çalışma ağacının kök yolu (tester
   bunu `git rev-parse --show-toplevel` ile kendisi belirler; clone almaz).
2. Benzersiz build dizini: `mktemp -d /tmp/saqut-sq090-ir-baseline-XXXXXX`
   (bundan sonra `$WORK`). Build **out-of-tree** olarak `$WORK/build`
   içine yapılır. `REPO_ROOT` içindeki mevcut/varsa build dizini
   kullanılmaz, silinmez, değiştirilmez.
3. Provenance kaydı — `$EVIDENCE_DIR/00-provenance.md`
   (bkz. §2 için `EVIDENCE_DIR` tanımı):
   - `git -C "$REPO_ROOT" branch --show-current`
   - `git -C "$REPO_ROOT" rev-parse HEAD`
   - `git -C "$REPO_ROOT" status --short` (başlangıç, tam çıktı — dirty
     olması beklenir, "görevle ilgisiz" denmez; bu dosyaların bir kısmı
     test edilen aktif kaynağın parçasıdır)
   - `git -C "$REPO_ROOT" diff --stat HEAD -- src CMakeLists.txt cmake tests examples`
   - `git -C "$REPO_ROOT" status --short -- src CMakeLists.txt cmake tests examples`
   - `git -C "$REPO_ROOT" diff HEAD -- src CMakeLists.txt cmake tests examples | sha256sum`
     (bu üç komut §2'de tanımlanan **exact aynı beş yolu** kullanır — başlangıç
     ve bitiş karşılaştırması hiçbir farklı yol kümesiyle tekrarlanmaz)
   - şu dört dosyanın exact SHA-256'sı (`sha256sum`), mutlak yolla:
     `"$REPO_ROOT/src/cli/commands/run.hpp"`,
     `"$REPO_ROOT/src/cli/commands/check.hpp"`,
     `"$REPO_ROOT/src/cli/commands/ir.hpp"`,
     `"$REPO_ROOT/src/cli/exit_codes.hpp"`
   - toolchain sürümleri: `cmake --version`, `g++ --version` veya
     `clang++ --version` (hangisi kullanılıyorsa), `ctest --version`
4. Release build:
   ```
   cmake -S "$REPO_ROOT" -B "$WORK/build" -DCMAKE_BUILD_TYPE=Release
   cmake --build "$WORK/build" -j
   ```
   Her komutun tam stdout, stderr ve exit code'u ayrı dosyalara kaydedilir:
   `$EVIDENCE_DIR/01-configure.{stdout,stderr,exit}`,
   `$EVIDENCE_DIR/02-build.{stdout,stderr,exit}`.
   Configure veya build exit code ≠ 0 ise **tüm görev BLOCKED**; hiçbir IRB
   maddesi ölçülmez, tahmini sonuç yazılmaz.
5. Üretilen `saqut` binary'sinin tam yolu, `stat -c%s <yol>` (boyut) ve
   `sha256sum <yol>` çıktısı `$EVIDENCE_DIR/03-binary-provenance.txt`'ye
   kaydedilir. Binary'nin kendisi veya herhangi bir build bağımlılığı
   `$EVIDENCE_DIR` altına **kopyalanmaz** — yalnız yol, boyut, hash.

Bu binary bundan sonra `$SAQUT` olarak anılır. Bütün IRB ölçümleri yalnız
`$SAQUT` ile yapılır.

---

## 2. Evidence dizini ve karşılaştırma yüzeyi

```
EVIDENCE_DIR="$REPO_ROOT/tasks/SQ-090-IR-BASELINE/evidence/revalidation-01"
```

(Amendment 03 — bkz. R.2: bu, önceki amendment'lerdeki `evidence/` kök
dizini tanımının yerini alır. Önceki denemenin kök seviyesindeki dosyaları
R.1 gereği silinmez/değiştirilmez; yeni kanıt yalnız bu `revalidation-01`
alt dizinine yazılır.)

Bütün ham kanıt dosyaları bu tek dizin altına yazılır. Build dizini
(`$WORK/build`) `/tmp` altında kalır; binary veya bağımlılık `$EVIDENCE_DIR`
içine kopyalanmaz. R.3–R.5 gereği, burada üretilen her `.stdout`/`.stderr`
dosyasının yanında zorunlu bir `.cmd` ve `.exit` eşleniği bulunur.

Tester başlangıçta ve bitişte **exact aynı beş yolu** karşılaştırır — başka
bir yol kümesi, alt küme veya üst küme kullanılmaz:

- `src/`
- `CMakeLists.txt`
- `cmake/`
- `tests/`
- `examples/`

Karşılaştırma yöntemi: `git -C "$REPO_ROOT" status --short -- src
CMakeLists.txt cmake tests examples` ve `git -C "$REPO_ROOT" diff HEAD --
src CMakeLists.txt cmake tests examples | sha256sum` başta (§1.3'te zaten
bu exact beş yolla alınan) ve bitişte (§Son kontrol'de yine bu exact beş
yolla) tekrar çalıştırılır; iki çıktı birebir eşleşmelidir.
`validation-report.md` ve `evidence/` dizininin `tasks/SQ-090-IR-BASELINE/`
altına eklenmesi bu görevin beklenen çıktısıdır ve "bütün worktree birebir
aynı kalmalı" şartına sokulmaz — yalnız yukarıdaki beş yol karşılaştırılır.

---

## 3. Fixture sözdizimi doğrulama — ZORUNLU, IRB'lerden önce

Herhangi bir fixture çalıştırılmadan önce, aşağıdaki referans dosyalar tam
içerikleriyle `$EVIDENCE_DIR/04-syntax-verification.md`'ye alıntılanır:

- `"$REPO_ROOT/examples/merhaba.sqt"`
- `"$REPO_ROOT/examples/fibonacci.sqt"`
- `"$REPO_ROOT/tests/golden/numeric/widths.sqt"` (tam içerik + `sha256sum`
  çıktısı) —
  bu dosya `longint x = ...` ve `float x = ...` surface sözdiziminin ve
  "bu dilde `float` 32-bit surface tipidir, `LOAD_FLOAT32` yolunu hedefler"
  bilgisinin **tracked** kanıtıdır. IRB-5'in `longint`/`float` alt maddeleri
  bu kanıt karşısında "knowledge-base'de örnek yok" gerekçesiyle önceden
  BLOCKED ilan edilmez.

Doğrulanmış temel biçim: dönüş tipi fonksiyon adından önce yazılır
(`int main() { ... }`), `func`/`:` biçimi **kullanılmaz**, gövde `{}` bloğudur.

Bu contract'taki her fixture, kullanılmadan önce ayrı ayrı, **kendi gerekli
bayraklarıyla** (örn. capability fixture'ı için `--allow-fs`; bkz. §7) ön-
doğrulanır (exit code kaydedilir). Bir fixture kendi gerekli bayrağı
verildiğinde E-kodlu diagnostic ile dönerse ve o fixture'ın amacı zaten bir
diagnostic gözlemlemek değilse, o **spesifik** madde için sonuç
`BLOCKED — fixture derleyici tarafından reddedildi (beklenmeyen diagnostic)`
olarak işaretlenir; tester fixture'ı kendi tahminiyle düzeltmez, PM'e döner.
Fixture'ı gerekli bayrağı vermeden çalıştırıp bu şekilde BLOCKED üretmek
yasaktır.

---

## 4. Ortak ölçüm yöntemi

- Her komut ayrı ayrı çalıştırılır; her biri için ayrı `$EVIDENCE_DIR/IRB-
  <n>-<adım>.{cmd,stdout,stderr,exit}` dosyaları üretilir (`cmd` dosyası
  tam, shell quoting'i belli exact komut satırıdır).
- stdout/stderr karşılaştırmaları **byte-level** yapılır (`diff` veya
  `sha256sum`); insan gözüyle "aynı görünüyor" yeterli değildir.
- **Instruction satırı tanıma (IRB-2 ve IRB-3 için):**
  1. Ham `$SAQUT ir` stdout'u **değiştirilmeden** korunur:
     `$EVIDENCE_DIR/IRB-<n>-dump-raw.stdout`.
  2. Analiz için bu ham dosyanın **ikinci bir kopyası** üretilir ve bu
     kopyadan yalnız ANSI CSI dizileri (`\x1b\[...` biçimindeki kaçış
     dizileri) temizlenir (örn. `sed -E 's/\x1b\[[0-9;]*[A-Za-z]//g'` veya
     eşdeğeri): `$EVIDENCE_DIR/IRB-<n>-dump-clean.stdout`. Ham dosya bu
     işlemden etkilenmez, ayrıca korunur.
  3. Temizlenmiş kopya üzerinde şu regex ile satır eşleştirilir:
     ```
     ^[[:space:]]*[0-9]+[[:space:]]+[A-Z][A-Z0-9_]*
     ```
     (satır başında opsiyonel boşluk, ardından bir tam sayı — instruction
     index'i —, ardından boşluk, ardından büyük harfle başlayan opcode
     adı). Bu regex ile `IR DUMP`, `GLOBALS`, `global[...]`, `NAME=...`
     biçimindeki başlık/metadata satırları, boş satırlar ve `END` dump
     marker'ı zaten eşleşmez (hiçbiri `<sayı> <BÜYÜK-HARF-TOKEN>`
     desenine uymaz) — bunlar için ayrı bir hariç tutma listesi
     tutulmaz. Operand içeriği (örn. satırın geri kalanındaki sabit
     değerler, register adları) sayımı etkilemez; regex yalnız satırın
     başını eşleştirir.
  4. Regex'e uyan **bütün satırlar**, eşleştikleri exact haliyle ayrı bir
     dosyaya yazılır: `$EVIDENCE_DIR/IRB-<n>-instruction-lines.txt`.
  5. Instruction sayısı bu dosyanın `wc -l` sonucudur.
- `--profile` çıktısındaki ilgili satır `StageTimer::printReport` biçimini
  kullanır: sol hizalı aşama adı, ardından sağa hizalı sayı, ardından sola
  hizalı birim (`instr`), ardından ms değeri (bkz. `  ir-gen         <sayı>
  instr    <ms> ms` deseni). Tester `ir-gen` satırındaki `instr` birimli
  sayıyı ayrıştırır. Satır bu desenle eşleşmezse (örn. `--profile` çıktı
  vermiyorsa) ilgili madde BLOCKED yazılır.

---

## IRB-1 — Determinizm

Fixture: `"$REPO_ROOT/examples/fibonacci.sqt"` (dokunulmadan, olduğu gibi
kullanılır, mutlak yolla referanslanır).

1. `$SAQUT ir "$REPO_ROOT/examples/fibonacci.sqt"` **tam olarak üç kez**,
   art arda, temiz shell state'te, `$REPO_ROOT` cwd'sinde çalıştır.
2. Her çalıştırma için: exit code, stdout SHA-256, stderr SHA-256 kaydet.
3. Sınıflandırma:
   - Üç tekrar byte-identical → `GÖZLENDİ — seçilen kaynak ve bu
     binary/ortam için üç çalıştırma byte-identical sonuç üretti.`
   - Üç tekrar byte-identical değil → `GÖZLENMEDİ — ... byte-identical
     sonuç üretmedi` + farkların `diff` çıktısı.
   - Ölçüm (üç çalıştırmanın herhangi biri) çalıştırılamadı (binary yok,
     crash, vb.) → `BLOCKED`.
4. Genel compiler determinizmi ilan edilmez; hüküm yalnız bu exact kaynak ve
   bu binary için yazılır.

---

## IRB-2 — Instruction sayısı

Fixture: `"$REPO_ROOT/examples/fibonacci.sqt"` (mutlak yol; çevre bağımsız,
deterministik `n=10` sabit değeri kullanır — girdi argv/stdin gerektirmez).

1. `$SAQUT ir "$REPO_ROOT/examples/fibonacci.sqt"` çalıştır; §4'teki instruction satırı
   yöntemiyle `main`, `fibonacci`, `fibonacciIterative` gövdelerindeki
   instruction satırlarının toplam sayısını (`wc -l`) kaydet.
2. `$SAQUT run --profile "$REPO_ROOT/examples/fibonacci.sqt"` çalıştır; `ir-gen`
   satırının `instr` sayısını kaydet.
3. Sınıflandırma:
   - Dump instruction sayısı == profile `ir-gen`/`instr` sayısı →
     `GÖZLENDİ`.
   - Sayılar eşit değil → `GÖZLENMEDİ` + fark ham olarak kaydedilir.
   - İki sayıdan biri güvenilir biçimde ayrıştırılamadı (örn. `--profile`
     beklenen satırı basmadı, veya `ir` dump'ı boş/hata) → `BLOCKED`.
4. Bu sayı runtime dispatch sayısı olarak yorumlanmaz; yalnız "dump satır
   sayısı" ile "ir-gen profil sayacı" karşılaştırması olarak raporlanır.

---

## IRB-3 — Çapraz-modül aynı adlı fonksiyon

Fixture grafiği (`$WORK/fixtures/irb3/`), yalnız `shared` adının çakışmasını
izole edecek şekilde tasarlanmıştır — `main` tektir, import edilen adlar
(`fromA`, `fromB`) birbirinden farklıdır:

`a.sqt`:
```
int shared(int x) {
    return x + 1;
}

export int fromA() {
    return shared(5);
}
```

`b.sqt`:
```
int shared(int x) {
    return x + 2;
}

export int fromB() {
    return shared(5);
}
```

`main.sqt`:
```
import {fromA} from "a.sqt";
import {fromB} from "b.sqt";

int main() {
    print(fromA());
    print(fromB());
    return 0;
}
```

**Pozitif kontrol — önce bu ölçülür** (`single-main.sqt`):
```
import {fromA} from "a.sqt";

int main() {
    print(fromA());
    return 0;
}
```

**cwd kesin kuralı:** Bu IRB'deki bütün komutlar, istisnasız,
`$WORK/fixtures/irb3` dizininde (cwd) çalıştırılır — başka bir cwd'den
göreli yol veya mutlak yol ile çağrılmaz. Her adımın `.cmd` evidence
dosyası şu üç alanı ayrı ayrı, açıkça kaydeder: `cwd: $WORK/fixtures/irb3`,
`argv: [...]` (tam, quoting korunmuş argüman listesi) ve `command:` (tam
çalıştırılan komut satırı). Fixture dosya adları (`a.sqt`, `b.sqt`,
`main.sqt`, `single-main.sqt`) bu sabit cwd içinde göreli olarak
verilir; bu, §Amendment 02'nin "mutlak yol veya explicit cwd" kuralını
IRB-3 için "explicit cwd" seçeneğiyle karşılar.

1. `cd "$WORK/fixtures/irb3" && "$SAQUT" check single-main.sqt`,
   `"$SAQUT" ir single-main.sqt`, `"$SAQUT" run single-main.sqt` (a.sqt aynı
   dizinde) ayrı ayrı çalıştırılır; exit code, stdout, stderr tam
   kaydedilir. Bu pozitif kontrol üçünde de temiz geçmezse (beklenmeyen
   diagnostic veya hata), **asıl çift-modül senaryosu (main.sqt) BLOCKED**
   olarak işaretlenir — `shared` çakışmasının izolasyonu, kendisi
   doğrulanmamış bir temel grafik üzerinde anlamlı ölçülemez; devam
   edilmez.
2. Pozitif kontrol geçerse, aynı cwd'de (`$WORK/fixtures/irb3`), `main.sqt`
   (a.sqt ve b.sqt aynı dizinde) için:
   `"$SAQUT" check main.sqt` → diagnostic üretilip üretilmediği, exit code,
   tam stderr kaydedilir.
3. Reddediliyorsa (diagnostic var, exit ≠ 0): IRB-3 burada durur, sonuç
   `GÖZLENDİ — semantic kapıda diagnostic üretildi` + tam diagnostic metni
   **ve** R.9'daki ikinci soru (diagnostic'in kategorisi: same-name
   collision mı, `E_SYMBOL_NOT_IMPORTED` gibi başka bir name-resolution
   davranışı mı, yoksa belirsiz mi) ayrı bir satırda, birincinin içine
   gizlenmeden raporlanır.
4. Reddedilmiyorsa devam (yine aynı cwd'de):
   - `"$SAQUT" ir main.sqt` → dump'ta `shared` fonksiyon başlığının kaç kez
     göründüğü (grep sayımı, tam eşleşen satırlar alıntılanır);
   - instruction gövdelerinin (a'nın `x+1` mi, b'nin `x+2` mi, ikisi birden
     mi) dump'ta göründüğü — §4 kuralına göre çıkarılan ham instruction
     satırları alıntılanır;
   - `"$SAQUT" run main.sqt` → stdout'ta hangi sonucun (6/7, 6 ve 7, hata mı)
     yazıldığı;
   - `"$SAQUT" run --profile main.sqt` → `ir-gen`'in `instr` sayısı,
     `main.sqt` için IRB-2 yöntemiyle çıkarılan `ir` dump satır sayısıyla
     karşılaştırılır.
5. Tester bu davranışı düzeltmez veya "beklenen" ilan etmez; yalnız ham
   kanıtla raporlar.

---

## IRB-4 — Void fallthrough

Fixture (`$WORK/fixtures/irb4/main.sqt`):
```
void sideEffect() {
    print("once");
}

int main() {
    sideEffect();
    print("after");
    return 0;
}
```

(`void sideEffect()` açık `return;` içermez; `sideEffect()` çağrısından
sonra gözlenebilir ikinci bir işlem — `print("after")` — vardır.)

Bütün komutlar mutlak fixture yoluyla (`"$WORK/fixtures/irb4/main.sqt"`)
çağrılır; cwd önemsizdir çünkü tek dosyalık, importsuz bir fixture'dır.

1. `"$SAQUT" check "$WORK/fixtures/irb4/main.sqt"` → exit code, diagnostic
   var mı.
2. `"$SAQUT" ir "$WORK/fixtures/irb4/main.sqt"` → `sideEffect` fonksiyon
   gövdesinin **son** instruction satırı (§4 yöntemiyle) tam olarak
   alıntılanır; bunun gerçek bir `RETURN` (veya eşdeğeri) olup olmadığı
   kaydedilir.
3. `"$SAQUT" run "$WORK/fixtures/irb4/main.sqt"` → stdout, stderr, exit
   code tam kaydedilir (kaç satır basıldı, hata var mı, exit code ne).
4. Sonuç yalnız mevcut davranışı kaydeder; "RETURN 0 eklenmeli" gibi bir
   öneri veya DoD hükmü yazılmaz.

---

## IRB-5 — Görünmeyen executable operandlar

98 opcode için fixture üretilmez. Yalnız 3 temsilî sınıf + 1 kontrol grubu.
`longint`/`float` surface sözdizimi §3'te tracked `tests/golden/numeric/
widths.sqt` ile zaten kanıtlanmıştır; bu iki alt madde önceden belirsiz
ilan edilmez.

**Kontrol grubu — LOAD_CONST (bugün operandı görünen basit durum):**
```
int main() {
    int x = 42;
    print(x);
    return 0;
}
```
`ir` dump'ında `LOAD_CONST` (veya eşdeğer int sabit yükleme) satırının tam
metni alıntılanır; `42` değerinin satırda görünüp görünmediği kaydedilir.

**LOAD_LONG:**
```
int main() {
    longint x = 4200000000;
    print(x);
    return 0;
}
```
§3 gereği önce `check` ile doğrulanır (`tests/golden/numeric/widths.sqt`
zaten `longint x = ...` biçimini kanıtlar). `check` yine de reddederse
(örn. bu fixture'a özgü başka bir sorun), bu alt madde
`BLOCKED — fixture check tarafından reddedildi (tam diagnostic: ...)`
olarak işaretlenir. Kabul edilirse: `ir` dump'ında `LOAD_LONG` (veya
eşdeğer) satırının tam metni alıntılanır; `4200000000` sabit değerinin
satırda görünüp görünmediği kaydedilir.

**LOAD_FLOAT32:**
```
int main() {
    float x = 1.5;
    print(x);
    return 0;
}
```
Aynı kural: önce `check` ile doğrula (`widths.sqt` `float x = ...`
biçimini ve `float`'ın 32-bit surface tipi olduğunu kanıtlar). Kabul
edilirse `ir` dump'ında `LOAD_FLOAT32` (veya eşdeğer) satırının tam metni
alıntılanır; hedef/kaynak slot veya sabit operandının satırda görünüp
görünmediği kaydedilir.

**requiredCap gerektiren instruction:**
Fixture (`$WORK/fixtures/irb5/cap.sqt`):
```
import {readFile} from fs;

int main() {
    string content = readFile("/tmp/saqut-ir-baseline-never-created");
    return 0;
}
```
Fixture mutlak yolla anılır: `CAP_FIXTURE="$WORK/fixtures/irb5/cap.sqt"`.
Ön doğrulama ve IR komutu **flagsiz çalıştırılmaz**:
- `"$SAQUT" check --allow-fs "$CAP_FIXTURE"`
- `"$SAQUT" ir --allow-fs "$CAP_FIXTURE"`

Bu görev capability host çağrısını `run` ile çalıştırmaz (amaç yalnız IR
içindeki `requiredCap` görünürlüğüdür; `readFile` hedef dosya kasıtlı
olarak var olmayan bir yola işaret eder ve zaten çağrılmaz). `check
--allow-fs` reddederse, tam diagnostic ile `BLOCKED — capability fixture'ı
--allow-fs ile de reddedildi` yazılır — flagsiz denemeyle BLOCKED
üretilmez. Kabul edilirse: `"$SAQUT" ir --allow-fs "$CAP_FIXTURE"` dump
satırında o instruction için capability bilgisinin (örn. bir
`[cap: ...]` eki) görünüp görünmediği tam alıntıyla kaydedilir;
`--capabilities` çıktısıyla karıştırılmaz (bu IRB-7'nin konusudur).

Her fixture için: tam kaynak içeriği + SHA-256 `$EVIDENCE_DIR`'e kaydedilir.

---

## IRB-6 — Main'siz global initializer

Fixture (`$WORK/fixtures/irb6/globals.sqt` — `main` fonksiyonu **içermez**):
```
int counter = 7;
```
(Gözlenebilir initializer: `7` sabit değeri.)

Fixture mutlak yolla anılır: `GLOBALS_FIXTURE="$WORK/fixtures/irb6/globals.sqt"`.

1. `"$SAQUT" ir "$GLOBALS_FIXTURE"` çalıştır.
2. Ayrı ayrı kaydet: `GLOBALS` bölümü var mı (tam metin); `counter` adı
   görünüyor mu; `counter`'ı `7` ile ilişkilendiren bir `STORE_GLOBAL` (veya
   eşdeğer) instruction'ı var mı (tam satır alıntısı).
3. `"$SAQUT" run "$GLOBALS_FIXTURE"` çalıştır; exit code, stdout, stderr tam
   kaydedilir — ancak bu davranış kabul kriteri **değildir**, yalnız ham
   veri olarak kaydedilir (eksik-main exit sözleşmesi
   SQ-090-RUN-ENTRYPOINT kapsamındadır).

---

## IRB-7 — Capabilities modu

Fixture: IRB-5'teki `$CAP_FIXTURE` (aynı dosya, tekrar üretilmez, mutlak
yolla anılır).

1. `"$SAQUT" ir --allow-fs "$CAP_FIXTURE"` (normal mod, capability flag'i
   ile) → tam stdout kaydet.
2. `"$SAQUT" ir --allow-fs --capabilities "$CAP_FIXTURE"` → tam stdout
   kaydet.
3. Ayrı ayrı kaydet: `--capabilities` yalnız özet satır(lar) mı basıyor;
   normal instruction dump'ı bu modda kayboluyor mu; exit code her ikisinde
   de aynı mı; normal (madde 1) dump'ta instruction-seviyesi capability
   bilgisi zaten görünüyor muydu (IRB-5 requiredCap bulgusuna referansla).
4. Bu ölçüm S-4 ürün kararını (capability gösterim biçimi) uygulamaz;
   yalnız bugünkü ham davranışı kaydeder. Mevcut `--allow-fs` biçimi bu
   baseline'ın gözlenen aktif CLI biçimidir; gelecekteki olası `--allow
   fs` ürün kararıyla karıştırılmaz.

---

## IRB-8 — ANSI ve yönlendirme

1. `$SAQUT ir "$REPO_ROOT/examples/fibonacci.sqt" > "$EVIDENCE_DIR/irb8-stdout.raw" 2>
   "$EVIDENCE_DIR/irb8-stderr.raw"` — gerçek dosyaya yönlendirme (terminal
   değil), mutlak fixture yoluyla. Ham dosyalar temizlenmeden korunur.
2. Exact byte yöntemi:
   - `xxd -p "$EVIDENCE_DIR/irb8-stdout.raw" | tr -d '\n' > "$EVIDENCE_DIR/irb8-stdout.hex"`
     (ham dosyanın `xxd -p` çıktısını tek satıra birleştirir).
   - Bu tek satırlık hex dizide `1b5b` (ESC + `[`, yani CSI başlangıcı) alt
     dizisinin kaç kez geçtiği sayılır (örn. `grep -o '1b5b' "$EVIDENCE_DIR/irb8-stdout.hex" | wc -l`).
   - Aynı işlem `irb8-stderr.raw` için de tekrarlanır.
   - `xxd` çıktısında düz `1b\[` (ASCII karakter) araması **kullanılmaz** —
     güvenilir değildir; yalnız hex dize eşleştirmesi (`1b5b`) geçerlidir.
3. Sayı kaydedilir (0 veya N). Terminal görünümü hakkında hiçbir tahmin
   yapılmaz; yalnız yönlendirilmiş dosyadaki ham bayt içeriği raporlanır.

---

## IRB-9 — Mevcut tracked IR testleri

1. `ctest --test-dir "$WORK/build" -N` (`--show-only` eşdeğeri kabul
   edilebilir, ancak dizin her zaman `--test-dir "$WORK/build"` ile açıkça
   belirtilir — cwd'ye güvenilmez) çalıştır; tam çıktı
   `$EVIDENCE_DIR/irb9-inventory.stdout` olarak kaydedilir.
2. Bu envanterden, adında `ir_opt` geçen **golden** testlerin (bkz.
   `CMakeLists.txt`'teki `golden_${TNAME}_ir_opt` adlandırma deseni —
   contract bunu tarif eder, tester kaynağı yorumlamaz, yalnız `ctest -N`
   çıktısındaki tam test adı string'ini okur) exact adları çıkarılır.
   Envanterde iki tane bulunmuyorsa (0, 1 veya 3+ ise), bu sayı ham olarak
   kaydedilir ve madde `BLOCKED — beklenen sayıda ir_opt golden testi
   envanterde bulunamadı (bulunan: N)` yazılır; tahminle "doğru" iki test
   seçilmez.
3. Tam olarak iki tane varsa, her ikisi **anchored** (satırın tamamını
   eşleştiren, yanlış eşleşmeye kapalı) exact adlarla çalıştırılır:
   `ctest --test-dir "$WORK/build" -R '^(<exact isim1>|<exact isim2>)$'`
   (veya iki ayrı `ctest --test-dir "$WORK/build" -R '^<exact isim>$'`
   çağrısı). Regex'in başına `^` ve sonuna `$` eklenmeden çalıştırma
   yasaktır — çünkü anchor'sız bir regex, adı bu iki test adının bir alt
   dizesi olan başka bir testi de yanlışlıkla eşleştirebilir.
4. Kaydedilir: exact ctest komutu (dizin + anchored regex dahil), test
   adları, geçen/kalan sayısı, tam ctest çıktısı, exit code.
5. "Tüm golden testler geçti" veya "tüm compiler testleri geçti" gibi bir
   genelleme yapılmaz; hüküm yalnız bu iki test için yazılır.

---

## Kapsam dışı (bu görevde ölçülmez, kaybolmaz)

- **SQ-090-RETURN-PATH-BASELINE adayı:** non-void if/else, loop, switch,
  try/catch kombinasyonlarında E006/`pathAlwaysReturns` sağlamlığı.
- **SQ-090-OPCODE-REACHABILITY-AUDIT adayı:** katalogdaki 98 opcode'un
  güncel kaynak dilinden üretilebilirlik envanteri (statik kaynak analizi —
  black-box tester görevi değil).
- Dump formatını düzeltmek.
- Yeni opcode/flag eklemek.
- `--types` uygulamak.
- ANSI davranışını değiştirmek.
- JIT/MIR dump'ı.
- IR redesign.
- Performance benchmark.

---

## Sonuç sınıflandırma kuralları

- Her IRB maddesi tek başına `GÖZLENDİ`, `GÖZLENMEDİ` veya `BLOCKED` alır.
  Alt maddeler (IRB-5, IRB-3 adım 4) ayrı ayrı sınıflandırılabilir.
- `GÖZLENMEDİ` yalnız "ölçüm başarıyla yapıldı ve hipotezin **tersi**
  gözlendi" durumunda kullanılır (örn. determinizm kırıldıysa, sayılar
  uyuşmuyorsa) — "ölçülemedi" anlamına gelmez; ölçülemeyen/yürütülemeyen
  durum her zaman `BLOCKED`'tır.
- Gözlenmeyen (GÖZLENMEDİ) hiçbir hipotez için bu validation-report
  otomatik olarak yeni bir implementation task açmaz veya önermez; PM'e
  ham bulgu olarak döner.
- Kapsam dışına genelleme yasaktır: "tüm IR dump'ı X" veya "compiler Y"
  gibi ifadeler yazılmaz; her hüküm yalnız ölçülen exact fixture+binary
  kombinasyonu için geçerlidir.

## Regression test konumu

Bu görev **validation-only baseline**'dır; yeni tracked regression test
eklenmez. Fixture'lar yalnız `$WORK/fixtures/` altında kalır, repository'ye
commit edilmez. İleride bir implementation task açılırsa (bu görevin
kapsamı dışında), o task kendi regression test'ini
`tests/golden/` konumuna ekler.

## Validation-only ek zorunluluklar (özet — yukarıda dağıtılmış halde var)

12. Production source değişikliği yasağı — §0.
13. Fresh ve izole (out-of-tree) build dizini — §1.
14. Başlangıç branch/HEAD/dirty worktree kaydı — §1.3.
15. §2'deki beş yolun (src/, CMakeLists.txt, cmake/, tests/, examples/)
    HEAD'e göre başta ve bitişte aynı kaldığının doğrulanması — §Son kontrol.
16. Her komut için raw stdout/stderr/exit ayrı dosya — §4.
17. Shell quoting ve ham bayt kaydı yöntemi — her `.cmd` dosyası tam quoting
    ile exact komut satırını içerir; IRB-8 ham bayt için hex-dize (`1b5b`)
    yöntemi kullanır.
18. `GÖZLENDİ / GÖZLENMEDİ / BLOCKED` — yukarıda §Sonuç sınıflandırma.
19. Gözlenmeyen hipotez için uygulama task'ı açmama — yukarıda.
20. Test sonunda §2'deki beş yolun değişmediğinin doğrulanması — aşağıda.

## Son kontrol — zorunlu

Rapor tamamlanmadan önce, `git -C "$REPO_ROOT" status --short -- src
CMakeLists.txt cmake tests examples` ve `git -C "$REPO_ROOT" diff HEAD --
src CMakeLists.txt cmake tests examples | sha256sum` tekrar çalıştırılır ve
§1.3'te kaydedilen başlangıç değerleriyle birebir eşleştiği doğrulanır —
tester bu beş yolda hiçbir dosya oluşturmaz/silmez/değiştirmez. Bunun
dışındaki dirty durum (örn. görev dışı önceden var olan değişiklikler)
görevin kabul kriteri değildir; yalnız yukarıdaki beş yol karşılaştırılır.
`tasks/SQ-090-IR-BASELINE/` altına `validation-report.md` ve `evidence/`
eklenmesi beklenen ve izinli değişikliktir.

---

## AGENTS.md §10 — Zorunlu çalışma sonu raporu

Testçi, `validation-report.md`'yi şu başlıklarla bitirir:

1. **Rol ve görev kimliği**
2. **İncelenen kanıt**
3. **Yapılan değişiklik veya karar** (bu görevde: yalnız ölçüm, davranış
   değişikliği yok — açıkça yazılır)
4. **Çalıştırılan komutlar** (exact liste, `evidence/` dosyalarına referans)
5. **DoD durumu** (bu görev DoD'u `Tasarlandı`/`Uygulandı` ilerletmez —
   yalnız ölçüm; `Test Edildi` yalnız SQ-090-IR-FINAL-FORM-AUDIT'in kendi
   kabul kriterleri karşılanırsa ayrı bir kararla değerlendirilir)
6. **Kanıtlanmayanlar** (her `BLOCKED` madde tek tek burada listelenir)
7. **Riskler ve regresyon yüzeyi**
8. **Sonraki yetkili rol** (bulgular ürün sahibine/ağır mimara döner)
