#include "ir/ir_program.hpp"
#include <iostream>

void IRProgram::dump() const {
    std::cout << "IR DUMP\n\n";

    if (globalCount > 0) {
        std::cout << "GLOBALS (" << globalCount << ")\n";
        for (int i = 0; i < (int)globalNames.size(); i++)
            std::cout << "  global[" << i << "] = " << globalNames[i] << "\n";
        std::cout << "\n";
    }

    for (const auto& name : functionOrder) {
        auto it = functions.find(name);
        if (it != functions.end()) it->second.dump();
    }
    std::cout << "END\n";
}
