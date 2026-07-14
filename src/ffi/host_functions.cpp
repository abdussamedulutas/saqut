// ============================================================================
// saQut FFI — Host Fonksiyon Registry Gerçeklemesi
// ============================================================================
//
// math modülü (ADR-034, #107; issue #89): saf hesap, capability'siz.
// Overload YOK → int/float ayrımı isimle (abs/absf, min/minf, max/maxf).
// IEEE754 korunur: sqrt(-1) NaN döner, Error FIRLATMAZ (#89).
// ============================================================================

#include "ffi/host_functions.hpp"
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
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

// ── caps implementasyonları (#91, ADR-036) ──────────────────────────────────
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
        { "CAPS_DROP",  1, caps_drop  },
        { "CAPS_HAS",   1, caps_has   },
        { "FS_READ_FILE",   1, fs_readFile   },
        { "FS_WRITE_FILE",  2, fs_writeFile  },
        { "FS_APPEND",      2, fs_append     },
        { "FS_READ_BYTES",  1, fs_readBytes  },
        { "FS_WRITE_BYTES", 2, fs_writeBytes },
        { "FS_EXISTS",      1, fs_exists     },
        { "FS_REMOVE",      1, fs_remove     },
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
