# saQut Compiler Toolbox — Yol Haritası ve Yapılacaklar

> **Felsefe**: saQut bir derleyici değil, bir **derleyici alet çantasıdır**.
> Amacı en hızlı kodu üretmek, en güvenli sandbox'u kurmak veya en optimize
> binary'yi çıkarmak değildir. Amacı, **derleyicinin kendisidir**.
>
> Her aşama (Lexer, Parser, Optimizer, Compiler) birbirinden bağımsız,
> kendi parametreleriyle yönlendirilebilen, gerektiğinde devre dışı
> bırakılabilen birer **araç kutusu modülü** olarak tasarlanır.
>
> Geliştirici, isterse sadece token listesini alır, isterse AST'yi JSON
> olarak çeker, isterse sembol tablosunu inceler, isterse kodu çalıştırır.
> Bu, ileride LSP sunucusu yazmayı, IDE eklentileri geliştirmeyi ve
> kod hiyerarşisi görselleştirmeyi mümkün kılar.

---

## Temel Prensipler

1. **Her aşama bağımsız**: Lexer → Tokenizer → Parser → Optimizer → IR → Backend.
   Her biri tek başına çalışabilir, kendi parametrelerini alır.

2. **Her şey metadata**: Her token ve AST düğümü; dosya adı, satır, sütun,
   karakter offset'i taşır. Debug ve hata mesajları bu veri üzerine kurulur.

3. **AST bellek canavarı**: Token'lar AST'de yaşar. Kaynak kodun neredeyse
   birebir izdüşümü AST üzerinde korunur. Hiçbir bilgi atılmaz.

4. **Sembol tablosu aynı felsefede**: Basit yapı, çok veri. Her sembolün
   tanımlandığı yer, tipi, kullanıldığı tüm noktalar, scope bilgisi.

5. **Feature toggle**: Her keyword, her operatör, her optimizasyon adımı
   açılıp kapatılabilir. `--disable-while`, `--skip-constant-folding`.

6. **Çoklu çıktı formatı**: Düz metin, JSON, (ileride) HTML/SVG. Tüm ara
   gösterimler dışa aktarılabilir.

7. **IR merkezli backend**: Tüm backend'ler (C transpile, QBE, LLVM, JIT)
   aynı IR'den beslenir. Yeni backend eklemek = yeni bir IR yürüteci yazmak.

---

## Aşama 0: Metadata ve Konum Takibi

> **Hedef**: Her token ve AST düğümü nereden geldiğini bilsin.

### 0.1 SourceFile Yöneticisi
- [ ] `SourceFile` sınıfı: dosya adı, tam metin, satır başı offset'leri
- [ ] `SourceLocation` yapısı: `{file, line, column, offset}`
- [ ] `offset → (line, column)` dönüşümü (binary search ile O(log n))
- [ ] Çoklu dosya desteği (import/include için temel)

### 0.2 Lexer'a Konum Ekleme
- [ ] Lexer her `nextChar()`'da satır/sütun takibi yapsın
- [ ] `getLocation()` → mevcut konumu `SourceLocation` olarak döndürsün
- [ ] `INumber` yapısına `SourceLocation startLoc, endLoc` eklensin

### 0.3 Token'lara Konum Ekleme
- [ ] `Token` temel sınıfına `SourceLocation loc` eklensin
- [ ] Her token oluşturulurken konumu kaydedilsin
- [ ] `start`/`end` offset'leri `SourceLocation` ile değiştirilsin

### 0.4 AST Düğümlerine Konum Ekleme
- [ ] `ASTNode` temel sınıfına `SourceLocation loc` eklensin
- [ ] Her düğüm oluşturulurken ilgili token'ın konumu kopyalansın

---

## Aşama 1: Interpreter (CLI + Dosya)

> **Hedef**: `saqut` komutu ile REPL veya dosya çalıştırma.

### 1.1 CLI Altyapısı
- [ ] `argc/argv` ile komut satırı argümanları
- [ ] `./saqut` → REPL modu (etkileşimli)
- [ ] `./saqut file.sqt` → dosyayı çalıştır
- [ ] `./saqut --mode=parse file.sqt` → sadece AST üret
- [ ] `./saqut --mode=tokens file.sqt` → sadece token listesi
- [ ] `./saqut --mode=ir file.sqt` → sadece IR
- [ ] `./saqut --mode=symbols file.sqt` → sembol tablosu
- [ ] `./saqut --format=json file.sqt` → JSON çıktı

### 1.2 REPL Modu
- [ ] `>` komut istemi
- [ ] Her satır ayrı ayrı parse edilir
- [ ] `.ast` komutu → son ifadenin AST'sini göster
- [ ] `.tokens` komutu → son ifadenin token'larını göster
- [ ] `.symbols` komutu → şu ana kadarki sembolleri göster
- [ ] `.exit` / `.quit` → çıkış
- [ ] Çok satırlı giriş (bloklar için)

### 1.3 Dosya Modu
- [ ] Kaynak dosyayı oku → pipeline'ı çalıştır
- [ ] Çıktıyı seçilen formatta bas
- [ ] `-o output.json` → dosyaya yaz

---

## Aşama 2: AST'yi Bellek Canavarı Yapma

> **Hedef**: AST, kaynak kodun tam bir izdüşümü olsun.
> Hiçbir bilgi kaybolmasın. Token'lar AST'de yaşasın.

### 2.1 Token Tutma
- [ ] AST düğümleri token'ları **sahiplensin** (unique_ptr veya shared_ptr)
- [ ] `LiteralNode`, `IdentifierNode` → ilgili token'ı doğrudan tutar
- [ ] `BinaryExpressionNode` → operatör token'ını tutar
- [ ] Her statement → ilgili keyword token'ını tutar (`if` token'ı, `for` token'ı)

### 2.2 Kaynak Kod Parçası Erişimi
- [ ] `ASTNode::getSourceText()` → bu düğümün kaynak koddaki ham metni
- [ ] `ASTNode::getSourceRange()` → başlangıç/bitiş SourceLocation
- [ ] Hata mesajlarında kaynak kod parçası gösterimi:
  ```
  Hata: test.sqt:5:10: 'x' değişkeni tanımlı değil
    4 | int main() {
    5 |     y = x + 1;
      |         ^
    6 | }
  ```

### 2.3 AST Görselleştirme
- [ ] `--format=json` → tam AST'yi JSON olarak dök
- [ ] `--format=dot` → Graphviz DOT formatı (görsel ağaç)
- [ ] `--format=html` → Etkileşimli HTML ağaç görünümü (ileride)
- [ ] JSON çıktısı her düğüm için:
  ```json
  {
    "kind": "BinaryExpression",
    "operator": "PLUS",
    "location": { "file": "test.sqt", "line": 5, "column": 9 },
    "sourceText": "x + 1",
    "left": { ... },
    "right": { ... }
  }
  ```

---

## Aşama 3: Sembol Tablosu

> **Hedef**: AST kadar zengin, basit yapılı bir sembol tablosu.
> Her sembolün tüm kullanım noktaları, tip bilgisi, scope'u kayıtlı.

### 3.1 Symbol Sınıfı
- [ ] `Symbol` yapısı:
  - `name`: sembol adı
  - `kind`: variable, function, parameter, type, ...
  - `type`: tip bilgisi (şimdilik string, ileride Type sistemi)
  - `definitionLoc`: tanımlandığı SourceLocation
  - `references`: kullanıldığı tüm SourceLocation'ların listesi
  - `scope`: ait olduğu scope (global, function, block)
  - `metadata`: opsiyonel anahtar-değer çiftleri

### 3.2 SymbolTable Sınıfı
- [ ] İç içe scope desteği (global → function → block)
- [ ] `enterScope()` / `exitScope()`
- [ ] `define(name, kind, type, location)` → yeni sembol ekle
- [ ] `resolve(name)` → en yakın scope'tan başlayarak ara
- [ ] `addReference(name, location)` → kullanım noktası ekle
- [ ] `getAllSymbols()` → tüm scope'lardaki tüm semboller (düz liste)
- [ ] JSON'a serialize edilebilme

### 3.3 Sembol Toplama (AST Walker)
- [ ] `SymbolCollector` sınıfı: AST'yi dolaşır, sembolleri toplar
- [ ] Değişken tanımlarını yakala → `define()`
- [ ] Değişken kullanımlarını yakala → `addReference()`
- [ ] Fonksiyon tanımlarını yakala
- [ ] Hata durumları: tanımsız değişken, çift tanım, tip uyuşmazlığı

---

## Aşama 4: Feature Toggle Sistemi

> **Hedef**: Derleyicinin her özelliği açılıp kapatılabilir olsun.

### 4.1 CompilerConfig Yapısı
- [ ] `CompilerConfig` struct'ı:
  ```cpp
  struct CompilerConfig {
      // Dil özellikleri
      bool enableWhile     = true;
      bool enableFor       = true;
      bool enableDoWhile   = true;
      bool enableSwitch    = true;
      bool enableClass     = false;  // henüz yok
      bool enableInterface = false;
      bool enableEnum      = false;
      
      // Operatörler
      bool enableTernary   = true;
      bool enablePostfix   = true;
      bool enableUnary     = true;
      
      // Optimizasyon adımları
      bool optConstantFolding   = false;
      bool optDeadCodeElim      = false;
      bool optUnusedVariable    = false;
      
      // Çıktı
      std::string outputFormat  = "text";  // text, json
      std::string mode          = "run";   // tokens, ast, ir, symbols, run
      std::string inputFile;
      std::string outputFile;
  };
  ```

### 4.2 Keyword Kontrolü
- [ ] Tokenizer, config'te kapalı keyword'leri identifier olarak tanısın
- [ ] Parser, kapalı keyword'ler için hata değil, görmezden gelsin
- [ ] `--disable-while` → `while` artık bir keyword değil

### 4.3 Optimizasyon Kontrolü
- [ ] Her optimizasyon adımı bir `OptimizationPass` soyut sınıfı
- [ ] `OptimizationManager`: config'e göre pas'ları çalıştırır
- [ ] `--skip-constant-folding` → o adım atlanır
- [ ] `--opt-all` → tüm optimizasyonlar aktif
- [ ] `--opt-none` → hiçbiri aktif değil

---

## Aşama 5: Çoklu Backend Altyapısı (IR Merkezli)

> **Hedef**: Tüm backend'ler aynı IR'den beslenir.
> Yeni backend eklemek = yeni bir `IRRunner` yazmak.

### 5.1 IR'yi Güçlendirme
- [ ] Kontrol akışı: `br`, `jmp`, `cmp`, `br_eq`, `br_lt`, `br_gt`
- [ ] Fonksiyon: `call`, `ret`, `param`
- [ ] Bellek: `load`, `store`, `alloca`
- [ ] Tip bilgisi: her IR komutu tipleri taşısın
- [ ] Debug bilgisi: kaynak satır eşlemesi
- [ ] `IRModule` sınıfı: fonksiyon listesi, global değişkenler, IR komutları

### 5.2 Backend Arayüzü
- [ ] `IBackend` soyut sınıfı:
  ```cpp
  class IBackend {
  public:
      virtual bool initialize(CompilerConfig& config) = 0;
      virtual bool compile(IRModule& ir) = 0;
      virtual bool run() = 0;
      virtual std::string getOutput() = 0;
      virtual ~IBackend() = default;
  };
  ```

### 5.3 C Transpile Backend (Aşama 1)
- [ ] IR → C kaynak kodu dönüşümü
- [ ] Değişken tanımları, if/for/while, fonksiyonlar
- [ ] GCC/Clang ile derleme ve çalıştırma
- [ ] Üretilen C kodu okunabilir ve debug edilebilir olmalı

### 5.4 Interpreter Backend (Aşama 1)
- [ ] IR'yi doğrudan yürüten sanal makine
- [ ] Stack tabanlı veya register tabanlı
- [ ] Adım adım çalıştırma (step) desteği
- [ ] Değişken değerlerini gösterme

### 5.5 QBE Backend (Aşama 2)
- [ ] QBE C API'si ile entegrasyon
- [ ] IR → QBE IR dönüşümü
- [ ] Native binary üretimi

### 5.6 LLVM Backend (Aşama 3, opsiyonel)
- [ ] LLVM C API ile entegrasyon
- [ ] IR → LLVM IR dönüşümü
- [ ] Agresif optimizasyonlar, çok platformlu kod üretimi

---

## Aşama 6: Hata Sistemi

> **Hedef**: Sadece hata mesajı değil, **açıklama**.
> Kullanıcıya neyi yanlış yaptığını ve nasıl düzelteceğini söyle.

### 6.1 Diagnostic Sistemi
- [ ] `Diagnostic` yapısı: `{level, location, message, hint, sourceLine}`
- [ ] Seviyeler: error, warning, note, hint
- [ ] Birden fazla diagnostic toplama (ilk hatada durma)
- [ ] `DiagnosticEngine`: diagnostic'leri toplar, formatlar, basar

### 6.2 Hata Mesajı Formatı
- [ ] Kaynak satır gösterimi (^^^^ işaretleme)
- [ ] Renkli çıktı (opsiyonel, `--color`)
- [ ] JSON formatında da alınabilir
- [ ] Hata kodları (örn: E0001, W0002)

### 6.3 Zengin Hata Açıklamaları
- [ ] Tanımsız değişken → "x tanımlı değil. Bunu mu demek istediniz: xy?"
- [ ] Tip uyuşmazlığı → "int bekleniyor ama float verilmiş. Cast ekleyin."
- [ ] Eksik noktalı virgül → "; eksik. Bu satırın sonuna ekleyin."
- [ ] Sembol tablosu kullanarak bağlamlı öneriler

---

## Aşama 7: LSP Sunucusu (Uzun Vade)

> **Hedef**: VS Code ve diğer IDE'ler için tam dil desteği.
> Tüm metadata ve çoklu çıktı formatları LSP'yi kolaylaştırır.

### 7.1 LSP Protokolü Temelleri
- [ ] `initialize`, `shutdown`, `exit`
- [ ] `textDocument/didOpen`, `didChange`, `didClose`
- [ ] JSON-RPC 2.0 üzerinden iletişim

### 7.2 Dil Özellikleri
- [ ] **Syntax highlighting**: Token listesini LSP'e bildir
- [ ] **Diagnostics**: Hata/uyarıları anlık bildir
- [ ] **Go to definition**: Sembol tablosundan tanım konumuna git
- [ ] **Find references**: Sembolün tüm kullanım noktalarını bul
- [ ] **Hover**: İmleç üstündeki sembol hakkında bilgi göster
- [ ] **Code completion**: Bağlama göre öneriler (sembol tablosundan)
- [ ] **Document symbols**: Dosyadaki tüm sembolleri listele (AST'den)
- [ ] **Code lens**: Fonksiyon başına reference sayısı

### 7.3 Kod Hiyerarşisi Görselleştirme
- [ ] `--format=html` ile etkileşimli AST ağacı
- [ ] AST'yi JSON olarak alıp web UI'da görselleştirme
- [ ] Sembol tablosunu grafik olarak gösterme
- [ ] Fonksiyon çağrı grafiği (call graph)

---

## Aşama 8: Test ve Kalite

### 8.1 Birim Testleri
- [ ] Lexer testleri: her sayı formatı, her operatör, string
- [ ] Tokenizer testleri: keyword'ler, delimiter'lar, yorumlar
- [ ] Parser testleri: her statement tipi, iç içe ifadeler, hata durumları
- [ ] IR testleri: her opcode, doğru register tahsisi
- [ ] Sembol tablosu testleri: tanımlama, çözümleme, scope

### 8.2 Anlık Görüntü (Snapshot) Testleri
- [ ] `source.sqt` → `output.json` karşılaştırması
- [ ] Her commit'te AST/IR/sembol çıktısı değişmemeli (regresyon)

### 8.3 Benchmark
- [ ] Büyük dosya parse süresi
- [ ] Tokenizer throughput (token/saniye)
- [ ] Bellek kullanımı

---

## Öncelik Sıralaması

```
Şu an (Aşama 0) ──▶ Metadata ve konum takibi
                    │
Haftaya (Aşama 1) ──▶ CLI + REPL + dosya modu
                    │
2 hafta (Aşama 2) ──▶ Bellek canavarı AST + JSON çıktı
                    │
3 hafta (Aşama 3) ──▶ Sembol tablosu
                    │
4 hafta (Aşama 4) ──▶ Feature toggle + CompilerConfig
                    │
5 hafta (Aşama 5) ──▶ IR güçlendirme + C transpile backend
                    │
6 hafta (Aşama 6) ──▶ Diagnostic + zengin hata mesajları
                    │
... (Aşama 7-8)  ──▶ LSP, testler, benchmark
```

---

## Anlık Görevler (Bu Hafta)

### Bugün / Yarın
- [ ] `SourceLocation` yapısını tanımla (`src/core/location.hpp`)
- [ ] `SourceFile` sınıfını yaz (offset → satır/sütun dönüşümü)
- [ ] Lexer'a satır/sütun takibi ekle
- [ ] `INumber` yerine `SourceLocation` kullanan yeni yapı

### Bu Hafta
- [ ] Token'lara `SourceLocation` ekle
- [ ] AST düğümlerine `SourceLocation` ekle
- [ ] Token'ları AST'nin sahiplenmesi (unique_ptr)
- [ ] CLI modülü: argc/argv, temel komutlar

---

## Mimari Notlar

### Neden Bellek Canavarı?
Çünkü amaç **bilgiyi korumak**. Derleyiciler genelde parse ettikten sonra
kaynak kod bilgisini atar. Biz atmayacağız. Bu sayede:
- Hata mesajlarında kaynak kodu gösterebiliriz
- IDE'de "go to definition" yapabiliriz
- Kodu yeniden oluşturabiliriz (pretty printer)
- Dönüşümleri (refactoring) güvenle yapabiliriz

### Neden Feature Toggle?
Çünkü bu bir **toolbox**. Kullanıcı dili özelleştirebilmeli:
- Eğitim amaçlı: önce sadece if/else, sonra while ekle
- Deney amaçlı: "while olmadan program yazabilir misin?"
- Güvenlik: belirli yapıları yasakla (eval, system call)
- DSL: kendi alt-dilini oluştur

### Neden IR Merkezli?
IR, tüm backend'lerin ortak dilidir. Yeni bir backend eklemek
istediğimizde parser'ı, lexer'ı, optimizer'ı değiştirmemize gerek kalmaz.
Sadece IR → Hedef dönüşümü yazarız. Bu, LLVM dışında QBE, Cranelift,
hatta kendi JIT'imizi eklemeyi mümkün kılar.
