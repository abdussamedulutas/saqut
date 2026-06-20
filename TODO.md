# TODO — Teknik Borçlar ve Ertelenen Kararlar

Bu dosya, kasıtlı olarak ertelenen teknik kararları ve ileride ele alınması gereken
tasarım noktalarını tutar. Her giriş hangi issue'ya bağlı olduğunu belirtir.

---

## ✅ TAMAMLANDI — GC-hazır nesne modeli + array runtime (2026-06-20)

ADR-020…024 (değer/referans semantiği, null güvenliği, mark-sweep GC, eşitlik)
doğrultusunda uçtan uca array çalışıyor:
- `src/vm/object.hpp` — Object, ArrayObject, Heap (v1: toplama yok, GC-hazır header)
- `src/vm/value.hpp` — ValueKind::Ref + Null eklendi (dil anahtar sözcüğü `null`, `nil` DEĞİL)
- `src/ir/instruction.hpp` — ARRAY_NEW/GET/SET/LEN eklendi
- Array literal parser (`[1,2,3]`), `int[]` tip sözdizimi (Java/C# stili)
- Referans semantiği, kimlik `==` (ADR-023), sınır kontrolü
- Golden test: `tests/golden/array/ref_semantics.sqt` ✓

## ✅ TAMAMLANDI — Struct runtime + E010 revizyonu (2026-06-20)

ADR-020 doğrultusunda struct referans semantiğiyle uçtan uca çalışıyor:
- `src/vm/object.hpp` — StructObject : Object, alan erişimi (GC-hazır header)
- `src/vm/value.hpp` — ValueKind::Null eklendi (dil anahtar sözcüğü `null`)
- E010 revizyonu: referansla tutulan `struct Node { Node next; }` artık meşru
  (döngü kurulabilir, GC v2 ile toplanacak)

## ✅ TAMAMLANDI — float/double aritmetik runtime (#44) (2026-06-20)

- `src/vm/value.hpp` — ValueKind::Float + floatValue alanı
- IR opcode'ları: FADD/FSUB/FMUL/FDIV + float karşılaştırma
- Literal bağlama-göre tipleme korundu (sabit folding dahil)

## ✅ TAMAMLANDI — String cilası (ADR-024) (2026-06-20)

- `src/core/type.hpp` — `isString()` yüklem yardımcısı eklendi
- `src/ir/instruction.hpp` — `STRING_CONCAT` opcode eklendi
- `src/semantic/type_checker.cpp` — `+` string için izin (her iki taraf `string` → sonuç `string`)
- `src/ir/ir_generator.cpp` — `generateBinaryArithmetic` string tespiti: `ADD` → `STRING_CONCAT`; `+=` için de `STRING_CONCAT`
- `src/vm/interpreter.cpp` — `STRING_CONCAT` case: `stringValue + stringValue`
- `tests/golden/string/concat.sqt` — concat, `+=`, içerik `==` golden testi
- İçerik `==` / `!=` VM'de zaten vardı (ADR-023/024); `print(string)` zaten çalışıyordu

## ✅ TAMAMLANDI — Hata yönetimi (ADR-025, #57) (2026-06-20)

- `src/parser/ast_node.hpp` — `TryStatement`, `ThrowStatement` ASTKind eklendi
- `src/parser/nodes/statements.hpp/.cpp` — `TryStatementNode`, `ThrowStatementNode`
- `src/parser/parser_base.hpp` + `parser.cpp` — `parseTryStatement()`, `parseThrowStatement()`
- `src/ir/instruction.hpp` — `ENTER_TRY`, `LEAVE_TRY`, `THROW` opcode'ları
- `src/ir/ir_generator.cpp` — try/catch/throw IR üretimi
- `src/vm/interpreter.hpp` — `TryFrame` struct, `tryStack_`, `pendingThrow_`, `makeErrorValue()`
- `src/vm/interpreter.cpp` — ENTER_TRY/LEAVE_TRY/THROW VM uygulaması; runtime hataları
  (DIV/0, array OOB) `pendingThrow_` üzerinden Error nesnesi olarak yakalanabilir hale getirildi
- `src/symbol/symbol_collector.cpp` — `Error` builtin struct kaydı (alan sırası: line,col,message,trace,code)
- `src/semantic/type_checker.cpp` — TryStatement/ThrowStatement tip kontrolü (unchecked)
- `tests/golden/error/basic_catch.sqt` — sıfıra bölme yakalama
- `tests/golden/error/throw_and_nested.sqt` — iç içe fonksiyon unwind + array OOB + throw
- **Not:** IR satır tablosu (stacktrace doldurma) ertelendi — trace alanı boş kalıyor

## ✅ TAMAMLANDI — Null akış-analizi (ADR-021) (2026-06-20)

- `src/core/type.hpp` — `bool nullable` alanı; `asNullable()`, `asNonNull()`, `equalsBase()`, `isNullLiteral()` yardımcıları; `fromName("int?")` desteği; `toString()` → `int?`
- `src/parser/parser.cpp` — `?` suffix ayrıştırma (değişken, parametre, dönüş tipi); dispatch'te `int? f()` → `parseFunctionDecl()` yönlendirmesi düzeltildi
- `src/ir/instruction.hpp` — `LOAD_NULL` opcode eklendi
- `src/ir/ir_generator.cpp` — `LiteralType::BOŞ` → `LOAD_NULL`
- `src/vm/interpreter.cpp` — `LOAD_NULL` case → `Value::null()`
- `src/semantic/type_checker.hpp` — `narrowedNonNull_` set; `extractNullCheck()`, `alwaysExits()` yardımcıları
- `src/semantic/type_checker.cpp`:
  - `checkAssign`: `T? ← null` OK; `T ← T?` hata (E003); `T? ← T` OK (widening)
  - `checkExpr Literal BOŞ`: null sentinel tipi (`Void+nullable`)
  - `checkExpr Identifier`: `narrowedNonNull_`'dan non-null olduğu bilinenlerde nullable flag'i kaldırılıyor
  - `checkExpr BinaryExpression`: `&&` sağ taraf narrowing; nullable operand hatası (E003)
  - `checkExpr MemberAccess`: nullable nesne üstünde doğrudan erişim hatası
  - `checkStmt Block`: guard pattern (`if (a == null) return;` → sonrasında `a` non-null)
  - `checkStmt IfStatement`: nested narrowing (`if (a != null)` → then'de non-null, `if (a == null)` → else'de non-null)
- `tests/golden/null/narrowing.sqt` — nested + guard narrowing, `int?` dönüş tipi ✓
- `tests/golden/null/nullable_assign_error.sqt` — `T? → T` atama E003 ✓
- `tests/golden/null/nullable_operand_error.sqt` — nullable aritmetik operand E003 ✓
- `tests/golden/null/and_narrowing.sqt` — `&&` sağ taraf narrowing ✓

## 🚀 SIRADAKİ İŞ

1. **mark-sweep GC v2 (#56)** — en son; özellik bloklamaz, en karmaşık. Trigger basit
   "her N tahsiste" yeter; nesne modeli zaten GC-hazır.

Açık mimari borçlar: **#56** (döngüsel referans → mark-sweep GC v2), **#57** (hata
modeli görünürlük alt-ekseni).

> ⚠️ **Terminoloji kilidi (Sonnet):** anlaşılan isimleri değiştirme/icat etme.
> Dil anahtar sözcüğü **`null`** (`nil` değil), array literal **`[...]`** (`{...}` değil),
> hata tipi **`Error`**. İsmi belirsizse **icat etme, Opus'a sor.** Ayrıntı:
> `docs/sonnet-handoff.md` Bölüm 6 (terminoloji kilidi).

---

## #modül-scope — IRFunction.moduleId: Modül-düzeyi değişken izolasyonu

**Etkilenen dosyalar:**
- `src/ir/instruction.hpp` — `LOAD_GLOBAL` / `STORE_GLOBAL` yorumları
- `src/ir/ir_program.hpp` — `globalCount`, `globalNames`
- `src/vm/interpreter.hpp` — `globalSlots_`
- `src/vm/interpreter.cpp` — `LOAD_GLOBAL` / `STORE_GLOBAL` case'leri

**Mevcut durum (tek dosya):**
`LOAD_GLOBAL dest, N` → `globalSlots_[N]` — düz vektör, tek modül varsayımı.
Tek dosyada bu doğru çalışır; `globalSlots_[0]` her zaman bu dosyanın 0. modül-değişkenidir.

**Çok modüllü derlemede yapılması gereken:**
Her `IRFunction`'a `std::string moduleId` (veya `int moduleIndex`) alanı ekle.
`Interpreter`'da `globalSlots_` yerine `std::unordered_map<std::string, std::vector<Value>> moduleSlots_` tut.
`LOAD_GLOBAL` çalışırken `frame.function->moduleId` ile doğru modülün slot alanına bak.

```
// Hedef tasarım (modül sistemi gelince):
case Opcode::LOAD_GLOBAL:
    frame.slots[instr.dest] = moduleSlots_[frame.function->moduleId][instr.intValue];
    break;
```

**Ne zaman:** Modül sistemi issue'ları (#3 import sözdizimi, #4 görünürlük, #5 çoklu dosya)
ele alınırken bu değişikliği de kapsama al.

---

## #sembol-modül — Symbol.sourceModule: Sembol tablosunda açık modül kimliği

**Etkilenen dosyalar:**
- `src/symbol/symbol.hpp` — `Symbol` struct

**Mevcut durum:**
`Symbol.definitionLoc.filePath` dosya yolunu tutuyor — modül bilgisi dolaylı olarak var.
Ama doğrudan `moduleId` / `moduleName` alanı yok; filtre/lookup için her seferinde
`definitionLoc.filePath`'i ayrıştırmak gerekir.

**Yapılması gereken:**
`Symbol` struct'ına `std::string sourceModule` alanı ekle (modül adı veya dosya yolu).
Özellikle cross-module sembol çözümleme, LSP "tanıma git" ve hata mesajlarında
"hangi modülden geldi" bilgisi için kritik.

**Ne zaman:** `#3` (import sözdizimi) veya `#4` (görünürlük) issue'larında.
