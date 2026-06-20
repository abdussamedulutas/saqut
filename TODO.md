# TODO — Teknik Borçlar ve Ertelenen Kararlar

Bu dosya, kasıtlı olarak ertelenen teknik kararları ve ileride ele alınması gereken
tasarım noktalarını tutar. Her giriş hangi issue'ya bağlı olduğunu belirtir.

---

## ✅ TAMAMLANDI — GC-hazır nesne modeli + array runtime (2026-06-20)

ADR-020…024 (değer/referans semantiği, null güvenliği, mark-sweep GC, eşitlik)
doğrultusunda uçtan uca array çalışıyor:
- `src/vm/object.hpp` — Object, ArrayObject, Heap (v1: toplama yok, GC-hazır header)
- `src/vm/value.hpp` — ValueKind::Ref + Nil eklendi
- `src/ir/instruction.hpp` — ARRAY_NEW/GET/SET/LEN eklendi
- Array literal parser (`[1,2,3]`), `int[]` tip sözdizimi (Java/C# stili)
- Referans semantiği, kimlik `==` (ADR-023), sınır kontrolü
- Golden test: `tests/golden/array/ref_semantics.sqt` ✓

## 🚀 SIRADAKİ İŞ (docs/sonnet-handoff.md Bölüm 2)

1. **Struct runtime** — StructObject : Object, alan erişimi, E010 revizyonu
2. **String cilası (ADR-024)** — içerik `==`, UTF-8 concat+print
3. **Null akış-analizi (ADR-021)** — `Type?`, flow-narrowing, `a!`
4. **float/double (#44)** — Value::Float + FADD/FSUB/… opcodes
5. **mark-sweep GC v2 (#56)** — header+kök kancası üstünde aç

Açık mimari borç: **#56** (döngüsel referans sızıntısı → mark-sweep GC v2).

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
