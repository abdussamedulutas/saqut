// ============================================================================
// saQut FFI — utf8 host fonksiyonları (#115 Faz 1)
// ============================================================================
//
// Saf dönüşüm — capability'siz, deterministik. encode: saQut string'i (zaten
// UTF-8) ham byte[]'a döker; decode: byte[]'ı string'e çevirir (geçersiz
// baytlara karşı utf8::fromBytes idempotent-toleranlıdır). file.readString/
// writeString ihtiyacının yerini bu + fs::readFile/writeFile birlikte karşılar.
// ============================================================================

#include <string>
#include "core/utf8.hpp"
#include "ffi/host_functions.hpp"
#include "ffi/host_bridge.hpp"

static int utf8_encode(HostCallFrame* f) {
    if (!f->env || !f->env->heap) { f->err.set("encode: heap yok", "E_HOST"); return 1; }
    const std::string& text = hostAsString(f->args[0]);
    ArrayObject* arr = f->env->heap->allocArray((int)text.size(), ArrayElemKind::Byte);
    arr->bytes.assign(text.begin(), text.end());
    f->ret = HostSlot::fromRef(arr);
    return 0;
}

static int utf8_decode(HostCallFrame* f) {
    if (f->args[0].kind != HostKind::Ref || !f->args[0].p) {
        f->err.set("decode: expected byte[]", "E_HOST"); return 1;
    }
    auto* arr = static_cast<ArrayObject*>(f->args[0].p);
    if (arr->elemKind != ArrayElemKind::Byte) {
        f->err.set("decode: expected byte[]", "E_HOST"); return 1;
    }
    hostSetRetString(*f, utf8::fromBytes(std::string_view(
        reinterpret_cast<const char*>(arr->bytes.data()), arr->bytes.size())));
    return 0;
}

// ── Tablo (utf8 alt kümesi) ─────────────────────────────────────────────────
const std::vector<HostFn>& utf8HostFunctions() {
    static const std::vector<HostFn> table = {
        { "UTF8_ENCODE", 1, HOST_NEEDS_HEAP | HOST_CAN_FAIL, HostKind::Ref, utf8_encode },
        { "UTF8_DECODE", 1, HOST_CAN_FAIL, HostKind::Str, utf8_decode },
    };
    return table;
}
