#ifndef SAQUT_MODULE_LOADER
#define SAQUT_MODULE_LOADER

#include <string>
#include <unordered_set>
#include <unordered_map>
#include "module/module_graph.hpp"
#include "core/module_registry.hpp"
#include "diagnostic/diagnostic_engine.hpp"

// ModuleLoader: import bildirimlerini izleyerek tüm bağımlı dosyaları
// yükler ve parse eder. Döngüsel bağımlılıklar sessizce atlanır (hata değil).
//
// Kullanım:
//   ModuleRegistry registry;
//   DiagnosticEngine diag;
//   ModuleLoader loader(registry, diag);
//   ModuleGraph graph = loader.load("main.sqt");
class ModuleLoader {
public:
    ModuleLoader(ModuleRegistry& registry, DiagnosticEngine& diag)
        : registry_(registry), diag_(diag) {}

    // Giriş dosyasından başlayarak tüm bağımlı modülleri yükle.
    // units[0] her zaman giriş dosyasıdır.
    ModuleGraph load(const std::string& entryFilePath);

private:
    // Tek bir dosyayı yükle, parse et, ImportDeclNode'larını takip et.
    // Zaten yüklenmiş dosyalar atlanır (seen_ ile kontrol).
    void loadUnit(const std::string& filePath, ModuleGraph& graph);

    // İmport yolunu çözümle: import eden dosyanın dizinine göre canonical yol üret.
    std::string resolvePath(const std::string& importerPath,
                            const std::string& rawPath);

    ModuleRegistry&   registry_;
    DiagnosticEngine& diag_;

    // Zaten yüklenmiş ya da yüklenmekte olan dosyalar (canonical path).
    std::unordered_set<std::string> seen_;
};

#endif // SAQUT_MODULE_LOADER
