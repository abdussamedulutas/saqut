// ============================================================================
// saQut — Veri Tipi Registry'si (gerçekleme)
// ============================================================================

#include "data/data_registry.hpp"

#include <cctype>
#include <cstring>
#include <unordered_map>

#include "data/array.hpp"
#include "data/string.hpp"
#include "data/struct.hpp"

// ── Birleşik tablo ───────────────────────────────────────────────────────────
//
// SIRA KARARLIDIR. İndeksler IR'ye gömülür (CALLHOST::intValue); mevcut bir
// kaydın indeksi kayarsa daha önce üretilmiş IR yanlış metoda gider. Bu yüzden
// yeni kayıtlar ilgili modülün tablosunun SONUNA, yeni modüller de bu listenin
// SONUNA eklenir.
//
// Mevcut düzen (eski runtimeId sırasıyla birebir — id kayması yok):
//   [0..11]  array   (12: length, push, pop, insert, remove, slice, reverse,
//                     concat, contains, indexOf, clear, ...)
//   [12..25] string  (14: length, upper, lower, trim, split, substring,
//                     replace, repeat, charAt, indexOf, contains,
//                     startsWith, endsWith, ...)
//   [26..27] struct  (2: toJson, dump)
const std::vector<DataMethod>& dataAllMethods() {
    static const std::vector<DataMethod> all = [] {
        std::vector<DataMethod> v;
        auto append = [&v](const std::vector<DataMethod>& src) {
            v.insert(v.end(), src.begin(), src.end());
        };
        append(dataArrayMethods());
        append(dataStringMethods());
        append(dataStructMethods());
        return v;
    }();
    return all;
}

// ── Derleme zamanı arama ─────────────────────────────────────────────────────

namespace {

// "kategori:isim" → indeks. Kategori öneki gerekli çünkü aynı ad birden fazla
// tipte olabilir (string::length ve E::length, string::indexOf ve E::indexOf).
const std::unordered_map<std::string, int>& byName() {
    static const std::unordered_map<std::string, int> index = [] {
        std::unordered_map<std::string, int> m;
        const auto& all = dataAllMethods();
        for (int i = 0; i < (int)all.size(); ++i) {
            const char* prefix = "";
            switch (all[i].category) {
                case DataMethodCategory::Array:     prefix = "ar:"; break;
                case DataMethodCategory::StringVal: prefix = "sv:"; break;
                case DataMethodCategory::StructVal: prefix = "st:"; break;
            }
            m[std::string(prefix) + all[i].name] = i;
        }
        return m;
    }();
    return index;
}

const DataMethod* lookupPrefixed(const char* prefix, const std::string& name) {
    auto it = byName().find(std::string(prefix) + name);
    return it != byName().end() ? &dataAllMethods()[it->second] : nullptr;
}

}  // namespace

const DataMethod* dataLookupMethod(const std::string& leftName,
                                   const std::string& methodName,
                                   bool               isStruct,
                                   bool               isReceiverArray) {
    if (!isReceiverArray) {
        // 1. string değer metodları (string::upper, ...)
        if (leftName == "string")
            if (const DataMethod* m = lookupPrefixed("sv:", methodName)) return m;
        // 2. struct değer metodları (S::toJson, S::dump)
        if (isStruct)
            if (const DataMethod* m = lookupPrefixed("st:", methodName)) return m;
    }
    // 3. array metodları (herhangi E[] için — scalar ve struct array dahil)
    return lookupPrefixed("ar:", methodName);
}

int dataMethodId(const DataMethod* m) {
    if (!m) return -1;
    const auto& all = dataAllMethods();
    // Pointer aritmetiği güvenli: m her zaman dataAllMethods() içindeki bir
    // kayda işaret eder (lookup yalnızca oradan döner).
    if (m < all.data() || m >= all.data() + all.size()) return -1;
    return (int)(m - all.data());
}

const DataMethod* dataMethodAt(int id) {
    const auto& all = dataAllMethods();
    if (id < 0 || id >= (int)all.size()) return nullptr;
    return &all[id];
}

Type dataResolveElemType(const std::string& leftName) {
    Type t = Type::fromName(leftName);
    if (!t.isError()) return t;
    // Struct tipi — fromName tanımaz ama structType ile üretilebilir.
    if (!leftName.empty() && std::isupper((unsigned char)leftName[0]))
        return Type::structType(leftName);
    return Type::error();
}
