// ============================================================================
// saQut MIR JIT — Fast-mode Dış Arayüz (Dilim 0, #80, MIRPLAN.md)
//
// KATMAN: Backend — VM'e (Interpreter) alternatif ikinci çalıştırma yolu.
// İZOLASYON: Bu dosya <mir.h>/<mir-gen.h> BİLMEZ (MIRPLAN.md §1 kuralı).
//   MIR header'ları YALNIZCA mir_backend.cpp'ye include edilir. src/ir/*,
//   src/vm/*, src/cli/* bu arayüzün dışında MIR'i hiç bilmez.
//
// KAPSAM (Dilim 0): yalnızca LOAD_CONST/ADD/SUB/MUL/RETURN opcode'larını,
// parametresiz fonksiyonları destekler — MIRPLAN.md §10'daki ilk dikey
// dilimin kanıtı. Debug modu YOK (bkz. issue #109, ayrı iş).
// ============================================================================

#ifndef SAQUT_MIR_BACKEND
#define SAQUT_MIR_BACKEND

#include <string>
#include "ir/ir_function.hpp"

namespace mir_backend {

// fn'i gerçekten MIR ile makine koduna derleyip çalıştırmayı DENER.
//   - fn desteklenmeyen bir opcode içeriyorsa ya da parametre alıyorsa:
//     false döner, errorOut insan-okunabilir sebebi taşır, outResult dokunulmaz.
//   - Desteklenen kümedeyse: gerçekten JIT'ler, native kodu çalıştırır,
//     RETURN'ün taşıdığı değeri outResult'a yazar, true döner.
bool tryCompileAndRun(const IRFunction& fn, int& outResult, std::string& errorOut);

}  // namespace mir_backend

#endif  // SAQUT_MIR_BACKEND
