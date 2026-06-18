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

int main(int argc, char* argv[]) {
    // Komutları kaydet
    CliDispatcher cli;

    cli.registerCommand({"run",
        "Pipeline'ı çalıştır (token → AST → IR)",
        false, cmdRun});

    cli.registerCommand({"tokens",
        "Token listesini göster",
        false, cmdTokens});

    cli.registerCommand({"ast",
        "JSON formatında AST hiyerarşisi ve analiz",
        false, cmdAst});

    cli.registerCommand({"symbols",
        "Sembol tablosu (fonksiyonlar, değişkenler)",
        false, cmdSymbols});

    cli.registerCommand({"check",
        "Semantik analiz — tip denetimi + yapısal doğrulama",
        false, cmdCheck});

    // --- Gelecek komutlar (TODO) ---
    cli.registerCommand({"compile",
        "TODO: Kaynak kodu derle",
        false, [](const CliArgs&) {
            std::cerr << "TODO: compile komutu henüz eklenmedi\n"; return 1;
        }});

    cli.registerCommand({"parse",
        "TODO: IR üret",
        false, [](const CliArgs&) {
            std::cerr << "TODO: parse komutu henüz eklenmedi\n"; return 1;
        }});

    cli.registerCommand({"transpile",
        "TODO: C koduna çevir",
        false, [](const CliArgs&) {
            std::cerr << "TODO: transpile komutu henüz eklenmedi\n"; return 1;
        }});

    cli.registerCommand({"interpret",
        "TODO: Interpreter modu",
        true, [](const CliArgs&) {
            std::cerr << "TODO: interpret komutu henüz eklenmedi\n"; return 1;
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
