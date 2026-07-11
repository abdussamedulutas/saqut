# Kod Dokümantasyon İndeksi

Bu dizin, saQut derleyicisinin her modülü için tek sayfalık mimari dokümantasyon
içerir. Her belge: sorumluluk, dosya envanteri, ana tipler, veri akışı, diğer
modüllerle temas, tasarım kararları ve bilinen sınırları kapsar.

| Modül | Belge | Sorumluluk |
|-------|-------|------------|
| Çekirdek | [core.md](core.md) | Tip sistemi, konum, kaynak dosya, decimal, modül kaydı, yapılandırma |
| Tanılama | [diagnostic.md](diagnostic.md) | Hata/uyarı biriktirme ve raporlama |
| Sözcüksel Analiz | [tokenizer.md](tokenizer.md) | Lexer + Tokenizer — kaynak kodu token dizisine dönüştürme |
| Ayrıştırıcı | [parser.md](parser.md) | Pratt parsing — token listesinden AST üretme |
| Sembol Sistemi | [symbol.md](symbol.md) | 3 geçişli sembol toplama, kapsam yönetimi |
| Semantik Analiz | [semantic.md](semantic.md) | TypeChecker + StructuralValidator |
| Optimizasyon | [opt.md](opt.md) | Sabit katlama, ölü kod eleme, AST klonlama |
| Ara Temsil | [ir.md](ir.md) | IRGenerator — 3-adresli bytecode üretimi |
| Sanal Makine | [vm.md](vm.md) | Interpreter — bytecode yorumlama, GC, DAP API |
| Modül Sistemi | [module.md](module.md) | Import çözümleme, çok dosyalı derleme |
| Yerleşik Metodlar | [builtin.md](builtin.md) | Array/String/Struct built-in metod kaydı |
| Komut Satırı | [cli.md](cli.md) | CLI komutları ve pipeline yönetimi |
| Dil Sunucusu | [lsp.md](lsp.md) | LSP protokolü — completion, definition, hover |
| Hata Ayıklama | [dap.md](dap.md) | DAP protokolü — breakpoint, adımlama, değişkenler |
| Performans | [bench.md](bench.md) | Profil ölçüm altyapısı |
