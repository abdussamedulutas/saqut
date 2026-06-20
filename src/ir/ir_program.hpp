// ============================================================================
// saQut IR — IRProgram (Bir .sqt Dosyasının Tüm IR İçeriği)
//
// IRProgram, üretilen tüm fonksiyonları tutar.
// Interpreter programı çalıştırmak için bu yapıyı kullanır.
//
// NEDEN İKİ YAPIDA TUTUYORUZ?
//   - functionOrder: fonksiyonları tanımlandıkları sırayla tutar (dump için)
//   - functions (unordered_map): CALL instruction'larında isimle hızlı arama için
//
// NOT: unordered_map değer semantiğiyle (IRFunction by value) tutar.
//   findFunction() bir pointer döndürür — bu pointer tüm addFunction() çağrıları
//   bittikten sonra alınmalıdır. Interpreter program üretildikten sonra çalıştığı
//   için bu kural otomatik olarak sağlanır.
// ============================================================================

#ifndef SAQUT_IR_PROGRAM
#define SAQUT_IR_PROGRAM

#include <string>
#include <unordered_map>
#include <vector>
#include "ir/ir_function.hpp"

struct IRProgram {
    // Fonksiyon adı → IRFunction (hızlı arama için)
    std::unordered_map<std::string, IRFunction> functions;

    // Ekleme sırası (dump'ta orijinal sırayla göstermek için)
    std::vector<std::string> functionOrder;

    // Global değişkenler (LOAD_GLOBAL / STORE_GLOBAL için)
    int                      globalCount = 0;
    std::vector<std::string> globalNames; // index → isim (dump için)

    // Yeni fonksiyon ekle
    void addFunction(IRFunction fn) {
        functionOrder.push_back(fn.name);
        functions.emplace(fn.name, std::move(fn));
    }

    // İsimle ara — bulunamazsa nullptr döner
    IRFunction* findFunction(const std::string& name) {
        auto it = functions.find(name);
        return (it != functions.end()) ? &it->second : nullptr;
    }

    // Tüm fonksiyonları ekleme sırasıyla yazdır
    void dump() const;
};

#endif // SAQUT_IR_PROGRAM
