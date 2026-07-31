// ============================================================================
// saQut CLI — SQ-100 JSONL şema sürümleri
//
// SQ-100 ailesi: check/symbols/tokens komutları stdout'da canonical JSONL
// yayınlar. Her komutun kendi şeması ayrı sürümlenir; schemaVersion yalnızca
// ilk kayıtta (check.header / symbols.header / tokens.header) taşınır.
// ============================================================================

#ifndef SAQUT_CLI_JSONL_SCHEMA
#define SAQUT_CLI_JSONL_SCHEMA

namespace saqut::jsonl {

// check komutu (SQ-100-CHECK-JSONL, #144)
constexpr int kCheckJsonlSchemaVersion = 1;

// symbols komutu (SQ-100-SYMBOLS-JSONL, #145)
constexpr int kSymbolsJsonlSchemaVersion = 1;

}  // namespace saqut::jsonl

#endif  // SAQUT_CLI_JSONL_SCHEMA
