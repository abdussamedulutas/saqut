# Compiler Structure

## 1 Source Code

- Yazılan kaynak kodun derleyiciye aktarılması
- Derleyici için belirlenen seçenekler ile derleyici yapısının yeniden yapılandırılması
- Derleyicinin outputlarının ayarlanması ve çıkışlarının aktarılması

## 2 Lexing

Kaynak kodun içindeki tüm harflerin gezilip tek parça büyük bir token listesinin oluşturulması

Bu işlemin sonucunda kaynak kodun içindeki tüm yapılar; semboller, sayılar, stringler ve operatörler olarak 4 kategoriye ayrılır

## 3 Tokenning

Tüm tokenler gezilerek bir Abstract Syntax Tree ağacı (AST) oluşturulur. Tek düze tokenler bu aşamada hiyerarşik olarak

File -> Class -> Methods -> Expressions / Statements -> Volumes -> Values

Şeklinde bir ağaç yapısına kavuşur. Böylece yazılan kaynak kodu anlaşılır ilk yapısına kavuşur

## 4 Parsing

Oluşturulan AST ağacı anlamlaştırılır ve zenginleştirilir. Tanımlanan değerler, fonksiyonlar, classler ve değişkenler belirlenir. Tip kontrolleri ve Syntax hataları burada keşfedilir. Ayrıca Ulaşılamayan kod alanları, sınıfların ulaşılamayan (private) accessorları kontrol edilir, tüm bir kod boyunca class, tipleme, değişken ve döngülerin kullanım adetleri analiz edilir. Sistem içinde kullanılan tüm yapılar için geniş kapsamlı bir Symbol tablosu oluşturulur

## 5 Optimizing

Zenginleştirilmiş AST üzerindeki analizler üzerinden bazı AST dalları silinir, değiştirilir veya yeni dallar eklenebilir

- Constant Folding : 4 + 1 gibi sonucu belli olan ifadeler 5 olarak tutulur
- Dead Code Elimination : returnden sonraki kod bloğunun silinmesi veya if(false) ve dengi statementlerin yapıdan kaldırılması
- Matematiksel olarak değişmez kodların kaldırılması x * 1, x+0, x * 1 gibi valuelerin direkt x olarak değiştirilmesi
- Hiç kullanılmayan değişkenlerin kaldırılması
- Sabit (const) değerlerin döngülerin dışına çıkarılması veya programın globaline taşınması
- Null Check Elimination : Daha önce nullcheck yapılmış bir değişkenin tekrar nullcheck yapılan kontrollerini devredışı bırakmak
- Type Check Elimination : Daha önce typecheck yapılmış bir değişkenin tekrar typecheck yapılan kontrollerini devredışı bırakmak

## 6 Compiling

Oluşturulmuş tüm AST ağacını tamamen aynı işi yapan daha alt bir veri kümesine indirgeme işidir. Bellekteki AST yapısı ardışık komutlar dizisine çevrilir (Intermediate Representation) IR daha sonra tekrar okunup çalıştırılabilir.

Daha sonra IR ile kurulacak yapı ile bazen HeavyIR bazende LightIR üretilir.
LightIR, en temiz ve hızlı ancak hiç bir ayrıntı içermeyen koddur. Kaynak kodun çalıştırılması için mükemmel veridir ancak debug verilerinden yoksundur
HeavyIR, kaynak kodu verilerinin yanı sıra orjinal AST üzerinde tanımlanmış değişken isimleri ayrıntıları ve tiplemeleride içerir. LightIRye göre daha ayrıntılı ve büyüktür ancak debugging ve kaynak kodun parça parça okunduğu durumlar için (örneğin interpreter) kullanışlıdır

## 7 Interpreting

Derleyici HeavyIR üretir ve Symbol tablosunu silmez.
Optimizasyonların çoğu kapatılır.
Oluşturulmuş HeavyIR kodu çalıştırılır çalışma bittikten sonra stackframe kapatılmaz yeni girişler beklenir. Yeni kaynak kodu girişleri yapıldığında yine derlenir ve anında çalıştırılır. Yeni çalıştırılan kaynak kodu bir önceki stackframe içersinde symbol tablosu dikkate alınarak output üretir böylece önceki değerler halen kullanılabilir

## or 7 Executing

Derleyici LightIR üretir IR oluşturulduktan sonra symbol tablosu silinir.
Üretilen IR otomatize edilmiş bellekte yüksek performans ile çalıştırılır.

Derleyici o an ürettiği kaynak kodu anında çalıştırdığı (JIT) gibi
Daha önce üretilmiş ve depolanmış IR kodunuda çalıştırabilir

## or 7 Transpiling

Derleyici HeavyIR üretir ve Symbol tablosunu silmez.
Derleyici IR üretmez bunun yerine AST Üzerinden yeni bir dile çevrilir.
Duruma göre daha üst bir seviye dile dönüşüm yapıldığı gibi daha alt bir dile dönüşüm yapılabilir. IR üretilmez bunun yerine alınan kaynak kodu farklı bir kaynak koduna çevrilir

# programmable compiler

Derleyici tek seferde kaynak kodu alıp okuyup çalıştırabilir veya derleyebilir.
Ayrıca debug ortamlarında veya daha ayrıntılı projelerde derleme anına müdehale edilebilir

## Stage Session

Derleyiciye verilen bazı parametreler ile Lexer ve Tokenizer anında bazı işlemlerin yapılması engellenebilir
Örneğin accessorler kapatılabilir, hexedecimal sayılar kapatılabilir. Veya class olmadan globale yazılmış kodlar engellenebilir

## AST Session

Derleyiciye verilen bazı parametreler ile class yapılarına müdehale edilebilir, tipleme sistemleri sabitlenebilir, tek bir metot içerisine yazılacak kod sınırlanabilir veya for içerisinde for döngüsü kısıtlanabilir. Yazılan kod için analizler sonucunda bazı bloklar görmezden gelinebilir veya manipüle edilebilir

## Optimizing Session

Optimizasyon aşamaları parametreler ile tek tek açılabilir veya kapatılabilir

## Output Session

Derleyici ürettiği veriyi erken bir aşamada dosyalayabilir ve loglayabilir.
Derleyicinin oluşturduğui token, Pure AST, Optimized AST, LightIR veya HeavyIR ayrı ayrı kaydedilebilir

## Compiling Session

AST ağacını IRye dönüştürürken neleri aktaracağı derleyici parametre şeklinde verilebilir.
IR, yanlızca JIT ve Compiling modunda değiştirilebilir. Interpreter modunda değiştirilmesine izin verilmez
IR olarak tipleme, sınıflar, değişken isimleri, tekrarlama kayıtları, işlemler ve statementlerin hangilerinin eklenebileceği değiştirilebilir