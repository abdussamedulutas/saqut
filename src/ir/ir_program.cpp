#include "ir/ir_program.hpp"
#include "tools.hpp"
#include <iostream>

void IRProgram::dump() const {
    std::cout << Color::Bold << Color::SoftMor << "IR DUMP" << Color::Reset << "\n\n";

    if (globalCount > 0) {
        std::cout << Color::SoftTurkuaz << "GLOBALS" << Color::Reset
                  << " (" << Color::SoftTuruncu << globalCount << Color::Reset << ")\n";
        for (int i = 0; i < (int)globalNames.size(); i++)
            std::cout << "  " << Color::SoftGri << "global[" << i << "] =" << Color::Reset
                      << " " << Color::SoftYesil << globalNames[i] << Color::Reset << "\n";
        std::cout << "\n";
    }

    for (const auto& name : functionOrder) {
        auto it = functions.find(name);
        if (it != functions.end()) it->second.dump();
    }
    std::cout << Color::SoftTurkuaz << "END" << Color::Reset << "\n";
}
