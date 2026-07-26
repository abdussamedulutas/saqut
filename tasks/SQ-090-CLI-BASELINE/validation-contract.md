# SQ-090-CLI-BASELINE — Validation Contract (VALIDATION-ONLY)

**Görev kimliği:** SQ-090-CLI-BASELINE
**Hedef sürüm:** 0.9.0
**Contract modu:** VALIDATION-ONLY
**Rol:** İzole Hafif Muhalif Testçi
**Kabul edilmiş karar:** `tasks/SQ-090-CLI-VM-CONTRACT/decision.md`
**Bu belgeyi üreten oturum:** Hafif Teslimat Yöneticisi (bu contract sözleşmedir, kanıt değildir)
**Denetim revizyonu (repo HEAD, contract yazılırken):** `7f871b75e917725dcdf46111fab88fb3be5663f2` (dal `0.9.0`)

Bu görev mevcut compiler davranışını **hiçbir düzeltme yapmadan** ölçer. Coder
görevi değildir. Baseline sonucu PASS/FAIL değildir; her hipotez
**GÖZLENDİ / GÖZLENMEDİ / BLOCKED** olarak sınıflandırılır.

---

## 0. Kesin yasaklar (tekrar)

- `implementation-contract.md` oluşturma.
- Coder prompt'u üretme.
- Production source (`src/**`) veya tracked test (`tests/**`, `cmake/**`)
  değiştirme.
- `build/` dizinini (repo köküne ait, mevcut) kullanma veya değiştirme.
- Issue açma, kapatma veya düzenleme.
- Commit, branch, push veya release işlemi yapma.
- Hedef sözleşmeyi (decision.md §4) mevcut davranışmış gibi yazma.
- Baseline sonucunu PASS/FAIL olarak tasarlama.
- `src/` altındaki C/C++ implementasyonunu okuma.
- Coder oturum geçmişini görme/kullanma (yok; testçi bu konuşmayı görmez).
- Mevcut bug açıklamalarını (#134, decision.md §1) oracle kabul etme.
- decision.md §4'teki **hedef** davranış ile şu an **gözlenen** davranışı
  karıştırma.
- Workaround uygulayıp sonucu başarılı gösterme.

---

## 1. Başlangıç provenance — ZORUNLU, evidence/00-provenance.md

Testçi, çalışmaya başlamadan önce şunları **tek bir dosyaya** kaydeder:

```
tasks/SQ-090-CLI-BASELINE/evidence/00-provenance.md
```

İçerik (her biri ayrı başlık altında, ham komut çıktısıyla):

1. `git rev-parse --abbrev-ref HEAD`
2. `git rev-parse HEAD`
3. `git status --short`
4. Şu dosyaların HEAD'e göre değişip değişmediği (`git diff --stat HEAD -- <path>`
   ve/veya `git status --short -- <path>`):
   - `CMakeLists.txt`
   - `cmake/`
   - `src/`
   - `build-release.sh`, `build-debug.sh`
5. Toolchain kaydı:
   - `g++ --version` (veya `cc --version` — kullanılan derleyici ne ise)
   - `cmake --version`
   - `ninja --version` (varsa)
   - `uname -a`
6. Kullanılacak compiler binary'sinin **exact yolu** (adım 2'de oluşacak,
   burada placeholder olarak yazılır, adım 2 sonrası bu dosya güncellenir):
   `<UNIQUE_BUILD_DIR>/saqut`

Bu dosya oluşturulmadan hiçbir komut matrisine geçilmez.

---

## 2. Fresh build — ZORUNLU, evidence/01-build-*.txt

Repository build tanımı: `CMakeLists.txt` (proje kökü), `build-release.sh`
şu komutları çalıştırır:

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Bu görev için `build/` (repo kökündeki mevcut dizin) KULLANILMAZ.**
Testçi kendi başına benzersiz bir `/tmp` dizini oluşturur, örn.:

```
BUILD_DIR=$(mktemp -d /tmp/saqut-sq090-baseline-XXXXXX)
```

Ve repo kökünden şu exact komutları çalıştırır:

```
cmake -S /home/saqut/Masaüstü/saqutcompiler -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" 2> >(tee /tmp/saqut-sq090-baseline-build-stderr.txt >&2) 1> >(tee /tmp/saqut-sq090-baseline-build-stdout.txt)
```

(Ninja yoksa `-G Ninja`'yı düşürüp varsayılan generator kullanılabilir; bu
sapma `00-provenance.md`'ye not edilir.)

Kaydedilecekler (ayrı dosyalar):

- `tasks/SQ-090-CLI-BASELINE/evidence/01-configure-stdout.txt`
- `tasks/SQ-090-CLI-BASELINE/evidence/01-configure-stderr.txt`
- `tasks/SQ-090-CLI-BASELINE/evidence/01-configure-exit.txt`
- `tasks/SQ-090-CLI-BASELINE/evidence/02-build-stdout.txt`
- `tasks/SQ-090-CLI-BASELINE/evidence/02-build-stderr.txt`
- `tasks/SQ-090-CLI-BASELINE/evidence/02-build-exit.txt`

**Eski binary kullanılmadığının mekanik kontrolü:**

```
ls -la "$BUILD_DIR/saqut"
stat --format='%Y %n' "$BUILD_DIR/saqut"
date +%s
md5sum "$BUILD_DIR/saqut"
```

çıktısı `00-provenance.md`'ye eklenir; binary'nin build komutundan **sonraki**
bir zaman damgası taşıdığı ve repo kökündeki `build/saqut` ile **farklı** bir
yol/hash olduğu (varsa) gösterilir. Tüm sonraki komutlar yalnız
`$BUILD_DIR/saqut`'u exact yol ile çağırır — asla `PATH` üzerinden `saqut`
çağrılmaz, asla repo kökündeki `build/saqut` kullanılmaz.

---

## 3. Fixture'lar — ZORUNLU, izole geçici dizinde

Fixture dizini: `FIXTURE_DIR=$(mktemp -d /tmp/saqut-sq090-baseline-fixtures-XXXXXX)`.
Repository production source'una veya `tests/` altına **hiçbir fixture
yazılmaz**.

Aşağıdaki exact kaynak içerikleri kullanılır (testçi bunları birebir bu
içerikle oluşturur; içerik değiştirilemez):

**F1 — geçerli, main içeren program** (`valid_main.sqt`):
```
func main(): int {
    print("hello");
    return 0;
}
```

**F2 — geçerli, main içermeyen kaynak** (`valid_no_main.sqt`):
```
func helper(): int {
    return 42;
}
```

**F3 — sözdizimi hatalı** (`syntax_error.sqt`):
```
func main(): int {
    let x = ;
    return 0;
}
```

**F4 — semantik/tip hatalı** (`type_error.sqt`):
```
func main(): int {
    let x: int = "not an int";
    return 0;
}
```

**F5 — boş dosya** (`empty.sqt`): sıfır bayt, `touch` ile oluşturulur.

**F6 — main dönüşü 0** (`return_0.sqt`):
```
func main(): int {
    return 0;
}
```

**F7 — main dönüşü 1** (`return_1.sqt`):
```
func main(): int {
    return 1;
}
```

**F8 — main dönüşü 255** (`return_255.sqt`):
```
func main(): int {
    return 255;
}
```

**F9 — main dönüşü 256** (`return_256.sqt`):
```
func main(): int {
    return 256;
}
```

**F10 — int dışında main dönüşü** (`return_non_int.sqt`):
```
func main(): string {
    return "not int";
}
```
(Eğer `saqut` dilinde `main`'in dönüş tipi sözdizimsel olarak zorunlu `int`
ise ve bu dosya parse aşamasında reddediliyorsa, bu **F10'un kendisi bir
semantik/parse hata vakası olarak gözlenir** — testçi bunu olduğu gibi
kaydeder, "beklenen" bir sonuca zorlamaz.)

**F11 — exec expression** (komut satırı argümanı, dosya değil):
```
1 + 1
```

**F12 — exec statement listesi** (komut satırı argümanı):
```
print(1); print(2);
```

**F13 — invalid exec snippet** (komut satırı argümanı):
```
let x = ;
```

Her fixture dosyası oluşturulduktan hemen sonra `cat -A <dosya> | md5sum`
veya eşdeğeri ile içerik hash'i `evidence/03-fixtures-manifest.txt`'e
kaydedilir (fixture'ların tam olarak yukarıdaki içerikle yazıldığının
kanıtı).

---

## 4. Komut matrisi — ZORUNLU

Aşağıdaki yüzeyler, uygun fixture(lar) üzerinde **ayrı ayrı** ölçülür:

- `run`
- `check`
- `ast`
- `ast --json`
- `ir`
- `symbols`
- `symbols --json`
- `exec`

Her (yüzey × fixture) çağrısı için ayrı kanıt dosyaları:

```
evidence/cmd-<NN>-<kisa-ad>.command.txt   (exact çalıştırılan komut, tek satır)
evidence/cmd-<NN>-<kisa-ad>.stdout.bin
evidence/cmd-<NN>-<kisa-ad>.stderr.bin
evidence/cmd-<NN>-<kisa-ad>.exit.txt
evidence/cmd-<NN>-<kisa-ad>.stdout.xxd    (ilk 64 satır `xxd` ham bayt gösterimi; boşsa "EMPTY" yazılır)
```

`NN` sıra numarası (01, 02, ...), `<kisa-ad>` yüzey+fixture'ı tanımlayan
kebab-case bir etiket (örn. `run-valid`, `ast-json-syntax-error`).

**En az şu kombinasyonlar zorunludur:**

| Yüzey | Fixture(lar) |
|---|---|
| `run` | F1, F3, F4, F2, F6, F7, F8, F9, F10, F5 |
| `check` | F1, F2, F3, F4, F5 |
| `ast` | F1, F3, F4, F5 |
| `ast --json` | F1, F3, F4, F5 |
| `ir` | F1, F3, F4 |
| `symbols` | F1, F3, F4, F5 |
| `symbols --json` | F1, F3, F4, F5 |
| `exec` | F11, F12, F13 |

**Tekrar çalıştırma / determinizm:** F1 üzerinde `run`, F11 üzerinde `exec`
ve F3 üzerinde `ast` çağrıları **ayrıca ikişer kez daha** (toplam 3 kez)
çalıştırılır; stdout/stderr/exit'in üç koşuda birebir aynı olup olmadığı
`evidence/04-determinism.md`'de raporlanır (byte-diff veya `diff` çıktısı
dahil).

---

## 5. #134 exact repro — ZORUNLU, ayrı kayıt

En az şu iki çağrı, birbirinden bağımsız ayrı kanıt dosyalarıyla kaydedilir:

```
"$BUILD_DIR/saqut" exec 'print(1);'
"$BUILD_DIR/saqut" exec 'print(1); print(2);'
```

Shell quoting: tek tırnak kullanılır, snippet shell tarafından **tek bir
argüman** olarak `saqut`'a geçmelidir; testçi bunu `evidence/05-issue134/`
altında `ps`/`argv` doğrulaması olmasa da en azından exact komut satırını ve
kullanılan shell'i (`bash -c '...'` mi, doğrudan terminal mi) belgeler.

Kayıtlar:
```
evidence/05-issue134/repro-1.command.txt / .stdout.bin / .stderr.bin / .exit.txt / .stdout.xxd
evidence/05-issue134/repro-2.command.txt / .stdout.bin / .stderr.bin / .exit.txt / .stdout.xxd
```

Rapor, issue'daki iddia edilen sonucu (`stdout: 10`, `exit: 0`) **doğru
varsaymaz**; yalnız bu iki komutun gerçekte ürettiği stdout/stderr/exit'i
yazar ve iddia ile karşılaştırır (eşleşiyor mu, eşleşmiyor mu — nötr dille).

---

## 6. CLI çağırma davranışı — ZORUNLU

Aşağıdakiler exact ölçülür, her biri ayrı kanıt dosyası alır (§4'teki
adlandırma şemasıyla, `evidence/06-invocation/` altında):

- `saqut` (argümansız)
- `saqut --help`
- `saqut help`
- `saqut program.sqt` (F1 içeriğiyle `program.sqt` adlı dosya kullanılarak,
  komut olmadan doğrudan dosya adı verilerek)
- `saqut kesinlikle-bilinmeyen-komut-xyz123`
- `saqut run` (dosya argümanı olmadan)
- `saqut check` (dosya argümanı olmadan)
- `saqut ast` (dosya argümanı olmadan)
- `saqut symbols` (dosya argümanı olmadan)
- `saqut compile`
- `saqut parse`
- `saqut transpile`
- `saqut interpret`
- stdin `-` kullanımı: `echo '<F1 içeriği>' | "$BUILD_DIR/saqut" run -` (veya
  ilgili komutun stdin sözleşimi neyse; testçi denediği exact komutu yazar)
- çalışma dizininde `source.sqt` **varken** argümansız `saqut` davranışı
  (F1 içeriğini `source.sqt` olarak `$FIXTURE_DIR` içine koyup o dizinden
  çağırarak)
- çalışma dizininde `source.sqt` **yokken** argümansız `saqut` davranışı
  (temiz bir geçici dizinden çağırarak)
- açılamayan output path verilen `ast` çağrısı (örn.
  `saqut ast --output /root/no-permission/out.json <F1>` veya erişilemeyen
  bir dizin — testçi ortamında gerçekten yazılamayan bir yol seçer ve seçtiği
  yolu kaydeder)
- `saqut --help` çıktısında görünen komut ve mod listesi (ham metin olarak
  kaydedilir; hangi komutların listelendiği rapora ayrı satır satır aktarılır)

---

## 7. Hipotez eşlemesi — ZORUNLU

decision.md §1'deki sekiz yapısal tespitin (1–8) her biri için raporda
**ayrı bir satır** bulunur. Her satır şu üçlüden birini taşır:

- **GÖZLENDİ** — black-box komut çıktısı, tespiti destekliyor.
- **GÖZLENMEDİ** — black-box komut çıktısı, tespitle çelişiyor veya tespiti
  doğrulamıyor.
- **BLOCKED** — mevcut fixture/komut matrisiyle ölçülemedi; neden yazılır.

Her sınıflandırma, hangi `evidence/cmd-*` veya `evidence/06-invocation/*`
dosyasına dayandığını **exact dosya adıyla** referanslar.

**GÖZLENMEDİ bir tespit test başarısızlığı değildir.** O madde için "bu
tespit için uygulama task'ı açılamaz; ağır mimari incelemeye geri dönülmesi
gerekir" cümlesi rapora eklenir.

---

## 8. Evidence düzeni

Tüm ham kayıtlar şurada tutulur:

```
tasks/SQ-090-CLI-BASELINE/evidence/
  00-provenance.md
  01-configure-stdout.txt / -stderr.txt / -exit.txt
  02-build-stdout.txt / -stderr.txt / -exit.txt
  03-fixtures-manifest.txt
  04-determinism.md
  05-issue134/...
  06-invocation/...
  cmd-NN-<ad>.command.txt / .stdout.bin / .stderr.bin / .exit.txt / .stdout.xxd
```

**Binary, dependency veya geniş generated build çıktısı (`$BUILD_DIR`
içeriği, `.o`, `.ninja_log` vb.) bu dizine kopyalanmaz.** Yalnız metin
kanıtları.

---

## 9. Testçi izolasyonu (tekrar, bağlayıcı)

- `src/` altındaki C/C++ implementasyonu okunmaz.
- Coder oturum geçmişi yok/görülmez.
- Mevcut bug açıklamaları (#134, decision.md §1) oracle sayılmaz; yalnız
  gözlenen çıktı esastır.
- decision.md §4'teki hedef davranış ile mevcut gözlem karıştırılmaz —
  rapor yalnız **şu an ne oluyor**'u anlatır, ne olması gerektiğini değil.
- Production kodu veya tracked test düzeltilmez.
- Workaround uygulanıp sonuç başarılı gösterilmez.

---

## 10. Rapor — ZORUNLU tek çıktı

```
tasks/SQ-090-CLI-BASELINE/validation-report.md
```

İçermesi zorunlu bölümler:

1. Provenance (evidence/00-provenance.md özeti + tam referans)
2. Fresh build kanıtı (configure/build exit kodları, binary yolu, hash)
3. Fixture matrisi (F1–F13, hangi içerikle oluşturulduğu)
4. Komut matrisi (§4 tablosu, her hücre evidence dosyasına referans verir)
5. stdout/stderr/exit sonuçları (özet tablo + evidence referansı; ham veri
   evidence dizininde kalır, rapora kopyalanmaz — yalnız kritik satırlar
   alıntılanabilir)
6. #134 sonucu (§5, iddia ile karşılaştırma, nötr dil)
7. Sekiz yapısal hipotezin sınıflandırması (§7)
8. Gözlenmeyenler (ayrı liste, "GÖZLENMEDİ" sınıflandırılan her madde)
9. Blocked maddeler (ayrı liste, neden + hangi ek bilgi/erişim gerektiği)
10. Hiçbir production source değişmediğinin final git kontrolü:
    `git status --short` çıktısı, görev başlangıcındaki ile karşılaştırmalı
    (rapor sonunda tekrar çalıştırılır)
11. Baseline sonrası açılabilecek ve açılamayacak task'lar (decision.md §10
    tablosundaki 1–9 task'larının her biri için: bu baseline kanıtı bu
    task'ı açmaya yetiyor mu, yetmiyor mu — nötr gerekçeyle)
12. DoD'nin hâlâ **Tasarlandı** olduğunun teyidi (bu görev DoD'yi
    "Uygulandı"ya taşımaz; yalnız decision.md §9 baseline kapısını kanıtla
    doldurur)

Rapor, decision.md §4'teki hedef tabloyu **mevcut davranışmış gibi** asla
sunmaz; yalnız gözlenen ham sonuçları anlatır.

---

## 11. Durma koşulları

Testçi şu durumlarda durur ve PM'e (bu oturuma) döner, tahminle devam etmez:

- Fresh build başarısız olursa (configure veya build exit ≠ 0) — bu durumda
  §4–§7 çalıştırılamaz, rapor yalnız build hatasını ve BLOCKED sınıflamasını
  içerir.
- `saqut` dilinde F1/F3/F4/F10 gibi fixture'ların söz dizimi (örn. `func`
  anahtar kelimesi, `print` çağrı biçimi) mevcut dil sözleşmesiyle
  uyuşmuyorsa (parser bu sözdizimini hiç tanımıyorsa) — testçi
  `knowledge-base/02_Language.md` ve `03_Frontend.md`'den (bunlar okunması
  serbest, `src/` değil) doğru yüzeysel sözdizimini teyit eder; hâlâ
  belirsizse fixture'ı "BLOCKED — dil sözdizimi teyit edilemedi" olarak işaretler,
  kendi tahminiyle fixture'ı "düzeltmez".
- Contract'ta tanımsız bir komut/bayrak davranışı gözlenirse (örn. beklenmeyen
  bir crash, segfault, sonsuz döngü) — o vakayı olduğu gibi kaydeder, timeout
  uygular (`timeout 10s`), durumu raporlar.
