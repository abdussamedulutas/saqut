# TODO — Teknik Borçlar ve Ertelenen Kararlar

Bu dosya, kasıtlı olarak ertelenen teknik kararları ve ileride ele alınması gereken
tasarım noktalarını tutar. Her giriş hangi issue'ya bağlı olduğunu belirtir.

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
