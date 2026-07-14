// ============================================================================
// saQut Compiler — Capability (ADR-035, #76)
// ============================================================================
//
// DİZİN:   src/core/capability.hpp
// BAĞIMLI: Yok (sadece standart kütüphane)
//
// AMAÇ:
//   Dış dünyaya açılan FFI host fonksiyonlarının bağlı olduğu izin
//   kategorisi. Varsayılan her şey kapalı; CLI bayrağıyla (--allow-fs vb.)
//   açıkça açılır. `ffi ... requires <cap>;` bildirimindeki string ile bu
//   enum arasındaki köprü capabilityFromName()'dir.
//
// ============================================================================

#ifndef SAQUT_CORE_CAPABILITY
#define SAQUT_CORE_CAPABILITY

#include <optional>
#include <string>

enum class Capability { Fs, Net, Sys };

inline const char* capabilityName(Capability c) {
    switch (c) {
        case Capability::Fs:  return "fs";
        case Capability::Net: return "net";
        case Capability::Sys: return "sys";
    }
    return "?";
}

inline std::optional<Capability> capabilityFromName(const std::string& name) {
    if (name == "fs")  return Capability::Fs;
    if (name == "net") return Capability::Net;
    if (name == "sys") return Capability::Sys;
    return std::nullopt;
}

#endif // SAQUT_CORE_CAPABILITY
