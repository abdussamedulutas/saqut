// ============================================================================
// saQut — ModuleRegistry (Modül Adı Havuzu)
//
// Derleyici pipeline'ı boyunca modül adlarını (dosya yollarını) tek merkezde
// tutar; her yerde int ID kullanılır, string kopyalanmaz.
//
// YAŞAM DÖNGÜSÜ:
//   IRProgram içinde yaşar. IRGenerator üretim sırasında intern() ile
//   dosya adını kaydeder; Interpreter çalışma sırasında filePath() ile
//   orijinal string'e ulaşır.
//
// ÖZEL ID'LER:
//   INVALID_ID = -1  →  atanmamış / bilinmiyor
//   BUILTIN_ID =  0  →  "__builtin__" (compile-time sabit; runtime gerekmez)
// ============================================================================

#ifndef SAQUT_CORE_MODULE_REGISTRY
#define SAQUT_CORE_MODULE_REGISTRY

#include <string>
#include <vector>
#include <unordered_map>

class ModuleRegistry {
public:
    static constexpr int INVALID_ID = -1;
    static constexpr int BUILTIN_ID =  0;   // paths_[0] = "__builtin__"

    ModuleRegistry() {
        paths_.push_back("__builtin__");
        index_["__builtin__"] = 0;
    }

    // filePath'i kayıt et; zaten varsa aynı ID'yi döndür.
    int intern(const std::string& path) {
        auto it = index_.find(path);
        if (it != index_.end()) return it->second;
        int id = (int)paths_.size();
        index_[path] = id;
        paths_.push_back(path);
        return id;
    }

    // ID → filePath; geçersiz ID'de "<invalid>" döner.
    const std::string& filePath(int id) const {
        static const std::string invalid = "<invalid>";
        if (id < 0 || id >= (int)paths_.size()) return invalid;
        return paths_[id];
    }

    int size() const { return (int)paths_.size(); }

private:
    std::vector<std::string>            paths_;
    std::unordered_map<std::string, int> index_;
};

#endif // SAQUT_CORE_MODULE_REGISTRY
