// ============================================================================
// saQut FFI — Birleşik Host Registry (gerçekleme)
//
// ADIM 2 KURALI (#222): mevcut gövdeler DEĞİŞMEZ. Bu dosya onları yeni ABI'ye
// SARAR. Böylece adım 2'de davranış değişikliği olmaz ve golden testler
// sarmalayıcının doğruluğunu kanıtlar. Gövdelerin kendisi adım 3–4'te
// yeni imzaya taşınır.
// ============================================================================

#include "ffi/host_registry.hpp"

#include <stdexcept>

#include "ffi/host_bridge.hpp"
#include "ffi/host_functions.hpp"

namespace {

// ── Adım 2 köprüsü: eski HostFn gövdesini yeni HostThunk imzasına sar ────────
//
// Eski gövde: Value(const std::vector<Value>&, HostContext&) — ve std::runtime_error
//             fırlatabilir.
// Yeni imza:  int(HostCallFrame*) — hata f->err'e yazılır.
//
// Sarmalayıcı çeviriyi ve exception yakalamayı yapar. Exception buradan öteye
// GEÇMEZ: bu, ABI'nin "backend'e C++ unwind sızmaz" garantisinin uygulandığı
// tek nokta.
int callLegacyHostFn(int legacyId, HostCallFrame* f) {
    const auto& table = hostFnTable();
    if (legacyId < 0 || legacyId >= (int)table.size() || !table[legacyId].impl) {
        f->err.set("gecersiz host fonksiyon id: " + std::to_string(legacyId), "E_FFI");
        return 1;
    }

    // HostSlot → Value (eski gövde Value bekliyor).
    std::vector<Value> args;
    args.reserve(static_cast<size_t>(f->argc));
    for (int32_t i = 0; i < f->argc; ++i)
        args.push_back(fromHostSlot(f->args[i]));

    HostContext ctx{
        f->env ? f->env->caps        : nullptr,
        f->env ? f->env->programArgs : nullptr,
        f->env ? f->env->heap        : nullptr,
    };

    if (!f->retOwner) {
        // Sözleşme ihlali: çağıran ömür sahibini bağlamamış. Sessizce sarkan
        // pointer üretmektense açık hata.
        f->err.set("host cagri cercevesinde retOwner bagli degil", "E_FFI");
        return 1;
    }

    try {
        hostSetRetValue(*f, table[legacyId].impl(args, ctx));
        return 0;
    } catch (const std::runtime_error& e) {
        f->err.set(e.what(), "E_FFI");
        return 1;
    }
}

}  // namespace

// ── Tablo ────────────────────────────────────────────────────────────────────

const std::vector<HostEntry>& hostRegistry() {
    static const std::vector<HostEntry> table = [] {
        std::vector<HostEntry> t;
        t.resize(static_cast<size_t>(kCoreBase) + 8, HostEntry{nullptr, 0, 0, HostKind::Void, nullptr});

        // Blok 1: gömülü host fonksiyonları. Adım 2'de eski tablodan
        // TÜRETİLİR — sembolik ad ve arite oradan gelir, gövde sarmalanır.
        // Böylece iki tablo arasında drift imkânsız (tek kaynak korunur).
        const auto& legacy = hostFnTable();
        for (size_t i = 0; i < legacy.size(); ++i) {
            HostEntry e;
            e.symbolicId = legacy[i].symbolicId;
            e.arity      = static_cast<int8_t>(legacy[i].arity);
            // Adım 2'de bayraklar korumacı: hepsi env gerektirebilir sayılır.
            // Adım 3'te gövde başına gerçek bayraklar konur (HOST_PURE olanlar
            // JIT'te safepoint gerektirmeyecek).
            e.flags      = HOST_NEEDS_HEAP | HOST_NEEDS_CAPS | HOST_NEEDS_ARGS;
            e.retKind    = HostKind::Void;  // adım 3'te gerçek dönüş türü
            e.thunk      = nullptr;         // dispatch rt_host_call'da özel
            t[kHostFnBase + i] = e;
        }
        return t;
    }();
    return table;
}

int32_t hostEntryIndex(const std::string& symbolicId) {
    // Gömülü ffi blokları için eski çözümü kullan — root.sqt ↔ C++ tek kaynak.
    int legacy = hostFnIndex(symbolicId);
    if (legacy >= 0) return kHostFnBase + legacy;
    return kHostIdInvalid;
}

const HostEntry* hostEntryAt(int32_t id) {
    const auto& t = hostRegistry();
    if (id < 0 || id >= (int32_t)t.size()) return nullptr;
    const HostEntry& e = t[static_cast<size_t>(id)];
    return e.symbolicId ? &e : nullptr;
}

// ── Tek giriş noktası ────────────────────────────────────────────────────────

extern "C" int rt_host_call(int32_t entryId, HostCallFrame* f) {
    if (!f) return 1;
    f->err.clear();

    if (entryId >= kHostFnBase && entryId < kBuiltinBase)
        return callLegacyHostFn(entryId - kHostFnBase, f);

    // Built-in ve çekirdek blokları adım 4–5'te bağlanır.
    f->err.set("host entry bagli degil: " + std::to_string(entryId), "E_FFI");
    return 1;
}
