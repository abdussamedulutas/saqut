# TODO — saQut Uygulama Sırası

Issue #50'deki mimari haritaya ve mevcut pipeline durumuna göre kronolojik
tavsiye listesi. Tamamlananlar arşiv olarak altta tutulur.

---

## 🏁 SIRALANMIŞ İŞ LİSTESİ

### 1 · Issue Kapatma (Hemen — commit zaten var)

- [ ] **#63** `ConstantFoldingPass` double-free — commit `462c6ba` ile düzeltildi,
      GitHub issue'su hâlâ açık; kapat.
- [ ] **#64** struct dönüş tipli fonksiyonlarda E003 — commit `793e372` ile düzeltildi,
      GitHub issue'su hâlâ açık; kapat.

---

### 2 · GC v2 — Mark-Sweep (#56)  ⚙️ IR / 🖥️ VM

Nesne modeli GC-hazır (header: tip+mark biti+liste). Eksik: traversal + trigger.

- [ ] `Heap::collect()` uygula: köklerden (VM frame slot'ları + globalSlots_) başlayan
      işaret-tarama, erişilemeyen nesneleri serbest bırak.
- [ ] Trigger: "her N tahsiste" `collect()` çağır (N = ayarlanabilir sabit, başlangıç 1024).
- [ ] `ArrayObject` ve `StructObject` çocuk referanslarını doğru sayma (`markChildren()`).
- [ ] Golden test: döngüsel referans oluşturan program bellek sızdırmıyor.

---

### ✅ 3 · IR Satır Tablosu / Stacktrace (2026-06-21)

- `emitBinaryOp(op, dest, left, right, line, col)` — `sourceLine`/`sourceCol` eklendi.
- `generateBinaryArithmetic` + tüm çağrı noktaları `bin->loc` iletiyor.
- `makeErrorValue` çağrılarına `instr.sourceLine, instr.sourceCol` geçildi
  (DIV, MOD, FDIV, ARRAY_GET, ARRAY_SET, CAST_* hataları).
- `Error.line` doğru kaynak satırını gösteriyor; `Error.trace` fonksiyon zincirini içeriyor.
- Golden test: `tests/golden/error/div_line.sqt` ✓

---

### ✅ 4 · Bitwise BXOR (^) ve ^= (2026-06-21)

BAND/BOR/SHL/SHR/BNOT zaten vardı; BXOR ve `^=` eksikti.

- `instruction.hpp` — `BXOR` opcode eklendi.
- `ir_function.cpp` — `"^"` sembolü ve `isBinaryOp` case eklendi.
- `ir_generator.cpp` — `CARET → BXOR`, `CARET_EQUAL → BXOR`.
- `interpreter.cpp` — `BXOR` VM case eklendi.
- `tests/golden/bitwise/basic.sqt` — `a ^ b` (12^10=6) ✓
- `tests/golden/bitwise/compound.sqt` — `x ^= 3` ✓

---

### 5 · `print` Çoklu Argüman (#46)  🔌 FFI

- [ ] Tip denetleyicide `print(a, b, c)` çoklu argümanı kabul et.
- [ ] IR'da çoklu `CALL_HOST PRINT` veya yeni `PRINT_N` opcode.
- [ ] VM: argümanları boşlukla/yeni satırla yazdır.

---

### 6 · FFI Seam + Builtin Kataloğu (#10, #11)  🔌 FFI

Stdlib zincirinin köküdür: `#10 → #11 → #12`.

- [ ] `callhost` imzasını resmileştir: `callhost <name> <argCount>` IR opcode'u,
      host fonksiyon tablosu (`std::unordered_map<std::string, HostFn>`).
- [ ] Builtin katalog: `len()`, `toString()`, `parseInt()`, `parseFloat()`, `type()`.
- [ ] Tip denetleyicide builtin imzaları kayıt altına al.

---

### 7 · Minimal Stdlib (#12)  🔌 FFI

Bekler: #10, #11.

- [ ] **String:** `len(s)`, `substring(s,i,j)`, `indexOf(s,sub)`, `toUpper(s)`, `toLower(s)`.
- [ ] **Math:** `abs(x)`, `min(a,b)`, `max(a,b)`, `sqrt(x)`, `floor(x)`, `ceil(x)`.
- [ ] **Array:** `len(arr)`, `push(arr, v)`, `pop(arr)`, `slice(arr,i,j)`.
- [ ] Her builtin için golden test.

---

### ✅ 8 · Enum (#8)  🔤 LEX / 🌳 PARSE / 🔎 SEM / ⚙️ IR / 🖥️ VM

- `EnumDeclNode` + `ASTKind::EnumDecl` + parser (`enum Color { Red = 0, Green, Blue }`)
- `TypeKind::Enum` + `Type::enumType()`, `isEnum()`
- `SymbolKind::Enum/EnumValue` + `table_.enumLayouts`
- `typeFromName` enum tiplerini tanıyor
- Tip denetleyici: `Color.Red` üye erişimi, switch subject enum, `==` karşılaştırma
- IR: `MemberAccess` → `LOAD_INT` (enum member int değeri)
- `tests/golden/enum/basic.sqt` ✓ / `tests/golden/enum/explicit_values.sqt` ✓

---

### 9 · Modül Sistemi (#3, #4, #5)  📦 DRV

Büyük alt sistem; zincir sert: `#3 → #4 → #5`.

**#3 Import sözdizimi:**
- [ ] `import "path/to/file"` veya `import moduleName from "path"` lexer/parser.
- [ ] Modül çözümleme: dosya yolu → `IRProgram` önbelleği.

**#4 Görünürlük:**
- [ ] `pub` anahtar kelimesi veya ön ek olmadan varsayılan private.
- [ ] Sembol tablosunda `visibility` alanı.
- [ ] Cross-module sembol erişiminde görünürlük denetimi.

**#5 Çoklu dosya derleme:**
- [ ] Derleyici sürücüsü: bağımlılık sırasını çözümle (DAG).
- [ ] `IRProgram`'ları birleştir, CALL cross-modül olduğunda `moduleId` ara.

**Modül sistemiyle birlikte (#53, #54):**
- [ ] `IRFunction.moduleId` alanı ekle (#53).
- [ ] `Interpreter.globalSlots_` → `moduleSlots_[moduleId]` (#53).
- [ ] `Symbol.sourceModule` alanı ekle (#54).

---

### 10 · Tooling — Bağımsız Paralel Çalışabilir  🧰 TOOL

Aşağıdakiler çekirdeğe bağlı değil; istediğinde sırayı değiştir.

- [ ] **#24** CLI'da AST görüntüleme: `saqut ast --format=tree`, `--format=dot`.
- [ ] **#14** Syntax highlighting grameri (TextMate / tree-sitter) — keyword listesi yeter.
- [ ] **#15** `saqut fmt` — parse + pretty-printer.
- [ ] **#20** Akıllı diagnostic ("neden / nasıl düzelt" açıklamaları).
- [ ] **#18** Dil-içi test bloğu `test { }` + `saqut test` komutu.
- [ ] **#13** LSP sunucusu (Tier 1 → Tier 4 kademeli).

---

### 11 · Gelecek Vizyon (Uzak)  📐 META

Çekirdeğin oturmasını bekle.

- [ ] **#6** Native decimal tipi.
- [ ] **#7** Native date/time tipi.
- [ ] **#19** Paket yöneticisi / registry (bekler: #5).
- [ ] **#16** Zaman-yolculuğu hata ayıklama (deterministik VM avantajı).
- [ ] **#17** WASM playground.
- [ ] **#59** JIT backend (libgccjit / LLVM — çok uzak).
- [ ] **#2** CFG/SSA gerekli mi? (ileri optimizasyon için karar).

---

## ✅ TAMAMLANDI (2026-06-20)

### GC-hazır nesne modeli + array runtime
ADR-020…024 doğrultusunda:
- `src/vm/object.hpp` — Object, ArrayObject, Heap (v1: toplama yok, GC-hazır header)
- `src/vm/value.hpp` — ValueKind::Ref + Null
- `src/ir/instruction.hpp` — ARRAY_NEW/GET/SET/LEN
- Array literal `[1,2,3]`, `int[]` tip sözdizimi
- Referans semantiği, kimlik `==` (ADR-023), sınır kontrolü
- `tests/golden/array/ref_semantics.sqt` ✓

### Struct runtime + E010 revizyonu
- `src/vm/object.hpp` — StructObject : Object
- E010 revizyonu: `Node next` artık meşru (döngü kurulabilir, GC v2 ile toplanacak)

### float/double aritmetik runtime (#44)
- `src/vm/value.hpp` — ValueKind::Float
- IR opcode'ları: FADD/FSUB/FMUL/FDIV + float karşılaştırma

### String cilası (ADR-024)
- `src/ir/instruction.hpp` — STRING_CONCAT opcode
- `src/ir/ir_generator.cpp` — `+`/`+=` string için STRING_CONCAT
- `src/vm/interpreter.cpp` — STRING_CONCAT çalışma zamanı
- İçerik `==` / `!=` (ADR-023/024)
- `tests/golden/string/concat.sqt` ✓

### Hata yönetimi — try/catch/throw (ADR-025, #57 ana kısım)
- `TryStatement`, `ThrowStatement` AST + parser
- `ENTER_TRY`, `LEAVE_TRY`, `THROW` IR opcode'ları
- `TryFrame`, `pendingThrow_`, `makeErrorValue()` VM
- Runtime hataları (DIV/0, OOB) yakalanabilir Error nesnesi olarak
- `Error` builtin struct (line, col, message, trace, code)
- `tests/golden/error/basic_catch.sqt` ✓
- `tests/golden/error/throw_and_nested.sqt` ✓
- ⚠️ **Trace alanı boş** (satır tablosu → Madde 3)

### Null akış-analizi (ADR-021)
- `Type.nullable`, `asNullable()`, `asNonNull()`
- `?` suffix parser (değişken, parametre, dönüş tipi)
- `LOAD_NULL` opcode
- `narrowedNonNull_` — nested + guard + `&&` narrowing
- `tests/golden/null/` — 4 senaryo ✓

### switch-case (ADR-027)
- `SwitchStatementNode`, `CaseClause`
- Çok-değerli `case 1,2,3:`, `default:` opsiyonel
- Subject tip homojenlik, float W005 uyarısı, `case null:` guard
- IR: EQUAL_EQUAL + JIF_TRUE/JIF_FALSE + JMP backpatch
- `tests/golden/switch/` — 4 senaryo ✓

### Tip dönüşümü `as` (ADR-026, #42)
- `KW_AS` keyword + precedence 12
- `CastExpressionNode` (operand + targetTypeName + targetNullable)
- Cast matrisi (bool↔int yasak, struct/array yasak)
- Fallible cast: `as int` → Error fırlatır; `as int?` → null
- Yeni opcode'lar: CAST_INT_TO_STR, CAST_FLOAT_TO_STR, CAST_BOOL_TO_STR,
  CAST_STR_TO_INT, CAST_STR_TO_FLOAT, CAST_FLOAT_TO_INT_CHECKED
- `tests/golden/cast/` — 3 senaryo ✓

---

## 📋 MİMARİ BORÇ NOTU (#53, #54)

`LOAD_GLOBAL`/`STORE_GLOBAL` ve `Symbol.sourceModule` değişiklikleri **modül sistemi
gelene kadar ertelendi** (Madde 9). Detay: dosyanın eski versiyonundaki `#modül-scope`
ve `#sembol-modül` bölümleri `docs/plan-53-54-57.md` dosyasına taşındı.

---

> ⚠️ **Terminoloji kilidi:** Dil anahtar sözcüğü **`null`** (`nil` değil), array
> literal **`[...]`** (`{...}` değil), hata tipi **`Error`**. İsim belirsizse
> **icat etme, Opus'a sor.** Ayrıntı: `docs/sonnet-handoff.md` Bölüm 6.
