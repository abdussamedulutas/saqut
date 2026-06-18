#include "ir/ir_program.hpp"
#include <iostream>

void IRProgram::dump() const {
    std::cout << "IR DUMP\n\n";
    for (const auto& name : functionOrder) {
        auto it = functions.find(name);
        if (it != functions.end()) it->second.dump();
    }
    std::cout << "END\n";
}
