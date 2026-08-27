// ============================================================================
// saQut IR — Slot Liveness Analizi
// ============================================================================
//
// DİZİN:   src/ir/ir_liveness.hpp
// KATMAN:  IR — slot-bazlı canlılık veri akışı (GC/threading altyapısı)
//
// AMAÇ:
//   "Talimat i ÇALIŞTIRILMADAN ÖNCE hangi slotlar canlı?" sorusunu cevaplar.
//   GC köklerinin daraltılmasının (kullanılmayan değişkenin kök olmaması,
//   docs/gc-threading-altyapi-denetimi.md A2.1) ve ileride slot-DCE'nin tek
//   veri kaynağıdır.
//
// YÖNTEM:
//   Önce her blok için "blokta yazılmadan okunanlar" ve "blokta yazılanlar"
//   özeti çıkarılır; sonra bu özetler blokların sonundan başına doğru
//   birleştirilir (art arda turlarla, sonuç değişmeyene dek) ve son olarak
//   her blok içinde talimat talimat geriye yürünerek "bu talimat çalışmadan
//   önce hangi slotlar canlı" kümesi üretilir. Slot temsili SSA değil —
//   slot-bazlı talimat listelerinde standart yaklaşımdır.
//
// GÜVENLİK SINIRI (exact=false):
//   Fonksiyon ENTER_TRY içeriyorsa analiz MUHAFAZAKÂR moda düşer: her slot
//   her noktada canlı sayılır. Neden: try bölgesindeki HERHANGİ bir talimat
//   runtime hatası ile catch bloğuna unwinding yapabilir; catch'in hangi
//   slotlara ihtiyaç duyduğunu talimat seviyesinde bilmek için try-bölgesi
//   analizi gerekir (henüz yok). Yanlış "ölü" işareti canlı nesnenin GC
//   tarafından silinmesi demek olduğundan, muhafazakâr taraf güvenli taraftır.
//   Try-bölgesi analizi eklenince daraltılır — geriye dönük kırılma olmaz.
// ============================================================================

#ifndef SAQUT_IR_LIVENESS
#define SAQUT_IR_LIVENESS

#include <vector>
#include "ir/ir_function.hpp"

struct SlotLiveness {
    int  slotCount = 0;
    bool exact     = true;  // false: ENTER_TRY var → her slot her yerde canlı

    // liveBefore[i] = talimat i çalıştırılmadan önce canlı slot'lar
    // (bitmask yerine bayt dizisi; slotCount küçüktür, okunabilirlik öncelik).
    // exact=false iken boş bırakılır — isLiveBefore her zaman true döner.
    std::vector<std::vector<char>> liveBefore;

    bool isLiveBefore(int ip, int slot) const {
        if (slot < 0 || slot >= slotCount) return false;
        if (!exact) return true;
        if (ip < 0 || ip >= (int)liveBefore.size()) return false;
        return liveBefore[(size_t)ip][(size_t)slot] != 0;
    }

    // Talimat ip'den önce canlı slot sayısı (gözlem/istatistik).
    int liveCountBefore(int ip) const {
        if (!exact) return slotCount;
        if (ip < 0 || ip >= (int)liveBefore.size()) return 0;
        int n = 0;
        for (char c : liveBefore[(size_t)ip]) n += (c != 0);
        return n;
    }
};

// Bir fonksiyonun slot liveness'ını hesapla. CFG fn.cfg üzerinde kurulur;
// fn.cfg boşsa burada kurulur (analiz sonucu fn.cfg'ye yazılmaz — analiz
// türetilmiş görünümdür, sahipliği çağıranındır).
SlotLiveness computeSlotLiveness(const IRFunction& fn);

#endif // SAQUT_IR_LIVENESS
