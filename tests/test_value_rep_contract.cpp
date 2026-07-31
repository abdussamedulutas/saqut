// #130 — Tip temsili sözleşme tablosu birim testleri (çerçevesiz; assert + çıktı).
// Koşmak için: tests/run.sh
//
// value_rep_contract.hpp borcu GÖRÜNÜR kılan bir artifact'tır (karar değil).
// Bu test tabloyu gerçekliğe bağlar: (a) her ValueKind tam bir satıra sahip,
// (b) JIT durumu gerçek JIT davranışıyla çelişmiyor (opcode düzeyinde),
// (c) satır alanları dolu.
#include "core/value_rep_contract.hpp"
#include "vm/value.hpp"
#include "ir/instruction.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <set>

// Test oracle'ı: ValueKind enumerator → kanonik ad (value_rep_contract'ın
// kind sütunu bu isimlerle eşleşmek zorunda).
static const char* kindName(ValueKind k) {
    switch (k) {
        case ValueKind::Int:     return "Int";
        case ValueKind::LongInt: return "LongInt";
        case ValueKind::Float:   return "Float";
        case ValueKind::Float32: return "Float32";
        case ValueKind::Decimal: return "Decimal";
        case ValueKind::String:  return "String";
        case ValueKind::Ref:     return "Ref";
        case ValueKind::Null:    return "Null";
        case ValueKind::Date:    return "Date";
    }
    return "?";
}

int main() {
    // 1) Satır sayısı ValueKind enumerator sayısıyla aynı (9).
    int enumCount = 0;
    for (int i = 0; i < 9; ++i) {
        ValueKind k = static_cast<ValueKind>(i);
        assert(kindName(k) != std::string("?"));
        ++enumCount;
    }
    assert(enumCount == 9);
    assert(kValueRepRowCount == enumCount);

    // 2) Her ValueKind tam bir kez tabloda; alanlar dolu.
    std::set<std::string> seen;
    for (int i = 0; i < kValueRepRowCount; ++i) {
        const ValueRepRow& r = kValueRepTable[i];
        assert(r.kind && r.kind[0]);
        assert(r.vmStorage && r.vmStorage[0]);
        assert(r.jitRegister && r.jitRegister[0]);
        assert(r.dapFormat && r.dapFormat[0]);
        assert(r.borcNotu && r.borcNotu[0]);
        assert(seen.insert(r.kind).second);  // duplicate yok
    }
    for (int i = 0; i < 9; ++i) {
        ValueKind k = static_cast<ValueKind>(i);
        assert(seen.count(kindName(k)) == 1);  // her kind kaplı
    }

    // 3) jitStatus değerleri kontrollü sözlükten; her satırın durumu var.
    std::set<std::string> validStatus = {"uyumlu", "tasarim", "temsil-yok"};
    for (int i = 0; i < kValueRepRowCount; ++i)
        assert(validStatus.count(kValueRepTable[i].jitStatus) == 1);

    // 4) Tablo JIT davranışıyla çelişmiyor (opcode düzeyinde gerçeklik):
    //    - Null: tablo "temsil-yok" diyor VE LOAD_NULL JIT'te gerçekten reddediliyor.
    //    - Skalerler (Int/LongInt/Float/Float32/Date): "uyumlu".
    auto row = [](const char* name) -> const ValueRepRow& {
        for (int i = 0; i < kValueRepRowCount; ++i)
            if (std::string(kValueRepTable[i].kind) == name)
                return kValueRepTable[i];
        assert(false && "row yok");
        return kValueRepTable[0];
    };
    assert(std::string(row("Null").jitStatus) == "temsil-yok");
    assert(!opcodeJitBaseSupported(Opcode::LOAD_NULL));
    assert(std::string(row("Int").jitStatus)     == "uyumlu");
    assert(std::string(row("LongInt").jitStatus) == "uyumlu");
    assert(std::string(row("Float").jitStatus)   == "uyumlu");
    assert(std::string(row("Float32").jitStatus) == "uyumlu");
    assert(std::string(row("Date").jitStatus)    == "uyumlu");
    assert(std::string(row("Decimal").jitStatus) == "tasarim");
    assert(std::string(row("String").jitStatus)  == "tasarim");
    assert(std::string(row("Ref").jitStatus)     == "tasarim");

    // 5) DAP sütunu kanonik davranışları yansıtıyor (spot kontrol).
    assert(std::string(row("Null").dapFormat) == "\"null\"");
    assert(std::string(row("Decimal").dapFormat) == "decimalValue.toString()");

    std::cout << "test_value_rep_contract: TUM TESTLER GECTI (" << kValueRepRowCount << " satır)\n";
    return 0;
}
