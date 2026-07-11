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

#include <string>
#include <vector>
#include <unordered_map>
#include "ir/instruction.hpp"
#include "core/module_registry.hpp"

struct IRFunction {
    std::string              name;       // kaynak koddaki fonksiyon adı
    int                      moduleId = ModuleRegistry::INVALID_ID; // ait olduğu modülün ID'si (registry'den)
    int                      paramCount;   // kaç parametresi var
    int                      slotCount;    // frame boyutu (üretim sonunda doldurulur)
    std::vector<Instruction> instructions; // bu fonksiyonun talimat listesi
    // Faz 5: slot indeksi → değişken adı (debug/DAP için). Geçici slotlar boş string.
    std::vector<std::string> slotNames;
    // Faz 5: (sourceLine) → ilk instruction IP indeksi (breakpoint eşlemesi için)
    std::unordered_map<int, int> lineToFirstIP;

    IRFunction(std::string name, int paramCount)
        : name(std::move(name)), paramCount(paramCount), slotCount(0) {}

    // Okunabilir IR dump — "saqut run" hata ayıklaması veya inceleme için
    void dump() const;
};

#endif // SAQUT_IR_FUNCTION
