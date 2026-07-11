# Sanal Makine — VM (`src/vm/`)

## Sorumluluk

IRProgram içindeki bytecode instruction'larını yorumlayarak saQut programlarını
çalıştırır. Stack-based olmayıp slot-tabanlı (register benzeri) bir yorumlayıcıdır.
ADR-020 (referans semantiği), ADR-021 (null), ADR-024 (string), ADR-025 (hata
yönetimi), ADR-026 (cast), ADR-028 (decimal) kurallarını runtime'da uygular.
DAP protokolü için breakpoint, adım adım çalıştırma ve bütçeli çalıştırma API'leri
sunar.

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `interpreter.hpp` | `Interpreter` sınıf bildirimi + `TryFrame`, `RunState`, `RunReason` enum'ları. |
| `interpreter.cpp` | Interpreter gerçeklemesi (~1303 satır) — tüm Opcode'ların işlenmesi, DAP API'leri, built-in metod dispatch. |
| `call_frame.hpp` | `CallFrame` struct'ı — fonksiyon çağrı kaydı (slots, IP, function pointer, kaynak dosya/satır). |
| `value.hpp` | `Value` yapısı + `ValueKind` enum — çalışma zamanı değerleri (int, float, bool, string, ref, decimal, null). |
| `object.hpp` | `Heap` sınıfı (GC), `GCList`, `ArrayObject`, `StructObject` — heap'te yaşayan nesneler. |

## Ana tipler ve ilişkileri

```
Value
  ├─ kind : ValueKind (Int | Float | Bool | String | Null | Ref | Decimal)
  ├─ intVal / floatVal / boolVal / strVal / decimalVal
  └─ refVal : size_t (Heap index)

CallFrame
  ├─ function    : IRFunction*
  ├─ slots       : vector<Value> — yerel değişkenler
  ├─ ip          : int — instruction pointer
  ├─ sourceLine  : int — geçerli kaynak satırı
  ├─ sourceFile  : string
  ├─ firstLine   : int — fonksiyonun ilk satırı
  └─ slotName(idx) → string

Heap
  ├─ objects_    : vector<GCList> — GC izleme listeleri
  ├─ allocCount_ : int
  ├─ alloc(obj)  → size_t (ref index)
  ├─ get<T>(ref) → T*
  ├─ collect()   — mark-sweep (şu an arena gibi, tetiklenmez)
  └─ markSweepCollect() — iskelet hazır (#77)

TryFrame (Interpreter iç struct)
  ├─ callStackDepth : int (unwind sınırı)
  ├─ catchTarget    : int (catch bloğu IP'si)
  └─ errorSlot      : int (catch değişken slotu)

Interpreter
  ├─ program_       : IRProgram&
  ├─ callStack_     : vector<CallFrame>
  ├─ heap_          : Heap
  ├─ moduleSlots_   : unordered_map<int, vector<Value>> — globaller
  ├─ tryStack_      : vector<TryFrame>
  ├─ pendingThrow_  : optional<Value>
  ├─ breakpoints_   : set<pair<string,int>>
  ├─ state_         : RunState (Running | Paused | Finished)
  │
  ├─ run() → int — VM'i çalıştır
  ├─ initForDebug() — DAP hazırlık
  ├─ runUntilEvent(maxInstr, startDepth) → RunReason
  ├─ resume() — devam ettir
  ├─ stepInstruction() — tek instruction
  ├─ stepOver() / stepLine() / stepOut() — DAP adımlama
  │
  ├─ setBreakpoint / clearBreakpoint / clearAllBreakpoints
  ├─ currentSourceLine() / currentSourceFile()
  ├─ callDepth() → int
  ├─ frameSourceLine/Name/File/SlotCount(depth)
  ├─ readSlotInFrame(depth, slot) → Value
  └─ slotName(depth, slot) → string

DispatchBuiltinMethod(runtimeId, args, heap) → Value
  ├─ id 0-10  : array metodları (length, push, pop, insert, remove, ...)
  ├─ id 11-23 : string metodları (upper, lower, trim, split, substring, ...)
  └─ id 24-25 : struct metodları (toJson, dump)
```

## Veri akışı

```
IRProgram (IRGenerator çıktısı)
       ↓
  Interpreter::initForDebug() → callStack_+moduleSlots_ hazır
       ↓
  Interpreter::run()
       └─ runUntilEvent(-1, -1)  ← sınırsız bütçe
            └─ while(state_ == Running)
                 ├─ instruction = currentFrame.instructions[IP]
                 ├─ switch(opcode):
                 │    LOAD_CONST → slots[dest] = intValue
                 │    ADD → slots[dest] = slots[left] + slots[right]
                 │    JIF_FALSE → if (!isTruthy(slots[cond])) IP = jumpTarget
                 │    CALL → yeni CallFrame push
                 │    RETURN → CallFrame pop, slots[dest] = dönüş değeri
                 │    STRUCT_NEW → heap_.alloc(StructObject)
                 │    THROW → tryStack_'den catch bul, unwind
                 │    CALLHOST → print() / builtinMethod dispatch
                 ├─ IP++
                 └─ checkBreakpoint() / shouldStop()
       ↓
  Dönüş değeri (main'in return'ü)
```

## Diğer modüllerle temas

| Modül | İlişki |
|-------|--------|
| IR | `IRProgram` + `IRFunction` + `Instruction` — yorumlanacak bytecode. |
| Builtin | `dispatchBuiltinMethod()` — built-in metodları çalıştırır. |
| Core | `ModuleRegistry::filePath()` — hata mesajları ve stacktrace için. |
| DAP | `stepLine/Over/Out`, `breakpoints_`, `slotName()`, `readSlotInFrame()`. |
| Diagnostic | Hata mesajları (runtime error → diagnostic engine). |

## Tasarım kararları

- **Slot-tabanlı (register benzeri)**: Stack-based değil. Her CallFrame'in
  sabit sayıda slotu vardır (`slotCount`). Operand'lar slot indeksi ile belirtilir.
- **Heap + GC**: `Heap` sınıfı mark-sweep GC altyapısını içerir.
  Şu an `collect()` tetiklenmez — arena gibi çalışır (#77). `markSweepCollect()`
  iskeleti hazır.
- **Referans semantiği** (ADR-020): Struct/Array değerleri `Ref` (Heap index)
  olarak taşınır. Atama referans kopyalar, derin kopya yapılmaz.
- **String immutable** (ADR-024): String`Value`'ler `strVal` olarak inline
  tutulur. `STRING_CONCAT` yeni string üretir.
- **Hata yönetimi** (ADR-025): `ENTER_TRY` → tryStack_'e kayıt eklenir,
  `THROW` → en yakın catch bulunana kadar frame'ler unwind edilir.
  `pendingThrow_` ile taşınır.
- **Decimal aritmetik** (ADR-028): DecimalValue ile kayıpsız ondalık işlemler.
  `__int128` taşma korumalı.
- **DAP API'leri**: `runUntilEvent(maxInstructions, startCallDepth)` bütçeli
  çalıştırma modeli. `shouldStop()` satır/derinlik/bütçe/breakpoint kontrolü yapar.
  `slotName()` IRFunction::slotNames üzerinden değişken adı döndürür.
- **CallStack geçerlilik**: Döngü güvenliği için her iterasyon başında
  `callStack_.back()` tazelenir; CALL/RETURN sonrası `continue` ile döngü
  başına dönülür (referans invalidation önlemi).

## Bilinen sınırlar / TODO

- GC `collect()` tetiklenmez — arena gibi çalışır, bellek sızdırmaz ama asla
  geri kazanmaz (#77).
- `dispatchBuiltinMethod` switch-case ile elle yazılmıştır; yeni metod eklemek
  için hem builtin kaydı hem de dispatch güncellenmelidir.
- `valueToJsonStr`/`structToJson` yalnızca debug içindir; serileştirme için
  ayrı bir modül düşünülebilir.
- `callhost` yalnızca `print` destekler; yeni host fonksiyon eklemek için
  `executeHostFunction` güncellenmelidir.
- Decimal `toString` karmaşıktır (INT64_MIN edge case); test kapsamı
  genişletilebilir.
