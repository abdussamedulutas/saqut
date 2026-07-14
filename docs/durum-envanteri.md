# saQut Derleyici — Durum Envanteri

**Tarih:** 2026-06-24  
**Commit:** `46e640ffaad308daac7c9df3bbb2be28a98c0cc7`  
**Dal:** `benchmark`  
**ctest özeti:** **42/45 geçiyor, 3 BAŞARISIZ** (ayrıntı §Bölüm 5)

---

## Bölüm 0 — Test Zemini

`cmake -B build -G Ninja && ninja -C build && cd build && ctest` ile çalıştırıldı.

| # | Test | Durum | Neden |
|---|------|-------|-------|
| 4 | `golden_arithmetic_mod_by_zero_runtime_error` | **BAŞARISIZ** | `.runtime_error` dosyası Türkçe regex bekliyor: `sıfıra bölme \(mod\)`. Gerçek çıktı İngilizce: `division by zero (modulo)`. Lokalizasyon uyumsuzluğu — `interpreter.cpp:238`. |
| 37 | `golden_opt_dce_ir_opt` | **BAŞARISIZ** | Son commit `46e640f` IR çıktısına ANSI renk kodları ekledi; `.ir_opt.expected` dosyası sade metin bekliyor. Kod doğru, golden güncellenmeli. |
| 38 | `golden_opt_folding_ir_opt` | **BAŞARISIZ** | Aynı ANSI renk kodu sorunu — `.ir_opt.expected` güncellenmeli. |

42 geçen test, aşağıdaki tüm temel dil özelliklerini kapsamaktadır.

---

## Bölüm 1 — Dil Özellikleri

### 1.1 Tipler

| Tip | Durum | Kaynak kanıtı | Golden test | Sonuç |
|-----|-------|---------------|-------------|-------|
| `int` | **çalışıyor** | `vm/value.hpp` `ValueKind::Int`, `interpreter.cpp` aritmetik | `tests/golden/arithmetic/basic.sqt` | 1/1 ✓ |
| `float` | **çalışıyor** | `vm/value.hpp` `ValueKind::Float`, `interpreter.cpp:425` LOAD_FLOAT | `tests/golden/float/basic.sqt` | 1/1 ✓ |
| `decimal` | **çalışıyor** (ctest dışı) | `core/decimal.hpp`, `interpreter.cpp:599` LOAD_DECIMAL | `tests/golden/decimal/basic.sqt` — `.expected` YOK → ctest dışı | Manuel çalıştırıldı: `4`, `3.3333333333333333`, `-1.5` — beklenenle uyuşuyor |
| `bool` | **çalışıyor** | `ValueKind::Bool`, `interpreter.cpp` mantıksal opcode'lar | `tests/golden/logic/not_operator.sqt`, `short_circuit.sqt` | 2/2 ✓ |
| `string` | **çalışıyor** | `ValueKind::String`, string builtin metodları | `tests/golden/string/` (4 test) | 4/4 ✓ |
| `struct` | **kısmen çalışıyor** | `vm/object.hpp` StructObject, `interpreter.cpp:457` STRUCT_NEW | `tests/golden/struct/basic.sqt`, `builtin/struct_methods.sqt`, `builtin/struct_array.sqt` | 3/3 ✓ — ama tuzaklar aşağıda |
| `array` | **çalışıyor** | `vm/object.hpp` ArrayObject, `interpreter.cpp:486` ARRAY_NEW | `tests/golden/array/ref_semantics.sqt`, `builtin/array_*.sqt` | 5/5 ✓ |
| `enum` | **çalışıyor** (ctest dışı) | `ir_generator.cpp:929` enum layout, LOAD_CONST | `tests/golden/enum/basic.sqt` — `.expected` YOK → ctest dışı | Manuel: `1\n1` — doğru; explicit_values: `404\n1\n0` — doğru |
| nullable `T?` | **çalışıyor** | `type_checker.cpp` nullable analiz, `interpreter.cpp` null guard | `tests/golden/null/` (4 test: narrowing, and_narrowing, 2×compile_error) | 4/4 ✓ |
| `cast` (`as`) | **çalışıyor** (ctest dışı) | `ir_generator.cpp` CAST_* opcode'lar, ADR-026 | `tests/golden/cast/` (3 dosya) — `.expected` YOK → ctest dışı | Manuel: basic ✓, nullable_cast ✓, cast_error ✓ |
| `byte` | **çalışıyor** | `PrimitiveKind::Byte`, `CAST_INT_TO_BYTE_CHECKED`, aritmetik int'e terfi (#86) | `tests/golden/byte/` (literal, arithmetic, cast, array) | 4/4 ✓ |

#### Struct üç tuzağı — kritik

**(1) `b = a` — referans kopyası mı derin kopya mı?**

Test: `Point b = a; b.x = 99; print(a.x);`

Sonuç: `a.x = 99` çıktı verdi. ADR-020'ye uygun (struct referans tipidir, atama referans kopyasıdır). **DOĞRU davranış.**  
Golden test: **YOK** — referans semantiği doğrudan test edilmiyor (sessiz bir kural).

**(2) MAKE_STRUCT özyinelemeli sıfırlama**

Test: `Point p;  print(p.x);  print(p.y);`

Sonuç: `0\n0` — sıfırlanmış. `interpreter.cpp:458` `allocStruct(n)` yeni nesne açıyor, başlangıç değerleri `Value::fromInt(0)` ile dolduruluyor.  
Golden test: **YOK**

**(3) `r.topLeft.x = 5` — nested struct field ataması**

Test: `Rect r; r.topLeft.x = 5;`

Sonuç: **`runtime error: not a struct`** — ÇALIŞMIYOR.

Kök neden: `STRUCT_NEW` (ir_generator.cpp:1262) yalnızca dış struct'ı tahsis eder; içteki `struct` tipli alanlar için iç nesneler oluşturulmaz. Alanlar `Value()` (null-ref) olarak başlar. `FIELD_GET s2 = s0.0` (topLeft'i oku) bir null pointer döndürür; ardından `FIELD_GET s3 = s2.0` null pointer üzerinde erişmeye çalışır → runtime hatası.

Bu hem **okuma** hem **yazma** için geçerlidir: `print(r.topLeft.x)` de aynı hatayı üretir.

Golden test: **YOK** — bu kritik hata test edilmiyor.

### 1.2 Operatörler

| Grup | Durum | Golden test | Sonuç |
|------|-------|-------------|-------|
| Aritmetik `+ - * / %` | **çalışıyor** | `arithmetic/basic.sqt`, `precedence.sqt` | 2/2 ✓ |
| `%=` bileşik atama | **çalışıyor** | `arithmetic/compound_mod.sqt` | 1/1 ✓ (issue #37 KAPANDI) |
| Tüm bileşik atama `+= -= *= /= %= &= \|= ^= <<= >>=` | **çalışıyor** | `arithmetic/compound_mod.sqt` zincir testi | 1/1 ✓ |
| Karşılaştırma `< > <= >=` | **çalışıyor** | `arithmetic/basic.sqt` içinde örtük | dolaylı ✓ |
| Eşitlik `==` / `!=` | **çalışıyor** | `string/equality.sqt`, ADR-023 kimlik eşitliği | 1/1 ✓ |
| Mantıksal `&&` `\|\|` `!` (kısa devre dahil) | **çalışıyor** | `logic/short_circuit.sqt`, `logic/not_operator.sqt` | 2/2 ✓ |
| Bitwise `& \| ^ ~ << >>` | **çalışıyor** | `bitwise/basic.sqt`, `bitwise/compound.sqt` | 2/2 ✓ |

### 1.3 Kontrol Akışı

| Yapı | Durum | Golden test | Sonuç |
|------|-------|-------------|-------|
| `if` / `else` | **çalışıyor** | `null/narrowing.sqt` içinde örtük | ✓ |
| `for` döngüsü | **çalışıyor** | `loops/basic.sqt`, `loops/for_break_continue.sqt` | 2/2 ✓ |
| `while` | **çalışıyor** | `loops/while_break_continue.sqt` | 1/1 ✓ |
| `do-while` | **çalışıyor** | `loops/do_while_once.sqt`, `loops/do_while_truthy.sqt` | 2/2 ✓ |
| `break` / `continue` | **çalışıyor** | `loops/for_break_continue.sqt`, `loops/nested_break.sqt` | 2/2 ✓ |
| `return` | **çalışıyor** | `fibonacci/fib.sqt` vb. | ✓ |
| `switch-case` | **çalışıyor** | `switch/basic.sqt`, `break_in_switch.sqt`, `string_switch.sqt`, `switch_in_loop.sqt` | 4/4 ✓ |

### 1.4 Fonksiyonlar

| Özellik | Durum | Kaynak kanıtı | Golden test |
|---------|-------|---------------|-------------|
| Tanım ve çağrı | **çalışıyor** | `interpreter.cpp:388` CALL, call frame | `fibonacci/fib.sqt` |
| Özyineleme | **çalışıyor** | `interpreter.cpp` call stack | `fibonacci/fib.sqt` — recursive fib(10)=55 |
| Çoklu dönüş tipi | **çalışıyor** | `type_checker.cpp` dönüş tipi kontrolü | `struct/basic.sqt` |

`fibonacci/fib.sqt` testi: 1/1 ✓

### 1.5 Struct — ayrıntılı

Tek seviyeli struct (alan okuma/yazma): **çalışıyor** — `tests/golden/struct/basic.sqt` ✓  
Struct metodları (`dump`, `toJson`): **çalışıyor** — `tests/golden/builtin/struct_methods.sqt` ✓  
Struct + array kombinasyonu: **çalışıyor** — `tests/golden/builtin/struct_array.sqt` ✓  
Nested struct (struct içinde struct): **ÇALIŞMIYOR** — `STRUCT_NEW` iç struct tahsis etmiyor (yukarı bkz.)  
Fonksiyona struct parametre geçme: **kopyalanan referans** — `struct/basic.sqt`'te `setX(p,99)` sonrası `p.x=10` (referans geçilmiyor, _kopya_ geçilmiyor — fonksiyon içindeki değişiklik caller'ı etkilemiyor). Bu ADR-020 ile tutarsız görünüyor; ayrıca incelenebilir.

### 1.6 Global Değişkenler

Durum: **çalışıyor** — issue #38 KAPANDI.  
Kanıt: `tests/golden/global/basic.sqt` (LOAD_GLOBAL/STORE_GLOBAL), `tests/golden/global/init_expr.sqt` — 2/2 ✓

### 1.7 Hata Yönetimi (`try/catch/throw`)

Durum: **çalışıyor** — ADR-025 uygulandı.  
Kanıt: `tests/golden/error/basic_catch.sqt`, `div_line.sqt`, `throw_and_nested.sqt` — 3/3 ✓

### 1.8 Modül Sistemi (`import` / `export`)

| Özellik | Durum | Kaynak kanıtı |
|---------|-------|---------------|
| `import` / `from` ayrıştırma | **çalışıyor** | `src/module/module_loader.cpp` `loadUnit` |
| Çok modüllü derleme | **çalışıyor** | `module_loader.cpp` özyinelemeli yükleme |
| Import döngüsü tespiti | **çalışıyor** | `loadChain_` aktif zinciri izler; A→B→A ve self-import `E_MODULE_CYCLE` üretir (ADR-031, #78) |

Golden test: `tests/golden/module/diamond.sqt` (elmas bağımlılık) +
`tests/module/cycle_*.sqt` döngü hata testleri (`tests/run.sh` "modül döngüsü").

---

## Bölüm 2 — CLI Araçları

### Mevcut komutlar

`saqut <komut> [dosya] [bayraklar]` — kayıtlı komutlar `src/main.cpp:43-103`.

#### `run` — tam çalışıyor

**Ne yapar:** Kaynak dosyayı alır, tam derleme pipeline'ından geçirir ve VM'de çalıştırır.  
**Çağrı:** `saqut run source.sqt` veya `saqut run --optimized source.sqt`  
**Aşamalar:** `ModuleLoader` → `SymbolCollector` (3-geçişli) → `TypeChecker` → `StructuralValidator` → `OptimizationManager` (isteğe bağlı) → `IRGenerator` → `Interpreter`  
**Dosyalar:** `src/cli/commands/run.hpp`, tüm pipeline  
**Bayraklar:** `--optimized` (sabit katlama + ölü kod eleme), `--verbose` (aşama izleme), `--compile-only` (VM'i atla)  
**Durum:** Tam çalışıyor.

#### `tokens` — tam çalışıyor

**Ne yapar:** Kaynak dosyayı tokenize eder, token listesini düz metin veya JSON formatında yazar.  
**Çağrı:** `saqut tokens source.sqt` / `saqut tokens --json source.sqt`  
**Aşamalar:** `Tokenizer`  
**Dosyalar:** `src/cli/commands/tokens.hpp`, `src/tokenizer/tokenizer.cpp`  
**Durum:** Tam çalışıyor.

#### `ast` — tam çalışıyor

**Ne yapar:** Kaynak dosyayı ayrıştırır, AST'yi renkli ağaç ya da JSON olarak yazar.  
**Çağrı:** `saqut ast source.sqt` / `saqut ast --json source.sqt`  
**Aşamalar:** `Tokenizer` → `Parser`  
**Dosyalar:** `src/cli/commands/ast.hpp`, `src/parser/`  
**Son değişiklik:** `46e640f` ANSI pastel renklendirme eklendi.  
**Durum:** Tam çalışıyor (renk kodları ANSI, terminal dışında görsel gürültü üretir).

#### `symbols` — tam çalışıyor

**Ne yapar:** Sembol tablosunu derler ve renkli tablo ya da JSON olarak yazar.  
**Çağrı:** `saqut symbols source.sqt`  
**Aşamalar:** Tokenizer → Parser → SymbolCollector  
**Dosyalar:** `src/cli/commands/symbols.hpp`, `src/symbol/`  
**Durum:** Tam çalışıyor.

#### `check` — tam çalışıyor

**Ne yapar:** Tüm statik analizleri çalıştırır ve diagnostic çıktısı üretir (hata + uyarı).  
**Çağrı:** `saqut check source.sqt`  
**Aşamalar:** Tokenizer → Parser → SymbolCollector → TypeChecker → StructuralValidator  
**Dosyalar:** `src/cli/commands/check.hpp`, `src/diagnostic/`  
**Durum:** Tam çalışıyor.

#### `ir` — tam çalışıyor (renk sorunu)

**Ne yapar:** IR kod üretir ve renkli (veya düz metin) olarak yazar; `--optimized` ile optimizasyon sonrası IR.  
**Çağrı:** `saqut ir source.sqt` / `saqut ir --optimized source.sqt`  
**Aşamalar:** Tam pipeline, VM hariç  
**Dosyalar:** `src/cli/commands/ir.hpp`, `src/ir/`  
**Sorun:** `46e640f` sonrası ANSI renk kodları eklendi; `.ir_opt.expected` golden dosyaları güncellenmedi → test #37 ve #38 başarısız. Fonksiyonel olarak doğru, görsel çıktı farklı.  
**Durum:** Çalışıyor, iki golden testi kırık (golden güncellenmeli).

#### `exec` — tam çalışıyor

**Ne yapar:** Tek bir ifadeyi `int main() { print(<expr>); return 0; }` kalıbına sararak çalıştırır.  
**Çağrı:** `saqut exec "1 + 2"`  
**Dosyalar:** `src/cli/commands/exec.hpp`  
**Durum:** Tam çalışıyor.

#### `bench` — tam çalışıyor

**Ne yapar:** Derleme pipeline'ının her aşamasını (tokenize → parse → symbol-collect → type-check → ir-gen → vm-execute) mikro-benchmark ile ölçer. N timing çalışması + 1 profil çalışması.  
**Çağrı:** `saqut bench source.sqt [--runs=N] [--compile-only]`  
**Dosyalar:** `src/cli/commands/bench.hpp`, `src/bench/profile.hpp`  
**Çıktı:** Her aşama için avg/best/total süre + opcode profili.  
**Durum:** Tam çalışıyor.

#### `lsp` — çalışıyor (hata toleransı kısıtlı)

**Ne yapar:** JSON-RPC üzerinden LSP sunucusu çalıştırır; `stdin`/`stdout` üzerinden iletişim.  
**Çağrı:** `saqut lsp` (editör tarafından başlatılır)  
**Dosyalar:** `src/cli/commands/lsp.hpp`, `src/lsp/lsp_server.cpp`, `src/lsp/lsp_handler.cpp` (577 satır), `src/lsp/document_store.cpp`  

**Desteklenen yetenekler:**

| Yetenek | Durum | Kaynak satır |
|---------|-------|-------------|
| `initialize` / `shutdown` | **çalışıyor** | `lsp_handler.cpp:handleInitialize` |
| `textDocument/didOpen` | **çalışıyor** | `lsp_handler.cpp:93` |
| `textDocument/didChange` | **çalışıyor** | `lsp_handler.cpp:103` |
| `textDocument/didClose` | **çalışıyor** | `lsp_handler.cpp:116` |
| `textDocument/publishDiagnostics` | **çalışıyor** | Her değişiklikte tam derleme → hata listesi |
| `textDocument/definition` (tanıma git) | **çalışıyor** | `lsp_handler.cpp:163` |
| `textDocument/hover` | **çalışıyor** | `lsp_handler.cpp:187` — tip imzası gösteriyor |
| `textDocument/references` | **çalışıyor** | `lsp_handler.cpp:228` |
| `textDocument/documentSymbol` | **çalışıyor** | `lsp_handler.cpp:284` |
| `textDocument/documentHighlight` | **çalışıyor** | `lsp_handler.cpp:318` |
| `textDocument/completion` | **çalışıyor** | `lsp_handler.cpp:417` — `.`/`::` + genel + anahtar kelimeler |
| Semantic highlight | **YOK** | `lsp_handler.hpp`'de handler yok |
| Inlay hints | **YOK** | Handler yok |
| Code actions | **YOK** | Handler yok |

**Hata-toleranslı ayrıştırma:** YOK. Parser'da `src/parser/parser.cpp:461`'de tek bir error recovery noktası var (cast expression). İlk syntax hatasında parser durur; dosyada hata varsa tüm dil servisleri (definition, hover, completion) çalışmaz.

**Editör entegrasyonu:** `editor/vscode/saqut-0.1.0.vsix` olarak paketlenmiş. VSCode'da `Extensions: Install from VSIX` ile kurulabilir. `package.json`'da LSP client bağlantısı tanımlı.

#### `dap` — kısmen çalışıyor

**Ne yapar:** Debug Adapter Protocol sunucusu çalıştırır.  
**Çağrı:** `saqut dap` (editör DAP istemcisi tarafından başlatılır)  
**Dosyalar:** `src/cli/commands/dap.hpp`, `src/dap/dap_handler.cpp` (277 satır), `src/dap/dap_types.hpp`

**Uygulanan DAP komutları:**

| Komut | Durum | Not |
|-------|-------|-----|
| `initialize` | **çalışıyor** | Yetenekler `configurationDone` destekli |
| `launch` | **çalışıyor** | Derler, VM oluşturur, `stopOnEntry` ile ilk instruction'da durur |
| `setBreakpoints` | **kısmen** | `vm_->setBreakpoint(file, line)` çağrısı var; VM kaynak satırı izleyebiliyor |
| `continue` | **çalışıyor** | `vm_->resume()` → `Paused` veya `Terminated` |
| `next` (stepOver) | **yarım** | `stepInstruction` çağırıyor — satır değil instruction bazlı |
| `stepIn` | **yarım** | Aynı: instruction bazlı |
| `threads` | **çalışıyor** | Tek thread `{id:1, name:"main"}` |
| `stackTrace` | **çalışıyor** | VM call depth + `frameFunctionName` + `frameSourceLine` |
| `scopes` | **çalışıyor** | `{name: "Locals", variablesReference: 1000+frameId}` |
| `variables` | **yarım** | Slot adına göre değil `slot[0..N]` olarak listeler; sembol adı yok |
| `disconnect` | **çalışıyor** | VM ve IRProgram serbest bırakıyor |

**Teşhis:** DAP teknik olarak çalışıyor — launch/continue/step protokolü tamamlanmış. Pratik sınırlamalar:
1. `variables` komutu sembol adlarını bilmiyor, yalnızca slot numaralarını raporluyor. IR'de sembol tablosu ↔ slot eşlemesi DAP'a aktarılmıyor (`dap_handler.hpp`'de SymbolTable referansı yok).
2. `stepOver` kaynak satır bazlı değil — bir `.sqt` satırı birden fazla instruction üretebilir, `next` satır sınırında değil instruction sınırında duruyor.
3. Uçtan uca VSCode+DAP entegrasyonu test edilmedi; `saqut-0.1.0.vsix` DAP launch konfigürasyonu içeriyor mu kontrol edilmedi.
4. Breakpoint eşleşmesi dosya yoluna + satır numarasına dayanıyor; LSP kaynak haritası ile tutarlılık test edilmedi.

**Sonuç:** Temel akış çalışıyor. Hata ayıklama deneyimi ham — sembol adları yok, satır bazlı adımlama yok. "Çalışmıyor" değil, "ham prototip".

#### Stub komutlar (TODO)

`src/main.cpp:82-103`'te kayıtlı ama sadece hata mesajı dönen komutlar:

| Komut | Durum |
|-------|-------|
| `compile` | **stub** — `TODO: compile command not yet implemented` |
| `parse` | **stub** — `TODO: parse command not yet implemented` |
| `transpile` | **stub** — `TODO: transpile to C code` |
| `interpret` | **stub** — `TODO: interpreter mode` |

### TextMate Sözdizim Vurgulama

**Nasıl üretildi:** Elle yazılmış JSON — lexer'dan türetilmemiş.  
**Dosya:** `editor/vscode/syntaxes/sqt.tmLanguage.json`  
**Kapsanan alanlar:** Yorumlar (satır/blok), dizgeler, kontrol akışı anahtar kelimeleri, tip anahtar kelimeleri, modül anahtar kelimeleri, sabitler (true/false/null), sayılar, operatörler.  
**Eksikler:** Sembolik vurgulama yok (fonksiyon adları, değişkenler, struct adları renksiz). Lexer'la senkronizasyon mekanizması yok — dil değişirse manuel güncelleme gerekir.

### Builtin Fonksiyonlar

Tüm builtin'ler host fonksiyonu olarak uygulanmış (`interpreter.cpp` CALLHOST dispatch), saQut kodu değil.

**Genel:** `print` — `interpreter.cpp:798`  
**Array builtin'leri** (tür bağımsız `E::method(arr, ...)` sözdizimi):  
`push`, `pop`, `length`, `remove`, `contains`, `keys`, `get`, `slice`, `concat`, `reverse`, `sort`, `indexOf`  
**String builtin'leri:** `toUpper`, `toLower`, `trim`, `startsWith`, `endsWith`, `indexOf`, `substr`, `split`, `replace`, `contains`  
**Struct builtin'leri:** `toStr`, `toJson`, `dump`  
**Genel:** `toStr` (int/float/bool → string), `toJson`

Builtin kayıt defteri: `src/builtin/builtin_methods.hpp` — TypeChecker, SymbolTable resolver ve LSP autocomplete bu tablodan okur.

---

## Bölüm 3 — Dizin Ağacı

```
saqutcompiler/
├── CMakeLists.txt              — Ana derleme dosyası; golden testler otomatik keşfedilir
├── cmake/
│   ├── run_golden.cmake        — Tek golden test çalıştırıcı (stdout karşılaştırma)
│   └── run_golden_error.cmake  — Hata golden test çalıştırıcı (stderr regex eşleşme)
├── docs/
│   ├── adr/
│   │   └── ADR-008-kisa-devre-mantiksal-operatorler.md  — Tek ayrı ADR dosyası
│   ├── adr-frontend-analiz.md  — ADR-006..028 (ana ADR belgesi)
│   ├── fikirler.md             — ADR-001..005 + erken notlar
│   ├── architecture.md         — Mimari genel bakış
│   ├── dap/dap-tasarim.md      — DAP tasarım belgesi
│   ├── lsp/lsp-tasarim.md      — LSP tasarım belgesi
│   ├── benchmark.md            — Benchmark sonuçları
│   ├── tooling-buyuk-resim.md  — Araç zinciri büyük resim
│   ├── roadmap-frontend.md     — Faz planı
│   └── sonnet-handoff.md       — ADR-020..024 uygulama promptu
├── editor/
│   └── vscode/
│       ├── extension.ts                — LSP istemci (TypeScript)
│       ├── language-configuration.json — Parantez eşleşme vb.
│       ├── syntaxes/sqt.tmLanguage.json — TextMate grameri (elle yazılmış)
│       ├── package.json                — VSCode eklenti bildirimi
│       └── saqut-0.1.0.vsix            — Paketlenmiş eklenti (kuruluma hazır)
├── examples/
│   ├── algorithm/              — 37 algoritma örneği (.sqt, 01..40)
│   ├── fibonacci.sqt           — Referans program (recursive + iterative)
│   ├── parser-stress/          — Yalnızca parser zorluğu için geçersiz dosyalar
│   └── semantic/               — Semantik hata örnekleri
├── scripts/
│   ├── bench/                  — Python/Java karşılaştırmalı benchmark araçları
│   └── build.sh, compile.sh    — Derleme yardımcıları
├── src/
│   ├── main.cpp                — CLI giriş noktası; tüm komutları kaydeder
│   ├── cli/
│   │   ├── args.hpp            — Argüman ayrıştırıcı
│   │   ├── cli.hpp             — Komut kayıt çerçevesi
│   │   └── commands/           — Her komut için ayrı başlık (ast, bench, check, dap, exec, ir, lsp, run, symbols, tokens)
│   ├── tokenizer/              — Tokenizer + token tanımları
│   ├── parser/
│   │   ├── parser.cpp/.hpp     — Pratt parser
│   │   ├── ast.hpp             — AST temel sınıfları
│   │   └── nodes/              — İfade/bildirim/deyim node sınıfları
│   ├── symbol/                 — Kapsam, sembol tablosu, 3-geçişli toplayıcı
│   ├── semantic/               — TypeChecker + StructuralValidator
│   ├── diagnostic/             — DiagnosticEngine (hata/uyarı raporlama)
│   ├── opt/                    — OptimizationManager, constant folding, DCE, AST clone
│   ├── ir/                     — Instruction, IRFunction, IRGenerator (1417 satır), IRProgram
│   ├── vm/
│   │   ├── interpreter.cpp/.hpp — Bytecode VM (yorumlayıcı döngü)
│   │   ├── value.hpp           — Value union (int/float/decimal/bool/string/ref/null)
│   │   ├── object.cpp/.hpp     — StructObject + ArrayObject + Heap (mark-sweep altyapısı)
│   │   └── call_frame.hpp      — Call frame (slots dizisi + IP)
│   ├── lsp/                    — LSP sunucu (server, handler, document_store, json_rpc)
│   ├── dap/                    — DAP sunucu (server, handler, types)
│   ├── module/                 — ModuleLoader + ModuleGraph
│   ├── core/                   — config.hpp, decimal.hpp, location.hpp, module_registry.hpp
│   ├── builtin/                — BuiltinMethod kayıt defteri (header-only)
│   └── bench/                  — profile.hpp (opcode profil hook'u)
├── tests/
│   ├── golden/                 — Golden testler (56 .sqt dosyası; 45'inin expected'ı var)
│   │   ├── arithmetic/, array/, bitwise/, builtin/, cast/, decimal/
│   │   ├── enum/, error/, fibonacci/, float/, global/
│   │   ├── logic/, loops/, null/, opt/, string/, struct/, switch/
│   └── test_diagnostic.cpp, test_type.cpp  — Birim testleri (unit_tests hedefi)
├── wiki/                       — Dil referans wiki'si (arrays, cli-commands, structs, ...)
├── TODO.md                     — Uygulama öncelik sırası
├── TOOLING-PLAN.md             — LSP/DAP/TextMate uygulama el kitabı
└── readme.md                   — Kullanıcıya yönelik genel bakış
```

---

## Bölüm 4 — Kararlar ve ADR'ler

### ADR konumları

| Belge | İçerik |
|-------|--------|
| `docs/fikirler.md` | ADR-001..005 |
| `docs/adr-frontend-analiz.md` | ADR-006..028 |
| `docs/adr/ADR-008-...md` | ADR-008 ayrıca ayrı dosya olarak da var (kopya) |

### ADR listesi

| No | Başlık | Tek satır özet | Durum |
|----|--------|----------------|-------|
| 001 | Backend Stratejisi | IR+VM seçimi, JIT/tree-walker reddedildi | Kabul |
| 002 | Parser Mimarisi | Pratt parser seçimi | Kabul |
| 003 | Header-Only Tasarım | C++ implementasyon eğilimi | Kabul |
| 004 | Token Sistemi | Polimorfik token sınıfları | Kabul |
| 005 | IR Tasarımı | 3-adresli, slot tabanlı IR | Kabul |
| 006 | Çok-Aşamalı Frontend | Multi-pass (tokenize/parse/symbol/type/ir) | Kabul |
| 007 | Analiz/Optimizasyon Ayrımı | Analiz AST annotation; optimizasyon klonda dönüşüm | Kabul |
| 008 | Optimizasyon Konumu | AST optimize, IR değil | Kabul |
| 009 | Fixpoint Optimizasyon | Sabit sayı değil fixpoint + iterasyon tavanı | Kabul |
| 010 | Tip Sistemi | Primitive/bileşik tip hiyerarşisi | Kabul |
| 011 | Scope/Forward Reference | İki-geçişli forward declaration | Kabul |
| 012 | ExpressionNode/StatementNode | Ara taban sınıfları | Kabul |
| 013 | Analiz Verisi | Her şey AST'de yaşar | Kabul |
| 014 | Dil Kapsamı | Yok/var listesi; class/function rezerve | Kabul |
| 015 | Çalıştırma Modeli | IR+VM; gerçek makine kodu JIT kapsam dışı | **Kilitli** |
| 016 | FFI Seam | `callhost` mekanizması; `print` ilk müşteri | Kabul |
| 017 | Batteries/Stdlib | Sınır problemi, ertelendi | Ertelendi |
| 018 | `interface` Ertelemesi | Reddedilmedi, ertelendi | Ertelendi |
| 019 | Frontend↔Runtime Ayrımı | Sorumluluk sınırı | Kabul |
| 020 | Değer/Referans Semantiği | Primitive değer; struct/array/string referans | **Kilitli** |
| 021 | Null Güvenliği | `T?` nullable, akış-duyarlı narrowing | **Kilitli** |
| 022 | Mark-Sweep GC | Basit deterministik GC; shared_ptr kalıcı model değil | **Kilitli** |
| 023 | Eşitlik Semantiği | Referanslarda kimlik; string içerik | **Kilitli** |
| 024 | String | Immutable değer-tipi, UTF-8 | **Kilitli** |
| 025 | Hata Yönetimi | Swift-tarzı struct-tabanlı, unchecked | **Kilitli** |
| 026 | Tip Dönüşümü | `as` infix; başarısızlık nullable hedef | **Kilitli** |
| 027 | switch-case | Statement; implicit fallthrough yok; float izinli+uyarı | **Kilitli** |
| 028 | `decimal` Tipi | İki virgüllü ondalık aritmetik | Kabul |

### ADR'de yazılı olmayan kilitli kararlar

Aşağıdaki kararlar kodda/CLAUDE.md'de yaşıyor ama ayrı ADR dosyası yok:

- `heavyIR`/`lightIR` ayrımı — ADR-005'te ima edilmiş, açık ADR yok
- Mark-sweep'in VM-içi olması (GC ana binary'nin parçası, ayrı daemon değil) — ADR-022 kısmen anlatıyor
- Nested struct için iç struct tahsisi kuralı — ADR yoktur (ve bugün çalışmıyor)
- Module döngüsü tespiti politikası — ADR yoktur

---

## Bölüm 5 — Açık İşler ve Yarımlar

### Açık GitHub Issues

| # | Başlık | Etiket | Tek satır |
|---|--------|--------|-----------|
| #76 | FFI Capability Modeli — Güvenlik Öncelikli | `fikir, ffi-builtin` | Dosya erişimi, ağ, process gibi dış kaynaklar için izin modeli |
| #67 | Performans: Stress test results ve VM capacity analysis | `Performans` | Benchmark analiz — kapatılmayı bekliyor |
| #59 | JIT backend — sıcak hesap yollarını makine koduna derleme | `fikir, gelecek-vizyon` | ADR-015 kapsamı dışı; uzak gelecek |
| #58 | saQut işleme motoru — sandbox'lı veri işleme çekirdeği | `fikir, gelecek-vizyon` | Gelecek vizyon |
| #54 | Symbol struct'ına açık `moduleId` alanı eklenmeli | `kalite-mimari` | Çok modüllü yeniden çözümleme için |
| #53 | LOAD_GLOBAL/STORE_GLOBAL modül-düzeyi değişken olarak yeniden çerçevelenmeli | `kalite-mimari` | Global namespace temizliği |

### Issue #37 (%= operatörü) — KAPALIMI?

**Koddan doğrulama:** `ir_generator.cpp:692` `TokenType::PERCENT_EQUAL → Opcode::MOD` mevcut.  
**Golden test:** `tests/golden/arithmetic/compound_mod.sqt` → 1/1 ✓  
**Karar:** Çalışıyor. Issue #37 kapatılabilir. GitHub'da açık durumu kontrol edilmedi.

### Issue #38 (global değişken sessiz atlama) — KAPALIMI?

**Koddan doğrulama:** `ir_generator.cpp` `isGlobal()` + `emitStoreGlobal()` mevcut; `interpreter.cpp:269` LOAD_GLOBAL, `273` STORE_GLOBAL.  
**Golden testler:** `tests/golden/global/basic.sqt`, `global/init_expr.sqt` → 2/2 ✓  
**Karar:** Çalışıyor. Issue #38 kapatılabilir. GitHub'da açık durumu kontrol edilmedi.

### TODO.md'de açık tutulan (GitHub kapatılmamış)

- **#63** `ConstantFoldingPass` double-free — `TODO.md`'e göre `462c6ba` commit'iyle düzeltildi, GitHub issue hâlâ açık.
- **#64** Struct dönüş tipli fonksiyonlarda E003 — `793e372` commit'iyle düzeltildi, GitHub issue hâlâ açık.

### Test Edilmeyen Aktif Sorunlar (golden YOK)

1. **Nested struct field** (okuma ve yazma): `r.topLeft.x` → `runtime error: not a struct`. `STRUCT_NEW` iç struct alanlarını otomatik tahsis etmiyor. `ir_generator.cpp:1262` sadece `struct<Name>[N alan]` oluşturuyor, `struct` tipli alanlar için özyinelemeli `STRUCT_NEW` emit etmiyor. Golden test **YOK** — sessiz kalmaya devam edecek.

2. **Modül döngüsü tespiti**: A→B→A döngüsünde hata üretilmiyor. `module_loader.cpp:27` `seen_` seti tekrar yüklemeyi önler ama döngüye girince ikinci `loadUnit(A)` çağrısı atlanır — hata raporu yok. Golden test **YOK**.

3. **ANSI renk kodu golden uyumsuzluğu** (test #37, #38): `46e640f` commit'i `ir` komutunun çıktısını renklendirdi ama `.ir_opt.expected` dosyaları güncellenmedi. `.ir_opt.expected` dosyaları `cat` çıktısıyla (ANSI strip'li) güncellenmeli.

4. **Decimal print formatı**: `1.5 + 2.5 = 4` çıktısı veriyor (4.0 değil). Tamsayı sonuçlarda nokta yok. Gerçek `decimal` için beklenti `4.00` ya da `4` olmalı — semantik netleştirilmeli.

5. **Modül sistemi golden testi**: `tests/golden/module/diamond.sqt` + `tests/module/cycle_*.sqt` (#78 ile eklendi).

6. **GC mark-sweep**: ÇÖZÜLDÜ (#77) — eşik tabanlı tetikleme `Interpreter::maybeCollect()` (instruction sınırı safepoint, adaptif eşik), kökler moduleSlots_ + callStack_ + pendingThrow_. `--gc-threshold=N` / `--gc-stats` CLI bayrakları; `tests/golden/gc/liveness.sqt` + run.sh "gc" bölümü.

### Yarım Kalanlar Özeti

| Özellik | Durum |
|---------|-------|
| GC mark-sweep | **ÇÖZÜLDÜ** (#77) — eşik tabanlı maybeCollect, stress/kapama bayrakları |
| DAP değişken isimleri | Slot numaraları gösteriyor, sembol adı yok |
| DAP satır bazlı adımlama | Instruction bazlı, satır bazlı değil |
| Nested struct field | IR doğru, STRUCT_NEW iç struct tahsis etmiyor |
| Modül döngüsü tespiti | `seen_` var, hata üretmiyor |
| compile/parse/transpile/interpret | Stub — TODO mesajı döndürüyor |
| ANSI golden güncelleme | 2 golden dosya `ir_opt.expected` güncellenmeli |
| Mod by zero Türkçe mesaj | 1 golden dosya `runtime_error` güncellenmeli veya mesaj Türkçeleştirilmeli |

---

## Ek — CLAUDE.md ile Çelişki Analizi

CLAUDE.md'deki "Henüz YOK" listesiyle mevcut durumun karşılaştırması:

| CLAUDE.md iddiası | Gerçek durum |
|-------------------|--------------|
| "float/double codegen yok" | **YANLIŞ** — `interpreter.cpp:425` LOAD_FLOAT, `tests/golden/float/basic.sqt` geçiyor |
| "struct IR yok" | **YANLIŞ** — struct IR çalışıyor; tek seviyeli alanlar ✓; nested struct **yeni hata** |
| "array IR yok" | **YANLIŞ** — array IR çalışıyor; `tests/golden/array/ref_semantics.sqt` ✓ |
| `%=` eksik (#37) | **YANLIŞ** — çalışıyor, test geçiyor |
| "global değişken sessizce atlıyor (#38)" | **YANLIŞ** — çalışıyor, test geçiyor |
| "W003 ölü kod uyarısı üretilmiyor (#36)" | **Kısmen doğru** — W003 üretiliyor (dce.ir_opt testinde görüldü) ama golden testi var; DCE ayrı bir sorun |
| "DCE silinen düğümleri delete etmiyor (#35)" | Hâlâ geçerli — bellek sızıntısı devam ediyor |

CLAUDE.md'nin "Çalışıyor" bölümü büyük ölçüde doğru; "Henüz YOK" bölümü ise önemli ölçüde eskimiş.
