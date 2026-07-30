# SQ-090-IR-BASELINE — Validation Report

**Revision:** Amendment 03 — Evidence-Completeness Revalidation
**Validation date:** 2026-07-26
**Rol:** İzole Hafif Muhalif Testçi (yeni, bağımsız oturum)
**Task ID:** SQ-090-IR-BASELINE
**Hedef sürüm:** 0.9.0
**Mod:** VALIDATION-ONLY

---

## 0. Supersession — önceki deneme neden geçersiz

Bu dosyanın önceki içeriği (Amendment 02 sonrası ilk ölçüm denemesi) Başmimar
Günlüğü #003'te (GitHub #101,
https://github.com/saqutlang/saqut/issues/101#issuecomment-5083670628)
**provisional** ilan edilmiş ve kabul edilmemiştir; gerekçe:

1. IRB-1/2/4/5/6/7/8/9 çağrılarının çoğunda exact `.cmd` kaydı yoktu.
2. IRB-5–9'un birçok çağrısında raw `.exit` kaydı yoktu.
3. Başlangıç beş-yol status/hash kaydı vardı, zorunlu bitiş karşılaştırması
   yoktu.
4. IRB-3, `E_SYMBOL_NOT_IMPORTED` ile semantic kapıda durmuştu; bu, "aynı
   adın çakışması doğru çözüldü" iddiasıyla aynı şey değildir ve önceki
   raporda bu ayrım gizlenmişti.

Önceki denemenin ham `evidence/` dosyaları (kök seviyesinde) **silinmedi,
üzerine yazılmadı**; bu revalidasyonun tüm yeni kanıtı
`tasks/SQ-090-IR-BASELINE/evidence/revalidation-01/` altında, sıfırdan
yeniden ölçülerek üretildi (R.1–R.11). Önceki denemenin `.stdout`/`.stderr`
değerleri bu raporda hiçbir yerde referans gösterilmemiş veya kopyalanmamıştır.

---

## 1. Binary Provenance

| Alan | Değer |
|---|---|
| HEAD | `eabc01ae44c04c211edae90ca69a6e10c4b95a97` |
| Branch | `0.9.0` |
| Binary path | `/tmp/saqut-sq090-ir-baseline-NdIQWO/build/saqut` |
| Binary boyut | 5,498,984 bytes |
| Build türü | Release (out-of-tree, fresh build, `$WORK` = mktemp) |
| Configure exit | 0 |
| Build exit | 0 |

Tüm ayrıntılar (tam HEAD hash, `git status --short` tam çıktısı, beş-yol diff
stat/hash, 4 aktif kaynak dosyanın SHA-256'sı, toolchain sürümleri):
`evidence/revalidation-01/00-provenance.md`, `01-configure.*`, `02-build.*`,
`03-binary-provenance.txt`.

Not: repository dirty idi (root `AGENTS.md`, `CLAUDE.md`, çeşitli `docs/`
ve `src/cli/commands/{run,check,ir}.hpp`, `src/cli/exit_codes.hpp` gibi
görevle ilgili aktif kaynak değişiklikleri dahil). Contract §1 gereği bu
fresh-clone yerine mevcut çalışma ağacından build edildi; dirty durum görevle
ilgisiz sayılmadı, tam `git status --short` çıktısı `00-provenance.md`'de
kayıtlıdır.

### Fixture doğrulama (§3)

`examples/merhaba.sqt`, `examples/fibonacci.sqt`,
`tests/golden/numeric/widths.sqt` (+ SHA-256) tam içerikleriyle
`evidence/revalidation-01/04-syntax-verification.md`'ye alıntılandı.
Doğrulanan temel form: `int main() { ... }` — dönüş tipi önce, `func`/`:`
biçimi kullanılmıyor, gövde `{}` bloğu.

---

## 2. Dondurulmuş Test Matrisi ve Sonuçlar

| ID | Fixture | Contract maddesi | Sonuç |
|---|---|---|---|
| IRB-1 | `examples/fibonacci.sqt` | Determinizm — üç tekrar byte-identical | **GÖZLENDİ** |
| IRB-2 | `examples/fibonacci.sqt` | Dump instr count == profile `ir-gen`/`instr` sayısı | **GÖZLENDİ** |
| IRB-3.1–3 | `$WORK/fixtures/irb3/single-main.sqt` | Pozitif kontrol temiz geçer | **GÖZLENDİ** (ön koşul) |
| IRB-3.2–3 (semantic gate) | `$WORK/fixtures/irb3/main.sqt` | `check` reddediyor mu | **GÖZLENDİ** — reddedildi, exit≠0 |
| IRB-3.2–3 (diagnostic kategorisi, R.9) | aynı | Same-name collision mi, başka name-resolution mu | **GÖZLENDİ** — `E_SYMBOL_NOT_IMPORTED`, same-name-collision **DEĞİL** |
| IRB-3.4 | `main.sqt` ir/run | (semantic kapıda durdu, adım 4'e ulaşılmadı) | Semantic kapı exit 65 ile durduğu için contract gereği ir/run adımları yürütülmedi; bu ayrı bir IRB sonucu değildir |
| IRB-4 (runtime) | `$WORK/fixtures/irb4/main.sqt` | Void fallthrough — runtime davranışı ve başarılı çıktı | **GÖZLENDİ** — `run` exit 0, stdout `"onceafter"` |
| IRB-4 (dump) | `$WORK/fixtures/irb4/main.sqt` | Void fallthrough — son instr gerçek `RETURN` mi | **GÖZLENMEDİ** — son instr `RETURN` değil (CALLHOST) |
| IRB-5 control (LOAD_CONST) | `$WORK/fixtures/irb5/loadconst.sqt` | Sabit değer dump'ta görünür mü | **GÖZLENDİ** — `42` görünür |
| IRB-5 LOAD_LONG | `$WORK/fixtures/irb5/loadlong.sqt` | Sabit değer dump'ta görünür mü | **GÖZLENMEDİ** — operand alanı tamamen boş |
| IRB-5 LOAD_FLOAT32 | `$WORK/fixtures/irb5/loadfloat32.sqt` | Sabit değer dump'ta görünür mü | **GÖZLENMEDİ** — operand alanı tamamen boş |
| IRB-5 requiredCap | `$WORK/fixtures/irb5/cap.sqt` | `[cap: ...]` benzeri işaret instr satırında görünür mü | **GÖZLENMEDİ** — instr satırında capability işareti yok |
| IRB-6a | `$WORK/fixtures/irb6/globals.sqt` | GLOBALS bölümü var mı | **GÖZLENDİ** — `GLOBALS (1)` başlığı dump'ta mevcut |
| IRB-6b | `$WORK/fixtures/irb6/globals.sqt` | `counter` adı dump'ta görünüyor mu | **GÖZLENDİ** — `global[0] = counter` satırı mevcut |
| IRB-6c | `$WORK/fixtures/irb6/globals.sqt` | `counter`'ı `7` ile bağlayan STORE_GLOBAL/eşdeğer instruction | **GÖZLENMEDİ** — dump'ta `counter=7` ilişkisini kuran instruction yok |
| IRB-7 | `$WORK/fixtures/irb5/cap.sqt` | `--capabilities` özet mi basıyor, instr dump kayboluyor mu | **GÖZLENDİ** — `--capabilities` yalnız `capabilities: fs` özetini basıyor (1 satır), normal dump (9 satır) kayboluyor; exit her ikisinde de 0 |
| IRB-8 | `examples/fibonacci.sqt` (redirect) | stdout/stderr'de CSI (`1b5b`) var mı | **GÖZLENDİ** — stdout'ta 411 CSI dizisi, stderr'de 0 |
| IRB-9 | tracked golden `ir_opt` testleri | tam 2 test var mı, ikisi de geçiyor mu | **GÖZLENDİ** — tam 2 test bulundu (`golden_opt_dce_ir_opt`, `golden_opt_folding_ir_opt`), anchored regex ile ikisi de Passed |

Hiçbir madde `BLOCKED` değildir; manifest'te `files_present=false` satırı
yoktur (bkz. §5).

---

## 3. Ham bulgu ayrıntıları

### IRB-1 — Determinizm

Üç ardışık `saqut ir examples/fibonacci.sqt` çalıştırması: stdout SHA-256 üçünde
`572d3d08...67fb48b`, stderr SHA-256 üçünde `e3b0c442...b7852b855` (boş
stderr). **GÖZLENDİ — bu binary/ortam ve bu exact kaynak için üç çalıştırma
byte-identical sonuç üretti.** Genel determinizm iddiası yapılmamıştır.

### IRB-2 — Instruction sayısı

`saqut ir examples/fibonacci.sqt` dump'ında §4 regex yöntemiyle eşleşen
satır sayısı: **32**. `saqut run --profile examples/fibonacci.sqt`
çıktısındaki `ir-gen` satırı: `ir-gen  32 instr  0.026 ms`. İki sayı eşit →
**GÖZLENDİ**.

### IRB-3 — Çapraz-modül aynı adlı fonksiyon

Pozitif kontrol (`single-main.sqt`, yalnız `a.sqt` import edilir): `check`,
`ir`, `run` üçü de exit 0; `run` stdout `6` (doğru: `shared(5)=5+1=6`).
**GÖZLENDİ — ön koşul temiz.**

`main.sqt` (`a.sqt` ve `b.sqt` her ikisi de `shared` tanımlar, `main.sqt`
`fromA`/`fromB` import eder) üzerinde `check main.sqt`: **exit 65**,
diagnostic:

```json
{
  "code": "E_SYMBOL_NOT_IMPORTED",
  "level": "error",
  "location": {"file": ".../b.sqt", "line": 6, "column": 12},
  "message": "'shared' is from another module and must be imported explicitly"
}
```

Contract §3.3 gereği IRB-3 burada durur (reddedildi, exit≠0).

**R.9 — iki ayrı soru, ayrı cevap:**

1. *Semantic kapı IR üretimini engelledi mi?* Evet — `check` exit 65 ile
   reddetti, IR/run adımına ulaşılmadı.
2. *Diagnostic gerçekten "aynı ad çakışması" mı açıkladı, yoksa başka bir
   name-resolution davranışı mı?* **Başka bir name-resolution davranışı** —
   `E_SYMBOL_NOT_IMPORTED`, `b.sqt` içindeki `shared` fonksiyonunun
   `fromB()` gövdesinden **import edilmeden** çağrılmasına işaret ediyor
   (mesaj: "'shared' is from another module and must be imported
   explicitly"). Bu, iki modülün aynı adı tanımlamasının bir "çakışma"
   diagnostic'i değildir; `b.sqt` kendi tanımladığı `shared`'ı kendi
   içinde çağırırken bile modül-dışı-isim kuralına takılıyor gibi
   görünüyor. "Semantic kapıda reddedildi, dolayısıyla çakışma doğru ele
   alınıyor" çıkarımı **yapılmamıştır** — bu ayrı, gizlenmemiş bir bulgu
   olarak kaydedilmiştir. Kesin kök neden (`b.sqt`'nin kendi
   fonksiyonunu çağırma bağlamı vs. isim çözümleme sırası) bu görevin
   kapsamı dışındadır; yalnız gözlenen diagnostic metni ve kategorisi
   raporlanmıştır.

### IRB-4 — Void fallthrough

İki ayrı olgu:

**Olgu 1 — Runtime davranışı:** `run main.sqt` exit 0, stdout `"onceafter"`
(satır sonu ayracı olmadan birleşik), stderr boş. Program void fonksiyonun
sonuna düşerek normal tamamlandı. → **GÖZLENDİ** — void fallthrough davranışı
çalışıyor ve başarılı çıktı üretiyor.

**Olgu 2 — Instruction dump'ta RETURN:** `sideEffect()` gövdesinin IR dump'ı:

```
NAME=sideEffect PARAMS=0 SLOTS=1
    0  LOAD_STRING     s0 = "once"
    1  CALLHOST        print(s0)
```

Son instruction `CALLHOST print(s0)`'dır; **gerçek bir `RETURN` (veya
eşdeğeri) değildir**. → **GÖZLENMEDİ** (hipotezin tersi: son instr RETURN
değil).

"RETURN eklenmeli" gibi bir öneri yapılmamıştır — yalnız mevcut davranış
kaydedilmiştir.

### IRB-5 — Görünmeyen executable operandlar

**Kontrol grubu (LOAD_CONST):** `saqut ir loadconst.sqt` dump'ında
`0  LOAD_CONST      s0 = 42` — değer görünür. **GÖZLENDİ.**

**LOAD_LONG:** `check loadlong.sqt` exit 0 (widths.sqt kanıtı doğrulandığı
gibi `longint` kabul edildi). `ir` dump'ı:

```
NAME=main PARAMS=0 SLOTS=2
    0  LOAD_LONG
    1  CALLHOST        print(s0)
```

Satırda **ne hedef slot ne de `4200000000` sabiti görünür** — opcode adından
sonra tamamen boş. → **GÖZLENMEDİ** (hipotezin tersi: operand görünmüyor;
beklenenden de eksik — hedef slot bile yok).

**LOAD_FLOAT32:** `check loadfloat32.sqt` exit 0. `ir` dump'ı:

```
NAME=main PARAMS=0 SLOTS=2
    0  LOAD_FLOAT32
    1  CALLHOST        print(s0)
```

Aynı durum: hedef slot ve `1.5` sabiti satırda yok. → **GÖZLENMEDİ**.

**requiredCap:** `check --allow-fs cap.sqt` exit 0, diagnostic yok.
`ir --allow-fs cap.sqt` dump'ı:

```
NAME=main PARAMS=0 SLOTS=3
    0  LOAD_STRING     s0 = "/tmp/saqut-ir-baseline-never-created"
    1  CALLHOST        __ffi__(s0)
    2  LOAD_CONST      s2 = 0
    3  RETURN          return s2
```

`CALLHOST __ffi__(s0)` satırında capability bilgisine dair bir ek
(`[cap: ...]` veya eşdeğeri) **yoktur**. → **GÖZLENMEDİ**.

### IRB-6 — Main'siz global initializer

Üç ayrı bulgu:

**1. GLOBALS bölümü:** `ir globals.sqt` dump'ında `GLOBALS (1)` başlığı
ve bölüm yapısı mevcut. → **GÖZLENDİ**.

**2. `counter` adı:** Aynı dump'ta `global[0] = counter` satırı ile
değişken adı görünüyor. → **GÖZLENDİ**.

**3. `counter`'ı `7` ile bağlayan instruction:** `STORE_GLOBAL` (veya
eşdeğeri) instruction **dump'ta hiçbir yerde yok** — fonksiyon gövdesi
bile yok (yalnız `GLOBALS` bölümü var, `main` olmadığı için instruction
listesi boş). Sabit değer `7` ile `counter` arasında görünür bir
instruction-level bağlantı gözlenmedi. → **GÖZLENMEDİ**.

```
GLOBALS (1)
global[0] = counter

END
```

`run globals.sqt`: exit 70, stderr `runtime error: 'main' function not
found`. Bu ham veridir, kabul kriteri değildir (SQ-090-RUN-ENTRYPOINT
kapsamı).

### IRB-7 — Capabilities modu

`ir --allow-fs cap.sqt` (normal, madde 1): stdout 9 satır — tam instruction
dump'ı basılıyor (yukarıdaki IRB-5 requiredCap dump'ıyla aynı).

`ir --allow-fs --capabilities cap.sqt` (madde 2): stdout **1 satır**:
`capabilities: fs`. Instruction dump'ı bu modda **kayboluyor**.

Exit her iki modda da 0. Normal dump'ta (madde 1) instruction-seviyesi
capability bilgisi zaten görünmüyordu (IRB-5 requiredCap bulgusu). →
**GÖZLENDİ** — `--capabilities` yalnız özet basıyor, normal dump'ı
bastırıyor.

### IRB-8 — ANSI ve yönlendirme

`saqut ir examples/fibonacci.sqt > irb8-stdout.raw 2> irb8-stderr.raw`.
Hex-dize (`1b5b`) sayımı: **stdout'ta 411**, **stderr'de 0**. → **GÖZLENDİ**
— dosyaya yönlendirilmiş stdout ham baytlarında CSI dizileri var (terminal
olmayan hedefe rağmen renklendirme baytları yazılıyor); stderr'de yok.
Terminal görünümü hakkında bir çıkarım yapılmamıştır.

### IRB-9 — Mevcut tracked IR testleri

`ctest --test-dir "$WORK/build" -N` envanterinde `ir_opt` içeren tam **2**
test bulundu: `golden_opt_dce_ir_opt` (Test #125), `golden_opt_folding_ir_opt`
(Test #126). Anchored regex `^(golden_opt_dce_ir_opt|golden_opt_folding_ir_opt)$`
ile ikisi de **Passed** (0.01 sec her biri, `100% tests passed, 0 tests
failed out of 2`). → **GÖZLENDİ**. "Tüm golden testler geçti" gibi bir
genelleme yapılmamıştır; yalnız bu iki test için hüküm verilmiştir.

---

## 4. Çalıştırılan komutlar

Tam, quoting korunmuş komut listesi her `<case>.cmd` dosyasında ve
`evidence/revalidation-01/manifest.tsv`'de (29 satır, başlık dahil) kayıtlıdır.
Her satırda `cwd`, `argv`, dört dosyanın yolu ve `files_present=true`
bulunur; hiçbir satırda `files_present=false` yoktur.

Ana adımlar: `cmake -S ... -B $WORK/build -DCMAKE_BUILD_TYPE=Release`,
`cmake --build $WORK/build -j`, ardından IRB-1..9 için yukarıda listelenen
`$SAQUT check|ir|run [flags] <fixture>` çağrıları ve iki `ctest
--test-dir $WORK/build` çağrısı.

---

## 5. Kanıt bütünlüğü doğrulaması (R.6–R.8)

- Başlangıç beş-yol (`src/ CMakeLists.txt cmake/ tests/ examples/`)
  `status --short` ve `diff HEAD | sha256sum`:
  `evidence/revalidation-01/start-five-path-status.txt`,
  `start-five-path-diff-sha256.txt`.
- Bitiş (aynı beş yol, aynı yöntem):
  `end-five-path-status.txt`, `end-five-path-diff-sha256.txt`.
- Başlangıç ve bitiş **byte-identical** (`diff` çıktısı boş, iki sha256
  eşleşti) — tester bu beş yolda hiçbir dosya oluşturmadı/silmedi/
  değiştirmedi.
- `manifest.tsv`'de `files_present=false` olan satır **yok**.

Sonuç: **görev bütünü BLOCKED değildir** (R.8 tetiklenmedi).

---

## 6. PASS/FAIL/BLOCKED disiplini

Bu görev VALIDATION-ONLY baseline'dır; contract gereği yalnız `GÖZLENDİ /
GÖZLENMEDİ / BLOCKED` sınıflandırması kullanılmıştır — PASS/FAIL veya DoD
yükseltme dili kullanılmamıştır. Yukarıdaki §2 tablosu nihai sınıflandırmadır.

---

## 7. Test edilmeyen iddialar

- IRB-3.4 (main.sqt için `ir`/`run`/`--profile` çıktıları) — semantic kapıda
  durulduğu için contract §3 gereği çalıştırılmadı.
- 98 opcode'un tamamı için operand görünürlüğü — contract kapsamı yalnız
  3 temsilî sınıf + 1 kontrol grubudur (§0 kesin yasak).
- `run` ile eksik-main davranışının kabul kriteri olarak değerlendirilmesi —
  SQ-090-RUN-ENTRYPOINT kapsamındadır, bu görevde yalnız ham veri olarak
  kaydedildi.
- Terminal (TTY) görünümünde ANSI davranışı — yalnız dosyaya yönlendirilmiş
  ham baytlar ölçüldü.

---

## 8. Regression testi

Bu görev validation-only baseline'dır; contract gereği yeni tracked
regression testi eklenmedi. Tüm fixture'lar `$WORK/fixtures/` altında kaldı
(repository'ye commit edilmedi).

---

## 9. Başmimarın yeniden değerlendirmesi gereken semantik bulgular

1. **IRB-5 (LOAD_LONG, LOAD_FLOAT32, requiredCap):** Üç ayrı executable
   operand sınıfı da dump'ta görünmüyor — yalnız beklenen sabit değer değil,
   LOAD_LONG/LOAD_FLOAT32'de hedef slot bile eksik. Bu, IR dump'ının
   debug-inceleme aracı olarak bu üç instruction sınıfı için ciddi ölçüde
   eksik olduğunu gösteriyor.
2. **IRB-3 diagnostic kategorisi:** `E_SYMBOL_NOT_IMPORTED`, iki modülün
   aynı `shared` adını tanımlamasından değil, `b.sqt`'nin kendi
   `shared`'ını içeriden çağırırken modül-dışı-isim kuralına takılmasından
   kaynaklanıyor gibi görünüyor. Bu, "aynı ad çakışması doğru ele
   alınıyor" varsayımının kanıtlanmadığı, ayrı bir isim çözümleme
   davranışı olabilir; kök neden analizi bu görevin kapsamı dışındadır.
3. **IRB-6:** `GLOBALS` bölümü yalnız isim listeliyor; initializer değerini
   (`7`) bir instruction'a bağlayan görünür bir kanıt IR dump'ında yok.

---

## AGENTS.md §10 — Zorunlu çalışma sonu raporu

1. **Rol ve görev kimliği:** İzole Hafif Muhalif Testçi, SQ-090-IR-BASELINE
   (Amendment 03 revalidasyonu).
2. **İncelenen kanıt:** AGENTS.md, knowledge-base/ (10 dosya), v1.0 kapsam
   bildirgesi, ADR-042, yol haritası, issue disposition kaydı, bu görevin
   `validation-contract.md`'si (Amendment 03), `examples/merhaba.sqt`,
   `examples/fibonacci.sqt`, `tests/golden/numeric/widths.sqt`. Production
   C++ kaynağı okunmadı; coder/mimar reasoning'i veya implementation report
   okunmadı; önceki validation-report/evidence oracle olarak kullanılmadı.
3. **Yapılan değişiklik veya karar:** Yalnız ölçüm; davranış değişikliği
   yapılmadı. `tasks/SQ-090-IR-BASELINE/validation-report.md` bu revalidasyon
   ile güncellendi; `tasks/SQ-090-IR-BASELINE/evidence/revalidation-01/`
   altına 137 ek kanıt dosyası + manifest.tsv = revalidation-01 altında toplam 138 dosya eklendi.
4. **Çalıştırılan komutlar:** §4'te özetlendi; tam liste `manifest.tsv` ve
   her `<case>.cmd` dosyasında.
5. **DoD durumu:** DoD durumu değişmedi. Bu rapor yalnız SQ-090-IR-BASELINE ölçüm kanıtını sunar; SQ-090-IR-FINAL-FORM-AUDIT için aşama yükseltmez.
6. **Kanıtlanmayanlar:** §7'de listelendi (IRB-3.4, 98 opcode'un tamamı,
   eksik-main kabul kriteri, TTY görünümü). Hiçbir madde BLOCKED değildir;
   §7 kapsam dışı bırakılan/koşullu olarak yürütülmeyen maddelerdir.
7. **Riskler ve regresyon yüzeyi:** LOAD_LONG/LOAD_FLOAT32/requiredCap IR
   dump görünürlüğü eksikliği debug/tooling güvenilirliğini etkiler; IRB-3
   diagnostic kategorisi modül isim çözümlemesinde beklenmeyen bir kural
   olabilir (§9 madde 2). Bu görev bu bulguları düzeltmedi, yalnız kaydetti.
8. **Sonraki yetkili rol:** Bulgular ürün sahibine/Şüpheci Başmimar'a
   döner (Başmimar Günlüğü / GitHub #101 üzerinden revalidasyonun kabul
   edilip edilmediği kararı).
