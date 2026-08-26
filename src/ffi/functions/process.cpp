// ============================================================================
// saQut FFI — process host fonksiyonları (#115 Faz 1)
// ============================================================================
//
// Süreç kontrolü: exit/cwd/chdir/pid. Dış-durum-okuyan ve dış-durumu değiştiren
// aile. Capability yok (ADR-043 capability kaldırıldı — herkese açık).
// exit, std::exit ile süreci sonlandırır (dönüş yok); cwd/chdir/pid dönüşü
// process" hakkında bilgi verir.
// ============================================================================

#include <filesystem>
#include <unistd.h>
#include <cstdlib>
#include "ffi/host_functions.hpp"
#include "ffi/host_bridge.hpp"

static int process_exit(HostCallFrame* f) {
    // SaQut programı istenen kodla sonlanır. C++ akış teardown'ı std::exit
    // içinde gerçekleşir (global destructor'lar + akış flush) — stdout verisi
    // core_print her seferinde flush ettiği için kaybolmaz.
    std::exit(static_cast<int>(hostAsI64(f->args[0])));
}

static int process_cwd(HostCallFrame* f) {
    std::error_code ec;
    std::filesystem::path p = std::filesystem::current_path(ec);
    if (ec) { f->err.set("cwd failed: " + ec.message(), "E_HOST"); return 1; }
    hostSetRetString(*f, p.string());
    return 0;
}

static int process_pid(HostCallFrame* f) {
    f->ret = HostSlot::fromInt(static_cast<int>(::getpid()));
    return 0;
}

static int process_chdir(HostCallFrame* f) {
    const std::string& path = hostAsString(f->args[0]);
    std::error_code ec;
    std::filesystem::current_path(path, ec);
    if (ec) { f->err.set("chdir failed: " + std::string(path) + ": " + ec.message(), "E_HOST"); return 1; }
    f->ret = HostSlot::voidVal();
    return 0;
}

// ── Tablo (process alt kümesi) ──────────────────────────────────────────────
const std::vector<HostFn>& processHostFunctions() {
    static const std::vector<HostFn> table = {
        { "PROCESS_EXIT",  1, HOST_CAN_FAIL, HostKind::Void, process_exit },
        { "PROCESS_CWD",   0, HOST_CAN_FAIL, HostKind::Str,  process_cwd },
        { "PROCESS_PID",   0, 0,             HostKind::Int,  process_pid },
        { "PROCESS_CHDIR", 1, HOST_CAN_FAIL, HostKind::Void, process_chdir },
    };
    return table;
}
