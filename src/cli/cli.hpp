// ============================================================================
// saQut Compiler — CLI Dispatcher
// ============================================================================
//
// DİZİN:   src/cli/cli.hpp
// BAĞIMLI: args.hpp, commands/*
//
// AMAÇ:
//   Komut kaydı ve dağıtımı. Yeni bir komut eklemek için:
//   1. src/cli/commands/x.hpp oluştur
//   2. registerCommand() ile kaydet
//
// MİMARİ:
//   Her komut bir CliCommand struct'ıdır:
//     - name:        "run", "tokens", "ast", ...
//     - description: Yardım metninde görünür
//     - hidden:      true ise yardımda listelenmez (alias'lar için)
//     - execute:     int(CliArgs&) döndürür (0 = başarılı)
//
//   Komutlar lazy olarak include edilmez — her biri kendi header'ında
//   inline fonksiyon olarak tanımlanır ve cli.hpp tarafından include edilir.
//
// ============================================================================

#ifndef SAQUT_CLI
#define SAQUT_CLI

#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include "cli/args.hpp"

// ============================================================================
// CliCommand — Kayıtlı bir komut
// ============================================================================
struct CliCommand {
    std::string name;
    std::string description;
    bool hidden = false;  // true = yardımda gösterme
    std::function<int(const CliArgs&)> execute;
};

// ============================================================================
// CliDispatcher — Komut kaydı ve çalıştırma
// ============================================================================
class CliDispatcher {
public:
    void registerCommand(const CliCommand& cmd) {
        commands.push_back(cmd);
    }

    int dispatch(const CliArgs& args) const {
        // Yardım özel durumu
        if (args.showHelp || args.command == "help") {
            printHelp();
            return 0;
        }

        // Komutu bul
        for (auto& cmd : commands) {
            if (cmd.name == args.command) {
                return cmd.execute(args);
            }
        }

        // Unknown command
        std::cerr << "error: unknown command '" << args.command << "'\n";
        std::cerr << "for available commands: saqut --help\n";
        return 1;
    }

    void printHelp() const {
        std::cout << "saqut <command> [options]\n\n";

        for (auto& cmd : commands) {
            if (cmd.hidden) continue;
            std::cout << "  " << cmd.name << " ----- " << commandUsage(cmd.name) << "\n";
        }

        std::cout << "\n  options ----- saqut [--help] [--version] [--allow fs,net,sys]\n";
    }

private:
    static std::string commandUsage(const std::string& command) {
        if (command == "run")     return "saqut run <file> [--jit]";
        if (command == "tokens")  return "saqut tokens <file>";
        if (command == "ast")     return "saqut ast <file> [--json]";
        if (command == "symbols") return "saqut symbols <file> [--jsonl]";
        if (command == "check")   return "saqut check <file>";
        if (command == "ir")      return "saqut ir <file> [--optimized] [--cfg]";
        if (command == "exec")    return "saqut exec \"<expression>\" [--jit]";
        if (command == "lsp")     return "saqut lsp";
        if (command == "dap")     return "saqut dap";
        if (command == "bench")   return "saqut bench <file> [--jit] --runs=<iterations>";
        if (command == "compile") return "saqut compile <file>";
        if (command == "parse")   return "saqut parse <file>";
        if (command == "transpile") return "saqut transpile <file>";
        return "saqut " + command;
    }

    std::vector<CliCommand> commands;
};

#endif // SAQUT_CLI
