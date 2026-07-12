// ============================================================================
// saQut LSP — DocumentStore (Açık Belgelerin Yöneticisi)
// ============================================================================
//
// DİZİN:   src/lsp/document_store.hpp
// KATMAN:  LSP — Açık belgelerin buffer'larını yönetir, overlay ile derler
//
// AMAÇ:
//   didOpen/didChange/didClose ile belge durumunu takip eder.
//   runPipeline() tüm açık belgeleri ModuleLoader overlay'i ile derler.
//
// ============================================================================

#ifndef SAQUT_LSP_DOCUMENT_STORE
#define SAQUT_LSP_DOCUMENT_STORE

#include <string>
#include <unordered_map>
#include <memory>
#include "parser/ast_node.hpp"
#include "symbol/symbol_table.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "core/module_registry.hpp"
#include "tokenizer/token.hpp"

struct DocumentState {
    std::string      uri;
    std::string      content;
    // content'in satır-başlangıç byte offset indeksi (position.hpp
    // buildLineStarts). Konum dönüşümü yapan döngüler (documentSymbol,
    // diagnostics, references...) satır metnine dosya başından taramadan
    // O(1) erişsin diye content ile birlikte güncellenir.
    std::vector<int> lineStarts;
    int              version = 0;
    ASTNode*         ast     = nullptr;
    // Faz 3: bu belgenin canonical dosya yolu (uriToPath + weakly_canonical).
    // symbolByOffset'i doldururken ve çok-dosyalı sorgularda "bu sembol BENİM
    // dosyamda mı" testinde kullanılır — ModuleLoader'ın SourceLocation.filePath'e
    // yazdığı yolla aynı biçimde (canonical) olmalı.
    std::string      filePath;
    // Faz 3: bu turun tokenleri — ast ile birlikte sahiplenilir (IdentifierNode
    // ::lexerToken bunlara işaret eder). findSymbolAt konum→token binary search'ü
    // için kullanır (kök neden #3).
    std::vector<Token*> tokens;
    // Faz 3: bu dosyaya ait (offset → Symbol*) indeksi — runPipeline'da bir kez
    // kurulur (SymbolTable.allSymbols() + her sembolün definitionLoc/references'ı,
    // yalnızca filePath == bu belgenin filePath'i olanlar). findSymbolAt burada
    // O(1) arar; isim-uzunluğu aralık eşleştirmesi ve allSymbols lineer taraması
    // artık yok.
    std::unordered_map<int, Symbol*> symbolByOffset;
    // Faz 2 ("son iyi tablo"): SymbolTable unique_ptr tabanlı sahiplik kullandığı
    // için kopyalanamaz, yalnızca taşınabilir — bu yüzden ayrı bir
    // lastGoodSymbolTable alanı yerine symbolTable'ın KENDİSİ bu rolü üstlenir:
    // runPipeline yalnızca yeni bir tablo üretebildiğinde üzerine yazar (bkz.
    // document_store.cpp), modül hiç yüklenemediğinde dokunmadan bırakır.
    // TODO(faz-ileri): SymbolCollector bir gün gerçekten yarıda kesilebilir hale
    // gelirse (bugün mümkün değil — hep tamamlanır), gerçek bir "son iyi" anlık
    // görüntüsü için SymbolTable derin kopyalanabilir hale getirilmeli.
    SymbolTable      symbolTable;
    DiagnosticEngine diagnostics;

    ~DocumentState() {
        delete ast;
        for (auto* t : tokens) delete t;
    }
    DocumentState() = default;
    DocumentState(const DocumentState&) = delete;
    DocumentState& operator=(const DocumentState&) = delete;
};

class DocumentStore {
public:
    DocumentState& update(const std::string& uri,
                          const std::string& content, int version);
    DocumentState* get(const std::string& uri);
    void           close(const std::string& uri);

    // Verilen canonical dosya yolunun içeriğini döndürür: açık bir belgeyse
    // buffer'ı, değilse diski okur (bulunamazsa boş string). Faz 3 — çok-dosya
    // konum dönüşümü (definition/references başka dosyaya işaret ettiğinde o
    // dosyanın satır metnine ihtiyaç var, bkz. src/lsp/position.hpp).
    std::string contentForPath(const std::string& path) const;

    // Verilen canonical dosya yolu için URI döndürür: dosya açık bir belgeyse
    // İSTEMCİNİN o belgeyi açarken gönderdiği ORİJİNAL uri string'i (böylece
    // aynı dosya için sonuç her zaman istemcinin kendi URI biçimiyle —
    // ör. yüzde-kodlamasız/kodlamalı — geri döner); açık değilse pathToUri
    // ile sentezlenmiş bir URI (Faz 3 — çok-dosya URI, kök neden #4).
    std::string uriForPath(const std::string& path) const;

private:
    void runPipeline(DocumentState& state);

    // store_'daki açık belgeler arasında canonical yola göre arar; bulursa
    // buffer'ını `out`'a yazar. Diske DÜŞMEZ — çağıran karar verir (ModuleLoader
    // overlay'i disk fallback'i kendi yapar, bkz. document_store.cpp).
    bool openContent(const std::string& path, std::string& out) const;

    std::unordered_map<std::string, std::unique_ptr<DocumentState>> store_;
};

#endif // SAQUT_LSP_DOCUMENT_STORE
