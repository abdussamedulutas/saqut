// ============================================================================
// saQut IR — IRProgram Gerçeklemesi
// ============================================================================
//
// DİZİN:   src/ir/ir_program.cpp
// KATMAN:  IR — Tüm programın IR temsili
//
// AMAÇ:
//   mainFunction() arama ve ModuleRegistry yönetimi.
//
// ============================================================================

#include "ir/ir_program.hpp"
#include "ir/ir_color.hpp"
#include "tools.hpp"
#include <iostream>

void IRProgram::dump() const {
    std::cout << IrColor::Bold() << IrColor::SoftMor() << "IR DUMP" << IrColor::Reset() << "\n\n";

    if (globalCount > 0) {
        std::cout << IrColor::SoftTurkuaz() << "GLOBALS" << IrColor::Reset()
                  << " (" << IrColor::SoftTuruncu() << globalCount << IrColor::Reset() << ")\n";
        for (int i = 0; i < (int)globalNames.size(); i++)
            std::cout << "  " << IrColor::SoftGri() << "global[" << i << "] =" << IrColor::Reset()
                      << " " << IrColor::SoftYesil() << globalNames[i] << IrColor::Reset() << "\n";
        std::cout << "\n";
    }

    for (const auto& name : functionOrder) {
        auto it = functions.find(name);
        if (it != functions.end()) it->second.dump();
    }
    std::cout << IrColor::SoftTurkuaz() << "END" << IrColor::Reset() << "\n";
}
