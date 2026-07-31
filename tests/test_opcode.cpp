// #132 — Opcode spec tablosu birim testleri (çerçevesiz; assert + çıktı).
// Koşmak için: tests/run.sh
//
// Spec tablosu (OPCODE_LIST) tek kaynaktır: enum, opcodeName(), opcodeArity(),
// opcodeBackends() ve opcodeJitBaseSupported() buradan türetilir. Bu test
// türetilmiş yüzeyin tutarlılığını doğrular — yeni opcode eklemek = OPCODE_LIST'e
// tek satır eklemek, bu test türetilmiş tüm yüzeyleri otomatik kapsar.
#include "ir/instruction.hpp"
#include <cassert>
#include <iostream>
#include <string>

int main() {
    // 1) Her opcode: kanonik isim döner, "UNKNOWN" değil
    for (int i = 0; i < kOpcodeCount; ++i) {
        Opcode op = static_cast<Opcode>(i);
        std::string name = opcodeName(op);
        assert(!name.empty());
        assert(name != "UNKNOWN");
    }

    // 2) VM normatif backend'dir — TÜM opcode'lar OP_VM bayrağı taşır
    for (int i = 0; i < kOpcodeCount; ++i) {
        Opcode op = static_cast<Opcode>(i);
        assert((opcodeBackends(op) & OP_VM) != 0);
        assert(opcodeArity(op) >= 0 && opcodeArity(op) <= 4);
    }

    // 3) JIT temel destek — VM-only dilimler (struct/array/global/try/null)
    //    OP_JIT bayrağı taşımaz; skaler dilimler taşır.
    assert(!opcodeJitBaseSupported(Opcode::LOAD_NULL));
    assert(!opcodeJitBaseSupported(Opcode::STRUCT_NEW));
    assert(!opcodeJitBaseSupported(Opcode::FIELD_GET));
    assert(!opcodeJitBaseSupported(Opcode::FIELD_SET));
    assert(!opcodeJitBaseSupported(Opcode::ARRAY_NEW));
    assert(!opcodeJitBaseSupported(Opcode::ARRAY_GET));
    assert(!opcodeJitBaseSupported(Opcode::ARRAY_SET));
    assert(!opcodeJitBaseSupported(Opcode::ARRAY_LEN));
    assert(!opcodeJitBaseSupported(Opcode::LOAD_GLOBAL));
    assert(!opcodeJitBaseSupported(Opcode::STORE_GLOBAL));
    assert(!opcodeJitBaseSupported(Opcode::ENTER_TRY));
    assert(!opcodeJitBaseSupported(Opcode::LEAVE_TRY));
    assert(!opcodeJitBaseSupported(Opcode::THROW));
    assert(opcodeJitBaseSupported(Opcode::LOAD_CONST));
    assert(opcodeJitBaseSupported(Opcode::ADD));
    assert(opcodeJitBaseSupported(Opcode::FADD));
    assert(opcodeJitBaseSupported(Opcode::LADD));
    assert(opcodeJitBaseSupported(Opcode::F32ADD));
    assert(opcodeJitBaseSupported(Opcode::DADD));
    assert(opcodeJitBaseSupported(Opcode::CALL));

    // 4) Arite — örnek sınıflar (spec tablosundaki kurala göre)
    assert(opcodeArity(Opcode::LOAD_CONST) == 2);   // dest, intValue
    assert(opcodeArity(Opcode::LOAD_NULL) == 1);    // dest
    assert(opcodeArity(Opcode::ADD) == 3);          // dest, left, right
    assert(opcodeArity(Opcode::BNOT) == 2);         // dest, src
    assert(opcodeArity(Opcode::JMP) == 1);          // jumpTarget
    assert(opcodeArity(Opcode::JIF_FALSE) == 2);    // cond, jumpTarget
    assert(opcodeArity(Opcode::RETURN) == 1);       // src
    assert(opcodeArity(Opcode::CALL) == 3);         // dest, callee
    assert(opcodeArity(Opcode::CALLHOST) == 2);     // callee
    assert(opcodeArity(Opcode::LEAVE_TRY) == 0);    // operandsız
    assert(opcodeArity(Opcode::THROW) == 1);        // src
    assert(opcodeArity(Opcode::ENTER_TRY) == 2);    // dest, jumpTarget
    assert(opcodeArity(Opcode::ARRAY_NEW) == 3);    // dest, intValue, elemKind
    assert(opcodeArity(Opcode::CAST_STR_TO_INT) == 3); // dest, src, nullable bayrağı

    // 5) Geçersiz opcode değerleri güvenli fallback döndürür
    Opcode bogus = static_cast<Opcode>(kOpcodeCount);
    assert(std::string(opcodeName(bogus)) == "UNKNOWN");
    assert(opcodeArity(bogus) == 0);
    assert(opcodeBackends(bogus) == 0);
    assert(!opcodeJitBaseSupported(bogus));

    std::cout << "test_opcode: TUM TESTLER GECTI (" << kOpcodeCount << " opcode)\n";
    return 0;
}
