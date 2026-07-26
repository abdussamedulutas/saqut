# SQ-090-CLI-BASELINE — Validation Contract Amendment 01 (VALIDATION-ONLY)

**Görev kimliği:** SQ-090-CLI-BASELINE-AMENDMENT-01
**Hedef sürüm:** 0.9.0
**Contract modu:** VALIDATION-ONLY
**Rol:** İzole Hafif Muhalif Testçi (yeni, bağımsız oturum — önceki testçi
oturumunun geçmişini görmez)
**Kabul edilmiş karar:** `tasks/SQ-090-CLI-VM-CONTRACT/decision.md`
**Değiştirdiği/eklediği belge:** `tasks/SQ-090-CLI-BASELINE/validation-contract.md`
(orijinal contract **silinmez, değiştirilmez** — bu amendment onu tamamlar)
**Denetim revizyonu (bu amendment yazılırken repo HEAD):** `7f871b75e917725dcdf46111fab88fb3be5663f2` (dal `0.9.0`)

Bu amendment, orijinal `SQ-090-CLI-BASELINE` görevinin fixture sözdizimindeki
tespit edilmiş kontaminasyonu düzeltmek ve bu kontaminasyondan etkilenen tüm
baseline maddelerini yeniden ölçmek içindir. Coder görevi **değildir**.

---

## 0. Amendment gerekçesi (bağlayıcı bulgu)

Orijinal `validation-report.md` §3, F1–F10 fixture'larının

```
func main(): int { ... }
```

biçimini kullandığını ve bunun derleyici tarafından tanınmadığını kendi
raporunda tespit etmiştir (`knowledge-base/02_Language.md` ve
`examples/fibonacci.sqt`, `examples/merhaba.sqt` ile çapraz kontrol edilmiş).
Bu tespit bu amendment'ın başlangıç noktasıdır ve **yeniden tartışılmaz.**

Güncel, iki bağımsız public örnekle doğrulanmış sözdizimi:

```
int main() {
    print("Merhaba");
    return 0;
}
```

(`examples/merhaba.sqt`, `examples/fibonacci.sqt`). Dönüş tipi fonksiyon
adından **önce** yazılır, `func`/`:` biçimi kullanılmaz, gövde `{}` bloğudur.

Sonuç: F1–F10 üzerinde çalıştırılan **her** komut (§4.1–4.3, §4.4 ir,
§4.5–4.6 symbols orijinal raporda) parser hatası ile kontamine olmuştur ve
decision.md §1'deki hipotezleri **gözlemlemek için kullanılamaz.** Bu
kombinasyonlar bu amendment ile yeniden ölçülmelidir.

`F11–F13` (exec snippet'leri) ve `F5` (boş dosya) sözdizimi hatasından
etkilenmez — bunlar dosya-tabanlı `func`/`int` ayrımına tabi değildir; bu
amendment onları yalnız §8 (argv görünürlüğü) ve §7 (determinizm kanıtı
sıkılaştırma) gerekçesiyle yeniden ele alır, syntax nedeniyle değil.

---

## 1. Kesin yasaklar (tekrar, değişmeden)

- Orijinal `validation-contract.md`, `validation-report.md` veya
  `evidence/` altındaki hiçbir dosyayı silme, yeniden adlandırma, üzerine
  yazma.
- `src/`, tracked test, issue, git durumu değiştirme.
- Build/test/binary'yi bu amendment'ın kendi fresh build'i dışında çalıştırma.
- `implementation-contract.md` veya coder prompt'u üretme.
- `src/` altındaki C/C++ implementasyonunu okuma.
- Önceki testçi oturumunun akıl yürütmesini/geçmişini görme veya varsayma —
  yalnız bu amendment ve genel proje belgeleri girdidir.
- decision.md §4'teki hedef davranışı mevcut davranışmış gibi yazma.
- Baseline sonucunu PASS/FAIL olarak sunma — yalnız
  GÖZLENDİ/GÖZLENMEDİ/BLOCKED.
- Sözdizimini kendi tahminiyle "düzeltme" — yalnız §2'de salt-okunur
  doğrulanmış biçimi kullan; doğrulama başarısızsa BLOCKED yaz, tahmin etme.

---

## 2. Fixture sözdizimi salt-okunur doğrulama — ZORUNLU, önce yapılır

Herhangi bir fixture yazılmadan **önce**, testçi şu iki kaynağı okur ve
`evidence/amendment-01/00-syntax-verification.md`'ye alıntılar:

1. `examples/merhaba.sqt` (tam içerik)
2. `examples/fibonacci.sqt` (tam içerik)
3. `knowledge-base/02_Language.md` içindeki fonksiyon tanımı / `main` ile
   ilgili bölüm (exact satır aralığı ve alıntı ile)

Bu üç kaynak **birbiriyle tutarlıysa**, aşağıdaki §3'teki düzeltilmiş
fixture'lar kullanılır. Tutarsızlık varsa (örn. knowledge-base farklı bir
sözdizimi öneriyorsa) testçi fixture yazmadan durur, bunu
`00-syntax-verification.md`'ye BLOCKED olarak işler ve PM'e döner.

---

## 3. Düzeltilmiş fixture'lar (F1–F10, birebir)

Fixture dizini: yeni ve ayrı bir `mktemp -d
/tmp/saqut-sq090-amendment01-fixtures-XXXXXX`. Orijinal fixture dizini
(varsa hâlâ diskte duruyorsa) **kullanılmaz** — yeniden oluşturulur.

**F1 — geçerli main:**
```
int main() {
    print("hello");
    return 0;
}
```

**F2 — geçerli, main içermeyen kaynak:**
```
int helper() {
    return 42;
}
```

**F3 — syntax hatası (yalnız parser/recovery ölçer):**
```
int main() {
    int x = ;
    return 0;
}
```

**F4 — semantik/tip hatası (yalnız type/semantic ölçer):**
```
int main() {
    int x = "not an int";
    return 0;
}
```

**F5 — sıfır bayt boş dosya** (orijinal F5 ile aynı; syntax'tan etkilenmez,
yeniden `touch` ile oluşturulur — tekrar kullanım için orijinali kopyalamak
yerine yeniden üretilir).

**F6:**
```
int main() {
    return 0;
}
```

**F7:**
```
int main() {
    return 1;
}
```

**F8:**
```
int main() {
    return 255;
}
```

**F9:**
```
int main() {
    return 256;
}
```

**F10:**
```
string main() {
    return "not int";
}
```
Eğer bu, §2'deki salt-okunur kaynaklarla teyit edilemeyen bir dönüş-tipi
sözdizimiyse (örn. `string` dönüş tipinin `main` için sözdizimsel olarak
reddedildiği başka bir yerde görülüyorsa), testçi bunu olduğu gibi kaydeder;
"beklenen" bir sonuca zorlamaz.

**F11, F12, F13** (exec snippet'leri) — orijinal contract'taki içerikle
**aynı**, değişmez:

- F11: `1 + 1`
- F12: `print(1); print(2);`
- F13: `let x = ;`

Her fixture oluşturulduktan hemen sonra içerik hash'i
`evidence/amendment-01/01-fixtures-manifest.txt`'e kaydedilir.

---

## 4. Fresh build — ZORUNLU, yeni ve ayrı

```
BUILD_DIR=$(mktemp -d /tmp/saqut-sq090-amendment01-build-XXXXXX)
cmake -S /home/saqut/Masaüstü/saqutcompiler -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR"
```

- Repo kökündeki `build/` **kullanılmaz.**
- Orijinal baseline'ın `/tmp/saqut-sq090-baseline-*` build dizini
  **yeniden kullanılmaz** — güvenilmez, yeni build yapılır.
- Binary üzerinde `sha256sum` çalıştırılır (orijinal görev yalnız MD5
  kullanmıştı; bu amendment SHA-256 zorunlu kılar).
- Configure/build stdout/stderr/exit ayrı dosyalara:
  `evidence/amendment-01/02-configure-*.txt`,
  `evidence/amendment-01/03-build-*.txt`.
- Eski binary kullanılmadığının mekanik kanıtı (timestamp, sha256,
  farklı path/hash vs. repo `build/saqut` ve vs. orijinal baseline binary'si)
  `evidence/amendment-01/00-provenance.md`'ye yazılır.

---

## 5. Yeniden ölçülecekler — kapsam

### 5.1 F1–F10 komut matrisi (tam tekrar)

`run`, `check`, `ast`, `ast --json`, `ir`, `symbols`, `symbols --json`
yüzeylerinin F1–F10 ile **tüm** kombinasyonları (orijinal contract §4
tablosundaki F1–F10 satırlarının birebir karşılığı) yeni fixture'larla
yeniden çalıştırılır. F11–F13 (exec) de §8 gerekçesiyle yeniden çalıştırılır.

Her komut için ayrı `command.txt` / `stdout.bin` / `stderr.bin` / `exit.txt`
/ `stdout.xxd`, `evidence/amendment-01/cmd-NN-<label>.*` altında.

### 5.2 Main status matrisi

Ayrı bir alt-bölümde açıkça özetlenir (rapora da yansır):

- `return 0` (F6) → `run` exit
- `return 1` (F7) → `run` exit
- `return 255` (F8) → `run` exit
- `return 256` (F9) → `run` exit — aralık dışı, tanımlı davranış gözlemi
- int dışında main (F10) → `run` sonucu
- main bulunmayan geçerli kaynak (F2) → `run` ve `check` sonucu ayrı ayrı

### 5.3 Parser/semantic ayrımı

- F3 (syntax hatası) sonucu **yalnız** parser/recovery davranışı olarak
  etiketlenir; F4 (type hatası) sonucu **yalnız** semantic davranışı olarak
  etiketlenir. İkisi rapor tablosunda karıştırılmaz.
- Her komutta diagnostic kodu (varsa, örn. `E9xx`), stdout, stderr, exit ayrı
  sütunlarda.
- `symbols` (ve `symbols --json`) için: F3'teki parser hatasının `symbols`
  komutunun exit kodunu etkileyip etkilemediği **ayrı bir gözlem satırı**
  olarak sınıflandırılır — decision.md §1(3)'teki tespitle doğrudan ilgili
  (`symbols` exit'i `diag.hasErrors()`'a bağlı ama parser hataları oraya
  ulaşmıyor iddiası).

### 5.4 Sözdiziminden etkilenen CLI invocation vakalarının tekrarı

Orijinal `evidence/06-invocation/` içindeki, F1 (veya eşdeğer geçerli
kaynak) kullanan şu vakalar yeni F1 ile yeniden çalıştırılır:

- `saqut program.sqt` (yeni F1 içeriğiyle `program.sqt`)
- `source.sqt` çalışma dizininde mevcutken argümansız `saqut` (yeni F1
  içeriğiyle)
- açılamayan output path verilen `ast` çağrısı (yeni F1 girdisiyle)

Bunların dışındaki invocation vakaları (argümansız `saqut`, `--help`,
`help`, bilinmeyen komut, `compile`/`parse`/`transpile`/`interpret`, stdin
`-`, source.sqt yokken davranış) **sözdiziminden etkilenmedi** — yeniden
çalıştırılmaz; orijinal `evidence/06-invocation/` kanıtı geçerliliğini
korur. Rapor bunu açıkça belirtir.

### 5.5 Tracked test envanteri (salt okunur + `ctest -N`)

Testçi şunları **salt okunur** inceler (değiştirmez):

- `CMakeLists.txt`
- `cmake/`
- `tests/`

Amendment'ın kendi fresh build dizininde:

```
ctest --test-dir "$BUILD_DIR" -N
ctest --test-dir "$BUILD_DIR" --show-only=json-v1
```

çalıştırılabilir (yalnız listeleme; test **yürütülmez**, `-N` bunu zaten
engeller). Amaç: hangi CLI komutlarının (`run`, `check`, vb.) tracked test
adlarında/komutlarında geçtiğinin envanteri — testlerin geçtiğini kanıtlamak
değil. Çıktı `evidence/amendment-01/04-tracked-test-inventory.txt`'e
kaydedilir.

### 5.6 Determinizm kanıtının sıkılaştırılması

Genel "CLI deterministiktir" hükmü **yasaktır.** Yalnız şu biçimde:

> "Seçilen `<komut> <fixture>` kombinasyonu, bu ortamda üç tekrar boyunca
> byte-identical sonuç üretti."

Ham kanıt dosyası `evidence/amendment-01/05-determinism.md` şunları
**eksiksiz** içerir (boş code block kabul edilmez):

- Test edilen her kombinasyon için 3 koşunun her birinin stdout SHA-256'sı
- Her koşunun stderr SHA-256'sı
- Her koşunun exit kodu
- `diff` (veya `cmp`) komutunun exact çağrısı ve çıktısı (fark yoksa boş
  diff çıktısı da **gösterilir**, "no output" diye açıklanır — sessizce
  atlanmaz)

En az şu kombinasyonlar 3'er kez koşulur: F1+`run` (yeni sözdizimiyle),
F11+`exec`, F3+`ast`.

### 5.7 Final worktree kanıtı

Amendment sonunda, placeholder kullanılmadan, ham komut ve çıktı:

```
git status --short
git diff --stat HEAD -- CMakeLists.txt cmake/ src/ build-release.sh build-debug.sh tests/
```

`evidence/amendment-01/06-final-worktree.txt`'e kaydedilir. Rapor bu
dosyadan **birebir alıntı** yapar, placeholder/özet metin kullanmaz.

### 5.8 Yeniden üretilebilir komut kaydı (argv görünürlüğü)

Her `command.txt` dosyası, özellikle `exec` snippet'leri için, snippet'in
**tek bir argüman** olduğunu görünür kılmalıdır. Kabul edilen biçim:

```
"$BUILD_DIR/saqut" exec $(printf '%q' 'print(1); print(2);')
```

veya eşdeğer bir argv-listesi gösterimi (örn. her argv elemanı ayrı satırda,
tırnaklanmış). Şu biçim **yetersizdir ve kabul edilmez:**

```
saqut exec print(1); print(2);
```

Bu kural F11–F13 kullanan **tüm** command.txt dosyaları için (hem bu
amendment'ta yeniden üretilenler hem de varsa referans verilenler) geçerlidir.

---

## 6. Evidence düzeni — ayrım zorunlu

Tüm yeni kanıt **yalnız** şurada:

```
tasks/SQ-090-CLI-BASELINE/evidence/amendment-01/
  00-provenance.md
  00-syntax-verification.md
  01-fixtures-manifest.txt
  02-configure-stdout.txt / -stderr.txt / -exit.txt
  03-build-stdout.txt / -stderr.txt / -exit.txt
  04-tracked-test-inventory.txt
  05-determinism.md
  06-final-worktree.txt
  cmd-NN-<label>.command.txt / .stdout.bin / .stderr.bin / .exit.txt / .stdout.xxd
  invocation-*.{command.txt,stdout.bin,stderr.bin,exit.txt,stdout.xxd}
```

Orijinal `evidence/` kök dosyaları (amendment dizini dışındakiler) **hiçbir
şekilde** üzerine yazılmaz, silinmez, taşınmaz.

---

## 7. Hipotez yeniden sınıflandırma

decision.md §1'deki sekiz yapısal tespitin (1–8) her biri için, **yalnız
kontamine olmuş komut kombinasyonlarına bağlı olanlar** bu amendment kanıtı
ile yeniden sınıflandırılır. Kontamine olmayanlar (orijinal raporda zaten
geçerli fixture ile — örn. F5 boş dosya, F11–F13 exec — ölçülmüş olanlar)
**değiştirilmez**, orijinal sınıflandırma korunur ve nedeni belirtilir.

Her tespit için rapor satırı şunu taşır:
`<tespit no> | <orijinal sınıflandırma> → <yeni sınıflandırma> | <evidence dosya adı> | <not>`

---

## 8. Rapor — ZORUNLU tek yeni çıktı

```
tasks/SQ-090-CLI-BASELINE/validation-report-amendment-01.md
```

Zorunlu bölümler:

1. Amendment gerekçesi özeti (bu contract §0'ın kısa tekrarı)
2. Sözdizimi doğrulama kanıtı (§2, iki örnek + knowledge-base alıntısı)
3. Fresh build kanıtı (§4, sha256 dahil)
4. Fixture matrisi (F1–F13, hangisinin değiştiği hangisinin aynı kaldığı)
5. Komut matrisi sonuçları (§5.1–5.3, main status ayrı tablo, parser/semantic
   ayrımı ayrı sütun)
6. CLI invocation tekrar sonuçları (§5.4) + "sözdiziminden etkilenmediği için
   yeniden çalıştırılmayan" vakaların listesi ve orijinal evidence referansı
7. Tracked test envanteri (§5.5) — PASS/FAIL iddiası yok, yalnız envanter
8. Determinizm kanıtı (§5.6, hash'ler ve diff çıktılarıyla)
9. Final worktree kanıtı (§5.7, ham git çıktısı, placeholder yok)
10. **İlk raporun hangi bulgularını koruduğu** — ayrı liste
11. **Hangilerini düzelttiği** — ayrı liste, eski/yeni sonuç karşılaştırmalı
12. **Hangilerini geri çektiği** — ayrı liste (örn. "orijinal rapor X'i
    GÖZLENDİ dedi ama kontamine fixture'a dayanıyordu, geri çekiliyor")
13. Sekiz hipotezin **yeni** GÖZLENDİ/GÖZLENMEDİ/BLOCKED durumu (§7 tablosu)
14. Baseline kapısının (decision.md §9) bu amendment ile tamamlanıp
    tamamlanmadığı — açık evet/hayır + gerekçe
15. DoD'nin hâlâ **Tasarlandı** kaldığının teyidi

Rapor decision.md §4'teki hedef tabloyu mevcut davranışmış gibi sunmaz;
yalnız gözlenen ham sonuçları anlatır.

---

## 9. Durma koşulları

- §2'deki salt-okunur sözdizimi doğrulaması başarısız/tutarsız olursa —
  fixture yazılmadan durulur, BLOCKED raporlanır, PM'e dönülür.
- Fresh build başarısız olursa (§4 exit ≠ 0) — sonraki adımlara geçilmez,
  yalnız build hatası ve BLOCKED sınıflaması raporlanır.
- Yeni bir sözdizimi belirsizliği (örn. F10'un dönüş tipi sözdizimi)
  §2 kaynaklarıyla çözülemezse — o fixture için BLOCKED yazılır, tahmin
  edilmez.
- Beklenmeyen crash/segfault/sonsuz döngü — `timeout 10s` uygulanır, olduğu
  gibi kaydedilir.
