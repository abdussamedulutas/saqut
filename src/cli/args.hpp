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

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct CliArgs {
    std::string command;
    std::vector<std::string> positional;
    std::string outputFile;
    std::string format;
    bool showHelp   = false;
    bool stdinMode  = false;
    bool compact    = false;  // --compact: boşluksuz JSON
    bool optimized  = false;  // --optimized: sabit katlama + ölü kod eleme
    bool jsonOutput = false;  // --json: JSON çıktı üret (varsayılan: düz metin)
};

// ============================================================================
// parseArgs
// ============================================================================
inline CliArgs parseArgs(int argc, char* argv[]) {
    CliArgs args;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

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
        if (arg.compare(0, 9, "--format=") == 0) {
            args.format = arg.substr(9);
            continue;
        }
        if (arg == "--output" || arg == "-o") {
            if (i + 1 < argc) args.outputFile = argv[++i];
            continue;
        }
        if (arg == "--format") {
            if (i + 1 < argc) args.format = argv[++i];
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
        if (arg == "--optimized") {
            args.optimized = true;
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
