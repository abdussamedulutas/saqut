// ============================================================================
// saQut FFI — date host fonksiyonları (#88)
// ============================================================================
//
// Yalnızca now() dış-durum-okuyandır; geri kalan 15 date fonksiyonu SAF
// hesaptır ve src/data/date.cpp'de aynı HostEntry tamlığıyla yaşar (#225).
// now() FFI'da kalır çünkü sistem saati gerçekten ortamdan gelen bilgidir.
//
// ⚠️ v1 kısıtı: saQut'ta 64-bit int yok — fromEpochMillis/toEpochMillis
// `int` (32-bit) taşır, epoch-ms günümüz tarihleri için bunu aşar (bilinen
// sınır). date DEĞERİNİN kendisi (Value::int64Value) tam hassasiyetlidir;
// year/month/day/addX/diffMillis bu yüzden güvenlidir.
// ============================================================================

#include <chrono>
#include "ffi/host_functions.hpp"
#include "ffi/host_bridge.hpp"

static int date_now(HostCallFrame* f) {
    auto now = std::chrono::system_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch()).count();
    f->ret = HostSlot::fromDate(ms);
    return 0;
}

// ── Tablo (date alt kümesi — yalnız now() burada) ───────────────────────────
const std::vector<HostFn>& dateHostFunctions() {
    static const std::vector<HostFn> table = {
        { "DATE_NOW", 0, HOST_PURE, HostKind::Date, date_now },
    };
    return table;
}
