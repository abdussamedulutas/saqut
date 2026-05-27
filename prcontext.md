# PR Açıklaması: Parser ve AST Bileşenlerinin Modülerleştirilmesi

## Açıklama
Bu PR, `src/parser/` dizini altındaki devasa `ast.hpp` ve `parser.hpp` dosyalarını mantıksal parçalara ayırarak kodun okunabilirliğini ve bakımını kolaylaştırmayı amaçlar. Kodun işlevselliği korunmuş, sadece dosya yapısı modüler hale getirilmiştir.

## Önemli Değişiklikler

### 1. AST (Soyut Sözdizim Ağacı) Modülerleştirme
`ast.hpp` dosyası artık bir **aggregator** (toplayıcı) görevi görüyor ve aşağıdaki yeni dosyalardan bileşenleri dahil ediyor:
- `ast_node.hpp`: Temel `ASTNode` sınıfı ve `ASTKind` enum'u.
- `ast_expr.hpp`: `LiteralNode`, `BinaryExpressionNode`, `CallExpressionNode` gibi ifade düğümleri.
- `ast_stmt.hpp`: `BlockNode`, `IfStatementNode`, `WhileStatementNode` gibi deyim (statement) düğümleri.
- `ast_decl.hpp`: `FunctionDeclNode`, `VariableDeclNode`, `StructDeclNode` gibi deklarasyon düğümleri.
- `ast_json.hpp`: JSON serileştirme için yardımcı fonksiyonlar (`childrenToJson`, `jsonEscape` vb.).

### 2. Parser Modülerleştirme
Parser mantığı da benzer şekilde parçalara ayrıldı:
- `parser_base.hpp`: `Parser` sınıfı tanımı ve üye değişkenleri.
- `parser_core.hpp`: Pratt Parser ana döngüsü, NUD ve LED mantığı.
- `parser_decl.hpp`: Fonksiyon, değişken ve struct deklarasyonlarının ayrıştırılması.
- `parser_stmt.hpp`: Deyimlerin (if, for, while, return vb.) ayrıştırılması.

## Teknik Avantajlar
- **Okunabilirlik:** Binlerce satırlık dosyalar yerine 100-300 satırlık, spesifik görevleri olan dosyalar oluşturuldu.
- **Bakım Kolaylığı:** Belirli bir dil özelliği (örn. yeni bir deyim tipi) eklendiğinde hangi dosyanın değiştirileceği artık çok daha net.
- **Derleme Hızı:** (Gelecekte) İncremental build süreçlerinde sadece değişen parçaların derlenmesine olanak sağlar.

## Notlar
- Mevcut tüm testler ve `Final.sqt` gibi örnek dosyaların ayrıştırılması sorunsuz çalışmaya devam etmektedir.
- Dosya başlıklarındaki DİZİN ve KATMAN bilgileri güncellenmiştir.
