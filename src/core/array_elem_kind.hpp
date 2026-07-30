// ============================================================================
// saQut Core — Array Element Kind (Packed Array Tip Etiketi)
// ============================================================================
//
// DİZİN:   src/core/array_elem_kind.hpp
// KATMAN:  Core — Tip tanımı (bağımlılıksız)
//
// AMAÇ:
//   ArrayObject'in eleman tipini belirtir. #206 düzeltmesi: byte[] gibi
//   primitive array'ler packed (vector<uint8_t> vb.) tutulur, ek yük
//   ortadan kalkar. ArrayElemKind, IR instruction'dan VM'e kadar tüm
//   pipeline'da taşınır.
//
// ============================================================================

#ifndef SAQUT_CORE_ARRAY_ELEM_KIND
#define SAQUT_CORE_ARRAY_ELEM_KIND

#include <cstdint>

enum class ArrayElemKind : uint8_t {
    Ref,       // struct[], string[], nested array[], nullable[] → elements (vector<Value>)
    Byte,      // byte[]
    Int,       // int[], bool[], char[]
    LongInt,   // longint[], date[]
    Float32,   // float[]
    Float64,   // double[]
    Decimal,   // decimal[]
};

#endif // SAQUT_CORE_ARRAY_ELEM_KIND
