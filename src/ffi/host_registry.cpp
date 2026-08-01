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
#include "ffi/host_functions.hpp"

namespace {

// ── Kayıt metadata'sı ────────────────────────────────────────────────────────
//
// Dönüş türü ve bayraklar. İkisi de gövdeden OTOMATİK türetilemez:
//   - retKind: eski gövde Value döner, statik türü kaybolmuştur (fs::exists
//     Value::fromInt döner ama root.sqt'te `bool`, sys::sleep `void`).
//   - flags:   gövdenin HostEnv'e dokunup dokunmadığı ancak okunarak bilinir.
//
// Dönüş türleri root.sqt'teki `ffi` bildirimleriyle ÖRTÜŞMELİ — tek kaynak
// orasıdır. test_host_abi bu tabloyu root.sqt'e karşı çapraz doğrular; drift
// olursa test kırılır, sessiz yanlış dispatch olmaz.
//
// HOST_PURE burada "HostEnv gerekmez + heap'e dokunmaz" demektir; dış dünyayı
// okumak (fs::exists, sys::env) bunu bozmaz — bkz. host_abi.hpp bayrak notu.
struct HostMeta {
    const char* symbolicId;
    HostKind    retKind;
    uint8_t     flags;
};

const HostMeta kHostMeta[] = {
    // core
    {"CORE_VERSION",       HostKind::Str,     HOST_PURE},
    // math — tamamı saf hesap (#89: IEEE754 korunur, throw yok)
    {"MATH_ABS",           HostKind::Int,     HOST_PURE},
    {"MATH_ABSF",          HostKind::Float,   HOST_PURE},
    {"MATH_MIN",           HostKind::Int,     HOST_PURE},
    {"MATH_MAX",           HostKind::Int,     HOST_PURE},
    {"MATH_MINF",          HostKind::Float,   HOST_PURE},
    {"MATH_MAXF",          HostKind::Float,   HOST_PURE},
    {"MATH_SQRT",          HostKind::Float,   HOST_PURE},
    {"MATH_POW",           HostKind::Float,   HOST_PURE},
    {"MATH_FLOOR",         HostKind::Float,   HOST_PURE},
    {"MATH_CEIL",          HostKind::Float,   HOST_PURE},
    {"MATH_ROUND",         HostKind::Float,   HOST_PURE},
    {"MATH_PI",            HostKind::Float,   HOST_PURE},
    {"MATH_E",             HostKind::Float,   HOST_PURE},
    // caps — VM capability kümesini okur/değiştirir (ADR-035)
    {"CAPS_DROP",          HostKind::Void,    HOST_NEEDS_CAPS | HOST_CAN_FAIL},
    {"CAPS_HAS",           HostKind::Int,     HOST_NEEDS_CAPS | HOST_CAN_FAIL},
    // fs — byte[] döndüren readBytes heap'te array tahsis eder
    {"FS_READ_FILE",       HostKind::Str,     HOST_CAN_FAIL},
    {"FS_READ_BYTES",      HostKind::Ref,     HOST_NEEDS_HEAP | HOST_CAN_FAIL},
    {"FS_WRITE_FILE",      HostKind::Void,    HOST_CAN_FAIL},
    {"FS_WRITE_BYTES",     HostKind::Void,    HOST_CAN_FAIL},
    {"FS_APPEND",          HostKind::Void,    HOST_CAN_FAIL},
    {"FS_EXISTS",          HostKind::Int,     HOST_PURE},
    {"FS_REMOVE",          HostKind::Void,    HOST_CAN_FAIL},
    // sys — args() string[] tahsis eder ve programArgs okur
    {"SYS_RANDOM",         HostKind::Float32, HOST_PURE},
    {"SYS_RANDOM_INT",     HostKind::Int,     HOST_CAN_FAIL},
    {"SYS_ENV",            HostKind::Str,     HOST_PURE},
    {"SYS_SLEEP",          HostKind::Void,    HOST_PURE},
    {"SYS_ARGS",           HostKind::Ref,     HOST_NEEDS_HEAP | HOST_NEEDS_ARGS},
    // date — saf hesap (date_calc.hpp), epoch-ms üzerinde
    {"DATE_NOW",           HostKind::Date,    HOST_PURE},
    {"DATE_FROM_EPOCH_MS", HostKind::Date,    HOST_PURE},
    {"DATE_TO_EPOCH_MS",   HostKind::Int,     HOST_PURE},
    {"DATE_ADD_DAYS",      HostKind::Date,    HOST_PURE},
    {"DATE_ADD_HOURS",     HostKind::Date,    HOST_PURE},
    {"DATE_ADD_MINUTES",   HostKind::Date,    HOST_PURE},
    {"DATE_ADD_SECONDS",   HostKind::Date,    HOST_PURE},
    {"DATE_YEAR",          HostKind::Int,     HOST_PURE},
    {"DATE_MONTH",         HostKind::Int,     HOST_PURE},
    {"DATE_DAY",           HostKind::Int,     HOST_PURE},
    {"DATE_HOUR",          HostKind::Int,     HOST_PURE},
    {"DATE_MINUTE",        HostKind::Int,     HOST_PURE},
    {"DATE_SECOND",        HostKind::Int,     HOST_PURE},
    {"DATE_DIFF_MS",       HostKind::Int,     HOST_PURE},
    // parse başarısızlıkta null döner (date?) — hata DEĞİL, ADR-021
    {"DATE_PARSE",         HostKind::Date,    HOST_PURE},
    {"DATE_FORMAT",        HostKind::Str,     HOST_PURE},
};

const HostMeta* findMeta(const char* symbolicId) {
    for (const auto& m : kHostMeta)
        if (std::strcmp(m.symbolicId, symbolicId) == 0) return &m;
    return nullptr;
}

uint8_t hostFlagsFor(const char* symbolicId) {
    const HostMeta* m = findMeta(symbolicId);
    // Metadata eksikse KORUMACI davran: env gerekebilir + hata verebilir.
    // Yanlış yönde hata yapmak (gereksiz safepoint) sessiz bozulmadan iyidir.
    return m ? m->flags
             : (uint8_t)(HOST_NEEDS_HEAP | HOST_NEEDS_CAPS | HOST_NEEDS_ARGS | HOST_CAN_FAIL);
}

HostKind hostRetKindFor(const char* symbolicId) {
    const HostMeta* m = findMeta(symbolicId);
    return m ? m->retKind : HostKind::Void;
}

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
            e.flags      = hostFlagsFor(legacy[i].symbolicId);
            e.retKind    = hostRetKindFor(legacy[i].symbolicId);
            // Natif thunk varsa doğrudan bağla — sıcak yolda arama yok.
            // Yoksa nullptr kalır ve rt_host_call eski gövdeye sarmalayıcıyla
            // düşer (geçiş sürüyor).
            e.thunk = nullptr;
            for (const auto& n : hostNativeThunks())
                if (std::strcmp(n.symbolicId, legacy[i].symbolicId) == 0) {
                    e.thunk = n.thunk;
                    break;
                }
            t[kHostFnBase + i] = e;
        }
        return t;
    }();
    return table;
}

std::vector<std::string> hostEntriesMissingMetadata() {
    std::vector<std::string> missing;
    for (const auto& fn : hostFnTable())
        if (!findMeta(fn.symbolicId)) missing.push_back(fn.symbolicId);
    return missing;
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

    if (entryId >= kHostFnBase && entryId < kBuiltinBase) {
        // Sıcak yol: natif thunk bağlıysa doğrudan çağır — Value'ya hiç
        // uğramaz, çağrı başına tahsis yok.
        const auto& t = hostRegistry();
        if (entryId < (int32_t)t.size() && t[static_cast<size_t>(entryId)].thunk)
            return t[static_cast<size_t>(entryId)].thunk(f);
        // Geçiş yolu: gövde henüz taşınmadı, sarmalayıcıyla çağır.
        return callLegacyHostFn(entryId - kHostFnBase, f);
    }

    // Built-in ve çekirdek blokları adım 4–5'te bağlanır.
    f->err.set("host entry bagli degil: " + std::to_string(entryId), "E_FFI");
    return 1;
}
