# saQut IR — Opcode Referansı

**Kaynak:** `src/ir/instruction.hpp` → `OPCODE_LIST` makrosu.
Bu belgedeki tablo o makrodan mekanik olarak üretilmiştir; elle düzenlenmez.
Yeni opcode eklemek = `OPCODE_LIST`'e bir satır eklemek; `enum`, `opcodeName()`,
`opcodeArity()`, `opcodeBackends()` ve JIT temel filtresi oradan türetilir.

**Durum (0.9.3 başlangıcı):** 98 opcode. VM (normatif) hepsini çalıştırır.
JIT `[EXPERIMENTAL]` — 85'i temel destekli, 13'ü hiç desteklenmiyor.

---

## 1. Yürütme modeli

Her fonksiyon çağrısı bir **frame** açar. Frame içinde numaralı **slot**'lar
vardır: parametreler slot 0'dan başlar, ardından yereller ve geçiciler gelir.
`slots[5] = 42` → "5 numaralı kutucuğa 42 yaz".

ADR-020 gereği bir slot çalışma zamanında tip değiştirmez. Bu statik tip
`SlotType` ile taşınır:

| SlotType | Anlamı | JIT register |
|---|---|---|
| `Int` | 32-bit signed | `MIR_T_I64` |
| `LongInt` | 64-bit signed | `MIR_T_I64` |
| `Float` | 64-bit IEEE double | `MIR_T_D` |
| `Float32` | 32-bit IEEE single | `MIR_T_D` (truncate'li) |
| `Str` | string referansı | `MIR_T_I64` (pointer) |
| `Decimal` | ADR-028 ondalık | `MIR_T_I64` (pointer) |
| `Ref` | array/struct nesnesi | **yok — JIT reddeder** |
| `Date` | UTC epoch-ms | **yok — JIT reddeder** |
| `Unknown` | çözülememiş | **yok — JIT reddeder** |

`Ref`'in register temsilinin olmaması, JIT kapsamının bugünkü asıl sınırıdır:
array, struct ve string metodlarının tamamı buna dayanır.

### Arite kuralı

`opcodeArity()` "anlamlı operand alanı" sayar:

- slot operandları (`dest`/`src`/`left`/`right`/`cond`) ve sabitler
  (`intValue`/`int64Value`/`floatValue`/`decimalValue`/`stringValue`) teker teker
- callee (`functionName` + `argSlots`) **tek** operand
- fallible cast'lerde nullable-modu bayrağı (`left`) bir operand
- `ARRAY_NEW`'de `arrayElemKind` bir operand
- IR-metadata (`valueType`, `fieldNames`, `source*`, `requiredCap`) **sayılmaz**

---

## 2. Opcode tablosu

### Değer yükleme

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `LOAD_CONST` | 2 | evet | slots[dest] = intValue (tam sayı sabiti) |
| `LOAD_STRING` | 2 | evet | slots[dest] = stringValue |
| `LOAD_NULL` | 1 | **hayır** | slots[dest] = null (ADR-021) |
| `LOAD_SLOT` | 2 | evet | slots[dest] = slots[src] |

### Aritmetik (dest = left OP right)

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `ADD` | 3 | evet |  |
| `SUB` | 3 | evet |  |
| `MUL` | 3 | evet |  |
| `DIV` | 3 | evet | UYARI: sıfıra bölme → runtime_error |
| `MOD` | 3 | evet |  |

### Bitsel (dest = left OP right)

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `BAND` | 3 | evet | slots[left] & slots[right] |
| `BOR` | 3 | evet | slots[left] | slots[right] |
| `BXOR` | 3 | evet | slots[left] ^ slots[right] |
| `SHL` | 3 | evet | slots[left] << slots[right] |
| `SHR` | 3 | evet | slots[left] >> slots[right] |
| `BNOT` | 2 | evet | slots[dest] = ~slots[src] (tekli) |

### Karşılaştırma (sonuç: 1 = doğru, 0 = yanlış)

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `LESS` | 3 | evet |  |
| `LESS_EQUAL` | 3 | evet |  |
| `GREATER` | 3 | evet |  |
| `GREATER_EQUAL` | 3 | evet |  |
| `EQUAL_EQUAL` | 3 | evet |  |
| `NOT_EQUAL` | 3 | evet |  |

### Kontrol akışı

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `JMP` | 1 | evet | ip = jumpTarget |
| `JIF_FALSE` | 2 | evet | cond falsy ise ip = jumpTarget |
| `JIF_TRUE` | 2 | evet | cond truthy ise ip = jumpTarget |

### Fonksiyon çağrısı

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `CALL` | 3 | evet | dest, functionName, argSlots |
| `RETURN` | 1 | evet | slots[src]'yi caller'a ilet |

### Float aritmetik (#44)

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `LOAD_FLOAT` | 2 | evet | slots[dest] = floatValue |
| `FADD` | 3 | evet |  |
| `FSUB` | 3 | evet |  |
| `FMUL` | 3 | evet |  |
| `FDIV` | 3 | evet | sıfır → runtime_error |
| `FNEG` | 2 | evet | -slots[src] |
| `INT_TO_FLOAT` | 2 | evet | gizli int→float |
| `FLOAT_TO_INT` | 2 | evet | açık cast |

### Float32 aritmetik (ADR-040: tek sonuç (float) truncate)

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `LOAD_FLOAT32` | 2 | evet | single sabit yükle |
| `F32ADD` | 3 | evet |  |
| `F32SUB` | 3 | evet |  |
| `F32MUL` | 3 | evet |  |
| `F32DIV` | 3 | evet | sıfır → runtime_error |
| `F32NEG` | 2 | evet |  |
| `INT_TO_FLOAT32` | 2 | evet | int → float32 |
| `FLOAT32_TO_INT` | 2 | evet | float32 → int (checked) |
| `FLOAT_TO_FLOAT32` | 2 | evet | double → float (E003 veri kaybı) |
| `FLOAT32_TO_FLOAT` | 2 | evet | float → double (kayıpsız) |

### LongInt aritmetik (ADR-040: 64-bit signed, wrap tanımlı)

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `LOAD_LONG` | 2 | evet | 64-bit sabit yükle |
| `LADD` | 3 | evet |  |
| `LSUB` | 3 | evet |  |
| `LMUL` | 3 | evet |  |
| `LDIV` | 3 | evet | sıfır → Error; INT64_MIN/-1 → INT64_MIN |
| `LMOD` | 3 | evet | sıfır → Error; INT64_MIN/-1 → 0 |
| `LNEG` | 2 | evet |  |
| `LBAND` | 3 | evet |  |
| `LBOR` | 3 | evet |  |
| `LBXOR` | 3 | evet |  |
| `LSHL` | 3 | evet |  |
| `LSHR` | 3 | evet | aritmetik |
| `LBNOT` | 2 | evet |  |
| `INT_TO_LONG` | 2 | evet | int → longint (kayıpsız) |
| `LONG_TO_INT_CHECKED` | 3 | evet | int32 aralığı dışı → fallible |

### Struct (ADR-020: referans semantiği)

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `STRUCT_NEW` | 3 | **hayır** | dest, intValue alan sayısı, functionName tip adı |
| `FIELD_GET` | 3 | **hayır** | slots[dest] = slots[src].fields[intValue] |
| `FIELD_SET` | 3 | **hayır** | slots[dest].fields[intValue] = slots[right] |

### Array (ADR-020: referans semantiği; #206 packed elemanlar)

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `ARRAY_NEW` | 3 | **hayır** | dest, intValue kapasite, arrayElemKind packed tip |
| `ARRAY_GET` | 3 | **hayır** | slots[dest] = slots[left][slots[right]] — sınır kontrolü |
| `ARRAY_SET` | 3 | **hayır** | slots[dest][slots[left]] = slots[right] — sınır kontrolü |
| `ARRAY_LEN` | 2 | **hayır** | slots[dest] = slots[src].uzunluk() |

### Modül-düzeyi değişken erişimi

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `LOAD_GLOBAL` | 2 | **hayır** | slots[dest] = moduleSlots[intValue] |
| `STORE_GLOBAL` | 2 | **hayır** | moduleSlots[intValue] = slots[src] |

### String işlemleri (ADR-024: immutable değer-tipi)

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `STRING_CONCAT` | 3 | evet | slots[dest] = slots[left] + slots[right] |

### Hata yönetimi (ADR-025: UNCHECKED try/catch/throw)

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `ENTER_TRY` | 2 | **hayır** | dest, jumpTarget; callDepth'i VM kaydeder |
| `LEAVE_TRY` | 0 | **hayır** | TryFrame'i çıkar (operand yok) |
| `THROW` | 1 | **hayır** | slots[src] değerini fırlat |

### Tip dönüşümleri (ADR-026: as operatörü) — hatasız

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `CAST_INT_TO_STR` | 2 | evet |  |
| `CAST_FLOAT_TO_STR` | 2 | evet |  |
| `CAST_BOOL_TO_STR` | 2 | evet |  |

### Tip dönüşümleri — fallible (left=0 → Error; left=1 → null)

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `CAST_STR_TO_INT` | 3 | evet |  |
| `CAST_STR_TO_FLOAT` | 3 | evet |  |
| `CAST_FLOAT_TO_INT_CHECKED` | 3 | evet | NaN/Inf/taşma → fallible |
| `CAST_INT_TO_BYTE_CHECKED` | 3 | evet | 0-255 dışı → fallible (#86) |
| `CAST_LONG_TO_STR` | 2 | evet | longint → string (hatasız) |
| `CAST_STR_TO_LONG` | 3 | evet | string → longint (fallible) |
| `CAST_FLOAT32_TO_STR` | 2 | evet | float32 → string (hatasız) |
| `CAST_STR_TO_FLOAT32` | 3 | evet | string → float32 (fallible) |
| `CAST_FLOAT_TO_LONG_CHECKED` | 3 | evet | NaN/Inf/int64 taşma → fallible |

### Decimal aritmetik (ADR-028)

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `LOAD_DECIMAL` | 2 | evet | decimal sabit yükle |
| `DADD` | 3 | evet |  |
| `DSUB` | 3 | evet |  |
| `DMUL` | 3 | evet |  |
| `DDIV` | 3 | evet | sıfır → Error |
| `DMOD` | 3 | evet | sıfır → Error |
| `DNEG` | 2 | evet |  |
| `INT_TO_DECIMAL` | 2 | evet | gizli int→decimal terfi |
| `FLOAT_TO_DECIMAL` | 2 | evet | gizli float→decimal terfi |
| `CAST_DECIMAL_TO_STR` | 2 | evet | hatasız |
| `CAST_DECIMAL_TO_FLOAT` | 2 | evet | hatasız |
| `CAST_DECIMAL_TO_INT` | 3 | evet | trunc; taşma → fallible |
| `CAST_STR_TO_DECIMAL` | 3 | evet | fallible |

### Dış dünya (FFI)

| Opcode | Arite | JIT | Açıklama |
|---|---|---|---|
| `CALLHOST` | 2 | evet | functionName, argSlots; şu an yalnız print |

Toplam: 98 opcode, JIT temel destekli: 85

---

## 3. JIT'in reddettikleri

### 3.1 Temel destek yok (`OPCODE_LIST`'te `OP_JIT` bayrağı yok) — 13 opcode

| Aile | Opcode'lar | Kök engel |
|---|---|---|
| Array | `ARRAY_NEW`, `ARRAY_GET`, `ARRAY_SET`, `ARRAY_LEN` | `SlotType::Ref` |
| Struct | `STRUCT_NEW`, `FIELD_GET`, `FIELD_SET` | `SlotType::Ref` |
| Global | `LOAD_GLOBAL`, `STORE_GLOBAL` | modül slot dizisi JIT'e görünmüyor |
| try/catch | `ENTER_TRY`, `LEAVE_TRY`, `THROW` | unwind protokolü yok |
| Null | `LOAD_NULL` | null'un register temsili tanımsız |

### 3.2 Temel destekli ama koşullu reddedilenler

`mir_backend.cpp::opcodeSupported()`:

- **Fallible cast'ler** (`CAST_STR_TO_INT`, `CAST_FLOAT_TO_INT_CHECKED`, …):
  `instr.left == 1` (nullable hedef) ise reddedilir. Non-nullable hedefte
  başarısızlık = uncaught throw, VM ile aynı.
- **`RETURN`**: `src < 0` (void return) reddedilir.
- **`CALLHOST`**: `isSupportedCallhost()` yalnızca `functionName == "print"`
  ve tek argümanlı olanı geçirir.
- **Sıralama karşılaştırmaları** (`LESS`/`LESS_EQUAL`/`GREATER`/`GREATER_EQUAL`):
  operandlardan biri `Str` ise reddedilir (frontend zaten E003 verir; bu
  savunmacı kalkan). Eşitlik (`==`/`!=`) string'de içerik karşılaştırmasıdır ve
  `rt_jit_string_eq` runtime call'una çevrilir — native pointer eşitliği yanlış
  olurdu.

### 3.3 "Hep ya da hiç" sözleşmesi

`wholeProgramSupported()` programdaki **her** fonksiyonun **her** talimatını
tarar. Tek bir reddedilen talimat tüm programı VM'e düşürür. Sessiz fallback
yoktur; ret gerekçesi `UnsupportedReason` ile (fonksiyon adı + opcode adı)
raporlanır.

Bu sözleşme kasıtlıdır: kısmi JIT, VM ve JIT semantiğinin aynı çalıştırma
içinde karışmasına izin verirdi ve determinizm garantisini (aynı program, aynı
backend-bağımsız sonuç) doğrulanamaz hale getirirdi.

---

## 4. `CALLHOST` — üç ayrı aile, tek opcode

`CALLHOST` bugün üç farklı çağrı ailesini `functionName` string'ine bakarak
ayırır. Bu, JIT kapsamının ikinci büyük engelidir.

| `functionName` | `intValue` | Dispatch | JIT |
|---|---|---|---|
| `"__ffi__"` | host id | `callHostFn(id, args, ctx)` — `hostFnTable()` | hayır |
| `"__builtin_method__"` | runtime id | `dispatchBuiltinMethod(id, args, heap)` | hayır |
| `"print"` ve diğerleri | — | `executeHostFunction(name, …)` | yalnız `print/1` |

Üç yolun da imzası farklı: biri `HostContext` (caps + args + heap) alır, biri
`Heap&` alır, biri hiçbiri. JIT'in bugün yalnız `print`'i bilmesinin sebebi
teknik bir sınır değil — her aile için ayrı trampoline yazmak gerekmesi.

**Host fonksiyon aileleri** (`src/ffi/host_functions.cpp`, 44 kayıt):
`MATH_*` (13), `DATE_*` (17), `FS_*` (6), `SYS_*` (5), `CAPS_*` (2), `CORE_*` (1).

**Built-in metodlar** (`src/builtin/builtin_methods.hpp`): `Array`, `StringVal`,
`StructVal` kategorileri; `runtimeId` ile `interpreter.cpp`'deki dev `switch`'e
bağlanır.

---

## 5. Instruction alan kullanımı

`Instruction` okunabilirlik için **tüm** alanları taşır; kullanılmayanlar
varsayılanda (`-1` veya boş) kalır.

| Alan | Kullanan opcode'lar |
|---|---|
| `dest` | sonuç üreten her opcode |
| `src` | `LOAD_SLOT`, `RETURN`, tekli işlemler |
| `left`, `right` | ikili aritmetik/karşılaştırma; fallible cast'lerde `left` = nullable bayrağı |
| `intValue` | `LOAD_CONST`, `FIELD_*` indeks, `ARRAY_NEW` kapasite, `CALLHOST` host/runtime id |
| `int64Value` | `LOAD_LONG` |
| `floatValue` | `LOAD_FLOAT`, `LOAD_FLOAT32` |
| `decimalValue` | `LOAD_DECIMAL` |
| `stringValue` | `LOAD_STRING` |
| `jumpTarget` | `JMP`, `JIF_FALSE`, `JIF_TRUE`, `ENTER_TRY` |
| `cond` | `JIF_FALSE`, `JIF_TRUE` |
| `functionName` | `CALL`, `CALLHOST`, `STRUCT_NEW` (tip adı) |
| `argSlots` | `CALL`, `CALLHOST` |
| `fieldNames` | `STRUCT_NEW` (dump/toJson) |
| `arrayElemKind` | `ARRAY_NEW` (#206 packed eleman tipi) |
| `valueType` | ADR-039: `FIELD_GET`/`ARRAY_GET`/`LOAD_GLOBAL` dest tipi, `ARRAY_NEW` eleman tipi |
| `sourceLine`, `sourceCol` | hata-odaklı opcode'lar |
| `requiredCap` | ADR-035: `CALLHOST("__ffi__")` capability backstop |

---

## 6. Bilinen kalite sorunları

Bu belge envanterdir, savunma değil. Sonraki oturumda konuşulacak başlıklar:

1. **`CALLHOST` aşırı yüklü.** Üç ayrı çağrı sözleşmesi bir opcode'a ve bir
   string karşılaştırmasına sıkıştırılmış. Her sıcak yolda `functionName`
   string'i karşılaştırılıyor.
2. **`Instruction` şişkin.** Her talimat `std::string` × 3 + `std::vector` × 2 +
   `DecimalValue` taşıyor; çoğu her zaman boş. (`sourceFile` alanı `fileId`'ye
   çevrildikten sonra bile.)
3. **`ARRAY_LEN` ile `builtin::length` çakışıyor.** Uzunluk hem opcode hem
   built-in metod olarak var.
4. **`FIELD_GET`/`ARRAY_GET` tip bilgisi `valueType`'ta ayrı taşınıyor** —
   opcode'dan türetilemiyor, çünkü kaynak heap.
5. **Backend bayrağı ikili.** `OP_JIT` "temel destek" demek; gerçek koşullar
   `mir_backend.cpp`'de ayrı bir `switch`'te. Tek kaynak iddiası bu noktada
   kırılıyor.
6. **`LOAD_NULL` register temsili yok** — nullable'ın JIT'e girmesini tek başına
   engelliyor, oysa `Ref` gerektirmiyor.
