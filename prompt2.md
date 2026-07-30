# MİMAR ELÇİSİ — saQut #206 Düzeltme ve Katmanlama Sözleşmesi

Bu belge bir mimar tarafından yazılmış, kod-bilen yeni mezun bir stajyer için
tek seferlik tam bir görev brief'idir. Belgeyi baştan sona oku. Her dosya yolu,
struct alanı, opcode adı ve fonksiyon adı burada REAL ve doğrulanmıştır. Eğer
bir şeyi repoda `grep`/`rg` ile bulamazsan — DUR ve BLOKER raporla; asla bir
alan, dosya, fonksiyon veya davranış UYDURMA. Boşlukları tahminle doldurmak en
büyük günahdır.

---

## 0. SEN KİMSİN ve NASIL ÇALIŞIRSIN

Sen dikkatli, şüpheci, kod yazmadan önce düşünen bir C++ geliştiricisisin. Hız
değil doğruluk önemli. Çalışma kuralların:

1. **Önce oku, sonra düşün, en son yaz.** Her fazdan önce: ilgili dosyaları
   tamamen oku, değişen dosyaları listele, hangi fonksiyona ne yapacağını
   not et. Sonra yaz.
2. **Belirsizlikte dur.** Bir şey belirsizse `grep`/`rg` ile doğrula. Hâlâ
   net değilse BLOKER olarak raporla (ne buldun, neye karar gerek) ve yalnızca
   net kısımlara devam et. Tahmin etme.
3. **Her fazdan sonra derle ve test et.** `cmake --build build -j` başarısızsa
   veya test kırılırsa DUR, geri al veya düzelt; bir sonraki faza geçme.
4. **Okunaklı kod.** İç içe pointer/parantez (`(*((**p)->f)->x)[i]`) yasak.
   Ara değişkenlere çıkar (`auto* obj = ...; auto& field = obj->f;`).
   C++17/20 standartlarına uy. RAII kullan, ham `new`'i sadece GC heap'inde
   kullan (orası zaten öyle).
5. **Repo convention'larına uy.** Dosya başı Türkçe comment bloğu
   (`DİZİN/KATMAN/BAĞIMLI/AMAÇ`), Türkçe açıklamalar, `camelCase` fonksiyon,
   `PascalCase` sınıf, `snake_case_` üye değişken (örn `gcThreshold_`),
   4 boşluk girinti. Mevcut dosyaların üslubunu kopyala.
6. **Testleri zayıflatma.** Hiçbir testi silme, beklenen çıktıyı "geçsin diye"
   değiştirme. Bir test makul olarak güncellenmeliyse (örn. artık packed
   byte[]'nin IR dökümü farklı) nedenini yaz.
7. **Commit yok.** Sadece çalışma ağacını değiştir; `git commit`/`push` yapma.

---

## 1. saQut NEDİR — 30 SANİYELİK GENEL BAKIŞ

saQut, C-benzeri, statik tipli, gizli dönüşümü olmayan bir dildir. Derleyici
tek bir `saqut` binary'si üretir; VM (yorumlayıcı) normatif backend'dir, MIR
JIT deneyseldir (`[EXPERIMENTAL]`, v1 sözleşmesi dışı). Build: CMake + Ninja.
Çalıştırma: `./build/saqut run <dosya>.sqt`.

Derleme pipeline'ı (her aşama bir sonrakine veri aktarır):

```
kaynak (.sqt)
  → tokenizer/lexer   (karakterler → Token)            src/lexer/, src/tokenizer/
  → parser            (Token → AST düğümleri)          src/parser/, src/parser/nodes/
  → symbol collection (isimler → SymbolTable)          src/symbol/
  → semantic          (AST → tiplenmiş AST + hata)     src/semantic/
  → IR generation     (tiplenmiş AST → Instruction)    src/ir/
  → optimization      (AST/IR pass'ları)               src/opt/
  → VM execution      (Instruction → çalışır program)  src/vm/
  → (deneysel) MIR JIT                                src/mir/
```

**Altın kural:** Tip bilgisi bu zincirde akışkan olmalı. Her katman, bir
öncekinin hesapladığı tip bilgisini SİLEMEZ, "hepsi aynı şeydir" diye collapse
edemez. #206'nın kök nedeni tam burada: tip bilgisi semantic'te var ama IR→VM
sınırında siliniyor.

---

## 2. ZORUNLU SÖZLÜK — her terimi sıfırdan açıkla

(Bunları bilmediğini varsayıyoruz. Hepsini doğru öğren.)

**Token:** Lexer'in ürettiği en küçük birim. `src/tokenizer/token.hpp`'de
`Token` temel sınıfı var; `NumberToken` (sayı, `base`/`isFloat` alanları),
`StringToken`, `KeywordToken`, `IdentifierToken` alt sınıfları. Polimorfik.

**AST (Abstract Syntax Tree):** Parser'in Token'lerden ürettiği soyut sözdizim
ağacı. Düğüm tipleri `src/parser/nodes/` altında: `LiteralNode` (sabit değer),
`VariableDeclNode` (değişken bildirimi), `FunctionDeclNode` (fonksiyon bildirimi,
`returnType` string, `params`, `isExported`), `BinaryExpressionNode`, vb. Temel
sınıf `ASTNode` (`src/parser/ast_node.hpp`). Her düğümün `resolvedType` alanı
var (semantic aşamasında doldurulur).

**SymbolTable:** İsim → sembol eşlemi. `src/symbol/`. Her değişken/fonksiyon/
parametre bir `Symbol` olarak kaydedilir; tipi taşır.

**Semantic (tip denetimi):** AST'yi gezip her ifadeye `resolvedType` atar,
tip hatalarını raporlar. `src/semantic/type_checker.cpp`, `structural_validator.cpp`.
Gizli dönüşüm YOKTUR (ADR-010): `int` otomatik `float` olmaz, açık `as` gerekir.

**Type sistemi:** `src/core/type.hpp`. `Type` yapısı:
- `TypeKind`: `Primitive, Array, Struct, Enum, Function, Error`.
- `PrimitiveKind`: `Int, LongInt, Float, Double, Decimal, Byte, Char, String,
  Bool, Void, Date`.
- Array: `Type::array(elem)` → `elementType` (shared_ptr<Type>) taşır.
  Yani `byte[]` = `Type::array(Type::Byte())`; element tipi BURADA bilinir.
- `Type::fromName("int[]")` → array tipi üretir.

**IR (Intermediate Representation):** AST'den üretilen 3-adresli talimat
listesi. `src/ir/instruction.hpp`:
- `Opcode` enum'i: `LOAD_CONST, LOAD_LONG, LOAD_FLOAT, LOAD_FLOAT32, LOAD_STRING,
  ADD, SUB, MUL, DIV, MOD, FADD, F32ADD, LADD, ... , ARRAY_NEW, ARRAY_GET,
  ARRAY_SET, ARRAY_LEN, STRUCT_NEW, STRUCT_GET, STRUCT_SET, CALL, RETURN,
  CAST_*`, vb.
- `SlotType` enum'i: `Int, LongInt, Float, Float32, Ref, Str, Decimal, Date,
  Unknown`. Bir slot'un (sanal register) tip etiketi. **Dikkat:** `Byte` YOK.
  Array slot'ları hep `SlotType::Ref`'tir (çünkü slot bir ArrayObject pointer'ı
  taşır).
- `Instruction` struct'ı: `dest, left, right, src` (slot indeksleri), `intValue`
  (ARRAY_NEW için kapasite), `int64Value, floatValue`, `fieldNames` (STRUCT_NEW
  için alan adları, `std::vector<std::string>`), `sourceLine, sourceCol`.

**VM (Virtual Machine):** IR Instruction'larını yorumlayan yığın tabanlı
makine. `src/vm/`. Her fonksiyon çağrısı bir `CallFrame` (`call_frame.hpp`)
yaratır; her frame'in `slots` (`std::vector<Value>`) dizisi vardır. Slot =
sanal register. `interpreter.cpp` ana döngü: her iterasyonda bir instruction
çalıştırır.

**Value:** `src/vm/value.hpp`. VM'deki tek bir değeri taşıyan yağlı (fat) tagged
union. Tüm alanları gömülü:
```
ValueKind kind;        // Int, LongInt, Float, Float32, Decimal, String, Ref, Null, Date
int       intValue;
double    floatValue;
DecimalValue decimalValue;   // ~16 byte (coeff int64 + exp int)
std::string  stringValue;    // ~32 byte (libstdc++)
Object*      ref;            // heap nesnesine pointer (array/struct)
long long  int64Value;       // LongInt/Date için
```
sizeof(Value) ≈ **80 byte**. Factory'ler: `Value::fromInt, fromLongInt, fromFloat,
fromFloat32, fromRef, fromString, fromDecimal, ...`. `asI64()`, `asDouble()`,
`toString()`, `isFloaty()`.

**Object / ArrayObject / StructObject:** `src/vm/object.hpp`. Heap'te yaşayan
GC-yönetilen nesneler:
- `Object` temel: `type` (ObjectType: Array/Struct/String/Decimal), `marked`
  (GC işareti), `next` (intrusive liste bağı), sanal `markChildren()`.
- `ArrayObject`: `std::vector<Value> elements`. (BUG: her byte bir Value.)
- `StructObject`: `std::vector<Value> fields` + `std::vector<std::string>
  fieldNames`. (BUG: fieldNames her örnekte kopya.)
- `StringObject`, `DecimalObject`: JIT sınırı için kutulu; VM'de az kullanılır.

**Heap:** `src/vm/object.hpp`. Tüm Object'leri intrusive linked list (`head`)
olarak tutar. `allocArray(capacity)`, `allocStruct(fieldCount)` tahsis eder ve
listeye ekler (`++allocCount`). `markValue(v)` (Ref ise işaretler), `markSlots`,
`sweep()` (işaretlenmeyenleri siler).

**GC (Garbage Collector, çöp toplayıcı):** Otomatik bellek yönetimi. Program
çalışırken erişilemez kalan heap nesnelerini otomatik serbest bırakır. saQut
**mark-sweep** kullanır:
- **Mark aşaması:** Köklerden (roots) başlayarak ulaşılabilir tüm nesneleri
  işaretler (recursively `markChildren`).
- **Sweep aşaması:** İşaretlenmemiş tüm nesneleri listeden çıkarıp `delete`.
- **Safepoint:** GC'nin çalışabileceği güvenli nokta. saQut'ta her instruction
  öncesi (`interpreter.cpp` döngü başı, `maybeCollect()`). Yani bir instruction
  çalışırken GC durur; instruction'lar arasında çalışır.
- **Roots (kökler):** `globalSlots_` (modül düzeyi değişkenler) + her `CallFrame`'in
  `slots`'u + `pendingThrow_` (yakalanmamış hata). GC SADECE bunları tarar.
  C++ stack lokallerini taramaz (konservatif değil). Bu yüzden tüm canlı Ref'ler
  bir safepoint'te mutlaka bir slot'ta olmalı.
- **Eşik (threshold):** `allocCount >= gcThreshold_` olunca toplama çalışır;
  toplama sonrası eşik = `max(initial, allocCount*2)` (adaptif).

**STRUCT_NEW (opcode):** Bir StructObject tahsis eder, `intValue` alan sayısı
kadar `fields` açar, `fieldNames`'i kopyalar. (`interpreter.cpp` ~satır 832.)
**STRUCT_GET/STRUCT_SET:** struct alanını indeksle oku/yaz.

**ARRAY_NEW (opcode):** Bir ArrayObject tahsis eder, `intValue` kadar elemanı
0 ile doldurur (`elements.resize(N, Value::fromInt(0))`). (`interpreter.cpp`
~satır 861.) **ARRAY_GET/SET:** indeksle oku/yaz. **ARRAY_LEN:** uzunluk.

**LSP (Language Server Protocol):** Editör (VS Code, Neovim) ile bir "dil
sunucusu" arasında JSON-RPC iletişimidir. IDE özellikleri sağlar: tanıma git
(go-to-definition), hover, tamamlama (completion), tanılama (diagnostics =
hata/uyarı), sembol taslağı, biçimlendirme. **Kaynak metin üzerinde çalışır**;
kodu PARSE eder ve STATIK analiz yapar (AST + semboller + tipler). Programı
ÇALIŞTIRMAZ. saQut'ta: `src/lsp/` (`document_store` açık dosyaları parse+tipli
tutar; `lsp_handler` istekleri yanıtlar). Tüketimi: `parser/symbol/semantic`
(statik katmanlar). VM'ye DOKUNMAZ. → LSP temiz katman.

**DAP (Debug Adapter Protocol):** Editörün DEBUG arayüzü ile bir "debug
adapter" arasında JSON-RPC. ÇALIŞAN bir programı kontrol eder: başlat/bağlan,
breakpoint, adımla (step over/in/out), değişken incele (locals/watches),
ifade değerlendir, çağrı yığını göster. CANLI program durumuna ihtiyaç duyar.
saQut'ta: `src/dap/` (`dap_handler` VM interpreter'ı sürer — instruction
instruction adımlar, slot'ları okur; heap nesnelerini inceler — StructObject
alanlarını, array'leri gösterir). **DAP, VM içine gömülüdür:** `vm/interpreter.hpp`,
`vm/value.hpp`, `vm/object.hpp` include eder; `StructObject::fields` ve
`fieldNames`'e direkt erişir. → DAP ihlalci katman (debug ihtiyacı runtime
nesnesini şekillendirmiş).

**Diferansiyel test (VM ≡ JIT):** Aynı programı VM ve JIT ile çalıştırıp çıktı
aynı mı diye bakar. JIT deneyseldir; array/struct desteklemez, "desteklenmeyen
opcode" ile atlar (normal). Senin değişikliklerin VM'de; JIT'i kırmamak yeter.

---

## 3. BUG #206 — SEMPTOM ve KANIT

#206: `byte[]` array "üret-kullan-at" deseni büyük girdide süper-doğrusal
yavaşlıyor. AES-128 benzeri kripto prototipi (`tests/general/crypto/`) 16 byte'lık
bloklar halinde her round'da birkaç küçük (≤16 eleman) `byte[]` ara değeri
oluşturup atıyor. Girdi büyüdükçe süre orantısız artıyor:

| girdi | süre | 2x beklenti (lineer) |
|---|---|---|
| 128KB | 6.5s | — |
| 512KB | 35.9s | ~26s |
| 1MB | 97.3s | ~52s |
| 2MB | >200s (timeout) | ~104s |

`--gc-stats`: `runs=6888 freed=6946343 live=500`. Sızıntı YOK (6.9M nesne
serbest bırakılmış). Maliyet alloc'de değil, **mark fazında**.

---

## 4. KÖK NEDEN — üç mekanizma (hepsi doğrulandı)

### Neden 1 — Tip-erasür (pipeline'daki asıl hata)
`byte[]` mi `int[]` mi `struct[]` mi ayrımı semantic'te VAR (`Type::elementType`)
ama IR→VM sınırında SİLİNİYOR:
- `slotTypeFromTypeName` (`src/ir/ir_generator.cpp:1604`): array'ler hep
  `SlotType::Ref` (satır 1620). Yorum: "Array (int[]) veya bilinen struct →
  referans."
- `emitArrayNew(destSlot, capacity, loc)` (`src/ir/ir_generator.cpp:1944`):
  element tipi ALMIYOR. Sadece kapasite.
- Sonuç: VM `ArrayObject`'in eleman tipini bilmiyor; hepsini `vector<Value>` sanıyor.

### Neden 2 — Value şişirmesi
`ARRAY_NEW` (`src/vm/interpreter.cpp:861`):
```cpp
ArrayObject* arr = heap_.allocArray(instr.intValue);
arr->elements.resize(instr.intValue, Value::fromInt(0));  // her byte = ~80 byte
```
`byte[1048576]` → 1M × ~80 byte = **~80MB** (mantıken 1MB). 80× bellek şişirmesi.

### Neden 3 — O(N²) GC re-scan (süper-lineerin kaynağı)
`ArrayObject::markChildren` (`src/vm/object.cpp:68`):
```cpp
void ArrayObject::markChildren() {
    for (const Value& v : elements)        // tüm N elemanı gezer
        if (v.kind == ValueKind::Ref)      // byte[]'de hepsi Int → boşuna N iterasyon
            markObject(v.ref);
}
```
Kripto deseni: büyük canlı plaintext `byte[]` + her blokta küçük temp `byte[]`
churn. Toplama sayısı ∝ girdi (N), her toplama büyük canlı array'i O(N) mark
ediyor → **O(N²)**. 512KB+ sapmanın sebebi bu. (Küçük girdide lineer çünkü
quadratik terimin katsayısı küçük.)

### Ek — debug kirliliği (DAP-driven)
`STRUCT_NEW` (`src/vm/interpreter.cpp:832`): `obj->fieldNames = instr.fieldNames;`
— her struct ÖRNEĞİNDE `std::vector<std::string>` kopyalanıyor. Nedeni: DAP
(`src/dap/dap_handler.cpp:91,143,676`) ve `print(struct)` (`structToJson`
`interpreter.cpp:1362`) alan adlarına ihtiyaç duyuyor. Alan adları TİPE aittir
(tüm örneklerde aynı), örneğe değil — ama tasarımda örneğe gömülmüş.

---

## 5. DÜZELTME PLANI — üç faz, sırayla

> Her fazdan sonra: derle → `ctest --test-dir build` → `bash tests/run.sh`.
> Kırılma varsa DUR. #206 tekrar üretülebilir: `time ./build/saqut run
> tests/general/crypto/crypto_stress_512kb.sqt` (önce baseline ölç, sonra).

### FAZ A — Packed type-tagged array'ler (#206'yı kökten çözer)

**Hedef:** `ArrayObject` eleman tipini bilsin; `byte[]` packed `uint8_t` dizisi,
`int[]` packed `int32_t` dizisi vb. tutsun. `markChildren` primitive array'de
O(1) olsun.

**Adım A1 — Yeni enum:** `src/vm/object.hpp`'ye ekle:
```cpp
enum class ArrayElemKind : uint8_t {
    Ref,       // struct[], string[], nested array[], nullable[] → elements (vector<Value>)
    Byte,      // byte[]
    Int,       // int[], bool[], char[]
    LongInt,   // longint[], date[]
    Float32,   // float[]
    Float64,   // double[]
    Decimal,   // decimal[]
};
```

**Adım A2 — ArrayObject'i genişlet** (aynı dosya):
```cpp
struct ArrayObject : Object {
    ArrayElemKind elemKind = ArrayElemKind::Ref;
    std::vector<Value>        elements;   // elemKind == Ref
    std::vector<uint8_t>      bytes;      // elemKind == Byte
    std::vector<int32_t>      ints;       // elemKind == Int
    std::vector<int64_t>      longs;      // elemKind == LongInt
    std::vector<float>        f32s;       // elemKind == Float32
    std::vector<double>       f64s;       // elemKind == Float64
    std::vector<DecimalValue> decimals;   // elemKind == Decimal
    explicit ArrayObject(int capacity = 0, ArrayElemKind k = ArrayElemKind::Ref);
    void markChildren() override;
};
```
Sadece ilgili buffer kullanılır; diğerleri boş kalır (memory'de ~0). Okunaklı,
`reinterpret_cast` YOK, UB yok. (Yeni mezun dostu.)

**Adım A3 — Heap::allocArray imzasını değiştir:**
```cpp
ArrayObject* allocArray(int capacity, ArrayElemKind k) {
    auto* obj = new ArrayObject(capacity, k);
    obj->next = head; head = obj; ++allocCount;
    // constructor ya da burada ilgili buffer'ı resize et (cap ile)
    return obj;
}
```
`ArrayObject` constructor'ı `elemKind`'a göre ilgili buffer'ı `capacity` kadar
resize etsin (0 ile).

**Adım A4 — markChildren** (`src/vm/object.cpp`): SADECE `elemKind == Ref` ise
`elements`'i tara; diğerleri no-op:
```cpp
void ArrayObject::markChildren() {
    if (elemKind != ArrayElemKind::Ref) return;   // primitive → çocuk yok, O(1)
    for (const Value& v : elements)
        if (v.kind == ValueKind::Ref) markObject(v.ref);
}
```

**Adım A5 — Instruction'a elemKind taşı:** `src/ir/instruction.hpp` `Instruction`
struct'ına alan ekle: `ArrayElemKind arrayElemKind = ArrayElemKind::Ref;`
(Sadece ARRAY_NEW için anlamlı; diğer opcodes'ta kullanılmaz.) `instruction.hpp`
`object.hpp`'ye include bağımlılık yaratmasın — `ArrayElemKind`'ı küçük bir
başlıkda tut (örn `core/array_elem_kind.hpp`) veya `instruction.hpp`'de kendisi
tanımla ve `object.hpp` onu include etsin. Döngüsel bağımlılıktan kaçın.

**Adım A6 — emitArrayNew imzasını değiştir** (`src/ir/ir_generator.cpp:1944`):
```cpp
void IRGenerator::emitArrayNew(int destSlot, int capacity,
                               ArrayElemKind k, const SourceLocation& loc) {
    Instruction ins(Opcode::ARRAY_NEW);
    ins.dest = destSlot; ins.intValue = capacity;
    ins.arrayElemKind = k; ins.loc = loc;
    // ... mevcut push mantığı
}
```
İki çağrı yeri var (`grep "emitArrayNew" src/ir/ir_generator.cpp`):
- `:310` — değişken bildiriminde (`int[] x`): element tipi `varType` string'inden
  çıkar (`"int[]"` → `"int"` → ArrayElemKind).
- `:1170` — array literalinde (`[1,2,3]`): element tipini literal'in
  `resolvedType.elementType`'ından çıkar.
Bir yardımcı yaz: `ArrayElemKind arrayElemKindFromType(const Type& elemType)`:
- `Byte` → Byte; `Int/Bool/Char` → Int; `LongInt/Date` → LongInt;
  `Float` → Float32; `Double` → Float64; `Decimal` → Decimal;
  `String/Struct/Enum/Array` (ve nullable) → Ref.

**Adım A7 — VM ARRAY_NEW handler** (`src/vm/interpreter.cpp:861`):
```cpp
case Opcode::ARRAY_NEW: {
    ArrayObject* arr = heap_.allocArray(instr.intValue, instr.arrayElemKind);
    // allocArray/constructor ilgili buffer'ı resize etti
    frame.slots[instr.dest] = Value::fromRef(arr);
    break;
}
```
(`elements.resize(...)` satırını kaldır — constructor yapıyor.)

**Adım A8 — VM ARRAY_GET handler** (`src/vm/interpreter.cpp:867`): bounds check
sonra `arr->elemKind`'a göre switch. Her dal ilgili buffer'dan okuyup Value üretir:
- Byte: `Value::fromInt((int)arr->bytes[idx])`
- Int: `Value::fromInt(arr->ints[idx])`
- LongInt: `Value::fromLongInt(arr->longs[idx])`
- Float32: `Value::fromFloat32((double)arr->f32s[idx])`
- Float64: `Value::fromFloat(arr->f64s[idx])`
- Decimal: `Value::fromDecimal(arr->decimals[idx])`
- Ref: `arr->elements[idx]` (mevcut davranış)
Ara değişken kullan (`auto* arr = ...; auto& buf = arr->ints;`), iç içe pointer yok.

**Adım A9 — VM ARRAY_SET handler** (`src/vm/interpreter.cpp:884`): benzer switch,
Value'dan ilgili tipe yaz:
- Byte: `arr->bytes[idx] = (uint8_t)value.intValue`  (typechecker byte sağlamalıdır)
- Int: `arr->ints[idx] = value.intValue`
- LongInt: `arr->longs[idx] = value.asI64()`
- Float32: `arr->f32s[idx] = (float)value.asDouble()`
- Float64: `arr->f64s[idx] = value.asDouble()`
- Decimal: `arr->decimals[idx] = value.decimalValue`
- Ref: `arr->elements[idx] = value`

**Adım A10 — ARRAY_LEN** (`:901`): `arr->elemKind`'a göre ilgili buffer'ın
`size()`'ını döndür.

**Adım A11 — Diğer array opcode'larını denetle:** `grep -n "Opcode::ARRAY"
src/vm/interpreter.cpp` ve `grep -n "ArrayObject" src/vm/interpreter.cpp`.
ARRAY_NEW/GET/SET/LEN dışında varsa onları da güncelle. `src/builtin/builtin_methods.hpp`'da
array builtin'i (push/append vb.) varsa güncelle (push, packed buffer'a ekler).

**Adım A12 — JIT'i kırmadan kontrol et:** `src/mir/mir_backend.cpp` şu an
ARRAY_NEW'u "desteklenmeyen opcode" ile reddediyor. Instruction'a yeni alan
ekledin; JIT instruction'ı okumuyorsa sorun yok. `grep -n "ARRAY_NEW\|ARRAY_GET\|ARRAY_SET"
src/mir/mir_backend.cpp` ile doğrula. JIT array desteklemediği için packed
storage'ı JIT'te implement etme (v1 dışı).

**Adım A13 — Doğrula:**
- `cmake --build build -j` başarılı.
- `ctest --test-dir build` — tüm golden testler geçmeli (array golden'ları:
  `tests/golden/array/`, `tests/golden/numeric/`).
- `bash tests/run.sh` — diferansiyel + GC + builtin kontrolleri.
- `time ./build/saqut run tests/general/crypto/crypto_stress_512kb.sqt` —
  süre belirgin düşmeli (80× daha az bellek, O(N²) → O(N) mark).
- `./build/saqut ir tests/golden/array/ref_semantics.sqt` — IR dökümünde
  ARRAY_NEW artık elemKind taşımalı (döküm formatını güncellemen gerekebilir,
  `instruction.hpp`'daki opcodeToString/toString fonksiyonuna bak).

### FAZ B — fieldNames → type-metadata (struct churn + DAP kirliliği)

**Hedef:** `StructObject::fieldNames` her örnekte kopyalanmasın; tip başına
bir kez tutulsun.

**Adım B1 — Metadata tablosu:** VM tarafında (örn `src/vm/object.hpp` veya
`src/vm/interpreter.hpp`) bir registry:
```cpp
// Struct alan adları tip başına bir kez. key = struct adı.
std::unordered_map<std::string, std::shared_ptr<std::vector<std::string>>>
    structFieldNames_;
```
`StructObject::fieldNames` alanını `std::shared_ptr<std::vector<std::string>>`
yap (örnek cheap copy yapar, paylaşımlı sahiplik). Veya raw pointer +
registry sahiplik (program sonuna kadar yaşar). shared_ptr daha güvenli.

**Adım B2 — STRUCT_NEW handler** (`interpreter.cpp:832`): `obj->fieldNames`
kopyalamak yerine registry'den al/ekle:
```cpp
auto names = std::make_shared<std::vector<std::string>>(instr.fieldNames);
// struct adını bul (instr'da veya structLayouts_'tan); registry'ye koy
obj->fieldNames = registry_get_or_set(structName, names);
```
Not: STRUCT_NEW instruction'ı struct ADINI taşımıyor olabilir. `grep` ile
doğrula; taşımıyorsa, ya struct adını instruction'a ekle ya da fieldNames
imzasından struct'ı eşle (alan adı kümesi unique ise). Belirsizse BLOKER raporla.

**Adım B3 — Tüketici güncelle:** `structToJson` (`interpreter.cpp:1362`),
`dap_handler.cpp:91,143,676` fieldNames'e erişiyor. `shared_ptr` dolaylığını
kaldır: `if (i < obj->fieldNames->size()) ... (*obj->fieldNames)[i]`. Ara
referans al: `auto& names = *obj->fieldNames;` sonra `names[i]` (okunaklı).

**Adım B4 — Doğrula:** build + golden + DAP testleri (`ctest --test-dir build -R dap_`).
Struct içeren golden'lar (`tests/golden/struct/`) geçmeli.

### FAZ C — DAP/VM observability boundary (mimari önlem, ileride çıkmaması için)

**Hedef:** DAP, `vm/object.hpp`/`vm/value.hpp` iç structlarına direkt erismesin;
stabil bir debug-view arayüzü tüketsin. Bu faz #206 için zorunlu DEĞİL (A+B
yeter) ama tekrarı önler. Zamanın varsa yap.

**Adım C1 — Arayüz tanımla** (yeni başlık, örn `src/vm/debug_view.hpp`):
```cpp
class IDebugValueView {
public:
    virtual ~IDebugValueView() = default;
    virtual std::string kindName() const = 0;        // "int", "struct Point", ...
    virtual std::string render() const = 0;          // insan-okur gösterim
    virtual bool hasChildren() const = 0;            // struct/array?
    virtual size_t childCount() const = 0;
    virtual std::string childName(size_t i) const = 0;
    virtual IDebugValueView* child(size_t i) const = 0; // sahiplik yok; GC-canlı
};
```
VM, `Value`/`Object` için bu arayüzü implement etsin (lightweight view, kopya
yok, ham pointer döner — GC canlı olduğu sürece geçerli).

**Adım C2 — DAP'ı geçir:** `src/dap/dap_handler.cpp` artık
`vm/object.hpp`/`StructObject::fields` kullanmasın; `IDebugValueView` tüketsin.
`dap_handler.hpp`'dan `vm/object.hpp` include'unu kaldır (value.hpp tutulabilir
veya o da view arkasına). `vm/interpreter.hpp` DAP için public stepping API'si
sunuyorsa onu koru.

**Adım C3 — LSP'ye dokunma:** LSP zaten temiz (statik katmanlar). Bu faz DAP odaklı.

**Not:** Faz C büyük; A+B doğrulanmadan başlama. C şart değil ama katman
disiplini için değerli. Sadece A+B yapıp C'yi "öneri" olarak bırakmak da kabul.

---

## 6. MİMARIN TECRÜBE NOTLARI (neden-bilgisi aktarımı)

1. **Tip akışkanlığı kuralı:** AST type → symbol type → IR element type → VM
   elemKind zinciri eksiksiz olmalı. Hiçbir katman öncekinin tip bilgisini
   "hepsi Ref zaten" diyerek silmemeli. #206 ve daha önceki array-return-tip
   bug'ı aynı ailedendir: tip pipeline'da kayboluyor. Düzeltmenin kalıcı
   olması için element tipini tüm zincire taşıdığından emin ol.
2. **Debug ayrı katmandır.** LSP/DAP'ın ihtiyaçları (alan adları, değer
   gösterimi) runtime representation'ı ŞEKİLLENDİRMEMELİ. Representation
   verimlilik için tasarlanır; debug, onun ÜZERİNE bir view'dır. fieldNames
   örnekte değil tipte olmalı. DAP ham struct'a değil arayüze bakmalı.
3. **Per-instance değil per-type metadata.** Tüm örneklerde aynı olan bilgi
   (field adları, element tipi) örneğe gömülmez; tip-meta-verisinde, örnek
   yalnız referans (id/pointer) taşır.
4. **Şüpheci ol.** "Çalışıyor" ≠ "doğru". GC için: freed sayısı yüksek diye
   "sağlıklı" deme; mark fazının maliyetini ölç. Performance iddialarını
   zaman/`--gc-stats` ile doğrula.
5. **Okunaklılık > zekilik.** İç içe pointer/parantez yasak. `reinterpret_cast`
   yasak. Switch'i ara değişkenlerle aç. Bir fonksiyon bir iş yapar.
6. **Sınırları bil.** JIT deneysel; onu düzeltmek senin işin değil, kırmamak
   yeter. Vendor (`src/mir/vendor/`) asla dokunulmaz. Dil sözdizimi/ADR/CLI
   değişmez.

---

## 7. DOKUNMAMAN GEREKEN YERLER (no-go)

- `src/mir/vendor/` — vendored MIR, yasak.
- Dil sözdizimi, ADR'ler (`docs/adr/`), public CLI yüzeyi (`src/cli/commands/`).
- `tests/golden/` `.expected` dosyaları — geçsin diye değiştirme (IR döküm
  formatı makul değişirse nedeniyle).
- `src/lexer/` (lexer doğru çalışıyor, #199 zaten düzeltildi).
- JIT array implementasyonu (v1 dışı).
- Başka bir bug'ı "fırsat" sanıp düzeltme; yalnızca bu plan.

---

## 8. SON RAPOR FORMATI (bittiğinde üret)

1. **Yapılan fazlar:** A/B/C hangileri tamamlandı.
2. **Değişen dosyalar:** exact liste.
3. **Build/test kanıtı:** `cmake --build build` çıkışı, `ctest` özeti,
   `bash tests/run.sh` özeti, #206 before/after `time` ölçümü.
4. **Kanıtlanamayanlar:** ölçmediğin şeyleri "hızlandı" diye yazma.
5. **Bloklar/Açık sorular:** belirsizlikte durduğun yerler.
6. **Riskler:** regresyon yüzeyi (özellikle ARRAY_GET/SET tip dallanması,
   JIT instruction formatı).
7. **Faz C durumu:** yapıldıysa/ne kadar.

---

## 9. BAŞLARKEN kontrol listesi

- [ ] Repo kökünde `git status --short` ve `git rev-parse HEAD` kaydet.
- [ ] `cmake --build build -j` ile binary'nin sağlam olduğunu doğrula.
- [ ] #206 baseline: `time ./build/saqut run tests/general/crypto/crypto_stress_512kb.sqt`
      (ve 64kb/2mb varsa) — ölç, not al.
- [ ] `grep -n "Opcode::ARRAY" src/vm/interpreter.cpp` ile tüm array opcode'larını listele.
- [ ] `grep -n "emitArrayNew\|STRUCT_NEW\|fieldNames" src/ir/ir_generator.cpp src/vm/interpreter.cpp`
      ile dokunacağın yerleri gör.
- [ ] Sonra Faz A'ya başla: A1 → A2 → ... → A13, her adımda derle.

Hız değil, doğruluk. Tahmin yok, kanıt var. Başla.
