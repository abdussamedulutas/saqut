# SQ-090-IR-FINAL-FORM — Normatif HeavyIR ve `saqut ir` Sözleşmesi

**Görev kimliği:** SQ-090-IR-FINAL-FORM  
**Hedef sürüm:** 0.9.0  
**Tarih:** 2026-07-26  
**Karar verenler:** Ürün sahibi, Şüpheci Başmimar  
**Kanıt revizyonu:** `eabc01ae44c04c211edae90ca69a6e10c4b95a97`
(dal `0.9.0`, dirty worktree provenance altında)  
**Dayanak:** `AGENTS.md`, ADR-038, ADR-039, ADR-042,
`tasks/SQ-090-IR-BASELINE/validation-contract.md`,
`tasks/SQ-090-IR-BASELINE/validation-report.md`, GitHub #101 ve #137

**DoD durumu: Tasarlandı.**

Bu belge ürün sahibince onaylanan mimari kararı ve ölçülebilir kabul
kriterlerini kaydeder. Production source, test veya build tanımı bu karar
belgesi tarafından değiştirilmemiştir.

---

## 1. Problem ve kullanıcı etkisi

`saqut ir`, 0.9 için normatif VM'e teslim edilen `IRProgram` nesnesinin aynı
üretim yolundan oluşan dump'ıdır. Kaynak denetimi ve SQ-090-IR-BASELINE şunları
göstermiştir:

1. Seçilen `fibonacci.sqt` fixture'ında dump instruction sayısı ile
   `--profile` IR-generation instruction sayacı eşleşmiştir (`32 = 32`). Bu,
   yalnız ölçülen fixture/binary/ortam için sayı invariantı kanıtıdır.
2. Aynı fixture'ın üç dump'ı byte-identical olmuştur.
3. `LOAD_LONG` ve `LOAD_FLOAT32` satırlarında hedef slot ve sabit operand
   görünmemektedir.
4. `requiredCap` instruction davranışını etkilediği halde dump satırında
   görünmemektedir.
5. Açık `return;` içermeyen geçerli `void` fonksiyon, IR'de `RETURN`
   instruction'ı bulunmadan VM fallthrough davranışıyla tamamlanmaktadır.
6. Redirect edilen `ir` stdout'unda 411 ANSI CSI dizisi ölçülmüştür.
7. `--capabilities` normal instruction dump'ının yerine tek satırlık özet
   basmaktadır.
8. Düz `saqut ir` için tracked golden kapsamı yoktur; yalnız iki
   `ir --optimized` fixture'ı ölçülmüştür.

Sonuç: Dump'ın instruction sayısı seçilen örnekte doğru olsa da executable
anlamı belirlemeye yetmemektedir. Kullanıcı aynı görünen dump'ın farklı sabit,
capability veya fallthrough davranışı taşıyıp taşımadığını göremez. Bu,
saQut'un cam-kutu ve geribildirim odaklı 0.9 hedefiyle bağdaşmaz.

---

## 2. Terimler ve katman sınırı

### 2.1 HeavyIR

Bu belgede `IR`, backend kararları için gereken frontend semantiğini kaybetmeyen
backend-bağımsız **HeavyIR** katmanıdır.

HeavyIR:

- exact numeric width ve signedness;
- type/signature/layout;
- explicit control-flow ve terminator;
- exception, capability ve typed host-call gereksinimi;
- string/bytes ayrımı;
- gerekli symbol/source/debug eşlemesi

gibi bilgileri taşıyabilecek mimari omurgadır. Host dil veya backend tercihini
IR içine gömmez. Örneğin IR `i64` taşır; gelecekteki JavaScript backend'inin
`BigInt`, emülasyon veya açık ret kararı backend'e aittir.

“Kaynağa geri çevrilebilirlik”, whitespace/yorumların birebir geri kurulması
değil, programın **semantik yapısının kayıpsız ve denetlenebilir** olmasıdır.

### 2.2 0.9 final-form

0.9'da VM tek normatif backend olduğu için `saqut ir`, VM'e giden HeavyIR'ın
final instruction biçimini gösterir. Dump'tan sonra geçerli programın
semantiğini değiştiren gizli instruction ekleme/lowering yapılamaz.

Bu karar gelecekte tek bir dump'ın bütün backend'ler için “final” olacağını
vaat etmez. MIR, WASM, transpiler veya portable SVM bytecode gibi gelecekteki
lowering katmanları kendi target-specific final-form dump'larını gerektirir;
`saqut ir` sessizce bunlardan biriymiş gibi yeniden tanımlanamaz.

### 2.3 HeavyIR portable artifact değildir

Gelecekteki `.sode`/SVM portable bytecode fikri GitHub #138 altında ayrı
araştırmadır. HeavyIR'ın zengin olması bu yönü mümkün kılar; HeavyIR'ın kendisi
dağıtım formatı, sandbox veya sürümlü portable bytecode ilan edilmez.

---

## 3. Kabul edilen kararlar

### 3.1 Canonical text dump

- 0.9 `saqut ir` varsayılanı deterministic, canonical, insan-okunur **text**
  dump'tır.
- Aynı revizyon, aynı kaynak, aynı bayraklar ve aynı desteklenen ortamda iki
  çalışma byte-identical plain-text içerik üretmelidir.
- 0.9'da `ir --json`, JSONL, HTTP veya socket yüzeyi eklenmez. JSON/şema kararı
  GitHub #137 kapsamında 1.0 için ayrıdır.

### 3.2 Executable anlam görünürlüğü

Bir `Instruction` alanının değişmesi normatif VM davranışını değiştirebiliyorsa
o alan canonical dump'ta ayırt edilebilir olmalıdır.

Asgari olarak:

- bütün opcode'ların kullandığı hedef/kaynak slotlar;
- sabit değerler (`intValue`, `int64Value`, float/float32/decimal/string vb.);
- jump target, condition, argument slotları ve function/host-call adı;
- `requiredCap`;
- executable struct/field/array bilgisi

görünür olur. Source line/column, breakpoint indeksi veya yalnız debug amaçlı
metadata bu temel dump'ın executable-completeness şartına dahil değildir.

Capability annotation canonical biçimi instruction satırının sonunda
`[cap:<ad>]` olur; capability taşımayan instruction bu eki basmaz.

### 3.3 Exhaustive renderer

Yeni veya mevcut bir opcode'un yalnız adı basılıp operandlarının sessizce
unutulması kabul edilmez.

Renderer mekanizması:

- opcode kataloğundaki her opcode için bilinçli bir rendering kararı taşır;
- yeni opcode eklendiğinde eksik rendering'i build/test aşamasında görünür
  kılar;
- operandsız olması gerçekten doğru olan opcode ile eksik implementasyonu
  ayırır;
- aynı operand bilgisini birden fazla bağımsız listede el ile senkron tutmayı
  mümkün olduğunca önler.

Exact C++ tekniği implementation contract'ta kaynak gerçeğine göre seçilir;
bu karar yeni opcode veya IR redesign zorunlu kılmaz.

### 3.4 Explicit void terminator

- Kaynak dilde `void` fonksiyonun sonunda açık `return;` yazmak zorunlu
  değildir.
- IRGenerator, açık terminator bulunmayan geçerli `void` fonksiyonun sonuna
  compiler-generated gerçek bir `RETURN` instruction'ı maddileştirir.
- Normatif VM davranışı, geçerli compiler çıktısında instruction listesinin
  sonuna düşerek örtük `0` üretmeye dayanmaz.
- Interpreter'ın malformed/eski IR için savunma davranışı ayrı implementation
  kararıdır; bu task onu public semantik ilan etmez.

Non-`void` fonksiyonlarda dönüş sözleşmesi değişmez:

- Bildirilen tipte dönüş değeri kesin olarak bulunmalıdır.
- `while(true)` veya karmaşık try/switch yapıları için “muhtemelen hiç
  bitmez” tahmini dönüş yerine geçmez.
- Kullanıcı gerçekten değer döndürmeyen fonksiyon istiyorsa `void` kullanır.
- Bu task return-completeness analizini genişletmez; yalnız kabul edilmiş
  `void` fonksiyonun IR terminator'ını maddileştirir.

### 3.5 Renk ve yönlendirme

- TTY'de insan ergonomisi için ANSI renk kullanılabilir.
- Pipe, redirect veya terminal olmayan stdout hedefinde ANSI escape dizisi
  bulunmaz.
- TTY seçimi opcode, operand, sıra, whitespace veya semantic formatı
  değiştirmez; yalnız renk katmanını etkiler.
- Yeni `--no-color` veya başka CLI bayrağı bu 0.9 task'ında eklenmez.

### 3.6 `--capabilities`

- `ir --capabilities` ayrı ve explicit summary modu olarak kalır.
- Normal instruction dump'ıyla otomatik birleştirilmez.
- Normal dump, instruction-seviyesi `requiredCap` bilgisini §3.2 uyarınca
  yine de gösterir.
- Summary modunun mevcut başarılı exit davranışı bu kararla değiştirilmez.

### 3.7 ADR-039 `--types`

- ADR-039'un HeavyIR'de kayıpsız tip bilgisi yönü korunur.
- `--types`, `valueType`, `globalSlotTypes` ve non-executable slot metadata
  görünümü bu 0.9 implementation paketine eklenmez.
- Bu yüzey ayrı 1.0 karar/task'ında uygulanır veya ADR-039 açıkça supersede
  edilir; sessizce unutulmuş kabul edilmez.

### 3.8 Main'siz global initializer

IRB-6'da main'siz tek dosyada `GLOBALS/counter` görünmüş, initializer `7`yi
instruction'a bağlayan kayıt görünmemiştir. Bu tek başına final-form bug'ı
ilan edilmez; program normatif backend tarafından çalıştırılabilir entrypoint
üretmemiştir.

Imported-module initializer'ın entrypoint'li module graph içinde dump ve VM
tarafından aynı instruction'larla ele alınıp alınmadığı ayrı validation-only
task ile ölçülmeden implementation açılmaz.

### 3.9 Ayrı name-resolution bulgusu

IRB-3'teki `E_SYMBOL_NOT_IMPORTED`, same-name collision davranışını
kanıtlamamıştır. Bu bulgu IR renderer veya void-return task'ına sokulmaz; ayrı
module/name-resolution incelemesidir.

---

## 4. Kullanıcıya görünen hedef davranış

| Senaryo | Hedef |
|---|---|
| `saqut ir valid.sqt` | Canonical HeavyIR text, exit `0` |
| Aynı komut üç kez | Plain-text bytes birebir aynı |
| `longint x = 4200000000` | `LOAD_LONG` satırında hedef slot ve exact değer görünür |
| `float x = 1.5` | `LOAD_FLOAT32` satırında hedef slot ve exact değer görünür |
| Capability taşıyan host call | Instruction satırında `[cap:fs]` veya exact capability adı görünür |
| Capability taşımayan call | Yanlış capability eki bulunmaz |
| Açık `return;` içermeyen `void` fonksiyon | Son geçerli instruction gerçek `RETURN` |
| Aynı program `run` | Önceki stdout/stderr/exit semantiği korunur |
| `saqut ir ... > out.txt` | `out.txt` içinde ANSI CSI yok |
| `saqut ir --capabilities ...` | Ayrı summary çıktısı; instruction dump basılmaz |

---

## 5. Ölçülebilir kabul kriterleri

- **IR-K1 — Sayı:** `examples/fibonacci.sqt` için dump instruction satır
  toplamı `run --profile` IR-generation `instr` sayısına eşittir.
- **IR-K2 — Determinizm:** Aynı binary/kaynak/bayrakla üç plain-text dump
  byte-identical olur.
- **IR-K3 — Long operand:** `LOAD_LONG` hedef slotu ve `4200000000` dump
  satırında görünür.
- **IR-K4 — Float32 operand:** `LOAD_FLOAT32` hedef slotu ve `1.5` dump
  satırında görünür.
- **IR-K5 — Capability:** Capability taşıyan ve taşımayan eşdeğer instruction
  dump'ta ayırt edilir; taşıyan satır `[cap:<ad>]` içerir.
- **IR-K6 — Exhaustiveness:** Opcode kataloğuna rendering kararı olmayan yeni
  opcode eklenmesi tracked test/build kapısında sessiz geçmez.
- **IR-K7 — Void return:** Açık return içermeyen geçerli void fonksiyonun son
  instruction'ı gerçek `RETURN` olur; program çıktısı ve exit'i baseline ile
  aynı kalır.
- **IR-K8 — ANSI:** Redirect edilen stdout ve stderr'te ANSI CSI sayısı `0`
  olur; TTY renk davranışı desteklenen PTY testiyle ayrıca kaydedilir.
- **IR-K9 — Cap summary:** Normal dump ve `--capabilities` modu birbirinden
  ayrı kalır; ikisi de beklenen exit'i üretir.
- **IR-K10 — Regression:** Mevcut iki anchored `ir_opt` golden testi ve yeni
  plain-IR positive/negative fixture'ları fresh build üzerinde çalışır.
- **IR-K11 — Katman sınırı:** JIT/MIR çıktısı, JSON, `--types`, SVM bytecode,
  module-init düzeltmesi veya name-resolution değişikliği bu diff'e girmez.

---

## 6. Zorunlu atomik task ayrımı

Teslimat Yöneticisi bu kararı tek implementation contract'a dönüştüremez.
En az şu bağımsız task'lar üretilir:

1. **SQ-090-IR-DUMP-COMPLETE** — exhaustive renderer mekanizması, executable
   operandlar, `requiredCap` ve bunların tracked plain-IR testleri.
2. **SQ-090-IR-VOID-RETURN** — compiler-generated void `RETURN`, runtime
   davranış koruması ve ayrı regression fixture'ı.
3. **SQ-090-IR-TTY-COLOR** — yalnız TTY renk katmanı, redirect/pipe ANSI
   temizliği ve PTY/non-PTY validation.
4. **SQ-090-IR-MODULE-INIT-BASELINE** — VALIDATION-ONLY; imported-module
   initializer için dump/VM eşitliğini ölçer. Gözlem gelmeden düzeltme task'ı
   açılmaz.

Planlanan fakat 0.9 implementation sırasına alınmayan:

- **SQ-100-IR-TYPES** — ADR-039 `--types` ve type metadata görünürlüğü.
- GitHub #137 — JSON/schema/output ergonomisi.
- GitHub #138 — portable SVM bytecode/host runtimeları.
- IRB-3 name-resolution bulgusu — ayrı audit/task kimliği.

İlk üç implementation task'ı aynı production dosyasına dokunuyorsa Teslimat
Yöneticisi paralel başlatmaz; dosya sahipliği ve dependency sırasını exact
contract'larda açıklar.

---

## 7. Kapsam dışı

- MIR/JIT feature parity veya MIR dump.
- AOT, WASM, transpiler, SVM veya `.sode` implementasyonu.
- Yeni opcode, IR redesign veya optimizer genişletmesi.
- `ir --json`, JSONL, HTTP/socket veya yeni CLI bayrağı.
- ADR-039 `--types` implementasyonu.
- Module initializer düzeltmesi (baseline gelmeden).
- Name-resolution, import veya same-name collision düzeltmesi.
- Return-completeness analizini switch/try/loop için genişletme.
- Performans optimizasyonu veya threaded-code deneyi.
- Genel sandbox/resource-limit sözleşmesi.

---

## 8. Durma koşulları

Teslimat Yöneticisi veya Uygulayıcı şu durumda durup Başmimar'a döner:

- yeni opcode, public flag veya output formatı gerekiyor;
- renderer düzeltmesi IR veri modelini değiştirmeyi gerektiriyor;
- `requiredCap` için mevcut instruction alanından farklı public capability
  modeli gerekiyor;
- void `RETURN` eklemek non-void return semantiğini veya JIT kapsamını
  değiştirmeyi gerektiriyor;
- TTY tespiti ortak CLI çıktılarının formatını görev dışı etkiliyor;
- task'lar arasında aynı dosyada çakışan paralel diff oluşuyor;
- kabul kriteri mevcut syntax/fixture ile ölçülemiyor;
- dirty worktree'deki kullanıcı değişikliği exact allowlist ile çakışıyor.

Tahminle kapsam genişletilmez; ayrı karar veya contract amendment istenir.

---

## 9. Kanıt ve DoD sınırı

Bu kararın `Tasarlandı` durumu şunlara dayanır:

- ürün sahibinin 2026-07-26 tarihli açık onayı;
- SQ-090-IR-BASELINE Amendment 03 revalidation evidence;
- Başmimar Günlüğü #004 ve #005 kabul kayıtları.

Bu belge implementation veya validation kanıtı değildir. Sonraki DoD aşamaları
her atomik task için ayrı implementation/validation report ve GitHub Project
akışıyla değerlendirilir.
