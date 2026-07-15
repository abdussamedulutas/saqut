// ============================================================================
// saQut IR — IRFunction (Tek Fonksiyonun IR Karşılığı)
//
// Bir IRFunction, kaynak koddaki tek bir fonksiyonun "pişmiş" halidir.
// IRGenerator bu yapıyı doldurur, Interpreter bu yapıyı çalıştırır.
//
// SLOT DÜZENI:
//   slot[0 .. paramCount-1]  →  parametreler (soldan sağa)
//   slot[paramCount ..]      →  lokal değişkenler ve geçici sonuçlar
//   slotCount                →  toplam kaç slot lazım (frame boyutu)
//
// Örnek — fibonacci(int n):
//   paramCount = 1          →  slot[0] = n
//   slotCount  = 11         →  slot[0..10] (0'ı parametre, 1-10 hesaplamalar)
// ============================================================================

#ifndef SAQUT_IR_FUNCTION
#define SAQUT_IR_FUNCTION

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "ir/instruction.hpp"
#include "core/module_registry.hpp"

// SlotType — bir slot'un statik değer türü. saQut'ta bir slot ÇALIŞMA ZAMANINDA
// tip değiştirmez (ADR-020; tip denetleyici garanti eder), bu yüzden slot başına
// tek bir tür yeterli. İki amaca hizmet eder:
//   1. MIR JIT (Dilim 1.5+): register tipi seçimi — Float → MIR_T_D, diğerleri
//      → MIR_T_I64 (MIRPLAN §3). VM bu alanı kullanmaz (Value zaten kind taşır).
//   2. Cam kutu: `saqut ir --types` slot türlerini dökebilir.
// IR katmanında tutulur — VM'in ValueKind'ına KASITLI olarak bağımlı değil
// (backend, frontend'in çözdüğü tipi devralır, yeniden türetmez — ADR-021).
enum class SlotType : uint8_t { Int, Float, Ref, Str, Decimal, Date, Unknown };

inline const char* slotTypeName(SlotType t) {
    switch (t) {
        case SlotType::Int:     return "int";
        case SlotType::Float:   return "float";
        case SlotType::Ref:     return "ref";
        case SlotType::Str:     return "string";
        case SlotType::Decimal: return "decimal";
        case SlotType::Date:    return "date";
        case SlotType::Unknown: return "?";
    }
    return "?";
}

struct IRFunction {
    std::string              name;       // kaynak koddaki fonksiyon adı
    int                      moduleId = ModuleRegistry::INVALID_ID; // ait olduğu modülün ID'si (registry'den)
    int                      paramCount;   // kaç parametresi var
    int                      slotCount;    // frame boyutu (üretim sonunda doldurulur)
    std::vector<Instruction> instructions; // bu fonksiyonun talimat listesi
    // Faz 5: slot indeksi → değişken adı (debug/DAP için). Geçici slotlar boş string.
    std::vector<std::string> slotNames;
    // Dilim 1.5 (MIRPLAN §3): slot indeksi → statik tür. slotCount ile aynı
    // boyutta (IRGenerator::finalizeSlotTypes doldurur). Boşsa (eski yol) tümü
    // Int varsayılır. JIT register tiplemesi + `saqut ir --types` bunu okur.
    std::vector<SlotType>    slotTypes;
    // Faz 5: (sourceLine) → ilk instruction IP indeksi (breakpoint eşlemesi için)
    std::unordered_map<int, int> lineToFirstIP;

    IRFunction(std::string name, int paramCount)
        : name(std::move(name)), paramCount(paramCount), slotCount(0) {}

    // Okunabilir IR dump — "saqut run" hata ayıklaması veya inceleme için
    void dump() const;
};

#endif // SAQUT_IR_FUNCTION
