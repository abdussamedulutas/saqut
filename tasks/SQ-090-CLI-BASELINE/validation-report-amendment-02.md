# SQ-090-CLI-BASELINE — Validation Report Amendment 02

**Görev kimliği:** SQ-090-CLI-BASELINE-AMENDMENT-02
**Hedef sürüm:** 0.9.0
**Contract modu:** VALIDATION-ONLY
**Rol:** İzole Hafif Muhalif Testçi (bağımsız yeni oturum)
**Tarih:** 2026-07-25
**Binary:** `/tmp/saqut-sq090-amendment02-build-wbStc9/saqut` (SHA-256 `f5920051a68ed5019d6f5158a2040f67d7d0e762d3fbc82e1f122ebbaedaff79`)

---

## 1. Amendment Gerekçesi

Bu amendment, `validation-report-amendment-01.md` §5.10'daki F3 yorum düzeltmesi ve decision.md §3.3'teki error-tolerant AST / syntax-error CLI davranışının gerçek, tracked bir syntax-error kaynağı ile ölçülmesi için açılmıştır.

### 1.1 F3 yorum düzeltmesi

Amendment-01 testçisi, F3 (`int x = ;`) için "gerçek bir syntax hatası içermez" yorumunu yapmıştır. Bu yorum **kabul edilmez.** `int x = ;` normatif olarak geçersizdir (eksik initializer expression). Parser'ın bunu sessizce kabul etmesi **silent-accept davranışıdır** — fixture'ın geçerli olduğu anlamına gelmez.

**Bu amendment F3'ün ham kanıtını yeniden çalıştırmaz.** Amendment-01'in F3 üzerindeki mevcut ham kanıtı (`evidence/amendment-01/cmd-02-*`, `cmd-17-*`, `cmd-21-*`, `cmd-25-*`, `cmd-28-*`, `cmd-32-*`) korunur ve geçerli kabul edilir. Yalnız yorum değişir.

### 1.2 Yeni F-LSP fixture

`tests/lsp/fixtures/syntax_error_recovery.sqt` (LSP için tracked beklenen E901 içerir) bu amendment'da CLI yüzeylerinde çalıştırılmıştır. Bu, decision.md §3.3'ün (error-tolerant AST + CLI exit 65) doğrulanması için gereklidir.

---

## 2. F3 Yorum Düzeltmesi

### Eski ifade (Amendment-01 §5.10)

> "Bu, `int x = ;`'in dil tarafından sözdizimsel olarak kabul edildiği anlamına gelir. Dil sözleşmesi açısından bu bekleniyorsa, F3 gerçek bir syntax hatası içermez."

### Yeni ifade (bu amendment)

> "F3 (`int x = ;`) üzerinde **eksik initializer expression sessizce kabul ediliyor — silent-accept GÖZLENDİ.** Parser, `int x = ;`'i boş başlatıcılı geçerli bir değişken bildirimi olarak ele alıyor; `ErrorNode` veya `E901` üretmiyor. Bu, decision.md §1(1) ve §3.3'teki 'Bozuk veya eksik alanlar geçerliymiş gibi sessizce yutulmaz' invariant'ının **yapısal ihlalidir** — invariant'ın var olmadığı sonucu değil, ihlalin gözlendiği sonucudur."

### Referans verilen ham kanıt (yeniden okundu, yeniden çalıştırılmadı)

| Kaynak | Kanıt |
|---|---|
| `evidence/amendment-01/cmd-02-run-syntax-error.*` | run F3: exit=0 |
| `evidence/amendment-01/cmd-17-ast-syntax-error.*` | ast F3: exit=0, ErrorNode yok |
| `evidence/amendment-01/cmd-21-ast-json-syntax-error.*` | ast --json F3: exit=0, ErrorNode yok |
| `evidence/amendment-01/cmd-25-ir-syntax-error.*` | ir F3: exit=0, IR dump üretildi |
| `evidence/amendment-01/cmd-28-symbols-syntax-error.*` | symbols F3: exit=0 |
| `evidence/amendment-01/cmd-32-symbols-json-syntax-error.*` | symbols --json F3: exit=0 |

---

## 3. Fresh Build Kanıtı

Tam kanıt: `evidence/amendment-02/00-provenance.md`, `evidence/amendment-02/02-configure-*.txt`, `evidence/amendment-02/03-build-*.txt`

| Ölçüt | Değer |
|---|---|
| HEAD | `7f871b75e917725dcdf46111fab88fb3be5663f2` |
| Branch | `0.9.0` |
| Build dizini | `/tmp/saqut-sq090-amendment02-build-wbStc9` |
| Configure exit | 0 |
| Build exit | 0 |
| Binary boyut | 5,498,984 bytes |
| Timestamp (epoch) | 1784977217 |
| SHA-256 | `f5920051a68ed5019d6f5158a2040f67d7d0e762d3fbc82e1f122ebbaedaff79` |
| Amendment-01 binary SHA-256 | `f5920051a68ed5019d6f5158a2040f67d7d0e762d3fbc82e1f122ebbaedaff79` |
| Karşılaştırma | **IDENTICAL** (aynı HEAD, aynı toolchain — reproducible build) |

**Fresh build onaylandı.** Repo kökündeki `build/` veya amendment-01 build dizini kullanılmamıştır.

---

## 4. Fixture Kopyalama Doğrulaması

Tam kanıt: `evidence/amendment-02/01-fixture-copy-verification.txt`

| Ölçüt | Değer |
|---|---|
| Kaynak | `/home/saqut/Masaüstü/saqutcompiler/tests/lsp/fixtures/syntax_error_recovery.sqt` |
| Kopya | `/tmp/saqut-sq090-amendment02-fixtures-9UGkWU/syntax_error_recovery.sqt` |
| Kaynak SHA-256 | `d54e0d1f9bf59c05fe5bac4da3b552eea4fc0481d7dac7652a566c4c3ffcdd7b` |
| Kopya SHA-256 | `d54e0d1f9bf59c05fe5bac4da3b552eea4fc0481d7dac7652a566c4c3ffcdd7b` |
| **Doğrulama** | **MATCH** |

**Kopyalama birebir doğrulandı.** Kaynak dosya `tests/` altında değiştirilmemiştir.

---

## 5. Komut Matrisi Sonuçları (F-LSP)

Tüm ham kanıt: `evidence/amendment-02/cmd-NN-<label>.*`

### F-LSP: `syntax_error_recovery.sqt` (geçersiz `)` token'ı)

| # | Komut | Exit | stdout | stderr | Evidence |
|---|---|---|---|---|---|
| 01 | `run` | **1** | (boş) | `E901: unexpected token ')' — expected a statement` | cmd-01 |
| 02 | `check` | **1** | JSON diagnostics (E901) | (boş) | cmd-02 |
| 03 | `ast` | **0** | AST text (Error düğümü içerir) | `parser error: ...` | cmd-03 |
| 04 | `ast --json` | **0** | Valid JSON AST (Error düğümü içerir) | `parser error: ...` | cmd-04 |
| 05 | `ir` | **1** | (boş) | `E901: unexpected token ')' — expected a statement` | cmd-05 |
| 06 | `symbols` | **0** | Symbols text (tüm fonksiyonlar) | `parser error: ...` | cmd-06 |
| 07 | `symbols --json` | **0** | Valid JSON (boş diagnostics!) | `parser error: ...` | cmd-07 |

### Kritik gözlemler

1. **`run` (cmd-01): exit=1, E901 stderr'de.** Structured diagnostic `[E901]` ile doğru davranış. ✅
2. **`check` (cmd-02): exit=1, JSON E901 diagnostics.** Structured diagnostic, JSON çıktı. ✅
3. **`ir` (cmd-05): exit=1, E901 stderr'de.** Structured diagnostic. ✅
4. **`ast` (cmd-03) ve `ast --json` (cmd-04): exit=0.** Parser hatasına rağmen `exit 0`. Stderr'de "parser error" mesajı var ama structured `E901` kodlu diagnostic **yok** — yalnız düz metin.
5. **`symbols` (cmd-06) ve `symbols --json` (cmd-07): exit=0.** Parser hatasına rağmen `exit 0`. JSON symbols çıktısında `diagnostics` dizisi **boş** (`"diagnostics": [], "errorCount": 0`).

---

## 6. `ast`/`ast --json`/`symbols`/`symbols --json` için Beş Soru × Dört Komut

### 6.1 `ast` (text, cmd-03)

| # | Soru | Cevap | Kanıt |
|---|---|---|---|
| 1 | **E901 görünüyor mu?** | **EVET** — `E901: unexpected token ')' — expected a statement` AST text içinde `Error` düğümünün parçası olarak görünür | stdout: `Error {E901: unexpected token ')' ...}` |
| 2 | **Kısmi payload üretiliyor mu?** | **EVET** — `topla` (2 kez) ve `main` (1 kez) AST'de görünür. Hatalı `broken()` dışındaki fonksiyonlar mevcut | stdout: grep topla=2, main=1 |
| 3 | **JSON geçerli mi?** | N/A (text modu) | — |
| 4 | **ErrorNode / eşdeğer açık recovery temsili var mı?** | **EVET** — `Error` düğümü (kind=`Error`, code=`E901`, message=...) açıkça `broken()` fonksiyonunun `Block` çocuğu olarak görünür | stdout: `kind: "Error"` |
| 5 | **Exit parser hatasını yansıtıyor mu?** | **HAYIR** — exit=0. Syntax hatasına rağmen başarı kodu döner | cmd-03 exit.txt: 0 |

### 6.2 `ast --json` (cmd-04)

| # | Soru | Cevap | Kanıt |
|---|---|---|---|
| 1 | **E901 görünüyor mu?** | **EVET** — JSON içinde `"code": "E901"` ve `"kind": "Error"` düğümü mevcut | grep 'E901' → 1 match; python3 ile doğrulandı |
| 2 | **Kısmi payload üretiliyor mu?** | **EVET** — `topla`, `main` ve içerdikleri ifadeler JSON AST'de tam olarak görünür | 9388 bytes JSON (boştan çok büyük) |
| 3 | **JSON geçerli mi?** | **EVET** — `jq .` ve `python3 -m json.tool` ile doğrulandı | Her iki araç da VALID dedi |
| 4 | **ErrorNode / eşdeğer açık recovery temsili var mı?** | **EVET** — `"kind": "Error"` düğümü `broken()` fonksiyonunun bloğu içinde, `"code": "E901"` ve `"message"` alanlarıyla | python3: `.ast.children[0].children[0].children[0].kind = Error` |
| 5 | **Exit parser hatasını yansıtıyor mu?** | **HAYIR** — exit=0. Syntax hatasına rağmen başarı kodu döner | cmd-04 exit.txt: 0 |

### 6.3 `symbols` (text, cmd-06)

| # | Soru | Cevap | Kanıt |
|---|---|---|---|
| 1 | **E901 görünüyor mu?** | **HAYIR** — stdout'ta E901 string'i bulunamadı. Hata yalnız stderr'de "parser error" olarak geçer | grep 'E901' → 0. stderr: "parser error: unexpected token..." |
| 2 | **Kısmi payload üretiliyor mu?** | **EVET** — `broken`, `topla`, `main` fonksiyonları ve `a`, `b`, `x` değişkenleri symbols çıktısında mevcut | grep topla=1, main=1 |
| 3 | **JSON geçerli mi?** | N/A (text modu) | — |
| 4 | **ErrorNode / eşdeğer açık recovery temsili var mı?** | **HAYIR (gözlenemedi)** — symbols çıktısında error düğümü olup olmadığı formatından tespit edilemedi. `broken()` symbols'de görünüyor ancak hatalı olduğunu belirten bir işaret yok | stdout: `broken: function` (normal görünüyor) |
| 5 | **Exit parser hatasını yansıtıyor mu?** | **HAYIR** — exit=0. Syntax hatasına rağmen başarı kodu döner | cmd-06 exit.txt: 0 |

### 6.4 `symbols --json` (cmd-07)

| # | Soru | Cevap | Kanıt |
|---|---|---|---|
| 1 | **E901 görünüyor mu?** | **HAYIR** — JSON çıktıda `E901` string'i yok. `diagnostics.diagnostics` dizisi **boş** | grep 'E901' → 0; python3: `"diagnostics": [], "errorCount": 0` |
| 2 | **Kısmi payload üretiliyor mu?** | **EVET** — `broken`, `topla`, `main` fonksiyonları ve parametreler/değişkenler JSON'da mevcut | 4128 bytes, 6 symbol entry |
| 3 | **JSON geçerli mi?** | **EVET** — `python3 -m json.tool` ile doğrulandı | VALID JSON |
| 4 | **ErrorNode / eşdeğer açık recovery temsili var mı?** | **HAYIR** — JSON symbols çıktısında error/hatalı işareti yok. `broken` normal bir fonksiyon olarak listeleniyor | Tüm 6 symbol aynı formatta |
| 5 | **Exit parser hatasını yansıtıyor mu?** | **HAYIR** — exit=0. Syntax hatasına rağmen başarı kodu döner, üstelik JSON diagnostics boş | cmd-07 exit.txt: 0; `errorCount: 0` |

---

## 7. LSP Beklentisi ile CLI Gözlemi Karşılaştırması

| Boyut | LSP Beklentisi (`09_syntax_error_recovery.expected.jsonl`) | CLI Gözlemi | Karşılaştırma |
|---|---|---|---|
| E901 varlığı | 2. satırda E901 beklenir | `run`, `check`, `ir` E901 üretir; `ast` text/JSON içinde E901 görünür; `symbols` hiç E901 göstermez | **Kısmen eşleşiyor** — run/check/ir/ast E901 içerir; symbols hiç E901 üretmez |
| Diagnostic konumu | Satır 1, karakter 4-5 (0-indexed) → 2. satır, kolon 5 (1-indexed) | `run`, `check`, `ir`: `line:2, column:5`. Aynı konum. | **Eşleşiyor** |
| Hata mesajı | `"unexpected token ')' — expected a statement"` | Tüm CLI yüzeyleri aynı mesajı kullanır | **Eşleşiyor** |
| Hata kodu | `E901` | `run`, `check`, `ir` structured `[E901]`; `ast` text/JSON içinde `E901`; `symbols` E901 **yok** | **Kısmen eşleşiyor** |
| Kısmi sembol çözümü | LSP hatalı kaynakta `topla`/`main` sembollerini çözümler | `symbols --json` tüm 6 sembolü (broken, topla, main, a, b, x) listeler | **Eşleşiyor** (beklenenden fazla) |

**Nötr değerlendirme:** LSP, `09_syntax_error_recovery.expected.jsonl`'de syntax hatası durumunda hangi sembollerin döndüğünü belirtmez — yalnız diagnostic bekler. CLI `symbols`'ün E901 üretmemesi ve exit=0 dönmesi LSP beklentisiyle çelişmez çünkü LSP kendi diagnostic akışını DocumentStore üzerinden ayrı yönetir. Ancak bu, **CLI `symbols`'ün syntax hatasını bildirmemesi** sorununu ortadan kaldırmaz.

---

## 8. decision.md §1'deki Sekiz Yapısal Tespit — Bu Amendment'ın Sınıflandırması

| # | Tespit | Amendment-01 Sınıflandırması | Bu Amendment Sınıflandırması | Dayanak |
|---|---|---|---|---|
| 1 | `ast`/`symbols`/`exec` DiagnosticEngine'siz; sözdizimi hatası yalnız stderr'e | GÖZLENDİ (korundu) | **GÖZLENDİ (yeni kanıtla doğrulandı)** | F-LSP fixture'ında `ast` exit=0, `symbols` exit=0 — hata yalnız stderr'de düz metin, diag'a girmez. `run`/`check`/`ir` structured E901 üretir. |
| 2 | `ast` koşulsuz `return 0` | GÖZLENDİ (korundu) | **GÖZLENDİ (yeni kanıtla doğrulandı)** | F-LSP fixture'ında `ast` exit=0 (hatalı girdi). |
| 3 | `symbols` exit = diag.hasErrors(); parser hataları diag'a ulaşmaz | GÖZLENDİ (ayrıntıyla) | **GÖZLENDİ (yeni kanıtla doğrulandı)** | F-LSP fixture'ında `symbols` exit=0; `symbols --json` `errorCount: 0` — parser hatası diag'a kaydedilmemiş. |
| 4 | `run` exit = main dönüş değeri; ValueKind denetimi yok | GÖZLENDİ (yeni) | Bu amendment kapsamı dışında, amendment-01'deki sınıflandırma geçerli | — |
| 5 | `main` yokluğu semantik kapıda değil, VM'de runtime_error | GÖZLENDİ (korundu) | Bu amendment kapsamı dışında, amendment-01'deki sınıflandırma geçerli | — |
| 6 | parseArgs: bilinmeyen argüman `run` sayar; `source.sqt` ghost; help/unknown ölü | GÖZLENDİ (korundu) | Bu amendment kapsamı dışında, amendment-01'deki sınıflandırma geçerli | — |
| 7 | `compile`/`parse`/`transpile`/`interpret` TODO stub; `-` TODO | GÖZLENDİ (korundu) | Bu amendment kapsamı dışında, amendment-01'deki sınıflandırma geçerli | — |
| 8 | Tracked testler yalnız `run`; `ast`/`symbols`/`exec` testi yok | GÖZLENDİ (yeni) | Bu amendment kapsamı dışında, amendment-01'deki sınıflandırma geçerli | — |

### F3 silent-accept invariant ihlali (yeni bulgu)

| # | Tespit | Sınıflandırma | Dayanak |
|---|---|---|---|
| 9 | `int x = ;` (eksik initializer) parser tarafından sessizce kabul ediliyor. ErrorNode/E901 üretilmiyor. `ast`/`symbols`/`run` exit=0. | **GÖZLENDİ — silent-accept** | Amendment-01 ham kanıdı: `cmd-02`, `cmd-17`, `cmd-21`, `cmd-25`, `cmd-28`, `cmd-32`. Bu amendment yorumu düzeltti. |

### §3.3 (error-tolerant AST) değerlendirmesi

F-LSP fixture'ı ile §3.3'ün bazı maddeleri artık ölçülebilir:

| §3.3 Maddesi | Durum | Kanıt |
|---|---|---|
| "Parser best-effort kısmi AST üretir" | **GÖZLENDİ** ✅ | `ast --json` hatalı `broken()` dışındaki `topla`/`main` için kısmi AST içerir |
| "Sözdizimi hataları AST üretimini tamamen durdurmaz" | **GÖZLENDİ** ✅ | AST başarıyla üretildi, Error düğümü içeriyor |
| "Recovery edilen her hata structured diagnostic üretir" | **GÖZLENDİ (run/check/ir)** ⚠️ **GÖZLENMEDİ (ast/symbols)** ❌ | `run`/`check`/`ir` structured E901 üretir. `ast`/`symbols` yalnız stderr'de düz metin, exit=0 |
| "Bozuk/eksik alanlar sessizce yutulmaz; ErrorNode kullanılır" | **GÖZLENDİ (ast)** ✅ **GÖZLENMEDİ (F3 silent-accept)** ❌ | ast JSON `Error` düğümü içerir. F3'te ErrorNode yok |
| "ast/symbols hatalı girdide kısmi çıktı üretmeye devam eder" | **GÖZLENDİ** ✅ | Kısmi payload mevcut |
| "Text modunda: kısmi payload stdout, diagnostics stderr" | **GÖZLENDİ** ✅ | ast/symbols stdout kısmi, stderr "parser error" |
| "JSON modunda: her zaman geçerli JSON; diagnostics + kısmi payload" | **KISMEN** ⚠️ | JSON geçerli; `ast --json`'da diagnostics yok (`errors`/`diagnostics` üst alanı yok); `symbols --json`'da diagnostics boş |
| "Sözdizimi hatası varsa CLI exit 65" | **GÖZLENMEDİ** ❌ | `ast` exit=0, `symbols` exit=0, `run` exit=1 (65 değil), `check` exit=1 (65 değil), `ir` exit=1 (65 değil). **Hiçbiri exit 65 kullanmıyor.** |

---

## 9. DoD Teyidi

**DoD durumu: Hâlâ Tasarlandı.**

Bu amendment VALIDATION-ONLY'dir. DoD'yi "Uygulandı"ya taşımaz. Kararın uygulama aşamasına geçmesi için ürün sahibinin ve başmimarın bu raporu değerlendirmesi gerekir.

Baseline kapısı (decision.md §9) için bu amendment şunları tamamlamıştır:

1. ✅ **F3 yorum düzeltmesi** — silent-accept olarak yeniden sınıflandırıldı
2. ✅ **§3.3 (error-tolerant AST)** — gerçek syntax-error fixture'ı ile 7 CLI yüzeyinde ölçüldü
3. ❌ **Exit 65** — hiçbir komut exit 65 kullanmıyor; `ast`/`symbols` hatalı girdide exit=0 dönüyor
4. ⚠️ **Structured diagnostic** — `run`/`check`/`ir` E901 üretiyor; `ast`/`symbols` yalnız düz metin stderr

---

## 10. Final Not

Bu amendment **build ve test çalıştırmıştır** (validation-only ölçüm amacıyla). Bu, kaynağı veya tracked test'i değiştirdiği anlamına gelmez.

**Görev sonu `git status --short`:** (ham çıktı)

```
D .claude/agents/architect.md
 D .claude/agents/coder.md
 D .claude/agents/project-manager.md
 D .claude/agents/tester.md
 D .claude/hooks/role-guard.sh
 D .claude/settings.json
 M .gitignore
 M CLAUDE.md
 M docs/adr/ADR-032-mir-jit-gomulu-runtime-aot.md
 M docs/adr/ADR-038-determinizm-surum-uyumlulugu.md
 M docs/adr/ADR-041-self-hosted-stdlib-thin-runtime.md
 M docs/architecture.md
 D organization.md
 D screenshots/.directory
 D screenshots/dap.png
 D screenshots/lsp.png
 D target.txt
 D wiki/arrays.md
 D wiki/cli-commands.md
 D wiki/compound-assignment.md
 D wiki/control-flow.md
 D wiki/error-handling.md
 D wiki/functions.md
 D wiki/getting-started.md
 D wiki/globals.md
 D wiki/home.md
 D wiki/literals.md
 D wiki/operators.md
 D wiki/optimization.md
 D wiki/pipeline.md
 D wiki/strings.md
 D wiki/structs.md
 D wiki/variables-types.md
?? .codewhale/
?? AGENTS.md
?? docs/adr/ADR-042-v1-feedback-mvp-ve-surumleme.md
?? docs/v0.9-v1.0-yol-haritasi.md
?? docs/v1.0-issue-disposition.md
?? docs/v1.0-kapsam-bildirgesi.md
?? docs/yerel-agent-kullanim-rehberi.md
?? knowledge-base/
?? prompts/
?? tasks/
?? tests/general/
```

**Değişiklik yok** — yalnızca `tasks/SQ-090-CLI-BASELINE/evidence/amendment-02/` altına yeni evidence dosyaları eklenmiştir (önceden `?? tasks/` olarak görünen untracked dizin altında). `src/`, `tests/`, ve tracked hiçbir dosya değiştirilmemiştir.

---

## Ek: Kritik Bulgular

1. **`ast` ve `symbols` syntax hatasında exit=0 dönüyor.** Decision.md §3.3 exit 65 sözleşmesi ihlal ediliyor. Bu, §1(2) ve §1(3)'teki "sahte başarı yolları" sınıfına girer.

2. **`symbols --json` boş diagnostics dönüyor.** `errorCount: 0` — hatalı girdide dahi hatasız bildirim. Bu, decision.md §3.3'ün "JSON modunda diagnostics ve kısmi payload birlikte bulunur" maddesini ihlal eder.

3. **`run` exit=1 (65 değil).** Decision.md §3.1'deki exit kodu sözleşmesine göre source/parse hatası exit 65 olmalıdır. Şu an exit 1 kullanılıyor — bu da §3.1'de tanımlanmamış bir durum.

4. **F3 silent-accept (eksik initializer) devam ediyor.** `int x = ;` hâlâ ErrorNode üretmiyor ve exit=0 ile kabul ediliyor. Bu, decision.md §3.3'ün "Bozuk veya eksik alanlar sessizce yutulmaz" kuralının ihlalidir.

5. **LSP `09_syntax_error_recovery.expected.jsonl` beklentisi CLI `symbols` davranışından farklıdır.** LSP E901 beklentisi varken CLI `symbols` hiçbir structured diagnostic üretmez. Bu ayrışma LSP'nın kendi `DocumentStore` diag yolundan geldiği için beklenebilir, ancak CLI ile LSP arasında diagnostic tutarlılığı yoktur.

6. **`ast` text modu Error düğümü içeriyor (kind="Error", code="E901"),** bu §3.3'te istenen açık recovery temsilini karşılar. Ancak stderr'de yalnız "parser error" var — structured `[E901]` yok; exit 0.
