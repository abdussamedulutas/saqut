# SQ-090-CLI-VM-CONTRACT — 0.9.0 CLI ve Normatif VM Çalıştırma Sözleşmesi

**Görev kimliği:** SQ-090-CLI-VM-CONTRACT
**Hedef sürüm:** 0.9.0
**Tarih:** 2026-07-25
**Karar verenler:** Ürün sahibi, şüpheci başmimar
**Dayanak:** `AGENTS.md`, ADR-042, `docs/v1.0-kapsam-bildirgesi.md`,
`docs/v0.9-v1.0-yol-haritasi.md`, `docs/v1.0-issue-disposition.md`
**Denetim revizyonu:** `7f871b7` (dal `0.8.0`)

**DoD durumu: Tasarlandı.**

Bu belge yalnız kabul edilmiş ürün kararını ve ölçülebilir kabul kriterlerini
kaydeder. Aktif kaynakta hiçbir değişiklik yapılmamıştır. Bu kararın bir sonraki
aşamaya geçmesi, §9'daki baseline kapısının kanıtla tamamlanmasına bağlıdır.

---

## 1. Problem ve kullanıcıya etkisi

0.9.0'ın ürün vaadi, ürün sahibinin VM üzerinde JSON/XML ve CPU ağırlıklı
programlar yazabildiği güvenilir bir geribildirim baseline'ıdır. Bu vaat,
komutların hata durumunda ne yaptığı tanımlı olmadan ölçülemez.

Salt okunur kaynak denetimi (revizyon `7f871b7`) şunları göstermiştir:

1. `ast`, `symbols` ve `exec` `Parser`'ı `DiagnosticEngine`'siz kurar
   (`src/cli/commands/ast.hpp:32`, `symbols.hpp:26`, `exec.hpp:103`;
   `src/parser/parser_base.hpp:23` varsayılan ctor → `diag_ = nullptr`).
   Sözdizimi hatası `src/parser/parser.cpp:88` üzerinden yalnız `stderr`'e
   yazılır, `diag`'a girmez; hiçbir exit kapısı onu göremez.
2. `ast` koşulsuz `return 0` yapar (`src/cli/commands/ast.hpp:85`) ve semantik
   tanıları yalnız `--optimized` verildiğinde basar (`:80`).
3. `symbols` exit kodunu `diag.hasErrors()`'a bağlar (`symbols.hpp:100`) fakat
   parser hataları oraya hiç ulaşmaz; ayrıca `TypeChecker`/`StructuralValidator`
   bu yolda hiç çalışmaz.
4. `run` başarı exit kodunu `main`'in dönüş değeri olarak aktarır
   (`src/vm/interpreter.cpp:659`, `src/cli/commands/run.hpp:159`) ve
   `ValueKind` denetimi yapmaz.
5. `main` yokluğu semantik kapıda değil, VM'de ham `std::runtime_error` olarak
   ortaya çıkar (`src/vm/interpreter.cpp:1277`).
6. `parseArgs` tanınmayan ilk argümanı sessizce `run` sayar
   (`src/cli/args.hpp:175`) ve positional boşsa hayalet `"source.sqt"` ekler
   (`:184-185`). Bunun sonucu olarak `src/cli/cli.hpp:55`'teki `help` dalı ve
   `:68`'deki bilinmeyen-komut dalı ölü koddur.
7. `compile`, `parse`, `transpile`, `interpret` yalnız `TODO` yazıp `1` dönen
   lambda'lardır (`src/main.cpp:82-104`); stdin `-` `readSource` içinde `TODO`
   basıp boş kaynak döndürür (`src/cli/args.hpp:194-197`). Üçü yardım
   ekranında görünür; stdin modu yardımda bir mod olarak ilan edilir
   (`src/cli/cli.hpp:77`).
8. Tracked testler yalnız `run` (ve `tests/run.sh` içinde üç yerde `check`)
   çağırır. `cmake/run_golden.cmake` ve `run_golden_error.cmake` sabit olarak
   `run` kullanır. `ast`, `symbols`, `exec`, `tokens` için tracked test yoktur.
   Sekiz `.compile_error` fixture'ının hiçbiri sözdizimi hatası değildir.
   `E901`/`E903` yalnız LSP JSONL beklenen dosyalarında geçer.

Kullanıcıya etkisi: derleyici, hatalı girdide sessizce başarı bildirebilir;
script ve CI, derleme hatası ile programın kendi çıkışını ayırt edemez;
yayımlanmış CLI belgesi kaynakta çalışmayan bayrak biçimleri anlatır.

---

## 2. Kanıt sınıfı ve sınırı

Bu kararın dayandığı kanıt **AGENTS.md §3 sınıf 3'tür: aktif kaynak ve build
tanımı.**

- Build çalıştırılmadı. Test çalıştırılmadı. Binary çalıştırılmadı.
- Hiçbir gözlenen stdout/stderr/exit değeri bu belgede iddia edilmemiştir.
- Yukarıdaki bulgular davranış hükmü değil, **aktif kod yolunun yapısal
  tespitidir.**

Bu nedenle §9'daki baseline kapısı zorunludur: kararın uygulanabilir hale
gelmesi, aynı bulguların güncel `0.9.0` binary'siyle black-box olarak yeniden
üretilmesine bağlıdır.

---

## 3. Kabul edilen kararlar

### 3.1 Exit sözleşmesi

Non-`run` komutlarda başarı `0`'dır.

| Sınıf | Kod |
|---|---|
| Başarı (non-`run`) | `0` |
| CLI kullanım / argüman hatası | `64` |
| Source, module, parse, semantic veya compile hatası | `65` |
| Yakalanmamış runtime veya compiler internal hatası | `70` |

`run` normal tamamlandığında `main`'in `int` değerini process status olarak
aktarır.

- `main`'in dönüş değeri `0..255` aralığında olmalıdır.
- Aralık dışı veya `int` olmayan dönüş **tanımlı bir diagnostic** üretir.
- Program status'u ile compiler hata kodlarının numerik olarak hiç
  çakışmayacağı **vaat edilmez.** Kaynak, `stderr`'deki diagnostic ile ayırt
  edilir.

> **Önceki taslağın S-1/S-2 çelişkisi kaldırılmıştır.** Taslakta compiler
> hatalarını `main` değerinden ayrı bir numerik aralığa taşıma önerisi vardı.
> Bu öneri geçersizdir. Ayrım numerik aralıkla değil, `stderr` diagnostic'i ile
> yapılır.

### 3.2 `check` ve entrypoint

- `check`, `main` fonksiyonu **gerektirmez.**
- `check` başarısı, kaynağın ve modül grafiğinin **semantik olarak geçerli**
  olduğunu gösterir; **çalıştırılabilir olduğunu göstermez.**
- `run`, VM'e girmeden önce `main`'in varlığını ve imzasını doğrular.
- Eksik veya geçersiz `main`, ham VM exception değil, **structured diagnostic +
  exit 65** üretir.
- 0.9'da `check --entry` benzeri yeni bir bayrak **eklenmez.**

> **Önceki taslağın "`main` zorunluluğunu semantik kapıya taşı" önerisi
> kaldırılmıştır.** Entrypoint doğrulaması `check`'in değil `run`'ın
> sorumluluğudur.

### 3.3 Error-tolerant AST ve `symbols`

Parser sözleşmesi:

- Okunabilir kaynak için parser **best-effort kısmi AST** üretir.
- Sözdizimi hataları AST üretimini tamamen durdurmaz.
- Recovery edilen **her** hata structured diagnostic üretir.
- Bozuk veya eksik alanlar geçerliymiş gibi **sessizce yutulmaz**; `ErrorNode`,
  `MissingToken` veya eşdeğer **açık temsil** kullanılır.

CLI sözleşmesi:

- `ast` ve `symbols` hatalı girdide **kısmi çıktı üretmeye devam eder.**
- Text modunda: kısmi payload `stdout`, diagnostics `stderr`.
- JSON modunda: **her zaman geçerli JSON**; `schemaVersion`, `diagnostics` ve
  kısmi payload birlikte bulunur.
- Sözdizimi veya semantik hata varsa CLI exit **65** olur.
- `run`, `check` ve `ir` diagnostic varken compilation veya execution'a
  **devam etmez.**
- LSP sunucusu belge hatası nedeniyle **kapanmaz**; kısmi modelle hizmet
  vermeye devam eder.

Bu, §1(2) ve §1(3)'teki sahte başarı yollarını kapatırken inceleme araçlarının
hata ayıklama değerini korur: yasak olan çıktı üretmek değil, **exit 0
dönmektir.**

### 3.4 `exec` korunur

`exec`, insanlar ve ajanlar için 0.9'da **desteklenen yardımcı tooling
yüzeyidir.** İkinci ve bağımsız bir compiler pipeline'ı **olamaz.**

> **Önceki taslağın `exec`'i public CLI'dan kaldırma önerisi iptal
> edilmiştir.**

Canonical akış:

```text
exec snippet
  → snippet parser / input adapter
  → run ile ORTAK compile-and-run servisi
  → aynı diagnostics, semantic, IR ve normatif VM
```

Davranış sözleşmesi:

- `saqut exec '1 + 1'` expression olarak değerlendirilir.
- Sonuç standart değer gösterimiyle `stdout`'a ve **tek trailing newline** ile
  yazılır.
- Function-body-compatible statement ve local declaration listesi desteklenir.
- Statement listesi **kendi çıktısından sorumludur**; otomatik sonuç basılmaz.
- Expression/statement ayrımı **string-prefix heuristic ile yapılmaz.**
  (Bugünkü `startsWithStatement` ön-probe'u — `src/cli/commands/exec.hpp:48-79`
  — bu sözleşmeyi karşılamaz.)
- Parser açık bir **`Expression | StatementList | Invalid`** sonucu üretir.
- Invalid snippet structured diagnostic + exit **65** üretir.
- Diagnostic konumları sentetik wrapper'a değil **kullanıcının snippet'ine**
  göre gösterilir.
- `exec` `run` ile **aynı capability bayraklarını** kullanır.
- 0.9 `exec` sözleşmesinin **normatif backend'i VM'dir.**
- Import, modül grafiği ve top-level function/struct declaration snippet'leri
  **0.9 `exec` kapsamı dışındadır.**
- `exec` hızlı keşif aracıdır; **tracked test ve bağımsız validation yerine
  geçmez.**
- Public help'te kaldığı için tracked **pozitif, negatif ve sınır testleri
  zorunludur.**

### 3.5 CLI çağırma sözleşmesi

- Çalıştırma açıkça `saqut run program.sqt` biçimindedir.
- `saqut program.sqt` **örtük `run` kısayolu kaldırılır** (`src/cli/args.hpp:175`).
- Bilinmeyen komut **dosya adı sayılmaz**; açık diagnostic + exit **64** üretir.
- Hayalet `source.sqt` varsayımı **kaldırılır** (`src/cli/args.hpp:184-185`).
- Argümansız `saqut`, `saqut --help` ve `saqut help` **global yardım** gösterir.
  (`src/cli/cli.hpp:55`'teki bugün ulaşılamaz olan `help` dalı bu kararla
  gerçek bir yol haline gelir.)
- `compile`, `parse`, `transpile`, `interpret` ve uygulanmamış stdin `-`
  yüzeyleri **dispatch ve help'ten kaldırılır.**
- `tokens` yardımcı inceleme aracı olarak kalabilir; **0.9 release engeli
  değildir.**
- `bench` public help'ten **gizlenir** ve 0.9 sözleşmesine **dahil edilmez.**
- Output dosyası açılamazsa `stdout`'a **sessiz fallback yapılmaz**
  (bugünkü davranış: `src/cli/commands/ast.hpp:64`).

### 3.6 Issue yönetimi

- **#134 henüz kapatılmaz veya bölünmez.**
- **Yeni issue açılmaz.**
- Önce güncel `0.9.0` binary ile black-box baseline üretilir.
- Kanıt geldikten sonra #134 doğru kapsamla yeniden yazılır.

Gerekçe: #134'ün kök neden açıklaması kaynakta doğrulanmıştır
(`src/cli/commands/exec.hpp:103` diag'siz parser; `:104-109` yalnız
`ast == nullptr` denetimi; ardından koşulsuz devam). Fakat issue'daki gözlenen
çıktı (`stdout: 10`, `exit: 0`) yeniden üretilmemiştir. AGENTS.md §4 gereği
doğru ifade **"uygulama iddiası doğrulandı, davranış yeniden doğrulanmadı"**
biçimindedir; bu nedenle issue'ya kanıtsız dokunulmaz.

---

## 4. Kullanıcıya görünen davranış (hedef)

| Yüzey | Girdi | stdout | stderr | exit |
|---|---|---|---|---|
| `run` | geçerli program | program çıktısı | — | `main` değeri (0..255) |
| `run` | parse/semantic hata | — | structured diagnostic | 65 |
| `run` | `main` yok/geçersiz imza | — | structured diagnostic | 65 |
| `run` | `main` dönüşü aralık dışı / `int` değil | program çıktısı | tanımlı diagnostic | tanımlı davranış |
| `run` | yakalanmamış runtime hatası | kısmi program çıktısı | runtime diagnostic | 70 |
| `check` | geçerli kaynak (`main` gerekmez) | JSON (`schemaVersion` + `diagnostics`) | — | 0 |
| `check` | hatalı kaynak | JSON (`schemaVersion` + `diagnostics`) | — | 65 |
| `ast` / `symbols` | geçerli kaynak | tam payload | — | 0 |
| `ast` / `symbols` | hatalı kaynak (text) | **kısmi payload** | diagnostics | 65 |
| `ast` / `symbols` | hatalı kaynak (JSON) | **geçerli JSON**: `schemaVersion` + `diagnostics` + kısmi payload | — | 65 |
| `ir` | geçerli kaynak | IR dump | — | 0 |
| `ir` | hatalı kaynak | — | diagnostics | 65 |
| `exec` | `'1 + 1'` | değer + tek trailing newline | — | 0 |
| `exec` | statement listesi | yalnız programın kendi çıktısı | — | 0 |
| `exec` | invalid snippet | — | snippet konumlu diagnostic | 65 |
| herhangi | bilinmeyen komut | — | açık diagnostic | 64 |
| herhangi | argüman/kullanım hatası | — | açık diagnostic | 64 |
| `saqut`, `--help`, `help` | — | global yardım | — | 0 |

Yardım ekranı çalışmayan hiçbir komut veya mod içermez.

---

## 5. Kapsam içi

- Exit sözleşmesinin (`0/64/65/70` + `run` status aktarımı) tek bir yerde
  tanımlanması ve komutlara eşlenmesi.
- Kaynak metni okuyan her komutun `DiagnosticEngine` ile aynı tanı kapısını
  kullanması.
- Parser'ın recovery edilen her hata için structured diagnostic üretmesi ve
  bozuk alanları açık temsille (`ErrorNode`/`MissingToken` veya eşdeğer)
  göstermesi.
- `ast` ve `symbols` için error-tolerant kısmi çıktı + doğru exit.
- `check` ve makine-okunur çıktılarda `schemaVersion`.
- `run` içinde `main` varlık/imza doğrulaması ve dönüş değeri sözleşmesi.
- `exec`'in `run` ile ortak compile-and-run servisine bağlanması ve açık
  `Expression | StatementList | Invalid` sonucu üreten snippet adapter'ı.
- Örtük `run` kısayolu, hayalet `source.sqt`, ölü `help`/bilinmeyen-komut
  dallarının kaldırılması.
- Stub yüzeylerin (`compile`, `parse`, `transpile`, `interpret`, stdin `-`)
  dispatch ve help'ten çıkarılması; `bench`'in help'ten gizlenmesi.
- `run` dışındaki komutları da çağırabilen tracked test altyapısı.

## 6. Kapsam dışı

- JIT feature parity, JIT davranış tasarımı, VM≡JIT kapısı (ADR-042 §3: JIT
  `[EXPERIMENTAL]`).
- AOT, `saqut build`, bundled-runtime executable packaging.
- LSP/DAP yetenek matrisinin içeriği ve implementasyonu.
- Concurrency, sandbox, süreç izolasyonu.
- Self-hosted stdlib ve ADR-041 katmanlaması.
- Yeni CLI komutu veya yeni bayrak icadı (`check --entry` dahil).
- Diagnostic kataloğunun tamamlanması, çok-konumlu span, fix-it (#131 ayrı
  kayıt).
- Parser'ın hata kurtarma mimarisinin **yeniden tasarımı**. Kapsam içi olan,
  recovery edilen hataların tanı üretmesi ve açık temsil kullanılmasıdır.
- `bench` metodolojisi ve performans iddiaları.
- `tokens` yüzeyinin kalıcılaştırılması.
- Import, modül grafiği ve top-level declaration desteğinin `exec`'e eklenmesi.
- Herhangi bir issue'nun kapatılması, bölünmesi veya yeni issue açılması.

---

## 7. Ölçülebilir kabul kriterleri (black-box)

Her kriter, güncel `0.9.0` binary'si üzerinde stdout, stderr ve exit ayrı ayrı
kaydedilerek doğrulanabilir olmalıdır.

**K-1.** Sözdizimi hatalı bir dosya için `run`, `check` ve `ir` exit **65**
döner ve `stdout`'a program veya IR çıktısı basmaz.

**K-2.** Sözdizimi hatalı bir dosya için `ast` ve `symbols` exit **65** döner,
kısmi payload üretir ve recovery edilen her hata için structured diagnostic
gösterir.

**K-3.** Semantik hatalı bir dosya için K-1 ve K-2 aynı biçimde geçerlidir.

**K-4.** Geçerli ve hatasız bir program için beş yüzey de exit **0** döner.

**K-5.** `check`, `main` içermeyen semantik olarak geçerli bir kaynak için exit
**0** döner.

**K-6.** Aynı kaynak `run` ile çağrıldığında structured diagnostic + exit **65**
üretir; ham VM exception metni görünmez.

**K-7.** `int main() { return N; }` için `run` process status'u `N`'dir
(`0 ≤ N ≤ 255`).

**K-8.** `main` dönüşü aralık dışı veya `int` değilse tanımlı diagnostic üretilir.

**K-9.** `check --json` ve `ast`/`symbols` JSON modu **her girdide** — mevcut,
olmayan, boş ve hatalı dosya dahil — geçerli JSON üretir ve `schemaVersion`
alanı taşır.

**K-10.** `saqut exec '1 + 1'` sonucu tek trailing newline ile `stdout`'a yazar
ve exit **0** döner.

**K-11.** Function-body-compatible bir statement listesi için `exec` yalnız
programın kendi çıktısını basar; otomatik sonuç basmaz.

**K-12.** Invalid snippet için `exec` structured diagnostic + exit **65** üretir
ve diagnostic konumu **kullanıcının snippet'ine** göre gösterilir.

**K-13.** `exec` ve `run`, aynı semantik girdi için aynı diagnostic kodunu
üretir (ortak servis kanıtı).

**K-14.** `saqut program.sqt` artık programı çalıştırmaz; bilinmeyen komut
diagnostic'i + exit **64** üretir.

**K-15.** Bilinmeyen komut hiçbir koşulda dosya adı olarak yorumlanmaz.

**K-16.** Dosya argümanı verilmeden çağrılan komut, `source.sqt` aramaz; exit
**64** ile kullanım hatası üretir.

**K-17.** `saqut`, `saqut --help` ve `saqut help` aynı global yardımı gösterir
ve exit **0** döner.

**K-18.** `compile`, `parse`, `transpile`, `interpret` ve `-` yardımda görünmez
ve dispatch edilmez.

**K-19.** `bench` yardımda görünmez.

**K-20.** Mevcut `ast --output <path>` yüzeyinde açılamayan bir output dosyası
verildiğinde çıktı `stdout`'a düşmez; hata bildirilir. Bu kriter `--output`
bayrağını başka komutlara yaymaz ve AST yüzeyinden kaldırmaz. Bayrağın
desteklenmediği komutlarda sessizce yutulup yutulmayacağı ve exact exit-code
davranışı `SQ-090-CLI-INVOCATION` contract'ında ayrıca sabitlenir.

**K-21.** Yukarıdaki her kriter için tracked test bulunur ve testler `run`
dışındaki komutları da çağırır.

---

## 8. Zorunlu test sınıfları

- **Pozitif golden:** her yüzey için geçerli girdi, beklenen stdout ve exit.
- **Negatif — sözdizimi:** her yüzey için, bugün hiç bulunmayan sınıf.
- **Negatif — semantik:** mevcut `.compile_error` fixture'larının non-`run`
  yüzeylere genişletilmesi.
- **Sınır:** boş dosya, olmayan dosya, argümansız çağrı, bilinmeyen komut,
  açılamayan output dosyası.
- **`exec`:** pozitif (expression), pozitif (statement listesi), negatif
  (invalid snippet), sınır (boş snippet), konum doğruluğu, capability bayrağı.
- **JSON şeması:** `schemaVersion` varlığı ve her girdide geçerli JSON.
- **`run` status:** `main` dönüş değeri aktarımı ve aralık/tip ihlali.

`cmake/run_golden.cmake` ve `run_golden_error.cmake` bugün sabit olarak `run`
çağırır; bu sınıfların kaydedilebilmesi komut parametresinin genelleştirilmesini
gerektirir.

---

## 9. Baseline kapısı — ZORUNLU

**Hiçbir uygulama task'ı, aşağıdaki validation-only görev kanıtla
tamamlanmadan başlatılamaz.**

### `SQ-090-CLI-BASELINE`

Bu görev bir coder görevi **değildir.** Yalnız ölçüm ve kayıt yapar.

Kapsam:

- Güncel `0.9.0` dalından **fresh build provenance** (commit, build komutu,
  toolchain, binary yolu ve zaman damgası).
- `run`, `check`, `ast`, `ir`, `symbols`, `exec` matrisi.
- Geçerli, sözdizimi-hatalı ve semantik-hatalı girdiler.
- `stdout`, `stderr` ve exit **ayrı ayrı** kaydedilir.
- **#134 exact repro** (`saqut exec 'print(1);'` ve
  `saqut exec 'print(1); print(2);'`) — ham bayt kaydıyla.
- `help`, bilinmeyen komut, stub yüzeyler, stdin `-` ve hayalet `source.sqt`
  davranışı.
- **Hiçbir production source değişikliği yok.**

Çıktı: `tasks/SQ-090-CLI-BASELINE/validation-report.md`.

Kapı kuralı: baseline raporu, §1'deki yapısal tespitlerin hangilerinin gerçek
binary'de gözlendiğini ve hangilerinin gözlenmediğini tek tek göstermelidir.
Gözlenmeyen bir tespit için uygulama task'ı açılmaz; karar o madde için yeniden
değerlendirilir.

---

## 10. Baseline sonrası önerilen task sırası

Baseline kanıtı geldikten sonra, kanıtlanan maddeler için:

| # | Task | Kapsam |
|---|---|---|
| 1 | `SQ-090-EXIT-CODE-CONTRACT` | `0/64/65/70` sınıflandırması ve `run` status aktarımı; tek tanım noktası. |
| 2 | `SQ-090-DIAG-GATE-SINGLE-FILE` | `ast`, `symbols` ve `exec` yollarının ortak `DiagnosticEngine` kapısına bağlanması. |
| 3 | `SQ-090-PARSER-RECOVERY-DIAG` | Recovery edilen her hatanın structured diagnostic üretmesi; `ErrorNode`/`MissingToken` açık temsili. |
| 4 | `SQ-090-AST-SYMBOLS-PARTIAL-OUTPUT` | Error-tolerant kısmi çıktı, text/JSON ayrımı, `schemaVersion`. |
| 5 | `SQ-090-RUN-ENTRYPOINT` | `run` içinde `main` varlık/imza doğrulaması ve dönüş değeri sözleşmesi. |
| 6 | `SQ-090-EXEC-SHARED-SERVICE` | `exec`'in ortak compile-and-run servisine bağlanması; `Expression \| StatementList \| Invalid` adapter'ı; snippet-göreli konumlar. |
| 7 | `SQ-090-CLI-INVOCATION` | Örtük `run` kısayolu, hayalet `source.sqt`, ölü `help`/bilinmeyen-komut dalları, stub kaldırma, `bench` gizleme, output fallback. |
| 8 | `SQ-090-CLI-TEST-HARNESS` | Golden runner'ın komut parametresinin genelleştirilmesi; §8'deki test sınıflarının kaydı. **1-7'nin kanıt katmanıdır; tek başına özellik değildir.** |
| 9 | `SQ-090-DOC-RECONCILE` | `saqutwebside/.../cli-reference.md` (EN+TR) ve `knowledge-base/06_Tooling.md#kb-34-cli` ile `00_Orientation.md#kb-04-status` güncellemesi. |

Task 1, 2 ve 7 birbirine bağlı değildir. Task 8 tamamlanmadan hiçbiri kanıt
zincirinde bir sonraki aşamaya geçemez.

---

## 11. Riskler ve regresyon yüzeyi

- Exit sözleşmesi değişikliği 73 golden `.expected` testinin tamamına dokunur:
  `cmake/run_golden.cmake:31` exit ≠ 0'da `FATAL_ERROR` verir. `main`'in dönüş
  değerine bağlı fixture varsa kırılır; Task 1 öncesinde envanter çıkarılmalıdır.
- `ast`/`symbols`'e diagnostic bağlamak, bugün `stderr`'e yazılan
  `parser error: ...` metnini konumlu `E9xx` biçimine çevirir. Bu metne bağlı
  script varsa kırılır.
- Örtük `run` kısayolunun kaldırılması `saqut program.sqt` alışkanlığını
  bozar; kullanıcıya görünen bilinçli bir kırılmadır ve release notunda
  belirtilmelidir.
- `exec`'in ortak servise taşınması, bugünkü sentetik `main` sarmalayıcısının
  ve `startsWithStatement` probe'unun çıktısını değiştirebilir; #134 baseline'ı
  bu değişimin öncesi/sonrası karşılaştırması için referanstır.
- `run` içinde `main` doğrulaması, `tests/module/*.sqt` gibi entrypoint'siz
  fixture'ların hangi komutla çağrıldığına bağlı olarak etkilenebilir.
- Parser recovery'sinin tanı üretmeye başlaması, bugün sessizce kabul edilen
  girdiler için yeni hatalar doğurabilir; `tests/golden/` içindeki mevcut
  fixture'ların bu sınıfa girip girmediği Task 3 öncesinde ölçülmelidir.

---

## 12. Durma ve ürün sahibine dönme koşulları

Aşağıdaki durumlarda ilgili task durur ve karar ürün sahibine getirilir:

- Baseline, §1'deki bir tespitin gerçek binary'de **gözlenmediğini** gösterirse.
- `main` dönüş değerinin aralık/tip ihlalinde diagnostic'in **uyarı mı hata mı**
  olacağı netleşmezse.
- Parser recovery'sinin tanı üretmeye başlaması mevcut geçerli sayılan bir
  fixture'ı kırarsa (dil sözleşmesi değişikliği demektir).
- `exec`'in ortak servise taşınması, `run`'ın gözlenen davranışını değiştirmek
  zorunda kalırsa.
- Herhangi bir adım yeni bir CLI komutu, bayrak veya public yüzey gerektirirse.

---

## 13. Kanıtlanmayanlar

- Hiçbir komutun gözlenen çıktısı, exit kodu veya stderr'i — build ve binary
  çalıştırılmadı.
- #134'ün ham repro'su yeniden üretilmedi; yalnız kök neden kaynakta doğrulandı.
- `run`/`check`'in belirli bir bozuk girdide exit 0 verip vermediği; yalnız bunu
  engelleyen mekanizmanın kaynakta bulunmadığı gösterildi.
- Hiçbir tracked testin geçtiği — kayıt varlığı geçiş kanıtı değildir.
- LSP `E901`/`E903` fixture'larının doğruluğu.
- `ast --optimized` yolundaki `deepClone` kısmi kapsamının hangi düğüm
  türlerinde aliasing ürettiği (ayrı bir bellek denetimi konusu).

---

## 14. Durum

**DoD: Tasarlandı.**

Sonraki yetkili adım: `SQ-090-CLI-BASELINE` validation-only görevi. O görevin
`validation-report.md` kanıtı olmadan bu karardan hiçbir uygulama task'ı
türetilemez.
