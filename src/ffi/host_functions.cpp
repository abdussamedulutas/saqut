// ============================================================================
// saQut FFI — Host Fonksiyon Registry Gerçeklemesi
// ============================================================================
//
// math modülü (ADR-034, #107; issue #89): saf hesap, capability'siz.
// Overload YOK → int/float ayrımı isimle (abs/absf, min/minf, max/maxf).
// IEEE754 korunur: sqrt(-1) NaN döner, Error FIRLATMAZ (#89).
// ============================================================================

#include "ffi/host_functions.hpp"
#include "ffi/date_calc.hpp"
#include "ffi/host_bridge.hpp"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_map>

// ── math implementasyonları ─────────────────────────────────────────────────

// #222: math ailesi NATİF thunk'tır — Value'ya hiç uğramaz.
//
// Bu ailenin sarmalayıcıdan çıkarılması ölçülebilir: sarmalayıcı her çağrıda
// bir std::vector<Value> tahsis eder ve HostSlot→Value→HostSlot çift
// dönüşümü yapar (80 baytlık Value'lar). Natif thunk doğrudan HostSlot okur.
//
// Hepsi HOST_PURE: env istemez, heap'e dokunmaz, hata döndürmez
// (#89: IEEE754 korunur — sqrt(-1) NaN döner, Error FIRLATMAZ).
static int math_abs(HostCallFrame* f) {
    f->ret = HostSlot::fromInt(std::abs(static_cast<int>(hostAsI64(f->args[0]))));
    return 0;
}
static int math_absf(HostCallFrame* f) {
    f->ret = HostSlot::fromFloat(std::fabs(hostAsDouble(f->args[0])));
    return 0;
}
static int math_min(HostCallFrame* f) {
    f->ret = HostSlot::fromInt(static_cast<int>(
        std::min(hostAsI64(f->args[0]), hostAsI64(f->args[1]))));
    return 0;
}
static int math_max(HostCallFrame* f) {
    f->ret = HostSlot::fromInt(static_cast<int>(
        std::max(hostAsI64(f->args[0]), hostAsI64(f->args[1]))));
    return 0;
}
static int math_minf(HostCallFrame* f) {
    f->ret = HostSlot::fromFloat(std::fmin(hostAsDouble(f->args[0]), hostAsDouble(f->args[1])));
    return 0;
}
static int math_maxf(HostCallFrame* f) {
    f->ret = HostSlot::fromFloat(std::fmax(hostAsDouble(f->args[0]), hostAsDouble(f->args[1])));
    return 0;
}
static int math_sqrt(HostCallFrame* f) {
    f->ret = HostSlot::fromFloat(std::sqrt(hostAsDouble(f->args[0])));  // sqrt(-1) → NaN
    return 0;
}
static int math_pow(HostCallFrame* f) {
    f->ret = HostSlot::fromFloat(std::pow(hostAsDouble(f->args[0]), hostAsDouble(f->args[1])));
    return 0;
}
static int math_floor(HostCallFrame* f) {
    f->ret = HostSlot::fromFloat(std::floor(hostAsDouble(f->args[0])));
    return 0;
}
static int math_ceil(HostCallFrame* f) {
    f->ret = HostSlot::fromFloat(std::ceil(hostAsDouble(f->args[0])));
    return 0;
}
static int math_round(HostCallFrame* f) {
    f->ret = HostSlot::fromFloat(std::round(hostAsDouble(f->args[0])));
    return 0;
}
// #89: sabit yok — ffi bildirimi yalnızca fonksiyon; PI/E sıfır-argümanlı
// saf fonksiyon olarak sunulur (import {PI, E} from math; PI();).
static int math_PI(HostCallFrame* f) {
    f->ret = HostSlot::fromFloat(3.14159265358979323846);
    return 0;
}
static int math_E(HostCallFrame* f) {
    f->ret = HostSlot::fromFloat(2.71828182845904523536);
    return 0;
}

// ── caps implementasyonları (#91, ADR-035) ──────────────────────────────────
// drop/has caps::drop kendisi capability istemez (izin düşürmek her zaman
// serbest); VM'nin gerçek caps_ kümesine ctx.caps üzerinden dokunur.

static int caps_drop(HostCallFrame* f) {
    const std::string& name = hostAsString(f->args[0]);
    auto cap = capabilityFromName(name);
    if (!cap) { f->err.set("unknown capability '" + name + "'", "E_FFI"); return 1; }
    if (f->env && f->env->caps) f->env->caps->erase(*cap);
    f->ret = HostSlot::voidVal();
    return 0;
}
static int caps_has(HostCallFrame* f) {
    const std::string& name = hostAsString(f->args[0]);
    auto cap = capabilityFromName(name);
    if (!cap) { f->err.set("unknown capability '" + name + "'", "E_FFI"); return 1; }
    bool has = f->env && f->env->caps && f->env->caps->find(*cap) != f->env->caps->end();
    f->ret = HostSlot::fromInt(has ? 1 : 0);
    return 0;
}

// ── fs implementasyonları (#87) ─────────────────────────────────────────────
// v1: yol güvenliği yok (hepsi-ya-hiçbir-şey, --allow-fs). Handle/descriptor
// YOK — tek atımlık read/write (record-replay v1.2.0 önkoşulu, ADR-034 §5).

static int fs_readFile(HostCallFrame* fr) {
    const std::string& path = hostAsString(fr->args[0]);
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f.is_open()) { fr->err.set("cannot open file '" + path + "'", "E_FFI"); return 1; }
    std::ostringstream ss;
    ss << f.rdbuf();
    hostSetRetString(*fr, ss.str());
    return 0;
}

static int fs_writeFile(HostCallFrame* fr) {
    const std::string& path = hostAsString(fr->args[0]);
    std::ofstream f(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f.is_open()) { fr->err.set("cannot open file '" + path + "' for writing", "E_FFI"); return 1; }
    f << hostAsString(fr->args[1]);
    fr->ret = HostSlot::voidVal();
    return 0;
}

static int fs_append(HostCallFrame* fr) {
    const std::string& path = hostAsString(fr->args[0]);
    std::ofstream f(path, std::ios::out | std::ios::binary | std::ios::app);
    if (!f.is_open()) { fr->err.set("cannot open file '" + path + "' for writing", "E_FFI"); return 1; }
    f << hostAsString(fr->args[1]);
    fr->ret = HostSlot::voidVal();
    return 0;
}

static int fs_readBytes(HostCallFrame* fr) {
    const std::string& path = hostAsString(fr->args[0]);
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f.is_open()) { fr->err.set("cannot open file '" + path + "'", "E_FFI"); return 1; }
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string bytes = ss.str();

    if (!fr->env || !fr->env->heap) {
        fr->err.set("readBytes: heap yok", "E_FFI");
        return 1;
    }
    ArrayObject* arr = fr->env->heap->allocArray((int)bytes.size(), ArrayElemKind::Byte);
    arr->bytes.resize(bytes.size());
    for (size_t i = 0; i < bytes.size(); ++i)
        arr->bytes[i] = (uint8_t)bytes[i];
    fr->ret = HostSlot::fromRef(arr);
    return 0;
}

static int fs_writeBytes(HostCallFrame* fr) {
    if (fr->args[1].kind != HostKind::Ref || !fr->args[1].p) {
        fr->err.set("writeBytes: expected byte[]", "E_FFI");
        return 1;
    }
    auto* arr = static_cast<ArrayObject*>(fr->args[1].p);
    const std::string& path = hostAsString(fr->args[0]);
    std::ofstream f(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f.is_open()) { fr->err.set("cannot open file '" + path + "' for writing", "E_FFI"); return 1; }
    if (arr->elemKind == ArrayElemKind::Byte) {
        f.write(reinterpret_cast<const char*>(arr->bytes.data()), arr->bytes.size());
    } else {
        for (const Value& v : arr->elements)
            f.put(static_cast<char>(v.intValue & 0xFF));
    }
    fr->ret = HostSlot::voidVal();
    return 0;
}

static int fs_exists(HostCallFrame* f) {
    f->ret = HostSlot::fromInt(std::filesystem::exists(hostAsString(f->args[0])) ? 1 : 0);
    return 0;
}

static int fs_remove(HostCallFrame* f) {
    const std::string& path = hostAsString(f->args[0]);
    std::error_code ec;
    bool removed = std::filesystem::remove(path, ec);
    if (ec)       { f->err.set("cannot remove '" + path + "': " + ec.message(), "E_FFI"); return 1; }
    if (!removed) { f->err.set("file not found: '" + path + "'", "E_FFI"); return 1; }
    f->ret = HostSlot::voidVal();
    return 0;
}

// ── sys implementasyonları (#90) ────────────────────────────────────────────
// Non-deterministik/dış-durum-okuyan — --allow-sys. Kaynak: OS CSPRNG
// (std::random_device), rand() DEĞİL.

static int sys_random(HostCallFrame* f) {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    // Eski davranış Value::fromFloat (double) idi — root.sqt `float` yazsa da
    // gözlemlenen çıktı double biçimidir. Birebir korunur.
    f->ret = HostSlot::fromFloat(dist(gen));
    return 0;
}

static int sys_randomInt(HostCallFrame* f) {
    int lo = (int)hostAsI64(f->args[0]), hi = (int)hostAsI64(f->args[1]);
    if (lo >= hi) {
        f->err.set("randomInt: invalid range [" + std::to_string(lo) +
                   ", " + std::to_string(hi) + ")", "E_FFI");
        return 1;
    }
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int> dist(lo, hi - 1);
    f->ret = HostSlot::fromInt(dist(gen));
    return 0;
}

static int sys_env(HostCallFrame* f) {
    const char* v = std::getenv(hostAsString(f->args[0]).c_str());
    // Tanımsız değişken null döner (root.sqt: `string?`) — hata DEĞİL.
    if (!v) { f->ret = HostSlot::null(); return 0; }
    hostSetRetString(*f, std::string(v));
    return 0;
}

static int sys_sleep(HostCallFrame* f) {
    std::this_thread::sleep_for(std::chrono::milliseconds(hostAsI64(f->args[0])));
    f->ret = HostSlot::voidVal();
    return 0;
}

static int sys_args(HostCallFrame* f) {
    if (!f->env || !f->env->heap) { f->err.set("args: heap yok", "E_FFI"); return 1; }
    // Eski kod burada `ctx.programArgs ? *ctx.programArgs : std::vector{}`
    // yazıyordu — programArgs null iken GEÇİCİ bir vector'e referans bağlayan
    // sarkan referanstı. Boş tablo doğrudan ele alınır.
    static const std::vector<std::string> kNoArgs;
    const auto& args = f->env->programArgs ? *f->env->programArgs : kNoArgs;
    ArrayObject* arr = f->env->heap->allocArray((int)args.size());
    for (const auto& s : args)
        arr->elements.push_back(Value::fromString(s));
    f->ret = HostSlot::fromRef(arr);
    return 0;
}

// ── date implementasyonları (#88, ADR-035) ──────────────────────────────────
// Yalnızca now() capability ister (--allow-sys); geri kalan saf hesap.
// ⚠️ v1 kısıtı: saQut'ta 64-bit int yok — fromEpochMillis/toEpochMillis
// `int` (32-bit) taşır, epoch-ms günümüz tarihleri için bunu aşar (bilinen
// sınır, ADR-035'te belgelenir). date DEĞERİNİN kendisi (Value::int64Value)
// tam hassasiyetlidir; year/month/day/addX/diffMillis bu yüzden güvenlidir.

static int date_now(HostCallFrame* f) {
    auto now = std::chrono::system_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch()).count();
    f->ret = HostSlot::fromDate(ms);
    return 0;
}

static int date_fromEpochMillis(HostCallFrame* f) {
    f->ret = HostSlot::fromDate(hostAsI64(f->args[0]));
    return 0;
}

static int date_toEpochMillis(HostCallFrame* f) {
    // root.sqt'te dönüş `int` — daraltma KASITLI ve eski davranışla birebir.
    f->ret = HostSlot::fromInt((int)hostAsI64(f->args[0]));
    return 0;
}

static int date_addDays(HostCallFrame* f) {
    f->ret = HostSlot::fromDate(hostAsI64(f->args[0]) + hostAsI64(f->args[1]) * 86400000LL);
    return 0;
}
static int date_addHours(HostCallFrame* f) {
    f->ret = HostSlot::fromDate(hostAsI64(f->args[0]) + hostAsI64(f->args[1]) * 3600000LL);
    return 0;
}
static int date_addMinutes(HostCallFrame* f) {
    f->ret = HostSlot::fromDate(hostAsI64(f->args[0]) + hostAsI64(f->args[1]) * 60000LL);
    return 0;
}
static int date_addSeconds(HostCallFrame* f) {
    f->ret = HostSlot::fromDate(hostAsI64(f->args[0]) + hostAsI64(f->args[1]) * 1000LL);
    return 0;
}

static int date_year(HostCallFrame* f) {
    f->ret = HostSlot::fromInt(date_calc::breakDown(hostAsI64(f->args[0])).y);
    return 0;
}
static int date_month(HostCallFrame* f) {
    f->ret = HostSlot::fromInt((int)date_calc::breakDown(hostAsI64(f->args[0])).mo);
    return 0;
}
static int date_day(HostCallFrame* f) {
    f->ret = HostSlot::fromInt((int)date_calc::breakDown(hostAsI64(f->args[0])).d);
    return 0;
}
static int date_hour(HostCallFrame* f) {
    f->ret = HostSlot::fromInt((int)date_calc::breakDown(hostAsI64(f->args[0])).h);
    return 0;
}
static int date_minute(HostCallFrame* f) {
    f->ret = HostSlot::fromInt((int)date_calc::breakDown(hostAsI64(f->args[0])).mi);
    return 0;
}
static int date_second(HostCallFrame* f) {
    f->ret = HostSlot::fromInt((int)date_calc::breakDown(hostAsI64(f->args[0])).s);
    return 0;
}

static int date_diffMillis(HostCallFrame* f) {
    // root.sqt dönüşü `int` — daraltma eski davranışla birebir korunur.
    f->ret = HostSlot::fromInt((int)(hostAsI64(f->args[0]) - hostAsI64(f->args[1])));
    return 0;
}

// "2026-07-12T10:00:00Z" — v1 yalnızca UTC (ADR-035); başka format → null.
static int date_parse(HostCallFrame* f) {
    const std::string& s = hostAsString(f->args[0]);
    int y, mo, d, h, mi, se;
    char zChar = 0;
    // Geçersiz girdi null döner — HATA DEĞİL (root.sqt dönüşü `date?`,
    // ADR-021). rt_host_call 0 döner, err boş kalır.
    if (s.size() != 20) { f->ret = HostSlot::null(); return 0; }
    if (std::sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d%c",
                     &y, &mo, &d, &h, &mi, &se, &zChar) != 7 || zChar != 'Z')
        { f->ret = HostSlot::null(); return 0; }
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || se > 59)
        { f->ret = HostSlot::null(); return 0; }
    long long ms = date_calc::assemble(y, (unsigned)mo, (unsigned)d,
                                        (unsigned)h, (unsigned)mi, (unsigned)se);
    f->ret = HostSlot::fromDate(ms);
    return 0;
}

// pattern alt kümesi: yyyy MM dd HH mm ss (ADR-035'te sabitlenir)
static int date_format(HostCallFrame* f) {
    auto b = date_calc::breakDown(hostAsI64(f->args[0]));
    char buf[16];
    std::string out;
    const std::string& pat = hostAsString(f->args[1]);
    size_t i = 0;
    auto matches = [&](const char* tok) {
        size_t n = std::string(tok).size();
        return pat.compare(i, n, tok) == 0;
    };
    while (i < pat.size()) {
        if (matches("yyyy")) { std::snprintf(buf, sizeof buf, "%04d", b.y); out += buf; i += 4; }
        else if (matches("MM")) { std::snprintf(buf, sizeof buf, "%02u", b.mo); out += buf; i += 2; }
        else if (matches("dd")) { std::snprintf(buf, sizeof buf, "%02u", b.d); out += buf; i += 2; }
        else if (matches("HH")) { std::snprintf(buf, sizeof buf, "%02u", b.h); out += buf; i += 2; }
        else if (matches("mm")) { std::snprintf(buf, sizeof buf, "%02u", b.mi); out += buf; i += 2; }
        else if (matches("ss")) { std::snprintf(buf, sizeof buf, "%02u", b.s); out += buf; i += 2; }
        else { out += pat[i]; ++i; }
    }
    // String dönüşü ömür sahibi üzerinden bağlanır (bkz. HostRetOwner).
    hostSetRetString(*f, std::move(out));
    return 0;
}

// core — derleyici sürümü (SAQUT_VERSION derleme zamanında gömülür)
static int core_version(HostCallFrame* f) {
    hostSetRetString(*f, SAQUT_VERSION);
    return 0;
}

// ── Tablo (index = sayısal host id) ──────────────────────────────────────────
// Sıra değişebilir; root.sqt sembolik ad kullandığı için etkilenmez.

const std::vector<HostFn>& hostFnTable() {
    static const std::vector<HostFn> table = {
        // #222: math ailesi natif thunk'a taşındı (hostNativeThunks). Eski
        // tabloda impl == nullptr olarak DURUR — sembolik id ve arite tek
        // kaynak olarak burada kalsın, indeksler kaymasın diye. Dispatch
        // rt_host_call'da natif tabloya gider.
        { "MATH_ABS", 1, math_abs },
        { "MATH_ABSF", 1, math_absf },
        { "MATH_MIN", 2, math_min },
        { "MATH_MAX", 2, math_max },
        { "MATH_MINF", 2, math_minf },
        { "MATH_MAXF", 2, math_maxf },
        { "MATH_SQRT", 1, math_sqrt },
        { "MATH_POW", 2, math_pow },
        { "MATH_FLOOR", 1, math_floor },
        { "MATH_CEIL", 1, math_ceil },
        { "MATH_ROUND", 1, math_round },
        { "MATH_PI", 0, math_PI },
        { "MATH_E", 0, math_E },
        { "CAPS_DROP", 1, caps_drop },
        { "CAPS_HAS", 1, caps_has },
        { "FS_READ_FILE", 1, fs_readFile },
        { "FS_WRITE_FILE", 2, fs_writeFile },
        { "FS_APPEND", 2, fs_append },
        { "FS_READ_BYTES", 1, fs_readBytes },
        { "FS_WRITE_BYTES", 2, fs_writeBytes },
        { "FS_EXISTS", 1, fs_exists },
        { "FS_REMOVE", 1, fs_remove },
        { "SYS_RANDOM", 0, sys_random },
        { "SYS_RANDOM_INT", 2, sys_randomInt },
        { "SYS_ENV", 1, sys_env },
        { "SYS_SLEEP", 1, sys_sleep },
        { "SYS_ARGS", 0, sys_args },
        { "DATE_NOW", 0, date_now },
        { "DATE_FROM_EPOCH_MS", 1, date_fromEpochMillis },
        { "DATE_TO_EPOCH_MS", 1, date_toEpochMillis },
        { "DATE_ADD_DAYS", 2, date_addDays },
        { "DATE_ADD_HOURS", 2, date_addHours },
        { "DATE_ADD_MINUTES", 2, date_addMinutes },
        { "DATE_ADD_SECONDS", 2, date_addSeconds },
        { "DATE_YEAR", 1, date_year },
        { "DATE_MONTH", 1, date_month },
        { "DATE_DAY", 1, date_day },
        { "DATE_HOUR", 1, date_hour },
        { "DATE_MINUTE", 1, date_minute },
        { "DATE_SECOND", 1, date_second },
        { "DATE_DIFF_MS", 2, date_diffMillis },
        { "DATE_PARSE", 1, date_parse },
        { "DATE_FORMAT", 2, date_format },
        { "CORE_VERSION", 0, core_version },
    };
    return table;
}



int hostFnIndex(const std::string& symbolicId) {
    static const std::unordered_map<std::string, int> index = [] {
        std::unordered_map<std::string, int> m;
        const auto& t = hostFnTable();
        for (int i = 0; i < (int)t.size(); ++i) m[t[i].symbolicId] = i;
        return m;
    }();
    auto it = index.find(symbolicId);
    return it != index.end() ? it->second : -1;
}

