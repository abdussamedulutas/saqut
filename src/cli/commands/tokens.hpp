// ============================================================================
// saQut CLI — tokens komutu
// ============================================================================

#ifndef SAQUT_CLI_TOKENS
#define SAQUT_CLI_TOKENS

#include <iostream>
#include "cli/args.hpp"
#include "tokenizer/tokenizer.hpp"

inline int cmdTokens(const CliArgs& args) {
    std::string source = readSource(args);
    if (source.empty()) return 1;

    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source, inputFilePath(args));

    std::cout << "Tokenler (" << tokens.size() << " adet):\n";
    for (auto* t : tokens) {
        std::cout << "  [" << t->gettype() << "] \"" << t->token << "\"\n";
    }

    for (auto* t : tokens) delete t;
    return 0;
}

#endif // SAQUT_CLI_TOKENS
