// ============================================================================
// saQut FFI — os + terminal host fonksiyonları (#115 Faz 2, izlenmiş)
// ============================================================================
//
// os: salt tanılama — name/arch/hostname/user/cpuCount. capability'siz
// (dış dünyaya yazmaz, salt okur; ADR-043 capability kaldırıldı).
// terminal: isTTY — renk çıktısı kararı için yaygın kullanım; stdout'un
// gerçek terminale bağlı olup olmadığını sorgular.
// ============================================================================

#include <thread>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ffi/host_functions.hpp"
#include "ffi/host_bridge.hpp"

static int os_name(HostCallFrame* f) {
#if defined(_WIN32)
    hostSetRetString(*f, "windows");
#elif defined(__APPLE__)
    hostSetRetString(*f, "macos");
#elif defined(__linux__)
    hostSetRetString(*f, "linux");
#else
    hostSetRetString(*f, "unknown");
#endif
    return 0;
}

static int os_arch(HostCallFrame* f) {
#if defined(__x86_64__) || defined(_M_X64)
    hostSetRetString(*f, "x86_64");
#elif defined(__aarch64__) || defined(_M_ARM64)
    hostSetRetString(*f, "aarch64");
#elif defined(__i386__) || defined(_M_IX86)
    hostSetRetString(*f, "x86");
#else
    hostSetRetString(*f, "unknown");
#endif
    return 0;
}

static int os_hostname(HostCallFrame* f) {
    char buf[256];
    if (::gethostname(buf, sizeof(buf)) != 0)
        buf[0] = '\0';
    buf[sizeof(buf) - 1] = '\0';
    hostSetRetString(*f, std::string(buf));
    return 0;
}

static int os_user(HostCallFrame* f) {
    const char* user = std::getenv("USER");
    if (!user) user = std::getenv("LOGNAME");
    if (!user) { f->ret = HostSlot::null(); return 0; }
    hostSetRetString(*f, std::string(user));
    return 0;
}

static int os_cpuCount(HostCallFrame* f) {
    unsigned n = std::thread::hardware_concurrency();
    f->ret = HostSlot::fromInt(n ? static_cast<int>(n) : 0);
    return 0;
}

// ── terminal ────────────────────────────────────────────────────────────────

static int term_isTTY(HostCallFrame* f) {
    f->ret = HostSlot::fromInt(::isatty(::fileno(stdout)) ? 1 : 0);
    return 0;
}

// ── Tablo (os / terminal alt kümesi) ────────────────────────────────────────
const std::vector<HostFn>& osHostFunctions() {
    static const std::vector<HostFn> table = {
        { "OS_NAME",      0, 0, HostKind::Str, os_name },
        { "OS_ARCH",      0, 0, HostKind::Str, os_arch },
        { "OS_HOSTNAME",  0, 0, HostKind::Str, os_hostname },
        { "OS_USER",      0, 0, HostKind::Str, os_user },
        { "OS_CPU_COUNT", 0, 0, HostKind::Int, os_cpuCount },
    };
    return table;
}

const std::vector<HostFn>& terminalHostFunctions() {
    static const std::vector<HostFn> table = {
        { "TERMINAL_IS_TTY", 0, 0, HostKind::Int, term_isTTY },
    };
    return table;
}
