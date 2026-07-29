// ============================================================================
// saQut IR — Instruction (Tek Talimat)
//
// Sanal makine bu talimatlara bakarak ne yapacağını anlar.
// Her talimatın bir "opcode"u (ne iş yapacağı) ve birkaç operandı vardır.
// Operandlar ya slot numarasıdır (fonksiyonun yerel değişken/geçici depoları)
// ya da doğrudan bir sayı/isim değeridir.
//
// SLOT NEDİR?
//   Her fonksiyon çağrısı kendi "frame"ini açar.
//   Frame içinde numaralı kutucuklar vardır: slot[0], slot[1], ...
//   Parametreler slot 0'dan başlar. Sonrasında lokal değişkenler
//   ve hesaplama sırasında oluşan geçici değerler gelir.
//   "slots[5] = 42" demek "5 numaralı kutucuğa 42 değerini koy" demektir.
//
// HANGİ OPCODE HANGİ ALANI KULLANIR?
//   LOAD_CONST     : dest, intValue
//   LOAD_SLOT      : dest, src
//   ADD/SUB/...    : dest, left, right
//   LESS/LEQ/...   : dest, left, right       (sonuç: 1=doğru, 0=yanlış)
//   JMP            : jumpTarget
//   JIF_FALSE      : cond, jumpTarget
//   JIF_TRUE       : cond, jumpTarget
//   CALL           : dest, functionName, argSlots
//   RETURN         : src
//   CALLHOST       : functionName, argSlots
// ============================================================================

#ifndef SAQUT_IR_INSTRUCTION
#define SAQUT_IR_INSTRUCTION

#include <optional>
#include <string>
#include <vector>
#include "core/decimal.hpp"
#include "core/capability.hpp"
#include "core/type.hpp"

// ----------------------------------------------------------------------------
// SlotType — bir slot'un statik değer türü (ADR-020: slot çalışma zamanında tip
// değiştirmez). Amaçlar: (1) MIR JIT register tipi seçimi (Float→MIR_T_D, diğerleri
// →I64, MIRPLAN §3); (2) cam kutu `saqut ir --types`. VM bu alanı kullanmaz (Value
// zaten kind taşır). IR katmanında — ValueKind'a KASITLI bağımsız (ADR-021).
// instruction.hpp'de tanımlı çünkü Instruction::valueType (ADR-039) buna ihtiyaç duyar.
// ----------------------------------------------------------------------------
enum class SlotType : uint8_t { Int, LongInt, Float, Float32, Ref, Str, Decimal, Date, Unknown };

inline const char* slotTypeName(SlotType t) {
    switch (t) {
        case SlotType::Int:     return "int";
        case SlotType::LongInt: return "longint";
        case SlotType::Float:   return "float";
        case SlotType::Float32: return "float32";
        case SlotType::Ref:     return "ref";
        case SlotType::Str:     return "string";
        case SlotType::Decimal: return "decimal";
        case SlotType::Date:    return "date";
        case SlotType::Unknown: return "?";
    }
    return "?";
}

// ----------------------------------------------------------------------------
// Opcode — Sanal Makinenin Anlayacağı İşlem Kodları
// ----------------------------------------------------------------------------
enum class Opcode {

    // --- Değer yükleme ---
    LOAD_CONST,    // slots[dest] = intValue (tam sayı sabitini slota yükle)
                   //   Örnek: LOAD_CONST dest=3 val=10  →  slot[3] = 10

    LOAD_STRING,   // slots[dest] = stringValue (metin sabitini slota yükle)
    LOAD_NULL,     // slots[dest] = null  (ADR-021: ValueKind::Null)
                   //   Örnek: LOAD_STRING dest=2 val="Merhaba"  →  slot[2] = "Merhaba"

    LOAD_SLOT,     // slots[dest] = slots[src]
                   //   Bir slotun değerini başka bir slota kopyalar.
                   //   Atama işlemlerinde (x = y) kullanılır.

    // --- Aritmetik (tümü: slots[dest] = slots[left] OP slots[right]) ---
    ADD,
    SUB,
    MUL,
    DIV,           // UYARI: sıfıra bölme → runtime_error fırlatılır
    MOD,

    // --- Bitsel (tümü: slots[dest] = slots[left] OP slots[right]) ---
    BAND,          // slots[left] & slots[right]
    BOR,           // slots[left] | slots[right]
    BXOR,          // slots[left] ^ slots[right]
    SHL,           // slots[left] << slots[right]
    SHR,           // slots[left] >> slots[right]
    BNOT,          // ~slots[src]  → slots[dest]  (tekli operatör; src kullanır, left/right değil)

    // --- Karşılaştırma (sonuç: 1 = doğru, 0 = yanlış) ---
    LESS,          // slots[left] <  slots[right]
    LESS_EQUAL,    // slots[left] <= slots[right]
    GREATER,       // slots[left] >  slots[right]
    GREATER_EQUAL, // slots[left] >= slots[right]
    EQUAL_EQUAL,   // slots[left] == slots[right]
    NOT_EQUAL,     // slots[left] != slots[right]

    // --- Kontrol akışı ---
    JMP,           // Koşulsuz atlama: ip = jumpTarget
    JIF_FALSE,     // Koşullu atlama:  slots[cond] falsy ise ip = jumpTarget
    JIF_TRUE,      // Koşullu atlama:  slots[cond] truthy ise ip = jumpTarget

    // --- Fonksiyon çağrısı ---
    CALL,          // Başka bir saQut fonksiyonunu çağır.
                   //   Yeni frame açılır, argümanlar parametre slotlarına kopyalanır.
                   //   Fonksiyon RETURN ile bitince sonuç slots[dest]'e yazılır.

    RETURN,        // Bu frame'i kapat, slots[src]'yi caller'a ilet.

    // --- Float aritmetik (#44) ---
    LOAD_FLOAT,    // slots[dest] = floatValue (double sabit yükle)
    FADD,          // slots[dest] = slots[left] + slots[right]  (float)
    FSUB,          // slots[dest] = slots[left] - slots[right]  (float)
    FMUL,          // slots[dest] = slots[left] * slots[right]  (float)
    FDIV,          // slots[dest] = slots[left] / slots[right]  (float; sıfır → runtime_error)
    FNEG,          // slots[dest] = -slots[src]                 (float tekli eksi)
    INT_TO_FLOAT,  // slots[dest] = (double)slots[src]  — gizli int→float çevrimi (literal atamasında)
    FLOAT_TO_INT,  // slots[dest] = (int)slots[src]     — açık cast (ileride: int(x))

    // --- Float32 aritmetik (ADR-040: 32-bit IEEE single, her sonuç (float) truncate) ---
    LOAD_FLOAT32,  // slots[dest] = (float)floatValue (single sabit yükle)
    F32ADD,        // slots[dest] = (float)(slots[left] + slots[right])
    F32SUB,        // slots[dest] = (float)(slots[left] - slots[right])
    F32MUL,        // slots[dest] = (float)(slots[left] * slots[right])
    F32DIV,        // slots[dest] = (float)(slots[left] / slots[right])  (sıfır → runtime_error)
    F32NEG,        // slots[dest] = (float)(-slots[src])
    INT_TO_FLOAT32,   // slots[dest] = (float)slots[src]      — int → float32
    FLOAT32_TO_INT,   // slots[dest] = (int)slots[src]        — float32 → int (checked)
    FLOAT_TO_FLOAT32, // slots[dest] = (float)slots[src]      — double → float (E003 veri kaybı gerçekleşir)
    FLOAT32_TO_FLOAT, // slots[dest] = (double)slots[src]     — float → double (kayıpsız widening)

    // --- LongInt aritmetik (ADR-040: 64-bit signed, tanımlı 2's-complement wrap) ---
    LOAD_LONG,     // slots[dest] = int64Value (64-bit sabit yükle)
    LADD,          // slots[dest] = slots[left] + slots[right]  (longint, uint64 wrap)
    LSUB,          // slots[dest] = slots[left] - slots[right]  (longint)
    LMUL,          // slots[dest] = slots[left] * slots[right]  (longint)
    LDIV,          // slots[dest] = slots[left] / slots[right]  (sıfır → Error; INT64_MIN/-1 → INT64_MIN)
    LMOD,          // slots[dest] = slots[left] % slots[right]  (sıfır → Error; INT64_MIN/-1 → 0)
    LNEG,          // slots[dest] = -slots[src]                 (longint tekli eksi)
    LBAND,         // slots[dest] = slots[left] & slots[right]  (64-bit)
    LBOR,          // slots[dest] = slots[left] | slots[right]  (64-bit)
    LBXOR,         // slots[dest] = slots[left] ^ slots[right]  (64-bit)
    LSHL,          // slots[dest] = slots[left] << slots[right] (64-bit)
    LSHR,          // slots[dest] = slots[left] >> slots[right] (64-bit aritmetik)
    LBNOT,         // slots[dest] = ~slots[src]                 (64-bit)
    INT_TO_LONG,   // slots[dest] = (int64)slots[src]  — int → longint (kayıpsız genişletme)
    LONG_TO_INT_CHECKED, // slots[dest] = (int32)slots[src]; int32 aralığı dışı → fallible

    // --- Struct (ADR-020: referans semantiği) ---
    STRUCT_NEW,  // slots[dest] = yeni StructObject(intValue alan sayısı); functionName = struct tipi adı
    FIELD_GET,   // slots[dest] = slots[src].fields[intValue]  (src=nesne, intValue=alan indeksi)
    FIELD_SET,   // slots[dest].fields[intValue] = slots[right]  (dest=nesne, intValue=alan indeksi, right=değer)

    // --- Array (ADR-020: referans semantiği) ---
    ARRAY_NEW,   // slots[dest] = yeni ArrayObject(intValue eleman kapasitesi)
    ARRAY_GET,   // slots[dest] = slots[left][slots[right]]  — sınır kontrolü
    ARRAY_SET,   // slots[dest][slots[left]] = slots[right]  — sınır kontrolü (dest=dizi, left=idx, right=değer)
    ARRAY_LEN,   // slots[dest] = slots[src].uzunluk()

    // --- Modül-düzeyi değişken erişimi ---
    // "Global" değil: her değişken kendi dosyasına (modülüne) aittir.
    // Başka modüller bu alana doğrudan erişemez; yalnızca export/import ile ulaşabilir.
    // TODO(#modül-scope): IRFunction.moduleId eklenerek çok-modüllü derlemede
    //   her fonksiyonun kendi modülünün slot alanına bakması sağlanacak (bkz. TODO.md).
    LOAD_GLOBAL,   // slots[dest] = moduleSlots[intValue]  (bu modülün modül-düzeyi değişkeni)
    STORE_GLOBAL,  // moduleSlots[intValue] = slots[src]

    // --- String işlemleri (ADR-024: immutable değer-tipi, içerik ==) ---
    STRING_CONCAT, // slots[dest] = slots[left] + slots[right]  (yeni string üretir)

    // --- Hata yönetimi (ADR-025: UNCHECKED try/catch/throw) ---
    ENTER_TRY,  // try bloğuna giriş: TryFrame'i yığına it
                //   dest       = catch bloğundaki Error değerinin yazılacağı slot
                //   jumpTarget = catch bloğunun IR konumu (-1 → backpatch)
                //   callDepth  = VM, callStack.size()'ı kayıt altına alır (unwind için)
    LEAVE_TRY,  // try bloğundan normal çıkış: TryFrame'i çıkar (istisna olmadı)
    THROW,      // slots[src] değerini fırlat → en yakın ENTER_TRY'a unwind
                //   Yakalanmamışsa C++ exception olarak yükseltilir

    // --- Tip dönüşümleri (ADR-026: as operatörü) ---
    // Hatasız dönüşümler:
    CAST_INT_TO_STR,    // slots[dest] = to_string(slots[src])  — int  → string
    CAST_FLOAT_TO_STR,  // slots[dest] = to_string(slots[src])  — float → string
    CAST_BOOL_TO_STR,   // slots[dest] = "true"/"false"          — bool  → string
    // Fallible dönüşümler (left=0 → Error fırlat; left=1 → null döndür):
    CAST_STR_TO_INT,    // slots[dest] = parse_int(slots[src])
    CAST_STR_TO_FLOAT,  // slots[dest] = parse_float(slots[src])
    CAST_FLOAT_TO_INT_CHECKED,  // slots[dest] = (int)slots[src]; NaN/Inf/taşma → fallible
    CAST_INT_TO_BYTE_CHECKED,   // slots[dest] = slots[src]; 0-255 dışı → fallible (#86)
    // ADR-040 longint/float32 string cast'leri:
    CAST_LONG_TO_STR,   // slots[dest] = to_string(int64Value)   — longint → string (hatasız)
    CAST_STR_TO_LONG,   // slots[dest] = parse_int64(slots[src]) — string → longint (fallible)
    CAST_FLOAT32_TO_STR,// slots[dest] = to_string single         — float32 → string (hatasız)
    CAST_STR_TO_FLOAT32,// slots[dest] = (float)parse            — string → float32 (fallible)
    CAST_FLOAT_TO_LONG_CHECKED, // slots[dest] = (int64)slots[src]; NaN/Inf/int64 taşma → fallible

    // --- Decimal aritmetik (ADR-028) ---
    LOAD_DECIMAL,       // slots[dest] = decimalValue (decimal sabit yükle)
    DADD,               // slots[dest] = slots[left] + slots[right]  (decimal)
    DSUB,               // slots[dest] = slots[left] - slots[right]  (decimal)
    DMUL,               // slots[dest] = slots[left] * slots[right]  (decimal)
    DDIV,               // slots[dest] = slots[left] / slots[right]  (sıfır → Error)
    DMOD,               // slots[dest] = slots[left] % slots[right]  (sıfır → Error)
    DNEG,               // slots[dest] = -slots[src]                 (tekli eksi)
    INT_TO_DECIMAL,     // slots[dest] = decimal(slots[src])         — gizli int→decimal terfi
    FLOAT_TO_DECIMAL,   // slots[dest] = decimal(slots[src])         — gizli float→decimal terfi
    // Decimal cast'ler (ADR-026 genişlemesi):
    CAST_DECIMAL_TO_STR,    // slots[dest] = slots[src].toString()         — hatasız
    CAST_DECIMAL_TO_FLOAT,  // slots[dest] = (double)slots[src]            — hatasız
    CAST_DECIMAL_TO_INT,    // slots[dest] = trunc(slots[src])             — fallible (taşma)
    CAST_STR_TO_DECIMAL,    // slots[dest] = decimal::fromString(slots[src]) — fallible

    // --- Dış dünya (FFI — Foreign Function Interface) ---
    CALLHOST,      // Host (C++) fonksiyonunu çağır. Şu an sadece "print" destekli.
                   //   Dönüş değeri yok; sadece yan etki (stdout'a yazmak gibi).
};

// Hata ayıklama ve IR dump için okunabilir isim
inline const char* opcodeName(Opcode op) {
    switch (op) {
        case Opcode::LOAD_CONST:    return "LOAD_CONST";
        case Opcode::LOAD_STRING:   return "LOAD_STRING";
        case Opcode::LOAD_NULL:     return "LOAD_NULL";
        case Opcode::LOAD_SLOT:     return "LOAD_SLOT";
        case Opcode::ADD:           return "ADD";
        case Opcode::SUB:           return "SUB";
        case Opcode::MUL:           return "MUL";
        case Opcode::DIV:           return "DIV";
        case Opcode::MOD:           return "MOD";
        case Opcode::BAND:          return "BAND";
        case Opcode::BOR:           return "BOR";
        case Opcode::BXOR:          return "BXOR";
        case Opcode::SHL:           return "SHL";
        case Opcode::SHR:           return "SHR";
        case Opcode::BNOT:          return "BNOT";
        case Opcode::LOAD_FLOAT:    return "LOAD_FLOAT";
        case Opcode::FADD:          return "FADD";
        case Opcode::FSUB:          return "FSUB";
        case Opcode::FMUL:          return "FMUL";
        case Opcode::FDIV:          return "FDIV";
        case Opcode::FNEG:          return "FNEG";
        case Opcode::INT_TO_FLOAT:  return "INT_TO_FLOAT";
        case Opcode::FLOAT_TO_INT:  return "FLOAT_TO_INT";
        case Opcode::LOAD_FLOAT32:  return "LOAD_FLOAT32";
        case Opcode::F32ADD:        return "F32ADD";
        case Opcode::F32SUB:        return "F32SUB";
        case Opcode::F32MUL:        return "F32MUL";
        case Opcode::F32DIV:        return "F32DIV";
        case Opcode::F32NEG:        return "F32NEG";
        case Opcode::INT_TO_FLOAT32:   return "INT_TO_FLOAT32";
        case Opcode::FLOAT32_TO_INT:   return "FLOAT32_TO_INT";
        case Opcode::FLOAT_TO_FLOAT32: return "FLOAT_TO_FLOAT32";
        case Opcode::FLOAT32_TO_FLOAT: return "FLOAT32_TO_FLOAT";
        case Opcode::LOAD_LONG:     return "LOAD_LONG";
        case Opcode::LADD:          return "LADD";
        case Opcode::LSUB:          return "LSUB";
        case Opcode::LMUL:          return "LMUL";
        case Opcode::LDIV:          return "LDIV";
        case Opcode::LMOD:          return "LMOD";
        case Opcode::LNEG:          return "LNEG";
        case Opcode::LBAND:         return "LBAND";
        case Opcode::LBOR:          return "LBOR";
        case Opcode::LBXOR:         return "LBXOR";
        case Opcode::LSHL:          return "LSHL";
        case Opcode::LSHR:          return "LSHR";
        case Opcode::LBNOT:         return "LBNOT";
        case Opcode::INT_TO_LONG:         return "INT_TO_LONG";
        case Opcode::LONG_TO_INT_CHECKED: return "LONG_TO_INT_CHECKED";
        case Opcode::CAST_INT_TO_STR:         return "CAST_INT_TO_STR";
        case Opcode::CAST_FLOAT_TO_STR:       return "CAST_FLOAT_TO_STR";
        case Opcode::CAST_BOOL_TO_STR:        return "CAST_BOOL_TO_STR";
        case Opcode::CAST_STR_TO_INT:         return "CAST_STR_TO_INT";
        case Opcode::CAST_STR_TO_FLOAT:       return "CAST_STR_TO_FLOAT";
        case Opcode::CAST_FLOAT_TO_INT_CHECKED: return "CAST_FLOAT_TO_INT_CHECKED";
        case Opcode::CAST_INT_TO_BYTE_CHECKED:  return "CAST_INT_TO_BYTE_CHECKED";
        case Opcode::CAST_LONG_TO_STR:        return "CAST_LONG_TO_STR";
        case Opcode::CAST_STR_TO_LONG:        return "CAST_STR_TO_LONG";
        case Opcode::CAST_FLOAT32_TO_STR:     return "CAST_FLOAT32_TO_STR";
        case Opcode::CAST_STR_TO_FLOAT32:     return "CAST_STR_TO_FLOAT32";
        case Opcode::CAST_FLOAT_TO_LONG_CHECKED: return "CAST_FLOAT_TO_LONG_CHECKED";
        case Opcode::LOAD_DECIMAL:          return "LOAD_DECIMAL";
        case Opcode::DADD:                  return "DADD";
        case Opcode::DSUB:                  return "DSUB";
        case Opcode::DMUL:                  return "DMUL";
        case Opcode::DDIV:                  return "DDIV";
        case Opcode::DMOD:                  return "DMOD";
        case Opcode::DNEG:                  return "DNEG";
        case Opcode::INT_TO_DECIMAL:        return "INT_TO_DECIMAL";
        case Opcode::FLOAT_TO_DECIMAL:      return "FLOAT_TO_DECIMAL";
        case Opcode::CAST_DECIMAL_TO_STR:   return "CAST_DECIMAL_TO_STR";
        case Opcode::CAST_DECIMAL_TO_FLOAT: return "CAST_DECIMAL_TO_FLOAT";
        case Opcode::CAST_DECIMAL_TO_INT:   return "CAST_DECIMAL_TO_INT";
        case Opcode::CAST_STR_TO_DECIMAL:   return "CAST_STR_TO_DECIMAL";
        case Opcode::STRUCT_NEW:    return "STRUCT_NEW";
        case Opcode::FIELD_GET:     return "FIELD_GET";
        case Opcode::FIELD_SET:     return "FIELD_SET";
        case Opcode::ARRAY_NEW:     return "ARRAY_NEW";
        case Opcode::ARRAY_GET:     return "ARRAY_GET";
        case Opcode::ARRAY_SET:     return "ARRAY_SET";
        case Opcode::ARRAY_LEN:     return "ARRAY_LEN";
        case Opcode::LOAD_GLOBAL:   return "LOAD_GLOBAL";
        case Opcode::STORE_GLOBAL:  return "STORE_GLOBAL";
        case Opcode::LESS:          return "LESS";
        case Opcode::LESS_EQUAL:    return "LESS_EQUAL";
        case Opcode::GREATER:       return "GREATER";
        case Opcode::GREATER_EQUAL: return "GREATER_EQUAL";
        case Opcode::EQUAL_EQUAL:   return "EQUAL_EQUAL";
        case Opcode::NOT_EQUAL:     return "NOT_EQUAL";
        case Opcode::JMP:           return "JMP";
        case Opcode::JIF_FALSE:     return "JIF_FALSE";
        case Opcode::JIF_TRUE:      return "JIF_TRUE";
        case Opcode::CALL:          return "CALL";
        case Opcode::RETURN:        return "RETURN";
        case Opcode::STRING_CONCAT: return "STRING_CONCAT";
        case Opcode::ENTER_TRY:     return "ENTER_TRY";
        case Opcode::LEAVE_TRY:     return "LEAVE_TRY";
        case Opcode::THROW:         return "THROW";
        case Opcode::CALLHOST:      return "CALLHOST";
    }
    return "UNKNOWN";
}

// ----------------------------------------------------------------------------
// Instruction — Tek bir IR talimatı
//
// Okunabilirlik öncelikli bir tasarım: her talimat TÜM alanları içerir,
// kullanılmayanlar varsayılan değerde (-1 veya boş) kalır.
// Bu yaklaşım bellek israfeder ama her talimatın hangi veriyle çalıştığı
// açıkça görünür — karmaşık union/variant yapısı gerekmez.
// ----------------------------------------------------------------------------
struct Instruction {
    Opcode opcode;

    // Hedef slot — sonucun yazılacağı yer (LOAD_CONST, ADD, CALL vb.)
    int dest       = -1;

    // Kaynak slot — kopyalama veya döndürme için (LOAD_SLOT, RETURN)
    int src        = -1;

    // Aritmetik/karşılaştırma operandları
    int left       = -1;
    int right      = -1;

    // LOAD_CONST için yüklenecek tam sayı sabiti
    int         intValue    =  0;

    // LOAD_LONG için yüklenecek 64-bit tam sayı sabiti (ADR-040)
    long long   int64Value  =  0;

    // LOAD_FLOAT / LOAD_FLOAT32 için yüklenecek double sabiti (#44; float32'de (float) truncate)
    double       floatValue   = 0.0;

    // LOAD_DECIMAL için yüklenecek decimal sabiti (ADR-028)
    DecimalValue decimalValue;

    // LOAD_STRING için yüklenecek metin sabiti (tırnak işaretleri olmadan)
    std::string stringValue;

    // JMP / JIF_FALSE için hedef instruction indeksi
    // Üretim sırasında bilinmiyorsa -1 bırakılır, sonradan doldurulur (backpatch).
    int jumpTarget = -1;

    // JIF_FALSE için kontrol edilecek koşul slotu
    int cond       = -1;

    // CALL / CALLHOST için çağrılacak fonksiyonun adı
    std::string functionName;

    // CALL / CALLHOST için argüman slot indeksleri (sırayla)
    std::vector<int> argSlots;

    // STRUCT_NEW için alan adları (sırasıyla) — toJson/dump'ta kullanılır
    std::vector<std::string> fieldNames;

    // STRUCT_NEW için alan tipleri (sırasıyla) — VM default ref alanlarını
    // gerçek boş string/array olarak başlatır (#184).
    std::vector<Type> fieldTypes;

    // ADR-039: GET-tarafı opcode'ların sonuç/eleman türü — FIELD_GET / ARRAY_GET /
    // LOAD_GLOBAL dest tipi, ARRAY_NEW eleman tipi. IR'de kaybolan tip bilgisini
    // taşır (kaynak heap/global olduğu için opcode'dan türetilemez). finalizeSlotTypes
    // GET dest'ini buradan çözer; JIT register/köprü tipi buradan seçer. SET-tarafı
    // (FIELD_SET/ARRAY_SET/STORE_GLOBAL) gerektirmez — değer slot'undan bilinir.
    SlotType valueType = SlotType::Unknown;

    // Kaynak konum — yalnızca hata-odaklı opcode'larda (CALL, RETURN, THROW,
    // ARRAY_GET/SET, FIELD_SET) set edilir. filePath IRFunction::moduleId'den
    // türetilir; burada sadece satır/sütun tutulur.
    int         sourceLine = 0;
    int         sourceCol  = 0;
    std::string sourceFile;

    // ADR-035 (#76): CALLHOST("__ffi__") için gereken capability — yoksa
    // nullopt. VM'de runtime backstop (B), `saqut ir --capabilities`'te
    // statik raporlama için kullanılır.
    std::optional<Capability> requiredCap;

    explicit Instruction(Opcode op) : opcode(op) {}
};

#endif // SAQUT_IR_INSTRUCTION
