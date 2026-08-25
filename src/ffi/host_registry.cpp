// ============================================================================
// saQut FFI — Birleşik Host Registry (gerçekleme)
//
// ADIM 2 KURALI (#222): mevcut gövdeler DEĞİŞMEZ. Bu dosya onları yeni ABI'ye
// SARAR. Böylece adım 2'de davranış değişikliği olmaz ve golden testler
// sarmalayıcının doğruluğunu kanıtlar. Gövdelerin kendisi adım 3–4'te
// yeni imzaya taşınır.
// ============================================================================

#include "ffi/host_registry.hpp"

#include <cstring>
#include <stdexcept>

#include "ffi/host_bridge.hpp"
#include "data/data_registry.hpp"
#include "data/date.hpp"
#include "ffi/host_functions.hpp"

// ── Birleştirici ─────────────────────────────────────────────────────────────
//
// #229: kayıt birliği — çapraz metadata tablosu (kHostMeta) ve legacy şekil
// yok. Host fonksiyonlarının TEK tanımı hostFnTable()'da (math/fs/sys/
// date_now/core) ve dataDateFunctions()'da (date 15) durur; ikisi de thunk'ının
// yanında TAM HostEntry taşır. Bu fonksiyon yalnızca onları tek indeks
// uzayına YERLEŞTİRİR (assembler).
//
// Yerleşim sırası: hostFnTable (0..25) ardından date (26..40) — blok içi
// düzen korunur; tüm çözümler semboliktir (hostEntryIndex), elle sayı yok.

const std::vector<HostEntry>& hostRegistry() {
    static const std::vector<HostEntry> table = [] {
        std::vector<HostEntry> t;
        t.resize(static_cast<size_t>(kCoreBase) + 8, HostEntry{nullptr, 0, 0, HostKind::Void, nullptr});

        // Blok 1: gömülü host fonksiyonları — tek kaynak, tam kayıt.
        const auto& fns = hostFnTable();
        for (size_t i = 0; i < fns.size(); ++i) {
            HostEntry e;
            e.symbolicId = fns[i].symbolicId;
            e.arity      = static_cast<int8_t>(fns[i].arity);
            e.flags      = fns[i].flags;
            e.retKind    = fns[i].retKind;
            e.thunk      = fns[i].thunk;
            t[kHostFnBase + i] = e;
        }
        // date: src/data/date.cpp — thunk'ının yanında tam kayıt.
        const auto& dates = dataDateFunctions();
        for (size_t i = 0; i < dates.size(); ++i) {
            HostEntry e;
            e.symbolicId = dates[i].symbolicId;
            e.arity      = static_cast<int8_t>(dates[i].arity);
            e.flags      = dates[i].flags;
            e.retKind    = dates[i].retKind;
            e.thunk      = dates[i].thunk;
            t[kHostFnBase + fns.size() + i] = e;
        }

        // Blok 2: built-in metodlar (#223). Kayıtlar src/data/ modüllerinden
        // gelir — imza ve gövde orada AYNI kayıtta durur, bu yüzden burada
        // yalnızca indeks uzayına yerleştirme yapılır.
        const auto& methods = dataAllMethods();
        for (size_t i = 0; i < methods.size(); ++i) {
            HostEntry e;
            e.symbolicId = methods[i].name;
            e.arity      = static_cast<int8_t>(methods[i].params.size());
            e.flags      = methods[i].flags;
            e.retKind    = methods[i].retKind;
            e.thunk      = methods[i].thunk;
            t[kBuiltinBase + i] = e;
        }
        return t;
    }();
    return table;
}

int32_t hostEntryIndex(const std::string& symbolicId) {
    // TEK lookup: birleşik registry'nin blok-1 aralığında (0..kBuiltinBase)
    // sembolik adı ara. Bulunan kaydın indeksi kHostFnBase + sıradır —
    // root.sqt sembolik ad kullandığı için yerleşim serbesttir.
    const auto& t = hostRegistry();
    const int32_t limit = std::min<int32_t>(kBuiltinBase, (int32_t)t.size());
    for (int32_t i = 0; i < limit; ++i)
        if (t[static_cast<size_t>(i)].symbolicId &&
            symbolicId == t[static_cast<size_t>(i)].symbolicId)
            return i;
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

    // Sıcak yol: doğrudan thunk. Value'ya hiç uğramaz, çağrı başına tahsis yok.
    const auto& t = hostRegistry();
    if (entryId >= 0 && entryId < (int32_t)t.size()) {
        HostThunk thunk = t[static_cast<size_t>(entryId)].thunk;
        if (thunk) return thunk(f);
    }

    // Bağlı olmayan bir id sessizce yanlış fonksiyona gitmez — açık hata döner.
    f->err.set("host entry bagli degil: " + std::to_string(entryId), "E_HOST");
    return 1;
}
