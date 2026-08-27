// ============================================================================
// saQut — JIT Shadow Stack (GC kök görünürlüğü)
// ============================================================================
//
// DİZİN:   src/gc/shadow_stack.hpp
// KATMAN:  GC — JIT register'larının GC'ye görünen yansıması
//
// SORUN (#228):
//   GC canlılığı KÖKLERDEN tarar. Kökler bugün iki yerde: globalSlots_ ve
//   Interpreter::callStack_ frame'leri. JIT çalışırken VM'in callStack_'i
//   BOŞTUR — değerler MIR register'larındadır ve GC oraya bakamaz.
//
//   Sonuç: JIT'te heap nesnesi üretilirse (array, struct, string) GC onu
//   ulaşılamaz sanır ve CANLIYKEN siler. Bu yüzden bugüne dek:
//     - array/struct opcode'ları JIT'te reddediliyor (SlotType::Ref)
//     - heap gerektiren host çağrıları reddediliyor (HOST_NEEDS_HEAP)
//     - JIT string'leri GC'ye HİÇ bağlanmadı; kendi unique_ptr tamponunda
//       süresiz birikiyor (ölçüldü: 200k concat → JIT 21,8 MB / VM 6,8 MB)
//
// ÇÖZÜM:
//   JIT, referans tutan slot'larını bu diziye yansıtır. GC toplama sırasında
//   diziyi de kök olarak tarar. Böylece JIT register'ındaki bir nesne
//   "ulaşılabilir" sayılır.
//
//   Yansıtma noktaları (JIT codegen'de rt_jit_shadow_set çağrısı):
//     - Ref/Str/Decimal üreten her opcode dest'ini yazar
//     - Fonksiyon girişinde frame açılır, çıkışında kapanır
//
// NEDEN "SHADOW":
//   Gerçek yığın MIR register'larıdır; bu dizi onun GC'ye görünen gölgesidir.
//   Kesin (precise) GC'nin standart tekniğidir — muhafazakâr yığın taraması
//   (conservative scanning) yerine seçilmiştir çünkü saQut'un GC'si taşımasız
//   olsa da deterministik olmak zorundadır (ADR-022): muhafazakâr tarama
//   "yanlışlıkla canlı" nesneler bırakır ve toplama zamanı programa göre
//   değişir.
//
// TEK İŞ PARÇACIĞI varsayımı kaldırıldı: depo thread_local'dır (bkz.
// shadow_stack.cpp). Her iş parçacığı kendi JIT kök dizisini tutar; JIT
// çalışma bağlamının geri kalanı mir_backend.cpp içindeki JitRuntime/rt()
// altında toplanmıştır — oradaki THREAD NOTU ile aynı model.
// ============================================================================

#ifndef SAQUT_VM_SHADOW_STACK
#define SAQUT_VM_SHADOW_STACK

#include <cstdint>
#include <vector>

struct Object;

// JIT'in canlı referansları. GC bunu kök olarak tarar.
//
// Düz bir dizi: JIT slot'ları çakışmasın diye her fonksiyon girişinde taban
// (base) kaydedilir ve çıkışta geri sarılır — çağrı yığını gibi davranır.
struct ShadowStack {
    std::vector<Object*> slots;

    // Fonksiyon girişi: mevcut tepe döndürülür (çıkışta buraya geri sarılır).
    int enter() const { return (int)slots.size(); }

    // Fonksiyon çıkışı: girişte alınan tabana geri sar.
    void leave(int base) {
        if (base >= 0 && base <= (int)slots.size())
            slots.resize((size_t)base);
    }

    // Bir referansı görünür kıl. Aynı slot birden çok kez yazılabilir
    // (her atamada tazelenir); dizide yer ayrılmamışsa büyütülür.
    void set(int index, Object* obj) {
        if (index < 0) return;
        if ((size_t)index >= slots.size()) slots.resize((size_t)index + 1, nullptr);
        slots[(size_t)index] = obj;
    }

    void clear() { slots.clear(); }
    bool empty() const { return slots.empty(); }
};

// İş parçacığı başına tek örnek (depo thread_local — shadow_stack.cpp).
// JIT codegen buraya yazar; JitRootSource (mir_backend.cpp) toplama sırasında
// bunu kök olarak Heap'e bildirir.
ShadowStack& jitShadowStack();

#endif // SAQUT_VM_SHADOW_STACK
