// ============================================================================
// saQut — date veri tipi
// ============================================================================
//
// DİZİN:   src/data/date.cpp
// KATMAN:  data — date'in çalışma zamanındaki TEK sahibi
//
// TEMSİL (#88, ADR-036): date = UTC epoch-ms (int64). Zaman dilimi YOK,
//   takvim nesnesi YOK — tek bir andı temsil eden sayı.
//
// NEDEN BURAYA TAŞINDI (#225):
//   date'in 16 fonksiyonundan YALNIZ now() gerçekten dış dünyadır (sistem
//   saatini okur). Kalan 15'i saf epoch-ms aritmetiğidir: addDays bir toplama,
//   year() bir takvim hesabı, parse() bir string→sayı çevrimi. Bunların
//   sonucunu saQut'un sözleşmesi belirler, ortam değil.
//
//   Öncesinde 15'i de CALLHOST'taydı, yani "dışarıya sor" diye işaretliydi.
//   WASM'e veya bir transpiler'a gittiğimizde bu 15 gereksiz köprü demekti —
//   oysa hepsi taşınabilir hesap.
//
//   now() FFI'da KALIR: sistem saati gerçekten ortamdan gelen bilgidir ve
//   --allow-sys capability'si gerektirir (ADR-035).
//
// TAKVİM HESABI: date_calc.hpp (Howard Hinnant'ın civil_from_days algoritması)
//   — yalnız int64 aritmetiği, hiçbir platform API'si yok.
//
// ============================================================================

#include "data/date.hpp"

#include <cstdio>
#include <string>

#include "data/date_calc.hpp"
#include "ffi/host_bridge.hpp"

namespace {

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
}  // namespace

// ── Metod tablosu ────────────────────────────────────────────────────────────
//
// Bunlar built-in METOD değil, host fonksiyonudur (root.sqt'teki `ffi`
// bildirimleriyle eşleşir) — ama artık gövdeleri FFI katmanında değil, tipin
// kendi modülünde durur. Registry sembolik id ile bağlar.
const std::vector<DateFn>& dataDateFunctions() {
    static const std::vector<DateFn> fns = {
        {"DATE_FROM_EPOCH_MS",   date_fromEpochMillis},
        {"DATE_TO_EPOCH_MS",     date_toEpochMillis},
        {"DATE_ADD_DAYS",        date_addDays},
        {"DATE_ADD_HOURS",       date_addHours},
        {"DATE_ADD_MINUTES",     date_addMinutes},
        {"DATE_ADD_SECONDS",     date_addSeconds},
        {"DATE_YEAR",            date_year},
        {"DATE_MONTH",           date_month},
        {"DATE_DAY",             date_day},
        {"DATE_HOUR",            date_hour},
        {"DATE_MINUTE",          date_minute},
        {"DATE_SECOND",          date_second},
        {"DATE_DIFF_MS",         date_diffMillis},
        {"DATE_PARSE",           date_parse},
        {"DATE_FORMAT",          date_format},
    };
    return fns;
}
