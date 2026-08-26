// ============================================================================
// saQut FFI — path host fonksiyonları (#115 Faz 1)
// ============================================================================
//
// Saf string yol işlemleri — capability'siz, dış dünyaya açılmaz, deterministik.
// join string[] (üye değişken parametre olmayan API) yol parçalarını platform
// ayracıyla birleştirir. absolute/relative BİLEREK burada yok: cwd'ye bağımlı
// oldukları için dış-durum taşırlar; ihtiyaç olursa process::cwd() + join ile
// string düzeyinde yapılır (#115 açık soru).
// ============================================================================

#include <filesystem>
#include <string>
#include <vector>
#include "ffi/host_functions.hpp"
#include "ffi/host_bridge.hpp"

static int path_join(HostCallFrame* f) {
    if (f->args[0].kind != HostKind::Ref || !f->args[0].p) {
        f->err.set("join: expected string[]", "E_HOST");
        return 1;
    }
    auto* arr = static_cast<ArrayObject*>(f->args[0].p);
    std::filesystem::path p;
    for (const auto& v : arr->elements) {
        if (v.kind != ValueKind::String) continue;
        if (v.stringValue.empty()) continue;
        p /= v.stringValue;
    }
    hostSetRetString(*f, p.lexically_normal().string());
    return 0;
}

static int path_normalize(HostCallFrame* f) {
    hostSetRetString(*f, std::filesystem::path(hostAsString(f->args[0])).lexically_normal().string());
    return 0;
}

static int path_dirname(HostCallFrame* f) {
    std::filesystem::path p(hostAsString(f->args[0]));
    const auto parent = p.has_parent_path() ? p.parent_path() : std::filesystem::path(".");
    hostSetRetString(*f, parent.string());
    return 0;
}

static int path_basename(HostCallFrame* f) {
    std::filesystem::path p(hostAsString(f->args[0]));
    hostSetRetString(*f, p.filename().string());
    return 0;
}

static int path_extension(HostCallFrame* f) {
    std::filesystem::path p(hostAsString(f->args[0]));
    hostSetRetString(*f, p.extension().string());  // nokta dahil (".txt"); yoksa boş
    return 0;
}

static int path_isAbsolute(HostCallFrame* f) {
    f->ret = HostSlot::fromInt(std::filesystem::path(hostAsString(f->args[0])).is_absolute() ? 1 : 0);
    return 0;
}

static int path_separator(HostCallFrame* f) {
    // std::filesystem::path::preferred_separator — POSIX '/' , Windows '\\'.
    hostSetRetString(*f, std::string(1, std::filesystem::path::preferred_separator));
    return 0;
}

// ── Tablo (path alt kümesi) ─────────────────────────────────────────────────
const std::vector<HostFn>& pathHostFunctions() {
    static const std::vector<HostFn> table = {
        { "PATH_JOIN",         1, HOST_CAN_FAIL, HostKind::Str, path_join },
        { "PATH_NORMALIZE",    1, 0,             HostKind::Str, path_normalize },
        { "PATH_DIRNAME",      1, 0,             HostKind::Str, path_dirname },
        { "PATH_BASENAME",     1, 0,             HostKind::Str, path_basename },
        { "PATH_EXTENSION",    1, 0,             HostKind::Str, path_extension },
        { "PATH_IS_ABSOLUTE",  1, 0,             HostKind::Int, path_isAbsolute },
        { "PATH_SEPARATOR",    0, 0,             HostKind::Str, path_separator },
    };
    return table;
}
