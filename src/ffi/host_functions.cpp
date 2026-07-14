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
