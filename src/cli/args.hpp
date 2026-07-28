// ============================================================================
// saQut Compiler — CLI Argüman Ayrıştırıcı ve Kaynak Okuma
// ============================================================================
//
// DİZİN:   src/cli/args.hpp
// BAĞIMLI: Yok (sadece standart kütüphane)
//
// AMAÇ:
//   1. POSIX tarzı argüman ayrıştırma
//   2. Kaynak dosya okuma (tüm komutlar tarafından paylaşılır)
//
// DESTEKLENEN FORMATLAR:
//   saqut <komut> [dosya] [-o çıktı] [--format json] [--help]
//   saqut run file:source.sqt           (eski sözdizimi)
//   saqut -                             (stdin — TODO)
//
// ============================================================================

#ifndef SAQUT_CLI_ARGS
#define SAQUT_CLI_ARGS

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "cli/exit_codes.hpp"
#include "core/capability.hpp"

struct CliArgs {
    std::string command;
    std::vector<std::string> positional;
    std::string outputFile;
    bool showHelp    = false;
    bool stdinMode   = false;
    bool compact     = false;  // --compact: boşluksuz JSON
    bool optimized   = false;  // --optimized: sabit katlama + ölü kod eleme
    bool jsonOutput  = false;  // --json: JSON çıktı üret (varsayılan: düz metin)
    bool jsonlOutput = false;  // --jsonl: makine-okunur satır akışı
    int  benchRuns   = 5;      // --runs=N: benchmark tekrar sayısı
    bool compileOnly = false;  // --compile-only: VM çalıştırmasını atla
    bool verbose     = false;  // --verbose: her aşamanın bitişini canlı yaz
    int  gcThreshold = 0;      // --gc-threshold=N: GC eşiği (0 = VM varsayılanı, negatif = GC kapalı)
    bool gcStats     = false;  // --gc-stats: koşu sonunda GC istatistiklerini stderr'e yaz

    // #80/MIRPLAN.md: --jit — Dilim 0 kapsamındaki fonksiyonlar için MIR
    // JIT backend'ini dener (yalnızca LOAD_CONST/ADD/SUB/MUL/RETURN,
    // parametresiz main). Desteklenmeyen bir şey görülürse VM'e düşer.
    // 0.8.0'da opt-in; 1.0.0'da varsayılan yön döner (bkz. CLAUDE.md).
    bool useJit = false;

    // src/profiling/: --profile — token/parser/ir-gen/vm-veya-jit
    // aşamalarını ayrı ayrı ölçüp stderr'e yazdırır (saqut run).
    bool profile = false;

    // ADR-035 (#76): --allow-fs/--allow-net/--allow-sys — varsayılan hepsi kapalı.
    std::set<Capability> allowedCaps;
    bool showCapabilities = false; // --capabilities: kullanılan cap'leri raporla (saqut ir)
    // `--` sonrası argümanlar — sys::args() ile programa geçilir.
    std::vector<std::string> programArgs;
};

// ============================================================================
// parseArgs
// ============================================================================
inline CliArgs parseArgs(int argc, char* argv[]) {
    CliArgs args;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // `--` sonrası her şey programArgs — sys::args() (#90).
        if (arg == "--") {
            for (int j = i + 1; j < argc; j++)
                args.programArgs.push_back(argv[j]);
            break;
        }
        if (arg == "--allow-fs")  { args.allowedCaps.insert(Capability::Fs);  continue; }
        if (arg == "--allow-net") { args.allowedCaps.insert(Capability::Net); continue; }
        if (arg == "--allow-sys") { args.allowedCaps.insert(Capability::Sys); continue; }
        if (arg == "--capabilities") { args.showCapabilities = true; continue; }

        if (arg == "-") {
            args.stdinMode = true;
            continue;
        }
        if (arg == "-h" || arg == "--help") {
            args.showHelp = true;
            continue;
        }
        if (arg.compare(0, 9, "--output=") == 0) {
            args.outputFile = arg.substr(9);
            continue;
        }
        if (arg.compare(0, 9, "--format=") == 0 || arg == "--format") {
            std::cerr << "error: --format option was removed; it was not consumed by any command\n";
            exit(saqut::exit_code::kUsageError);
        }
        if (arg == "--output" || arg == "-o") {
            if (i + 1 < argc) args.outputFile = argv[++i];
            continue;
        }
        if (arg == "--compact") {
            args.compact = true;
            continue;
        }
        if (arg == "--json") {
            args.jsonOutput = true;
            continue;
        }
        if (arg == "--jsonl") {
            args.jsonlOutput = true;
            continue;
        }
        if (arg == "--optimized") {
            args.optimized = true;
            continue;
        }
        if (arg == "--compile-only") {
            args.compileOnly = true;
            continue;
        }
        if (arg == "--verbose" || arg == "-v") {
            args.verbose = true;
            continue;
        }
        if (arg == "--version" || arg == "-V") {
            std::cout << "saQut " << SAQUT_VERSION << "\n";
            exit(0);
        }
        if (arg.compare(0, 7, "--runs=") == 0) {
            try { args.benchRuns = std::stoi(arg.substr(7)); } catch (...) {}
            continue;
        }
        if (arg.compare(0, 15, "--gc-threshold=") == 0) {
            try { args.gcThreshold = std::stoi(arg.substr(15)); } catch (...) {}
            continue;
        }
        if (arg == "--gc-stats") {
            args.gcStats = true;
            continue;
        }
        if (arg == "--jit") {
            args.useJit = true;
            continue;
        }
        if (arg == "--profile") {
            args.profile = true;
            continue;
        }
        if (arg.compare(0, 5, "file:") == 0) {
            args.positional.push_back(arg.substr(5));
            continue;
        }
        if (arg.compare(0, 7, "output:") == 0) {
            args.outputFile = arg.substr(7);
            continue;
        }
        if (arg.compare(0, 4, "ast:") == 0) {
            args.outputFile = arg.substr(4);
            continue;
        }

        // İlk argüman komut mu?
        if (args.command.empty() && i == 1) {
            if (arg == "run"    || arg == "tokens"  || arg == "ast" ||
                arg == "symbols" || arg == "check"   || arg == "ir"      ||
                arg == "exec"    || arg == "lsp"     || arg == "dap"     ||
                arg == "bench"   ||
                arg == "compile" || arg == "parse"   || arg == "transpile" ||
                arg == "interpret") {
                args.command = arg;
                continue;
            }
            args.command = "run";
            args.positional.push_back(arg);
            continue;
        }

        args.positional.push_back(arg);
    }

    if (args.command.empty()) args.command = "run";
    if (args.positional.empty() && !args.stdinMode)
        args.positional.push_back("source.sqt");

    return args;
}

// ============================================================================
// readSource: Dosyadan veya stdin'den kaynak kod oku (TODO: stdin)
// ============================================================================
inline std::string readSource(const CliArgs& args) {
    if (args.stdinMode) {
        // TODO: read from std::cin until EOF
        std::cerr << "TODO: stdin mode not yet supported\n";
        return "";
    }
    if (args.positional.empty()) return "";

    std::string path = args.positional[0];
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "error: cannot open file '" << path << "'\n";
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// ============================================================================
// inputFilePath: Kaynak dosyanın yolunu döndür (stdin modunda boş string)
// ============================================================================
inline std::string inputFilePath(const CliArgs& args) {
    if (args.stdinMode || args.positional.empty()) return "";
    return args.positional[0];
}

#endif // SAQUT_CLI_ARGS
