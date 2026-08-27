// ============================================================================
// saQut — string veri tipi (arayüz)
// ============================================================================
//
// DİZİN:   src/data/string.hpp
// KATMAN:  data — string'in çalışma zamanındaki TEK sahibi
//
// Gövdeler string.cpp'de. Bu başlık string'in DIŞARIYA görünen yüzünü tanımlar:
// bellek temsili ve metod tablosu.
//
// BELLEK TEMSİLİ (ADR-024 / ADR-037):
//   string IMMUTABLE bir değer tipidir. İki temsili vardır ve bu ayrım
//   bilinçlidir:
//
//     VM      → Value::stringValue (inline std::string)
//     Sınır   → StringObject* (host_abi.hpp: HostKind::Str)
//     JIT     → StringObject* (MIR_T_I64 register'da pointer)
//
//   Gözlemlenen davranış her üçünde aynıdır (#92); iç temsil backend'e göre
//   değişir. StringObject tanımı gc/gc_object.hpp'dedir çünkü GC nesne ailesine
//   aittir — GC işaretleme/süpürme bu modülün kapsamı DIŞINDADIR.
//
// ============================================================================

#ifndef SAQUT_DATA_STRING
#define SAQUT_DATA_STRING

#include "data/data_type.hpp"

// string'in tüm metodları: length, upper, lower, trim, split, substring,
// replace, repeat, charAt, indexOf, contains, startsWith, endsWith.
//
// Yeni metod eklemek = string.cpp'deki tabloya bir satır. Başka dosya yok.
const std::vector<DataMethod>& dataStringMethods();

#endif // SAQUT_DATA_STRING
