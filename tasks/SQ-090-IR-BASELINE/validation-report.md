# SQ-090-IR-BASELINE — Validation Report

**Revision:** Amendment 02  
**Validation date:** 2026-07-26  
**Rol:** İzole Hafif Muhalif Testçi  
**Task ID:** SQ-090-IR-BASELINE  
**Hedef sürüm:** 0.9.0  
**Mod:** VALIDATION-ONLY

---

## 1. Binary Provenance

| Alan | Değer |
|---|---|
| HEAD | `7f871b75e917725dcdf46111fab88fb3be5663f2` |
| Branch | `0.9.0` |
| Binary path | `/tmp/saqut-sq090-ir-baseline-187801/build/saqut` |
| Binary boyut | 5,498,984 bytes |
| Binary SHA-256 | `98c0acdedfb9eddac2c96c8b15aeadc37e977c725746c8bf8bd5a989580ee5a2` |
| Build türü | Release (out-of-tree, fresh build) |
| Build exit | 0 (configure: 0, build: 0) |
| Derleyici | g++ 16.1.1 20260625 |
| CMake | 4.3.4 |

Tüm ayrıntılar: `tasks/SQ-090-IR-BASELINE/evidence/00-provenance.md`

---

## 2. Dondurulmuş Test Matrisi

| ID | Fixture | Contract maddesi | Sonuç |
|---|---|---|---|
| IRB-1 | `examples/fibonacci.sqt` | Determinizm — üç tekrar byte-identical | **GÖZLENDİ** |
| IRB-2 | `examples/fibonacci.sqt` | Dump instr count == profile instr count | **GÖZLENDİ** |
| IRB-3 | `$WORK/fixtures/irb3/` (4 dosya) | Çapraz-modül aynı adlı fonksiyon | **GÖZLENDİ** |
| IRB-4 | `$WORK/fixtures/irb4/main.sqt` | Void fallthrough | **GÖZLENDİ** |
| IRB-5 control | `$WORK/fixtures/irb5/control.sqt` | LOAD_CONST operand görünür | **GÖZLENDİ** |
| IRB-5 long | `$WORK/fixtures/irb5/long_load.sqt` | LOAD_LONG operand görünür | **GÖZLENMEDİ** |
| IRB-5 float | `$WORK/fixtures/irb5/float_load.sqt` | LOAD_FLOAT32 operand görünür | **GÖZLENMEDİ** |
| IRB-5 cap | `$WORK/fixtures/irb5/cap.sqt` | requiredCap görünür | **GÖZLENMEDİ** |
| IRB-6 | `$WORK/fixtures/irb6/globals.sqt` | Main'siz global initializer | **GÖZLENDİ** |
| IRB-7 | `$WORK/fixtures/irb5/cap.sqt` | `--capabilities` modu | **GÖZLENDİ** |
| IRB-8 | `examples/fibonacci.sqt` | ANSI (redirect) | **GÖZLENDİ** |
| IRB-9 | CTest | `ir_opt` golden testler | **GÖZLENDİ** |

---

## 3. Exact Komutlar (komut başına evidence referansı)

### IRB-1
```bash
"$SAQUT" ir "$REPO_ROOT/examples/fibonacci.sqt"  # üç kez
```
Evidence: `IRB-1-run{1,2,3}.{stdout,stderr,exit}`

### IRB-2
```bash
"$SAQUT" ir "$REPO_ROOT/examples/fibonacci.sqt"
"$SAQUT" run --profile "$REPO_ROOT/examples/fibonacci.sqt"
```
Evidence: `IRB-2-dump-raw.{stdout,stderr,exit}`, `IRB-2-dump-clean.stdout`, `IRB-2-instruction-lines.txt`, `IRB-2-profile.{stdout,stderr,exit}`

### IRB-3
```bash
cd "$WORK/fixtures/irb3" && "$SAQUT" check single-main.sqt
cd "$WORK/fixtures/irb3" && "$SAQUT" ir single-main.sqt
cd "$WORK/fixtures/irb3" && "$SAQUT" run single-main.sqt
cd "$WORK/fixtures/irb3" && "$SAQUT" check main.sqt
```
Evidence: `IRB-3-pos-{1,2,3}.{stdout,stderr,exit}`, `IRB-3-pos-{1,2,3}-cmd.md`, `IRB-3-main-check.{stdout,stderr,exit}`, `IRB-3-main-check-cmd.md`

### IRB-4
```bash
"$SAQUT" check "$WORK/fixtures/irb4/main.sqt"
"$SAQUT" ir "$WORK/fixtures/irb4/main.sqt"
"$SAQUT" run "$WORK/fixtures/irb4/main.sqt"
```
Evidence: `IRB-4-check.{stdout,stderr,exit}`, `IRB-4-ir-raw.{stdout,stderr,exit}`, `IRB-4-run.{stdout,stderr,exit}`, `IRB-4-instruction-lines.txt`

### IRB-5
```bash
"$SAQUT" check "$WORK/fixtures/irb5/control.sqt" && "$SAQUT" ir ...
"$SAQUT" check "$WORK/fixtures/irb5/long_load.sqt" && "$SAQUT" ir ...
"$SAQUT" check "$WORK/fixtures/irb5/float_load.sqt" && "$SAQUT" ir ...
"$SAQUT" check --allow-fs "$WORK/fixtures/irb5/cap.sqt" && "$SAQUT" ir --allow-fs ...
```
Evidence: `IRB-5-ctrl-*`, `IRB-5-long-*`, `IRB-5-float-*`, `IRB-5-cap-*` içindeki `.{stdout,stderr,exit}` ve `-clean.stdout`

### IRB-6
```bash
"$SAQUT" ir "$WORK/fixtures/irb6/globals.sqt"
"$SAQUT" run "$WORK/fixtures/irb6/globals.sqt"
```
Evidence: `IRB-6-ir-raw.{stdout,stderr,exit}`, `IRB-6-run.{stdout,stderr,exit}`

### IRB-7
```bash
"$SAQUT" ir --allow-fs "$WORK/fixtures/irb5/cap.sqt"
"$SAQUT" ir --allow-fs --capabilities "$WORK/fixtures/irb5/cap.sqt"
```
Evidence: `IRB-7-ir-normal.{stdout,stderr}`, `IRB-7-ir-capabilities.{stdout,stderr}`

### IRB-8
```bash
"$SAQUT" ir "$REPO_ROOT/examples/fibonacci.sqt" > "$EVIDENCE_DIR/irb8-stdout.raw" 2>"$EVIDENCE_DIR/irb8-stderr.raw"
```
Evidence: `irb8-stdout.raw`, `irb8-stderr.raw`, `irb8-stdout.hex`, `irb8-stderr.hex`

### IRB-9
```bash
ctest --test-dir "$WORK/build" -N
ctest --test-dir "$WORK/build" -R '^golden_opt_dce_ir_opt$' --output-on-failure
ctest --test-dir "$WORK/build" -R '^golden_opt_folding_ir_opt$' --output-on-failure
```
Evidence: `irb9-inventory.{stdout,stderr}`, `irb9-test1.{stdout,stderr}`, `irb9-test2.{stdout,stderr}`

---

## 4. Her Vaka İçin Expected / Actual / Exit

### IRB-1 — Determinizm
| | Run 1 | Run 2 | Run 3 |
|---|---|---|---|
| Exit | 0 | 0 | 0 |
| stdout SHA-256 | `572d3d08...` | `572d3d08...` | `572d3d08...` |
| stderr SHA-256 | `e3b0c442...` | `e3b0c442...` | `e3b0c442...` |

**Sonuç: GÖZLENDİ** — Üç çalıştırma byte-identical sonuç üretti.

### IRB-2 — Instruction sayısı
| Ölçüm | Değer |
|---|---|
| Dump instruction satırı sayısı | 32 |
| Profile `ir-gen` instr sayısı | 32 |
| Eşitlik | ✅ birebir eşit |

**Sonuç: GÖZLENDİ** — 32 == 32.

### IRB-3 — Çapraz-modül aynı adlı fonksiyon
**Pozitif kontrol (single-main.sqt):** `check` exit 0, `ir` exit 0, `run` exit 0, stdout `6` (5+1).
**Asıl senaryo (main.sqt):** `check` exit 65, diagnostic `E_SYMBOL_NOT_IMPORTED`:
```
/tmp/.../fixtures/irb3/b.sqt:6:12: error [E_SYMBOL_NOT_IMPORTED]: 'shared' is from another module and must be imported explicitly
```
`shared` çakışması semantic kapıda diagnostic üretti.

**Sonuç: GÖZLENDİ** — Semantic kapıda diagnostic üretildi.

### IRB-4 — Void fallthrough
| Ölçüm | Değer |
|---|---|
| `check` exit | 0 |
| `sideEffect` son instruction | `CALLHOST print(s0)` (RETURN değil) |
| `run` exit | 0 |
| `run` stdout | `onceafter` |

**Sonuç: GÖZLENDİ** — Void fonksiyon `sideEffect`'in son instruction'ı RETURN değildir; fallthrough mevcuttur.

### IRB-5 — Görünmeyen executable operandlar

| Alt madde | check exit | IR exit | Operand görünür? | Gözlem |
|---|---|---|---|---|
| Control (LOAD_CONST) | 0 | 0 | **Evet** `s0 = 42` | LOAD_CONST operand olarak `42` görünür |
| LOAD_LONG (longint) | 0 | 0 | **Hayır** | `LOAD_LONG` var ama `4200000000` sabiti görünmez |
| LOAD_FLOAT32 (float) | 0 | 0 | **Hayır** | `LOAD_FLOAT32` var ama `1.5` sabiti görünmez |
| Capability (requiredCap) | 0 | 0 | **Hayır** | `CALLHOST __ffi__(s0)` dump'ta `[cap: ...]` eki görünmez |

**Kontrol grubu: GÖZLENDİ** — LOAD_CONST operandı görünür.
**LOAD_LONG: GÖZLENMEDİ** — `4200000000` sabit değeri dump satırında görünmez.
**LOAD_FLOAT32: GÖZLENMEDİ** — `1.5` sabit değeri dump satırında görünmez.
**requiredCap: GÖZLENMEDİ** — capability bilgisi dump'ta görünmez.

(Hiçbir gözlem düzeltilmez; ham veridir.)

### IRB-6 — Main'siz global initializer
| Ölçüm | Değer |
|---|---|
| `ir` exit | 0 |
| GLOBALS bölümü | Var: `GLOBALS (1)`, `global[0] = counter` |
| STORE_GLOBAL instruction | Yok (fonksiyon gövdesi olmadığı için) |
| `run` exit | 70 |
| `run` stderr | `runtime error: 'main' function not found` |

**Sonuç: GÖZLENDİ** — GLOBALS bölümü mevcut, `counter` adı görünüyor. `run` davranışı raw veridir (beklenti değil).

### IRB-7 — Capabilities modu
| Ölçüm | Değer |
|---|---|
| `--allow-fs` (normal) exit | 0, tam instruction dump |
| `--allow-fs --capabilities` exit | 0, yalnız `capabilities: fs` |
| Instruction dump kaybolur mu? | Evet, `--capabilities` modunda kaybolur |
| Exit code aynı mı? | Evet, her ikisi de 0 |

**Sonuç: GÖZLENDİ** — `--capabilities` modu özet satır basar, instruction dump'ı kaybolur.

### IRB-8 — ANSI
| Kanal | `1b5b` (ESC+[) sayısı |
|---|---|
| stdout (redirect) | 411 |
| stderr (redirect) | 0 |

**Sonuç: GÖZLENDİ** — Yönlendirilmiş dosyada stdout'ta 411, stderr'de 0 ANSI CSI dizisi tespit edildi.

### IRB-9 — Mevcut tracked IR testleri
| Test | Anchored regex | Exit | Sonuç |
|---|---|---|---|
| `golden_opt_dce_ir_opt` | `^golden_opt_dce_ir_opt$` | 0 | Passed |
| `golden_opt_folding_ir_opt` | `^golden_opt_folding_ir_opt$` | 0 | Passed |

**Sonuç: GÖZLENDİ** — İki `ir_opt` golden testi envanterde bulundu ve her ikisi de passed.

---

## 5. DoD Durumu

Bu görev **VALİDATION-ONLY baseline**'dır. Hiçbir DoD yükseltmesi yapılmaz. Mevcut durum:

- `Tasarlandı` / `Uygulandı` / `Test Edildi` / `Release Edildi` — hiçbiri ilerletilmemiştir.
- Bu rapor yalnızca ölçüm sonuçlarını ham kanıtla kaydeder.
- `Test Edildi` yalnız SQ-090-IR-FINAL-FORM-AUDIT'in kendi kabul kriterleri karşılanırsa ayrı bir kararla değerlendirilir.

---

## 6. Kanıtlanmayanlar (BLOCKED yok)

Bu görevde **BLOCKED** madde bulunmamaktadır. Tüm IRB maddeleri ölçülebilmiştir:

- **GÖZLENDİ:** IRB-1, IRB-2, IRB-3, IRB-4, IRB-5 (control), IRB-6, IRB-7, IRB-8, IRB-9
- **GÖZLENMEDİ:** IRB-5 (LOAD_LONG operand görünmez), IRB-5 (LOAD_FLOAT32 operand görünmez), IRB-5 (requiredCap dump'ta görünmez)

---

## 7. Riskler ve Regresyon Yüzeyi

1. **ANSI count (411) in stdout:** Yönlendirilmiş dosyada 411 ANSI CSI dizisi bulunmuştur. Bu sayı, instruction sayısına (32) göre yüksektir — her instruction satırı başına birden fazla ANSI kaçış dizisi olabilir. Bu, dump formatının okunabilirlik için renklendirme kullandığını ancak pipe/grep/redirect gibi araçlarla kullanımı zorlaştırdığını gösterir. Ham veridir, öneri değildir.

2. **LOAD_LONG / LOAD_FLOAT32 operand görünmez:** Bu iki opcode'un dump satırında sabit değer içermemesi, IR dump'ının teşhis amaçlı kullanımını sınırlayabilir. `LOAD_CONST` ise sabiti gösterir — bu farkın kasıtlı olup olmadığı bu görevin kapsamı dışındadır.

3. **IRB-3 diagnostic:** `shared` fonksiyonunun aynı modül `b.sqt` içinde `fromB()` tarafından çağrılması `E_SYMBOL_NOT_IMPORTED` hatası üretmiştir. Bu, çapraz-modül görünürlük/izolasyon mekanizmasının beklenenden farklı çalıştığını gösterebilir. Başmimar değerlendirmesi önerilir.

4. **IRB-4 void fallthrough:** `sideEffect` fonksiyonu void olmasına rağmen (ve hiçbir `return` instruction'ı bulunmamasına rağmen) `check` hatasız geçer ve `run` başarıyla çalışır. Bu ya kasıtlı bir tasarımdır ya da non-void return-path doğrulamasının yalnız non-void fonksiyonlar için çalıştığını gösterir.

5. **Capability IR annotation yok:** `CALLHOST __ffi__(s0)` dump satırında `[cap: ...]` veya benzeri bir capability eki bulunmamaktadır. Capability bilgisi yalnız `--capabilities` modunda özet olarak görünür.

6. **Stale binary riski yok:** Fresh out-of-tree build kullanılmıştır. Binary boyut/hash kaydedilmiştir.

---

## 8. Sonraki Yetkili Rol

Bulgular ürün sahibine/ağır mimara döner. Özellikle IRB-3'teki `E_SYMBOL_NOT_IMPORTED` diagnostic'inin beklenen davranış olup olmadığı ve IRB-5'teki LOAD_LONG/LOAD_FLOAT32 operand görünürlüğü eksikliğinin bir tasarım kararı mı yoksa eksiklik mi olduğu değerlendirilmelidir.
