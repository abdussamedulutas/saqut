# Ara Temsil — IR (`src/ir/`)

## Sorumluluk

AST'yi düşük seviyeli, 3-adresli sanal makine talimatlarına dönüştürür. IR,
VM (Interpreter) tarafından doğrudan yorumlanır. İki IR sistemi içerir:
**yeni sistem** (IRGenerator + Instruction + IRFunction + IRProgram) asıl
kullanılan yapıdır; **eski sistem** (CodeGenerator + IROpData) embriyoniktir
ve yeni sistemle değiştirilmiştir.

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `instruction.hpp` | `Opcode` enum (70+ komut) ve `Instruction` struct'ı — VM'in komut seti mimarisi. |
| `ir_generator.hpp` / `.cpp` | `IRGenerator` — AST'yi dolaşarak IR talimatları üretir. |
| `ir_function.hpp` / `.cpp` | `IRFunction` — bir fonksiyonun instruction listesi + slot sayısı + slot adları + satır tablosu. |
| `ir_program.hpp` / `.cpp` | `IRProgram` — tüm modül fonksiyonlarını + ModuleRegistry'yi içeren program birimi. |
| `ir.hpp` | **ESKİ SİSTEM**: `CodeGenerator`, `OPCode`, `IROpData`. Embriyonik, yerini yeni sisteme bırakmıştır. |

## Ana tipler ve ilişkileri

```
Opcode (70+ değer, kategoriler):
  Yükleme    : LOAD_CONST | LOAD_STRING | LOAD_NULL | LOAD_SLOT | LOAD_FLOAT | LOAD_DECIMAL
  Aritmetik  : ADD|SUB|MUL|DIV|MOD (int) | FADD|FSUB|FMUL|FDIV|FNEG (float)
               | DADD|DSUB|DMUL|DDIV|DMOD|DNEG (decimal)
  Bitsel     : BAND | BOR | BXOR | SHL | SHR | BNOT
  Karşılaştır: LESS | LESS_EQUAL | GREATER | GREATER_EQUAL | EQUAL_EQUAL | NOT_EQUAL
  Dönüşüm    : INT_TO_FLOAT | FLOAT_TO_INT | INT_TO_DECIMAL | FLOAT_TO_DECIMAL
               | CAST_INT_TO_STR | CAST_FLOAT_TO_STR | CAST_BOOL_TO_STR
               | CAST_STR_TO_INT | CAST_STR_TO_FLOAT | CAST_FLOAT_TO_INT_CHECKED
               | CAST_DECIMAL_TO_STR | CAST_DECIMAL_TO_FLOAT | CAST_DECIMAL_TO_INT
               | CAST_STR_TO_DECIMAL
  Kontrol    : JMP | JIF_FALSE | JIF_TRUE | CALL | RETURN
  Dizi/Struct: STRUCT_NEW | FIELD_GET | FIELD_SET | ARRAY_NEW | ARRAY_GET | ARRAY_SET | ARRAY_LEN
  Global     : LOAD_GLOBAL | STORE_GLOBAL
  String     : STRING_CONCAT
  Hata       : ENTER_TRY | LEAVE_TRY | THROW
  FFI        : CALLHOST

Instruction
  ├─ opcode       : Opcode
  ├─ dest/src/left/right : int (-1 = kullanılmıyor)
  ├─ intValue     : int (LOAD_CONST)
  ├─ floatValue   : double (LOAD_FLOAT)
  ├─ decimalValue : DecimalValue (LOAD_DECIMAL)
  ├─ stringValue  : string (LOAD_STRING)
  ├─ jumpTarget   : int (JMP/JIF, -1 = backpatch bekliyor)
  ├─ cond         : int (JIF koşul slotu)
  ├─ functionName : string (CALL/CALLHOST)
  ├─ argSlots     : vector<int> (CALL argümanları)
  ├─ fieldNames   : vector<string> (STRUCT_NEW)
  ├─ sourceLine/Col : int (kaynak konum)
  └─ sourceFile   : string

IRFunction
  ├─ name         : string
  ├─ instructions : vector<Instruction>
  ├─ slotCount    : int (max slot + 1)
  ├─ slotNames    : unordered_map<int, string>
  ├─ sourceLines  : vector<int> (IP → kaynak satır)
  ├─ lineToFirstIP: unordered_map<int, int> (satır → ilk IP)
  ├─ firstSourceLine : int
  └─ dump() → string (debug çıktısı)

IRProgram
  ├─ functions    : vector<IRFunction>
  ├─ entryPoint   : string (varsayılan "main")
  ├─ moduleRegistry : ModuleRegistry
  └─ mainFunction() → IRFunction&

IRGenerator
  ├─ program_     : IRProgram
  ├─ currentFn_   : IRFunction* (şu anki fonksiyon)
  ├─ currentLoc_  : SourceLocation (son kaynak konum)
  ├─ generateModuleGraph(graph, table) → IRProgram
  ├─ generate(program, table) → IRProgram (tek dosya)
  ├─ emit(opcode, ...) → int (instruction üretir, IP döndürür)
  ├─ allocateSlot(name) → int (slot tahsis eder)
  └─ visitFunction/Stmt/Expr — AST dolaşma
```

## Veri akışı

```
AST (optimize edilmiş)
       ↓
  IRGenerator::generateModuleGraph(graph, table)
       │  her modül için:
       │    emit(LOAD_CONST/JMP/...)  — 3-adresli talimatlar
       │    allocateSlot(name)        — slot tahsisi
       │    emit sourceLine/sourceCol — satır tablosu
       ↓
  IRProgram (functions + instructions + slotNames + sourceLines)
       ↓
  Interpreter::run()
       │  IP=0'dan başlayarak instruction'ları yorumlar
       │  JMP/JIF ile IP atlatma, CALL ile yeni frame
       ↓
  Çıktı (print/return değeri)
```

## Diğer modüllerle temas

| Modül | İlişki |
|-------|--------|
| Parser | `ASTKind` ile düğüm tipleri ayırt edilir. |
| Symbol | `SymbolTable::structLayouts`/`enumLayouts` — struct/enum düzeni için. |
| Optimizasyon | IRGenerator optimize edilmiş AST'yi alır (veya ham AST'yi). |
| VM (Interpreter) | `IRProgram`'ı doğrudan yorumlar. |
| DAP | `IRFunction::sourceLines`/`lineToFirstIP` — breakpoint eşleme ve satır bazlı adımlama. |
| Module | `ModuleGraph` — çok dosyalı derlemede modül listesi. |

## Tasarım kararları

- **3-adresli kod**: Her Instruction en fazla 3 operand (dest, left/right, src)
  ve bir sabit değer taşır. Kullanılmayan alanlar -1/0/default değerinde kalır.
- **Instruction union değil düz struct**: Tüm olası alanları içerir; kullanılmayanlar
  varsayılan değerde kalır. Bellek israf eder ama incelenebilirlik kazanır.
- **Slot tabanlı**: Sanal register (sınırsız) yerine sabit slot sayısı.
  `allocateSlot()` fonksiyon bazında slot tahsis eder. Slot adları
  `slotNames` ile taşınır (DAP değişken görüntüleme için).
- **Satır tablosu** (sourceLines/lineToFirstIP): Her instruction'ın hangi kaynak
  satırından geldiği kaydedilir. Breakpoint'ler `lineToFirstIP` ile instruction'a
  eşlenir. DAP stepLine/stepOver/stepOut bu tablo ile çalışır.
- **Backpatch**: JMP/JIF_TRUE/JIF_FALSE hedefi (jumpTarget) -1 ise henüz
  bilinmiyor demektir; ikinci geçişte doldurulur.
- **Eski sistem (ir.hpp)**: `CodeGenerator` embriyoniktir, yeni IRGenerator
  kullanılır. Eski sistem hala derlenir ama asıl pipeline yeni sistemi kullanır.
- **`callhost` FFI seam**: CALLHOST ile C++ tarafındaki `print` fonksiyonu
  çağrılır (ADR-016). İleride built-in metodlar da bu yolla çağrılabilir.

## Bilinen sınırlar / TODO

- Eski IR sistemi (ir.hpp) hala derleniyor ama kullanılmıyor — temizlenmeli.
- Backpatch mekanizması ikinci geçiş gerektirir; tek geçişte çözülebilir mi
  değerlendirilebilir.
- `Instruction` struct'ı tüm alanları içerdiği için bellek kullanımı yüksektir
  (her instruction ~200+ byte). Sıkıştırma düşünülebilir.
- `slotNames` yalnızca debug/DAP içindir; release build'te atlanabilir.
