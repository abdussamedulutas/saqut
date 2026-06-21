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

#include <string>
#include <vector>

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
        case Opcode::CAST_INT_TO_STR:         return "CAST_INT_TO_STR";
        case Opcode::CAST_FLOAT_TO_STR:       return "CAST_FLOAT_TO_STR";
        case Opcode::CAST_BOOL_TO_STR:        return "CAST_BOOL_TO_STR";
        case Opcode::CAST_STR_TO_INT:         return "CAST_STR_TO_INT";
        case Opcode::CAST_STR_TO_FLOAT:       return "CAST_STR_TO_FLOAT";
        case Opcode::CAST_FLOAT_TO_INT_CHECKED: return "CAST_FLOAT_TO_INT_CHECKED";
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

    // LOAD_FLOAT için yüklenecek double sabiti (#44)
    double      floatValue  = 0.0;

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

    // Kaynak konum — yalnızca hata-odaklı opcode'larda (CALL, RETURN, THROW,
    // ARRAY_GET/SET, FIELD_SET) set edilir. filePath IRFunction::moduleId'den
    // türetilir; burada sadece satır/sütun tutulur.
    int sourceLine = 0;
    int sourceCol  = 0;

    explicit Instruction(Opcode op) : opcode(op) {}
};

#endif // SAQUT_IR_INSTRUCTION
