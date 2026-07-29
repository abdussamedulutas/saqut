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

static Value math_abs(const std::vector<Value>& a, HostContext&) {
    return Value::fromInt(std::abs(a[0].intValue));
}
static Value math_absf(const std::vector<Value>& a, HostContext&) {
    return Value::fromFloat(std::fabs(a[0].floatValue));
}
static Value math_min(const std::vector<Value>& a, HostContext&) {
    return Value::fromInt(std::min(a[0].intValue, a[1].intValue));
}
static Value math_max(const std::vector<Value>& a, HostContext&) {
    return Value::fromInt(std::max(a[0].intValue, a[1].intValue));
}
static Value math_minf(const std::vector<Value>& a, HostContext&) {
    return Value::fromFloat(std::fmin(a[0].floatValue, a[1].floatValue));
}
static Value math_maxf(const std::vector<Value>& a, HostContext&) {
    return Value::fromFloat(std::fmax(a[0].floatValue, a[1].floatValue));
}
static Value math_sqrt(const std::vector<Value>& a, HostContext&) {
    return Value::fromFloat(std::sqrt(a[0].floatValue));   // sqrt(-1) → NaN (Error yok)
}
static Value math_pow(const std::vector<Value>& a, HostContext&) {
    return Value::fromFloat(std::pow(a[0].floatValue, a[1].floatValue));
}
static Value math_floor(const std::vector<Value>& a, HostContext&) {
    return Value::fromFloat(std::floor(a[0].floatValue));
}
static Value math_ceil(const std::vector<Value>& a, HostContext&) {
    return Value::fromFloat(std::ceil(a[0].floatValue));
}
static Value math_round(const std::vector<Value>& a, HostContext&) {
    return Value::fromFloat(std::round(a[0].floatValue));
}
// #89: sabit yok — ffi bildirimi yalnızca fonksiyon; PI/E sıfır-argümanlı
// saf fonksiyon olarak sunulur (import {PI, E} from math; PI();).
static Value math_PI(const std::vector<Value>&, HostContext&) {
    return Value::fromFloat(3.14159265358979323846);
}
static Value math_E(const std::vector<Value>&, HostContext&) {
    return Value::fromFloat(2.71828182845904523536);
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

// ── path implementasyonları (#177) ──────────────────────────────────────────
// Yalnız std::filesystem::path'in lexical API'leri kullanılır. Bu fonksiyonlar
// exists/status/canonical/current_path gibi filesystem veya cwd gözlemi yapmaz.

static Value path_join(const std::vector<Value>& a, HostContext&) {
    auto joined = (std::filesystem::path(a[0].stringValue) /
                   std::filesystem::path(a[1].stringValue)).lexically_normal();
    return Value::fromString(joined.string());
}

static Value path_normalize(const std::vector<Value>& a, HostContext&) {
    return Value::fromString(std::filesystem::path(a[0].stringValue)
                                 .lexically_normal()
                                 .string());
}

static Value path_dirname(const std::vector<Value>& a, HostContext&) {
    return Value::fromString(std::filesystem::path(a[0].stringValue)
                                 .parent_path()
                                 .string());
}

static Value path_basename(const std::vector<Value>& a, HostContext&) {
    return Value::fromString(std::filesystem::path(a[0].stringValue)
                                 .filename()
                                 .string());
}

static Value path_extension(const std::vector<Value>& a, HostContext&) {
    return Value::fromString(std::filesystem::path(a[0].stringValue)
                                 .extension()
                                 .string());
}

static Value path_isAbsolute(const std::vector<Value>& a, HostContext&) {
    return Value::fromInt(std::filesystem::path(a[0].stringValue).is_absolute() ? 1 : 0);
}

static Value path_separator(const std::vector<Value>&, HostContext&) {
    return Value::fromString(std::string(1, std::filesystem::path::preferred_separator));
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

    ArrayObject* arr = ctx.heap->allocArray((int)bytes.size());
    arr->elements.reserve(bytes.size());
    for (unsigned char b : bytes)
        arr->elements.push_back(Value::fromInt((int)b));
    return Value::fromRef(arr);
}

static Value fs_writeBytes(const std::vector<Value>& a, HostContext&) {
    if (a[1].kind != ValueKind::Ref || !a[1].ref)
        throw std::runtime_error("writeBytes: expected byte[]");
    auto* arr = static_cast<ArrayObject*>(a[1].ref);
    std::ofstream f(a[0].stringValue, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f.is_open())
        throw std::runtime_error("cannot open file '" + a[0].stringValue + "' for writing");
    for (const Value& v : arr->elements)
        f.put(static_cast<char>(v.intValue & 0xFF));
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
        { "MATH_ABS",   1, math_abs   },
        { "MATH_ABSF",  1, math_absf  },
        { "MATH_MIN",   2, math_min   },
        { "MATH_MAX",   2, math_max   },
        { "MATH_MINF",  2, math_minf  },
        { "MATH_MAXF",  2, math_maxf  },
        { "MATH_SQRT",  1, math_sqrt  },
        { "MATH_POW",   2, math_pow   },
        { "MATH_FLOOR", 1, math_floor },
        { "MATH_CEIL",  1, math_ceil  },
        { "MATH_ROUND", 1, math_round },
        { "MATH_PI",    0, math_PI    },
        { "MATH_E",     0, math_E     },
        { "CAPS_DROP",  1, caps_drop  },
        { "CAPS_HAS",   1, caps_has   },
        { "PATH_JOIN",        2, path_join       },
        { "PATH_NORMALIZE",   1, path_normalize  },
        { "PATH_DIRNAME",     1, path_dirname    },
        { "PATH_BASENAME",    1, path_basename   },
        { "PATH_EXTENSION",   1, path_extension  },
        { "PATH_IS_ABSOLUTE", 1, path_isAbsolute },
        { "PATH_SEPARATOR",   0, path_separator  },
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
