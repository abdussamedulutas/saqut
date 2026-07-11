#include "module/module_loader.hpp"
#include "parser/nodes/declarations.hpp"
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// load — giriş dosyasından başlayarak tüm bağımlılıkları yükle
// ─────────────────────────────────────────────────────────────────────────────

ModuleGraph ModuleLoader::load(const std::string& entryFilePath) {
    ModuleGraph graph;
    std::string canonical = fs::weakly_canonical(entryFilePath).string();
    loadUnit(canonical, graph);
    return graph;
}

// ─────────────────────────────────────────────────────────────────────────────
// loadUnit — tek bir dosyayı yükle, ImportDeclNode'larını takip et
// ─────────────────────────────────────────────────────────────────────────────

void ModuleLoader::loadUnit(const std::string& filePath, ModuleGraph& graph) {
    if (seen_.count(filePath)) return;
    seen_.insert(filePath);

    // Kaynağı önce overlay'den dene (editör buffer'ı), yoksa diske düş.
    std::string source;
    bool haveSource = overlay_ && overlay_(filePath, source);
    if (!haveSource) {
        std::ifstream file(filePath, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            diag_.report("E_MODULE_NOT_FOUND", SourceLocation{},
                "cannot open module '" + filePath + "': file not found");
            return;
        }
        std::stringstream buf;
        buf << file.rdbuf();
        source = buf.str();
    }

    // Tokenize + parse
    Tokenizer tokenizer;
    auto tokens = tokenizer.scan(source, filePath);

    Parser parser;
    ASTNode* ast = parser.parse(tokens);
    if (!ast) {
        diag_.report("E_MODULE_PARSE", SourceLocation{},
            "failed to parse module '" + filePath + "'");
        for (auto* t : tokens) delete t;
        return;
    }

    // ModuleUnit oluştur ve graph'a ekle
    ModuleUnit unit;
    unit.filePath = filePath;
    unit.moduleId = registry_.intern(filePath);
    unit.ast      = ast;
    unit.tokens   = std::move(tokens);
    graph.units.push_back(std::move(unit));

    // ImportDeclNode'ları tara ve bağımlıları yükle
    for (ASTNode* child : ast->getChildren()) {
        if (child->kind != ASTKind::ImportDecl) continue;
        auto* imp = static_cast<ImportDeclNode*>(child);
        if (imp->sourcePath.empty()) continue;

        std::string depPath = resolvePath(filePath, imp->sourcePath);
        loadUnit(depPath, graph);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// resolvePath — import eden dosyanın dizinine göre canonical yol üret
// ─────────────────────────────────────────────────────────────────────────────

std::string ModuleLoader::resolvePath(const std::string& importerPath,
                                      const std::string& rawPath) {
    fs::path base   = fs::path(importerPath).parent_path();
    fs::path target = base / rawPath;
    return fs::weakly_canonical(target).string();
}
