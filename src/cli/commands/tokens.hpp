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
        // SourceLocation::column is derived from the UTF-8 source byte
        // offset; it is not a Unicode code-point or visual-terminal column.
        const int byteLength = t->end - t->start;
        std::cout << "  [" << t->gettype() << "] \"" << t->token << "\""
                  << " file=" << inputFilePath(args)
                  << " byteOffset=" << t->start
                  << " byteLength=" << byteLength
                  << " line=" << t->loc.line
                  << " column=" << t->loc.column << "\n";
    }

    for (auto* t : tokens) delete t;
    return 0;
}

#endif // SAQUT_CLI_TOKENS
