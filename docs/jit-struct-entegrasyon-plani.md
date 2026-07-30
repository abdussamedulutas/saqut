# JIT + STRUCT Entegrasyon Planı

> Hedef: MIR JIT backend'inin struct erişimini, alan adı metadata'sını ve
> FIELD_GET/FIELD_SET opcode'larını desteklemesi.

## Mevcut Durum

- **JIT (MIR backend)**: `src/mir/mir_backend.cpp` — şu an array ve struct
  opcode'larını "desteklenmeyen opcode" ile reddediyor. Yalnızca temel
  aritmetik, kontrol akışı ve CALLHOST çalışıyor.
- **STRUCT_NEW**: VM heap'inde struct tahsisi, alan adları paylaşımlı metadata.
- **FIELD_GET/FIELD_SET**: VM interpreter'da switch-case ile çalışıyor.
- **JIT array**: v1 kapsamı dışı (`[EXPERIMENTAL]`), array'ler VM'de kalır.

## Aşamalı Plan

### Aşama 1 — JIT struct shadow-stack (en kısa yol)

Struct referanslarını JIT register'ında opaque pointer olarak taşı.
FIELD_GET/FIELD_SET VM'e geri düş (fallback).

```
JIT gordugu: REG = some_struct_ptr
FIELD_GET  →  JIT "bu opcode'u handle edemiyorum" → interpreter fallback
```

**Değişiklikler:**
- `mir_backend.cpp`'de `FIELD_GET`/`FIELD_SET`/`STRUCT_NEW` için
  "unsupported opcode" hatası yerine **VM shadow-stack fallback** ekle.
- MIR JIT instruction'ı VM interpreter'a geri yollar, VM yürütür, sonucu
  JIT register'ına yazar.
- JIT derlemesi bu noktada durmaz; kalan instruction'lar JIT'te kalır.

**Risk:** Düşük. Mevcut VM altyapısı aynen kullanılır, JIT yalnızca
struct opcode'larında yavaş kalır (yine de tüm programdan hızlı).

**Süre:** ~2-3 iş günü

### Aşama 2 — MIR struct ABI mapping

Struct alanlarını MIR memory operand'larına eşle. Bir struct pointer'ından
offset bazında field değerlerine MIR load/store talimatlarıyla eriş.

```
STRUCT: { x: int, y: float }
  → heap'te: [4 byte x][4 byte padding][8 byte y]
  → MIR: load_i32(base + 0) → x, load_f64(base + 8) → y
```

**Değişiklikler:**
- `FieldLayout` hesaplama: her struct tipi için field offset + size tablosu.
  IRGenerator veya yeni bir layout pass'ı.
- `Instruction::valueType` (ADR-039) tip bilgisini FIELD_GET/FIELD_SET'e taşır.
- MIR backend FIELD_GET için `MIR_load_insn`, FIELD_SET için `MIR_store_insn`
  üretir.
- Struct layout bilgisi JIT derleme zamanında kullanılabilir olmalı.

**Risk:** Orta. Layout alignment, padding, nested struct ve array field'ları
karmaşıklık ekler. Küçük struct'lar (≤2 field) ile başlanmalı.

**Süre:** ~1 hafta

### Aşama 3 — Struct dönüş değeri + parametre geçişi

MIR calling convention'ı struct değerlerini register veya stack'ten geçirebilir.
SaQut struct'ları referans semantiğinde (heap), dolayısıyla dönüş/parametre
olarak pointer geçer. Bu aşamada özel bir işlem gerekmez — ancak **struct
kopyalama** (`s2 = s1`) referans kopyasıdır (shallow), JIT için ek iş yok.

**Süre:** 0 gün (mevcut semantikle uyumlu)

### Aşama 4 — DAP + JIT koordinasyonu

DAP, struct field değerlerini VM'in `structFieldNamesRegistry_`'sinden okur.
JIT struct'ları aynı heap'te yaşadığı sürece DAP değişiklik gerektirmez.
Yalnızca JIT'in stack frame'lerinde yaşayan (register-only) struct'lar için
DAP'ın VM frame'ine düşmesi gerekir — bu shadow-stack (Aşama 1) ile çözülür.

**Süre:** Aşama 1 ile birlikte

## Detaylı ADR

Her aşama için yeni bir ADR yazılmalı:

| ADR | Konu |
|-----|------|
| ADR-043 | JIT struct shadow-stack fallback |
| ADR-044 | MIR struct field layout + ABI |
| ADR-045 | JIT-DAP frame koordinasyonu |

## Engeller

1. **Tip bilgisi**: `Instruction::valueType` (ADR-039) halen FIELD_GET/STRUCT_NEW
   için dolduruluyor mu? IRGenerator'da `finalizeSlotTypes` ARRAY_NEW'i
   `SlotType::Ref` yapıyor — FIELD_GET için de aynısı geçerli. JIT'in struct
   field tipini bilmesi için `Instruction::valueType`'ın field tipini taşıması
   gerekir. Bu ADR-039'un tam uygulanmasını gerektirir.

2. **Nested struct**: `Point[]` array → Ref. `Line.start.x` → FIELD_GET zinciri.
   JIT'te FIELD_GET'in sonucu başka bir struct ise, ardışık FIELD_GET'ler
   MIR load zinciri olarak derlenebilir.

3. **GC safe-point**: JIT, VM GC safepoint'lerine saygı göstermelidir.
   Şu an JIT yalnızca `callhost`'ta VM'e döner — bu GC tetiklemez.
   JIT struct alloc (STRUCT_NEW) VM'e düştüğünde GC otomatik tetiklenir.

## Önerilen İlk Adım

**Aşama 1 ile başla.** MIR backend'de struct opcode'larını fallback'e
yönlendir. Bu, mevcut tüm struct testlerinin JIT'te de çalışmasını sağlar
(şu an differential testler struct/array içerdiği için atlanıyor).
Ardından Aşama 2'ye geç.

```cpp
// mir_backend.cpp'de eklenecek kod (tasarım)
case Opcode::STRUCT_NEW:
case Opcode::FIELD_GET:
case Opcode::FIELD_SET:
    // Henuz JIT destegi yok → shadow-stack fallback
    // VM interpreter'a geri dus, sonucu MIR reg'ine yaz
    return compileFallback(ins, mir_func);
```

## v1 Kapsamı

v1'de JIT `[EXPERIMENTAL]` etiketini korur. STRUCT desteği opsiyoneldir
ancak aşağıdaki kriterleri karşılamalıdır:

- [ ] Differential testlerde struct/array testleri SKIP yerine PASS olmalı
- [ ] DAP struct değişken görüntülemesi JIT modunda da çalışmalı
- [ ] GC, JIT ile struct alloc/dealloc'ta leak/internal fragmentation
      üretmemeli (ASan doğrulamalı)
- [ ] `ctest` ve `tests/run.sh` tam geçmeli
