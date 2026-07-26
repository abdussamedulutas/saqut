# SQ-090-CLI-BASELINE — Validation Contract Amendment 02 (VALIDATION-ONLY)

**Görev kimliği:** SQ-090-CLI-BASELINE-AMENDMENT-02
**Hedef sürüm:** 0.9.0
**Contract modu:** VALIDATION-ONLY
**Rol:** İzole Hafif Muhalif Testçi (yeni, bağımsız oturum — amendment-01
testçisinin geçmişini görmez)
**Kabul edilmiş karar:** `tasks/SQ-090-CLI-VM-CONTRACT/decision.md`
**Değiştirdiği/eklediği belge:** `tasks/SQ-090-CLI-BASELINE/validation-contract-amendment-01.md`
ve `validation-report-amendment-01.md` (bunlar **silinmez, değiştirilmez** —
bu amendment onları düzeltir/tamamlar)
**Denetim revizyonu (bu amendment yazılırken repo HEAD):** `7f871b75e917725dcdf46111fab88fb3be5663f2` (dal `0.9.0`)

Bu görev bir kodlama veya test çalıştırma görevi değildir. Yalnız bu contract
yazılmıştır; build/test çalıştırılmamıştır.

---

## 0. Bağlayıcı mimari hükümler (bu amendment'ın kaynağı)

### 0.1 `int x = ;` normatif olarak hatalıdır

`validation-report-amendment-01.md` §5.10, F3 (`int x = ;`) için derleyicinin
`exit=0` ve boş diagnostic ürettiğini gözlemlemiş; bundan **"F3 gerçek bir
syntax hatası içermez"** yorumunu çıkarmıştır.

**Bu yorum kabul edilmez.** `int x = ;` bir eksik initializer expression'dır
ve saQut dil sözleşmesinde normatif olarak geçersizdir. Derleyicinin bunu
sessizce kabul etmesi, fixture'ın geçerli olduğunu değil, **eksik initializer
expression'ın parser tarafından sessizce yutulduğunu (silent-accept
davranışı)** gösterir. Bu tam olarak decision.md §1(1) ve §3.3'teki
"Bozuk veya eksik alanlar geçerliymiş gibi sessizce yutulmaz" invariant'ının
ihlali sınıfına girer — invariant'ın ihlal edilip edilmediği bu amendment'ın
ölçtüğü şeydir, ihlalin var olmadığı sonucu değildir.

**Bu amendment F3'ün ham kanıtını yeniden çalıştırmaz.** Amendment-01'in F3
üzerindeki mevcut ham kanıtı (`evidence/amendment-01/cmd-02-*`,
`cmd-17-*`, `cmd-21-*`, `cmd-25-*`, `cmd-28-*`, `cmd-32-*`) **korunur ve
geçerli kabul edilir** — yalnız **yorumu** değişir: "F3 syntax hatası
içermiyor" değil, **"F3 üzerinde eksik initializer sessizce kabul
ediliyor — silent-accept GÖZLENDİ"** olarak okunur. Testçi bu satırı
raporunda düzeltir; yeniden komut çalıştırmaz.

### 0.2 İkinci fixture: gerçek, tracked bir syntax-error kaynağı

`int x = ;`'in derleyici tarafından reddedilmemesi, decision.md'nin CLI
syntax-error davranışını ölçme ihtiyacını ortadan kaldırmaz. Bunun için
**gerçek ve tracked** bir syntax-error kaynağı kullanılır:

```
tests/lsp/fixtures/syntax_error_recovery.sqt
```

İçerik (bu amendment'ın yazıldığı anda okunmuş, birebir):

```
int broken() {
    )
    return 0;
}

int topla(int a, int b) {
    return a + b;
}

int main() {
    int x = topla(2, 3);
    print(x);
    return 0;
}
```

Bu fixture için **tracked LSP beklentisi** (`tests/lsp/09_syntax_error_recovery.expected.jsonl`,
2. satır) şunu ilan eder: `)` token'ında (satır 1, karakter 4-5 — 0-indexed;
kaynaktaki 2. satır) `E901` — `"unexpected token ')' — expected a
statement"`. Bu, LSP protokolü için **tracked ve kabul edilmiş** bir
beklentidir. Bu amendment, **CLI'ın aynı kaynak üzerinde aynı sınıf hatayı
üretip üretmediğini** ölçer — LSP beklentisini CLI'a otomatik olarak
doğru saymaz, yalnız karşılaştırma referansı olarak kullanır.

---

## 1. Kesin yasaklar (tekrar)

- `validation-contract.md`, `validation-contract-amendment-01.md`,
  `validation-report.md`, `validation-report-amendment-01.md` veya
  `evidence/` (kök ve `amendment-01/`) altındaki hiçbir dosyayı silme,
  yeniden adlandırma, üzerine yazma.
- `src/`, tracked test, issue, git durumu değiştirme.
- Bu amendment'ın kendi fresh build'i dışında build/test/binary çalıştırma.
- `implementation-contract.md` veya coder prompt'u üretme.
- `src/` altındaki C/C++ implementasyonunu okuma.
- Amendment-01 testçisinin akıl yürütmesini/geçmişini görme veya varsayma —
  yalnız bu contract, `validation-report-amendment-01.md`'nin ilgili
  bölümü ve genel proje belgeleri girdidir.
- Expected çıktıyı mevcut implementasyona uydurma; implementasyonun ürettiği
  sonucu "beklenen" ilan etme.
- Sonucu PASS/FAIL olarak sunma — yalnız GÖZLENDİ/GÖZLENMEDİ/BLOCKED.
- "CLI deterministiktir" gibi kapsamı aşan genelleme yapma.
- Build veya test bu görev kapsamında **çalıştırılmaz** demek yanlış olur —
  bu amendment'ın validation-only doğası gereği testçi build/binary'yi
  **çalıştırır** (ölçüm amacıyla); ama bu contract'ı yazan (PM) rolü hiçbir
  build/test çalıştırmaz.

---

## 2. Fixture'lar

### 2.1 F3-amended — mevcut, yeniden çalıştırılmaz

Amendment-01'in F3'ü (`int x = ;`, `syntax_error.sqt`) ile ilgili hiçbir yeni
komut çalıştırılmaz. Yalnız mevcut ham kanıt yeniden okunur ve §0.1'deki
düzeltilmiş yorumla rapora aktarılır.

### 2.2 F-LSP — yeni, tracked kaynaktan kopya

`tests/lsp/fixtures/syntax_error_recovery.sqt` içeriği **birebir**, izole bir
`mktemp -d /tmp/saqut-sq090-amendment02-fixtures-XXXXXX` dizinine
`syntax_error_recovery.sqt` adıyla kopyalanır (`cp`, elle yeniden yazılmaz —
kopyalama komutu `command.txt` benzeri bir dosyaya kaydedilir). Kaynak dosya
**tests/ altında değiştirilmez**, yalnız okunur ve kopyalanır.

Kopyalama sonrası içerik hash'i (`sha256sum`) hem kaynak hem kopya için
alınır ve **eşit olduğu** doğrulanır; sonuç
`evidence/amendment-02/01-fixture-copy-verification.txt`'e kaydedilir.

---

## 3. Fresh build — ZORUNLU, yeni ve ayrı

```
BUILD_DIR=$(mktemp -d /tmp/saqut-sq090-amendment02-build-XXXXXX)
cmake -S /home/saqut/Masaüstü/saqutcompiler -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR"
```

- Repo kökündeki `build/` **kullanılmaz.**
- Amendment-01'in `/tmp/saqut-sq090-amendment01-build-*` dizini **yeniden
  kullanılmaz** — yeni build yapılır.
- Binary üzerinde `sha256sum` alınır; amendment-01 binary'siyle karşılaştırılır
  (aynı HEAD'de aynı hash bekleniyor — beklenmedik farklılık ayrıca not
  edilir).
- Configure/build stdout/stderr/exit ayrı dosyalara:
  `evidence/amendment-02/02-configure-*.txt`,
  `evidence/amendment-02/03-build-*.txt`.
- Provenance: `evidence/amendment-02/00-provenance.md`
  (branch, HEAD, `git status --short`, toolchain, binary yolu+hash+timestamp).

---

## 4. Komut matrisi — F-LSP üzerinde

`tests/lsp/fixtures/syntax_error_recovery.sqt` kopyası üzerinde şu yüzeyler
**ayrı ayrı** çalıştırılır:

- `run`
- `check`
- `ast`
- `ast --json`
- `ir`
- `symbols`
- `symbols --json`

Her komut için ayrı ham dosyalar:

```
evidence/amendment-02/cmd-NN-<label>.command.txt
evidence/amendment-02/cmd-NN-<label>.stdout.bin
evidence/amendment-02/cmd-NN-<label>.stderr.bin
evidence/amendment-02/cmd-NN-<label>.exit.txt
evidence/amendment-02/cmd-NN-<label>.stdout.xxd   (yoksa "EMPTY")
```

`<label>` örnekleri: `run-lsp-syntax-error`, `check-lsp-syntax-error`,
`ast-lsp-syntax-error`, `ast-json-lsp-syntax-error`, `ir-lsp-syntax-error`,
`symbols-lsp-syntax-error`, `symbols-json-lsp-syntax-error`.

---

## 5. `ast` ve `symbols` için zorunlu ayrı gözlem alanları

`ast`, `ast --json`, `symbols`, `symbols --json` sonuçlarının her biri için
raporda **beş ayrı soru, beş ayrı satır** olarak cevaplanır (evet/hayır +
kanıt referansı, tahmin değil):

1. **E901 görünüyor mu?** (stdout veya stderr'de `E901` string'i veya
   eşdeğer kodlanmış hata var mı — exact arama sonucu yazılır)
2. **Kısmi payload üretiliyor mu?** (AST/symbols çıktısı, hatalı `broken()`
   fonksiyonu dışındaki `topla`/`main` için de veri içeriyor mu, yoksa
   çıktı tamamen mi boş/tamamen mi başarısız)
3. **JSON geçerli mi?** (`ast --json`, `symbols --json` için — çıktı bir
   JSON parser'dan geçirilerek (örn. `python3 -m json.tool` veya `jq .`)
   sözdizimsel geçerliliği mekanik olarak doğrulanır; sonuç ve kullanılan
   komut kaydedilir)
4. **ErrorNode veya eşdeğer açık recovery temsili var mı?** (`ast`/`ast --json`
   çıktısında `ErrorNode`, `MissingToken` veya işlevsel eşdeğeri bir düğüm
   türü görünüyor mu, yoksa hatalı bölge sessizce mi atlanıyor/yanlış
   temsil ediliyor)
5. **Exit parser hatasını yansıtıyor mu?** (komutun exit kodu, kaynağın
   sözdizimsel olarak hatalı olduğu gerçeğiyle tutarlı mı — `0` dönüyorsa bu
   açıkça "hayır" olarak işaretlenir)

Bu beş soru her dört komut (`ast`, `ast --json`, `symbols`, `symbols --json`)
için **ayrı ayrı** tekrarlanır — tek bir ortak cevap seti yazılmaz.

`run`, `check`, `ir` için bu beş soru zorunlu değildir; yalnız standart
stdout/stderr/exit kaydı yeterlidir (§4).

---

## 6. Testçi izolasyonu (tekrar)

- Testçi `src/` altındaki C++ kaynak kodunu okumaz.
- Production source veya tracked test'i değiştirmez (fixture kopyalama
  hariç — kopyalama hedefi izole `/tmp`'tir, `tests/` değil).
- Expected çıktıyı mevcut implementasyona uydurmaz; implementasyonun
  ürettiği sonucu doğru/beklenen ilan etmez.
- LSP `expected.jsonl`'daki `E901` beklentisini CLI için otomatik doğru
  saymaz — yalnız karşılaştırma referansı olarak kullanır, sonucu nötr
  dille raporlar ("LSP E901 bekliyor; CLI şunu üretti: ...").

---

## 7. Evidence düzeni

```
tasks/SQ-090-CLI-BASELINE/evidence/amendment-02/
  00-provenance.md
  01-fixture-copy-verification.txt
  02-configure-stdout.txt / -stderr.txt / -exit.txt
  03-build-stdout.txt / -stderr.txt / -exit.txt
  cmd-NN-<label>.command.txt / .stdout.bin / .stderr.bin / .exit.txt / .stdout.xxd
```

Amendment-01 ve orijinal `evidence/` dosyaları **hiçbir şekilde** üzerine
yazılmaz, silinmez, taşınmaz.

---

## 8. Rapor — ZORUNLU tek yeni çıktı

```
tasks/SQ-090-CLI-BASELINE/validation-report-amendment-02.md
```

Zorunlu bölümler:

1. Bu amendment'ın gerekçesi (§0'ın kısa özeti: F3 yorumu düzeltmesi + yeni
   LSP-tracked fixture'ın eklenme nedeni)
2. F3 yorum düzeltmesi — eski ifade → yeni ifade, referans verilen ham kanıt
   dosyaları (yeniden çalıştırılmadı, yalnız yeniden okundu)
3. Fresh build kanıtı (§3, hash dahil, amendment-01 binary'siyle karşılaştırma)
4. Fixture kopyalama doğrulaması (§2.2, hash eşleşmesi)
5. Komut matrisi sonuçları (§4, 7 yüzey, ham kanıt referanslı tablo)
6. `ast`/`ast --json`/`symbols`/`symbols --json` için §5'teki beş soru ×
   dört komut = 20 ayrı cevap satırı
7. LSP beklentisi ile CLI gözlemi karşılaştırması (nötr dil, "eşleşiyor" /
   "eşleşmiyor" / "kısmen eşleşiyor" + kanıt)
8. decision.md §1'deki sekiz yapısal tespitten bu amendment'ın kanıtladığı/
   kanıtlamadığı olanların GÖZLENDİ/GÖZLENMEDİ/BLOCKED sınıflandırması
   (yalnız bu amendment'ın kapsadığı maddeler; diğerleri "bu amendment
   kapsamı dışında, amendment-01'deki sınıflandırma geçerli" diye işaretlenir)
9. DoD'nin hâlâ **Tasarlandı** kaldığının teyidi
10. Final not: bu amendment build/test'i **çalıştırdı** (validation-only
    ölçüm amacıyla) — bu, kaynağı/tracked testi değiştirdiği anlamına
    gelmez; `git status --short` (görev sonu) ham çıktısıyla teyit edilir,
    placeholder kullanılmaz.

---

## 9. Durma koşulları

- Fresh build başarısız olursa (configure/build exit ≠ 0) — sonraki adımlara
  geçilmez, yalnız build hatası ve BLOCKED sınıflaması raporlanır.
- `tests/lsp/fixtures/syntax_error_recovery.sqt` bulunamaz veya kopyalama
  hash doğrulamasından geçemezse — fixture kullanılmadan durulur, BLOCKED
  raporlanır.
- Beklenmeyen crash/segfault/sonsuz döngü — `timeout 10s` uygulanır, olduğu
  gibi kaydedilir.
- Contract'ta tanımsız bir CLI bayrağı/çıktı biçimiyle karşılaşılırsa —
  tahmin edilmez, olduğu gibi kaydedilip BLOCKED olarak işaretlenir.
