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

static Value caps_drop(const std::vector<Value>& a, HostContext& ctx) {
    auto cap = capabilityFromName(a[0].stringValue);
    if (!cap) throw std::runtime_error("unknown capability '" + a[0].stringValue + "'");
    if (ctx.caps) ctx.caps->erase(*cap);
    return Value::fromInt(0); // void
}
static Value caps_has(const std::vector<Value>& a, HostContext& ctx) {
    auto cap = capabilityFromName(a[0].stringValue);
    if (!cap) throw std::runtime_error("unknown capability '" + a[0].stringValue + "'");
    bool has = ctx.caps && ctx.caps->find(*cap) != ctx.caps->end();
    return Value::fromInt(has ? 1 : 0);
}

// ── fs implementasyonları (#87) ─────────────────────────────────────────────
// v1: yol güvenliği yok (hepsi-ya-hiçbir-şey, --allow-fs). Handle/descriptor
// YOK — tek atımlık read/write (record-replay v1.2.0 önkoşulu, ADR-034 §5).

static Value fs_readFile(const std::vector<Value>& a, HostContext&) {
    std::ifstream f(a[0].stringValue, std::ios::in | std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("cannot open file '" + a[0].stringValue + "'");
    std::ostringstream ss;
    ss << f.rdbuf();
    return Value::fromString(ss.str());
}

static Value fs_writeFile(const std::vector<Value>& a, HostContext&) {
    std::ofstream f(a[0].stringValue, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f.is_open())
        throw std::runtime_error("cannot open file '" + a[0].stringValue + "' for writing");
    f << a[1].stringValue;
    return Value::fromInt(0); // void
}

static Value fs_append(const std::vector<Value>& a, HostContext&) {
    std::ofstream f(a[0].stringValue, std::ios::out | std::ios::binary | std::ios::app);
    if (!f.is_open())
        throw std::runtime_error("cannot open file '" + a[0].stringValue + "' for writing");
    f << a[1].stringValue;
    return Value::fromInt(0); // void
}

static Value fs_readBytes(const std::vector<Value>& a, HostContext& ctx) {
    std::ifstream f(a[0].stringValue, std::ios::in | std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("cannot open file '" + a[0].stringValue + "'");
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string bytes = ss.str();

    ArrayObject* arr = ctx.heap->allocArray((int)bytes.size(), ArrayElemKind::Byte);
    arr->bytes.resize(bytes.size());
    for (size_t i = 0; i < bytes.size(); ++i)
        arr->bytes[i] = (uint8_t)bytes[i];
    return Value::fromRef(arr);
}

static Value fs_writeBytes(const std::vector<Value>& a, HostContext&) {
    if (a[1].kind != ValueKind::Ref || !a[1].ref)
        throw std::runtime_error("writeBytes: expected byte[]");
    auto* arr = static_cast<ArrayObject*>(a[1].ref);
    std::ofstream f(a[0].stringValue, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f.is_open())
        throw std::runtime_error("cannot open file '" + a[0].stringValue + "' for writing");
    if (arr->elemKind == ArrayElemKind::Byte) {
        f.write(reinterpret_cast<const char*>(arr->bytes.data()), arr->bytes.size());
    } else {
        for (const Value& v : arr->elements)
            f.put(static_cast<char>(v.intValue & 0xFF));
    }
    return Value::fromInt(0); // void
}

static Value fs_exists(const std::vector<Value>& a, HostContext&) {
    return Value::fromInt(std::filesystem::exists(a[0].stringValue) ? 1 : 0);
}

static Value fs_remove(const std::vector<Value>& a, HostContext&) {
    std::error_code ec;
    bool removed = std::filesystem::remove(a[0].stringValue, ec);
    if (ec) throw std::runtime_error("cannot remove '" + a[0].stringValue + "': " + ec.message());
    if (!removed) throw std::runtime_error("file not found: '" + a[0].stringValue + "'");
    return Value::fromInt(0); // void
}

// ── sys implementasyonları (#90) ────────────────────────────────────────────
// Non-deterministik/dış-durum-okuyan — --allow-sys. Kaynak: OS CSPRNG
// (std::random_device), rand() DEĞİL.

static Value sys_random(const std::vector<Value>&, HostContext&) {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return Value::fromFloat(dist(gen));
}

static Value sys_randomInt(const std::vector<Value>& a, HostContext&) {
    int lo = a[0].intValue, hi = a[1].intValue;
    if (lo >= hi)
        throw std::runtime_error("randomInt: invalid range [" + std::to_string(lo) +
                                  ", " + std::to_string(hi) + ")");
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int> dist(lo, hi - 1);
    return Value::fromInt(dist(gen));
}

static Value sys_env(const std::vector<Value>& a, HostContext&) {
    const char* v = std::getenv(a[0].stringValue.c_str());
    return v ? Value::fromString(std::string(v)) : Value::null();
}

static Value sys_sleep(const std::vector<Value>& a, HostContext&) {
    std::this_thread::sleep_for(std::chrono::milliseconds(a[0].intValue));
    return Value::fromInt(0); // void
}

static Value sys_args(const std::vector<Value>&, HostContext& ctx) {
    const auto& args = ctx.programArgs ? *ctx.programArgs : std::vector<std::string>{};
    ArrayObject* arr = ctx.heap->allocArray((int)args.size());
    for (const auto& s : args)
        arr->elements.push_back(Value::fromString(s));
    return Value::fromRef(arr);
}

// ── date implementasyonları (#88, ADR-035) ──────────────────────────────────
// Yalnızca now() capability ister (--allow-sys); geri kalan saf hesap.
// ⚠️ v1 kısıtı: saQut'ta 64-bit int yok — fromEpochMillis/toEpochMillis
// `int` (32-bit) taşır, epoch-ms günümüz tarihleri için bunu aşar (bilinen
// sınır, ADR-035'te belgelenir). date DEĞERİNİN kendisi (Value::int64Value)
// tam hassasiyetlidir; year/month/day/addX/diffMillis bu yüzden güvenlidir.

static Value date_now(const std::vector<Value>&, HostContext&) {
    auto now = std::chrono::system_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch()).count();
    return Value::fromDate(ms);
}

static Value date_fromEpochMillis(const std::vector<Value>& a, HostContext&) {
    return Value::fromDate((long long)a[0].intValue);
}

static Value date_toEpochMillis(const std::vector<Value>& a, HostContext&) {
    return Value::fromInt((int)a[0].int64Value);
}

static Value date_addDays(const std::vector<Value>& a, HostContext&) {
    return Value::fromDate(a[0].int64Value + (long long)a[1].intValue * 86400000LL);
}
static Value date_addHours(const std::vector<Value>& a, HostContext&) {
    return Value::fromDate(a[0].int64Value + (long long)a[1].intValue * 3600000LL);
}
static Value date_addMinutes(const std::vector<Value>& a, HostContext&) {
    return Value::fromDate(a[0].int64Value + (long long)a[1].intValue * 60000LL);
}
static Value date_addSeconds(const std::vector<Value>& a, HostContext&) {
    return Value::fromDate(a[0].int64Value + (long long)a[1].intValue * 1000LL);
}

static Value date_year(const std::vector<Value>& a, HostContext&) {
    return Value::fromInt(date_calc::breakDown(a[0].int64Value).y);
}
static Value date_month(const std::vector<Value>& a, HostContext&) {
    return Value::fromInt((int)date_calc::breakDown(a[0].int64Value).mo);
}
static Value date_day(const std::vector<Value>& a, HostContext&) {
    return Value::fromInt((int)date_calc::breakDown(a[0].int64Value).d);
}
static Value date_hour(const std::vector<Value>& a, HostContext&) {
    return Value::fromInt((int)date_calc::breakDown(a[0].int64Value).h);
}
static Value date_minute(const std::vector<Value>& a, HostContext&) {
    return Value::fromInt((int)date_calc::breakDown(a[0].int64Value).mi);
}
static Value date_second(const std::vector<Value>& a, HostContext&) {
    return Value::fromInt((int)date_calc::breakDown(a[0].int64Value).s);
}

static Value date_diffMillis(const std::vector<Value>& a, HostContext&) {
    return Value::fromInt((int)(a[0].int64Value - a[1].int64Value));
}

// "2026-07-12T10:00:00Z" — v1 yalnızca UTC (ADR-035); başka format → null.
static Value date_parse(const std::vector<Value>& a, HostContext&) {
    const std::string& s = a[0].stringValue;
    int y, mo, d, h, mi, se;
    char zChar = 0;
    if (s.size() != 20) return Value::null();
    if (std::sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d%c",
                     &y, &mo, &d, &h, &mi, &se, &zChar) != 7 || zChar != 'Z')
        return Value::null();
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || se > 59)
        return Value::null();
    long long ms = date_calc::assemble(y, (unsigned)mo, (unsigned)d,
                                        (unsigned)h, (unsigned)mi, (unsigned)se);
    return Value::fromDate(ms);
}

// pattern alt kümesi: yyyy MM dd HH mm ss (ADR-035'te sabitlenir)
static Value date_format(const std::vector<Value>& a, HostContext&) {
    auto b = date_calc::breakDown(a[0].int64Value);
    char buf[16];
    std::string out;
    const std::string& pat = a[1].stringValue;
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
    return Value::fromString(out);
}

// ── Tablo (index = sayısal host id) ──────────────────────────────────────────
// Sıra değişebilir; root.sqt sembolik ad kullandığı için etkilenmez.

const std::vector<HostFn>& hostFnTable() {
    static const std::vector<HostFn> table = {
        // #222: math ailesi natif thunk'a taşındı (hostNativeThunks). Eski
        // tabloda impl == nullptr olarak DURUR — sembolik id ve arite tek
        // kaynak olarak burada kalsın, indeksler kaymasın diye. Dispatch
        // rt_host_call'da natif tabloya gider.
        { "MATH_ABS",   1, nullptr },
        { "MATH_ABSF",  1, nullptr },
        { "MATH_MIN",   2, nullptr },
        { "MATH_MAX",   2, nullptr },
        { "MATH_MINF",  2, nullptr },
        { "MATH_MAXF",  2, nullptr },
        { "MATH_SQRT",  1, nullptr },
        { "MATH_POW",   2, nullptr },
        { "MATH_FLOOR", 1, nullptr },
        { "MATH_CEIL",  1, nullptr },
        { "MATH_ROUND", 1, nullptr },
        { "MATH_PI",    0, nullptr },
        { "MATH_E",     0, nullptr },
        { "CAPS_DROP",  1, caps_drop  },
        { "CAPS_HAS",   1, caps_has   },
        { "FS_READ_FILE",   1, fs_readFile   },
        { "FS_WRITE_FILE",  2, fs_writeFile  },
        { "FS_APPEND",      2, fs_append     },
        { "FS_READ_BYTES",  1, fs_readBytes  },
        { "FS_WRITE_BYTES", 2, fs_writeBytes },
        { "FS_EXISTS",      1, fs_exists     },
        { "FS_REMOVE",      1, fs_remove     },
        { "SYS_RANDOM",     0, sys_random    },
        { "SYS_RANDOM_INT", 2, sys_randomInt },
        { "SYS_ENV",        1, sys_env       },
        { "SYS_SLEEP",      1, sys_sleep     },
        { "SYS_ARGS",       0, sys_args      },
        { "DATE_NOW",             0, date_now             },
        { "DATE_FROM_EPOCH_MS",   1, date_fromEpochMillis },
        { "DATE_TO_EPOCH_MS",     1, date_toEpochMillis   },
        { "DATE_ADD_DAYS",        2, date_addDays         },
        { "DATE_ADD_HOURS",       2, date_addHours        },
        { "DATE_ADD_MINUTES",     2, date_addMinutes      },
        { "DATE_ADD_SECONDS",     2, date_addSeconds      },
        { "DATE_YEAR",             1, date_year           },
        { "DATE_MONTH",            1, date_month          },
        { "DATE_DAY",              1, date_day            },
        { "DATE_HOUR",             1, date_hour           },
        { "DATE_MINUTE",           1, date_minute         },
        { "DATE_SECOND",           1, date_second         },
        { "DATE_DIFF_MS",          2, date_diffMillis     },
        { "DATE_PARSE",            1, date_parse          },
        { "DATE_FORMAT",           2, date_format         },
        { "CORE_VERSION",        0, [](const std::vector<Value>&, HostContext&) {
            return Value::fromString(SAQUT_VERSION); }},
    };
    return table;
}

// ── Natif thunk tablosu (#222) ───────────────────────────────────────────────
//
// Yeni ABI'ye taşınmış gövdeler. hostFnTable() ile AYNI sembolik id uzayını
// paylaşır: bir id burada varsa rt_host_call natif yolu kullanır, yoksa eski
// gövdeye (sarmalayıcı üzerinden) düşer.
//
// Geçiş bu şekilde aile aile yapılabiliyor — her adım kendi başına yeşil test
// bırakıyor ve hız kazancı ölçülebiliyor.
const std::vector<HostNativeFn>& hostNativeThunks() {
    static const std::vector<HostNativeFn> table = {
        { "MATH_ABS",   math_abs   },
        { "MATH_ABSF",  math_absf  },
        { "MATH_MIN",   math_min   },
        { "MATH_MAX",   math_max   },
        { "MATH_MINF",  math_minf  },
        { "MATH_MAXF",  math_maxf  },
        { "MATH_SQRT",  math_sqrt  },
        { "MATH_POW",   math_pow   },
        { "MATH_FLOOR", math_floor },
        { "MATH_CEIL",  math_ceil  },
        { "MATH_ROUND", math_round },
        { "MATH_PI",    math_PI    },
        { "MATH_E",     math_E     },
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

Value callHostFn(int id, const std::vector<Value>& args, HostContext& ctx) {
    const auto& t = hostFnTable();
    if (id < 0 || id >= (int)t.size() || !t[id].impl) return Value::null();
    return t[id].impl(args, ctx);
}
