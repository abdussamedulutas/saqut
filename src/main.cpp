// ============================================================================
// saQut Compiler — Giriş Noktası (main)
// ============================================================================
//
// DİZİN:   src/main.cpp
// KATMAN:  En üst — CLI dispatcher'ı başlatır
//
// KULLANIM:
//   saqut                         → yardım
//   saqut run <dosya>             → pipeline debug çıktısı
//   saqut tokens <dosya>          → token listesi
//   saqut ast <dosya> [-o çıktı]  → JSON AST + analiz
//   saqut symbols <dosya>         → sembol tablosu
//   saqut -                       → stdin modu (TODO)
//
// YENİ KOMUT EKLEMEK İÇİN:
//   1. src/cli/commands/x.hpp oluştur
//   2. Bu dosyada #include et
//   3. cli.registerCommand({...}) ile kaydet
//
// ============================================================================

#include <iostream>
#include "cli/args.hpp"
#include "cli/cli.hpp"
#include "cli/commands/run.hpp"
#include "cli/commands/tokens.hpp"
#include "cli/commands/ast.hpp"
#include "cli/commands/symbols.hpp"
#include "cli/commands/check.hpp"
#include "cli/commands/ir.hpp"
#include "cli/commands/exec.hpp"

int main(int argc, char* argv[]) {
    // Komutları kaydet
    CliDispatcher cli;

    cli.registerCommand({"run",
        "run program (token → AST → IR → VM)",
        false, cmdRun});

    cli.registerCommand({"tokens",
        "print token list",
        false, cmdTokens});

    cli.registerCommand({"ast",
        "print AST hierarchy and analysis as JSON",
        false, cmdAst});

    cli.registerCommand({"symbols",
        "print symbol table (functions, variables)",
        false, cmdSymbols});

    cli.registerCommand({"check",
        "semantic analysis — type checking + structural validation",
        false, cmdCheck});

    cli.registerCommand({"ir",
        "print IR instruction list (intermediate representation)",
        false, cmdIr});

    cli.registerCommand({"exec",
        "evaluate an expression and print the result  (saqut exec \"1+2\")",
        false, cmdExec});

    // --- Future commands (TODO) ---
    cli.registerCommand({"compile",
        "TODO: compile source to binary",
        false, [](const CliArgs&) {
            std::cerr << "TODO: compile command not yet implemented\n"; return 1;
        }});

    cli.registerCommand({"parse",
        "TODO: generate IR",
        false, [](const CliArgs&) {
            std::cerr << "TODO: parse command not yet implemented\n"; return 1;
        }});

    cli.registerCommand({"transpile",
        "TODO: transpile to C code",
        false, [](const CliArgs&) {
            std::cerr << "TODO: transpile command not yet implemented\n"; return 1;
        }});

    cli.registerCommand({"interpret",
        "TODO: interpreter mode",
        true, [](const CliArgs&) {
            std::cerr << "TODO: interpret command not yet implemented\n"; return 1;
        }});

    // Argümanları ayrıştır
    CliArgs args = parseArgs(argc, argv);

    // Argümansız çağrı → help
    if (argc <= 1) {
        cli.printHelp();
        return 0;
    }

    return cli.dispatch(args);
}
