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

        // Bilinmeyen komut
        std::cerr << "Hata: Bilinmeyen komut '" << args.command << "'\n";
        std::cerr << "Kullanılabilir komutlar için: saqut --help\n";
        return 1;
    }

    void printHelp() const {
        std::cout << "saQut Compiler — Dil Bağımsız Derleyici Alet Çantası\n\n";
        std::cout << "KULLANIM:\n";
        std::cout << "  saqut <komut> [dosya] [seçenekler]\n";
        std::cout << "  saqut -                          (stdin modu — TODO)\n\n";
        std::cout << "KOMUTLAR:\n";

        for (auto& cmd : commands) {
            if (cmd.hidden) continue;
            std::cout << "  " << cmd.name;
            // 12 karaktere hizala
            size_t pad = cmd.name.size() < 11 ? 11 - cmd.name.size() : 1;
            std::cout << std::string(pad, ' ') << cmd.description << "\n";
        }

        std::cout << "\nSEÇENEKLER:\n";
        std::cout << "  -o, --output <dosya>   Çıktı dosyası\n";
        std::cout << "  --format <json|text>   Çıktı formatı (varsayılan: text)\n";
        std::cout << "  -h, --help             Bu yardım metni\n\n";
        std::cout << "ÖRNEK:\n";
        std::cout << "  saqut run source.sqt\n";
        std::cout << "  saqut tokens source.sqt\n";
        std::cout << "  saqut ast source.sqt --format=json\n";
        std::cout << "  saqut symbols source.sqt\n";
    }

private:
    std::vector<CliCommand> commands;
};

#endif // SAQUT_CLI
