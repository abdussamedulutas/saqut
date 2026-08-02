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
#include <unordered_map>
#include "ir/instruction.hpp"
#include "ir/ir_cfg.hpp"
#include "core/module_registry.hpp"

// SlotType tanımı instruction.hpp'ye taşındı (Instruction::valueType için gerekli,
// ADR-039); buradan include ile gelir.

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
    // ADR-021 + #221: slot indeksi → bu slot `T?` (nullable) mı? slotTypes ile
    // aynı boyutta. Boşsa tümü non-nullable varsayılır.
    //
    // NEDEN AYRI ALAN: slotTypes bir slot'un DEĞER türünü söyler (Int/Float/...),
    // ama null'u temsil edemez — bir Int register'ında 64 bitin tamamı geçerli
    // değerdir, 0 ile null ayrılamaz. VM'de bu sorun yok (Value ayrıca `kind`
    // taşır), fakat JIT ham register kullanır. Bu maske, JIT'in nullable slot
    // başına gizli bir "isNull" yandaş register'ı ayırmasını sağlar.
    //
    // Bu bilgi olmadan LOAD_NULL JIT'te SESSİZCE YANLIŞ cevap verirdi:
    // `int? a = 0;` için `a == null` true dönerdi (VM false der).
    std::vector<bool>        slotNullable;
    // Faz 5: (sourceLine) → ilk instruction IP indeksi (breakpoint eşlemesi için)
    std::unordered_map<int, int> lineToFirstIP;
    CFG cfg;

    // #218: struct alan adları (Instruction'dan tasındı)
    std::unordered_map<std::string, std::vector<std::string>> structFieldNames;
    // ADR-021 zero-init: struct tipi → hangi alanların nullable (`T?`) olduğu.
    // structFieldNames ile aynı sırada, alan başına bir bayrak. STRUCT_NEW
    // sırasında VM bu maskeyi okuyup nullable alanları Int(0) yerine null ile
    // başlatır — aksi halde `s.f == null` sessizce false döner.
    std::unordered_map<std::string, std::vector<bool>> structFieldNullable;

    IRFunction(std::string name, int paramCount)
        : name(std::move(name)), paramCount(paramCount), slotCount(0) {}

    // Okunabilir IR dump — "saqut run" hata ayıklaması veya inceleme için
    void dump() const;
};

#endif // SAQUT_IR_FUNCTION
