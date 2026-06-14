#!/usr/bin/env python3
import sys
from gitea import make_request, REPO_PATH

issues_data = [
    {
        "title": "Faz 0 — Tip Sistemi ve Hata Kayıt Motoru (Type & Diagnostics)",
        "body": """### Giriş (Nedir, Neden Yapıyoruz?)
Bu iş paketinde derleyicinin çalışması için en temel iki altyapıyı kuracağız: Tip Sistemi (Type System) ve Hata Kayıt/Diagnostic Motoru.
Tip Sistemi, derleyicinin kaynak koddaki verileri (sayılar, metinler vb.) anlamlandırabilmesi ve tür doğruluğunu denetleyebilmesi için gerekli veri yapısını sağlar.
Hata Kayıt Motoru ise derleme sırasında oluşan hataları (örneğin tip uyuşmazlığı veya tanımsız değişken kullanımı) biriktirip, kullanıcıya hatanın kaynak koddaki tam yerini (dosya, satır, sütun koordinatları) göstererek raporlayan mekanizmadır. Bu yapı sayesinde derleyici ilk bulduğu hatada hemen durmayacak, tüm hataları toplayıp tek seferde kullanıcıya sunacaktır.

---

### Gelişme (Neyi, Nerede ve Nasıl Yapacaksın?)
Aşağıdaki adımları sırasıyla gerçekleştir:
1.  **Tip Yapısı:** `src/core/type.hpp` adında yeni bir dosya oluştur. Burada `TypeKind` (Primitive, Array, Struct, Function, Error) enum sınıfını ve `Type` sınıfını tanımla. Primitif türler için `int`, `float`, `double`, `char`, `string`, `bool`, `void` alt tiplerini destekle. Dizi (Array) türü için eleman tipini (`elementType`), fonksiyon için dönüş ve parametre tiplerini taşıyacak alanlar ekle. `equals()` ve `toString()` gibi metotları implement et.
2.  **Hata Yapısı:** `src/diagnostic/diagnostic.hpp` dosyasını oluştur. Burada hata seviyelerini (`Error`, `Warning`, `Note`, `Hint`) içeren `DiagLevel` enum sınıfını ve hatanın koordinatlarını tutan `Diagnostic` yapısını tanımla.
3.  **Diagnostic Kataloğu:** Aynı dosyada derleyicide oluşabilecek hata kodlarını içeren kataloğu (örneğin `E001` - Tanımsız Değişken, `E002` - Çift Tanım, `E003` - Tip Uyuşmazlığı, `E010` - Döngüsel Struct Tanımı vb.) sabitle.
4.  **DiagnosticEngine:** `src/diagnostic/diagnostic_engine.hpp` dosyasını oluştur. `DiagnosticEngine` sınıfı hataları bir vektörde biriktirmeli, `report()` metoduyla yeni hata almalı ve `printAll()` ile bunları ekrana formatlı yazmalıdır.

*Geliştirme Dalı (Branch):* `feature/faz0-temeller`

---

### Sonuç ve Başarı Kriterleri
Bu işin bittiğini ve başarılı olduğunu şu kriterlerle doğrulayacağız:
1.  `Type::equals()` metodu için yazılan birim testler (Unit Tests) farklı tip kombinasyonlarını (örneğin `int` ile `int` eşit, `int` ile `float` veya `int[]` farklı) hatasız doğrulamalıdır.
2.  `DiagnosticEngine` sınıfına 3 farklı hata raporlanıp `printAll()` çağrıldığında, çıktıda hata kodları (`E001` vb.) ve kaynak kod satır/sütun koordinatları doğru sırayla ve biçimde ekrana yazılmalıdır.
3.  Derleyici uyarısız (`-Wall -Wextra`) derlenmelidir.

*Mühendis Olmayanlar İçin Analiz:* Bu aşama derleyicinin arka planındaki veri yapılarını kurduğu için görsel bir çıktı üretmez, ancak sonraki tüm aşamaların temelidir. Karmaşıklığı **Düşük**, yapım süresi yaklaşık **1-2 gün**dür.
*İmza/Yorum:* Bu altyapı semantik analizin (Faz 3) hata raporlayabilmesi için kritik önkoşuldur."""
    },
    {
        "title": "Faz 1 — AST Hiyerarşisinin Refaktörü (Expression ve Statement Ayrımı)",
        "body": """### Giriş (Nedir, Neden Yapıyoruz?)
Bu iş paketinde, derleyicinin kaynak kodu okuduktan sonra hafızada oluşturduğu Soyut Sözdizimi Ağacı (Abstract Syntax Tree - AST) düğümlerini yeniden yapılandıracağız.
Şu anda tüm AST düğümleri tek bir `ASTNode` taban sınıfından türemektedir. Ancak anlamsal olarak iki farklı düğüm türü vardır:
1.  **Expression (İfade):** Bir değer üreten yapılar (örneğin `5 + 3` veya değişken isimleri). Bunların bir veri tipi (Type) olur.
2.  **Statement (Deyim):** Bir değer üretmeyen, sadece bir iş/eylem yürüten kontrol yapıları (örneğin `if` bloğu, döngüler veya `return`). Bunların tipi yoktur ancak erişilebilirlik (reachability) durumları analiz edilir.
Bu iki yapıyı birbirinden ayırmak, sonraki aşamada tip denetimi yapabilmemiz için zorunludur.

---

### Gelişme (Neyi, Nerede ve Nasıl Yapacaksın?)
Aşağıdaki adımları sırasıyla gerçekleştir:
1.  **Ara Taban Sınıflarını Ekle:** `src/parser/ast_node.hpp` dosyasına `ExpressionNode` ve `StatementNode` adında iki yeni sınıf ekle. `ExpressionNode` sınıfına `resolvedType` (çözümlenmiş tip), `isConstant` ve `foldedValue` (optimizasyon için) alanlarını koy. `StatementNode` sınıfına ise `isReachable` (erişilebilirlik bayrağı) alanını ekle.
2.  **Düğümleri Güncelle:** `src/parser/nodes/` dizinindeki tüm dosyaları tara. `LiteralNode`, `BinaryExpressionNode`, `IdentifierNode`, `CallExpressionNode` gibi değer üreten tüm sınıfları `ExpressionNode`'dan türet. `IfStatementNode`, `WhileStatementNode`, `ReturnStatementNode` vb. sınıfları ise `StatementNode`'dan türet.
3.  **Çocuk Gezintisini Düzelt:** AST taban sınıfındaki `getChildren()` metodunun alt sınıflar tarafından ezilmesini (override) sağla. Örneğin `BinaryExpressionNode` sınıfında `Left` ve `Right` düğümlerini dinamik olarak `getChildren()` içinde döndür. Bu, genel ağaç gezginlerinin (Tree Walker) alt düğümleri eksiksiz taramasını sağlayacaktır.
4.  **toJson Modifikasyonu:** `toJson()` metotlarını güncelleyerek yeni alanları (çözümlenmiş tip ve erişilebilirlik bilgileri) JSON çıktısına ekle.

*Geliştirme Dalı (Branch):* `feature/faz1-ast-refactor`

---

### Sonuç ve Başarı Kriterleri
Bu işin bittiğini ve başarılı olduğunu şu kriterlerle doğrulayacağız:
1.  `saqut ast examples/fibonacci.sqt` komutu çalıştırıldığında üretilen JSON çıktısında, ifadeler için `resolvedType` alanı (boş olarak) ve deyimler için `isReachable` alanı (true olarak) görünmelidir.
2.  JSON yapısının doğruluğu `python3 -m json.tool` ile test edildiğinde hiçbir parse hatası vermebelidir (regresyon testi).
3.  Tüm kodlar uyarısız derlenmelidir.

*Mühendis Olmayanlar İçin Analiz:* Bu aşama mevcut veri yapısını daha düzenli hale getirir. Dışarıdan bakıldığında JSON çıktısında yeni alanlar görülür. Karmaşıklığı **Düşük-Orta**, yapım süresi yaklaşık **2 gün**dür.
*İmza/Yorum:* Düğüm alanlarındaki `children` vektörü tutarsızlığı bu fazda tamamen çözülmeli, ağaçta yukarı ve aşağı gezinme API'si garanti altına alınmalıdır."""
    },
    {
        "title": "Faz 2 — Sembol Tablosu ve İki Geçişli Toplama (Scope & Symbol Table)",
        "body": """### Giriş (Nedir, Neden Yapıyoruz?)
Bu iş paketinde derleyicinin \"Hafızası\" sayılan Sembol Tablosunu (Symbol Table) kuracağız.
Sembol Tablosu, kaynak koddaki değişkenlerin, fonksiyonların ve veri yapılarının (struct) tanımlarını, veri tiplerini, hangi kapsamlarda (Scope) geçerli olduklarını ve kodun neresinde kullanıldıklarını (referanslarını) takip eden bir veri yapısıdır.
Dilimiz, tanımlanmadan önce fonksiyon çağrılmasına izin verdiği için (ileri başvuru / forward-reference), sembolleri toplama işlemini iki geçişli bir algoritma ile yapmak zorundayız.

---

### Gelişme (Neyi, Nerede ve Nasıl Yapacaksın?)
Aşağıdaki adımları sırasıyla gerçekleştir:
1.  **Sembol ve Kapsam Yapıları:** `src/symbol/` adında yeni bir dizin aç. Burada `Symbol` yapısını tanımla (ad, tür, tip, tanım konumu, referans listesi alanları bulunsun). `Scope` sınıfını iç içe geçebilir ağaç yapısında yaz; her scope bir üst scope'a pointer tutsun.
2.  **SymbolTable Yöneticisi:** `src/symbol/symbol_table.hpp` dosyasını oluştur. Scope yığınını yöneten `enterScope()`, `exitScope()`, sembol ekleyen `define()` (aynı scope'ta çift tanım varsa hata üret) ve içten dışa arayan `resolve()` metotlarını yaz.
3.  **İki Geçişli Toplayıcı:** `src/symbol/symbol_collector.hpp` ve `.cpp` dosyalarını oluştur.
    *   **Geçiş 1 (Global Hoisting):** AST'yi tarayarak sadece üst seviye fonksiyon ve struct imzalarını global scope'a kaydet.
    *   **Geçiş 2 (Local Resolution):** Fonksiyon gövdelerine in. Lokal değişkenleri declare-before-use (kullanmadan önce tanımlama) kuralıyla topla ve Identifier (isim referansı) düğümlerini sembol tablosundaki sembollerle bağla. Global değişken başlatıcılarında declare-before-use kuralını zorunlu kıl.
4.  **Döngüsel Struct Kontrolü:** Struct tanımları toplandıktan sonra, pointer olmadığı için birbirini değer olarak içeren döngüsel struct tanımlarını (örn. `struct A { B b; }` ve `struct B { A a; }`) DFS (Derinlik Öncelikli Arama) algoritmasıyla tespit et ve `E010` hatasını raporla.

*Geliştirme Dalı (Branch):* `feature/faz2-symbols`

---

### Sonuç ve Başarı Kriterleri
Bu işin bittiğini ve başarılı olduğunu şu kriterlerle doğrulayacağız:
1.  `saqut symbols examples/fibonacci.sqt` komutu çalıştırıldığında, programdaki fonksiyonlar, parametreler ve lokal değişkenler doğru tipleri ve tüm kullanım koordinatlarıyla (satır/sütun referans listesi) ekrana yazılmalıdır.
2.  Aynı isimde iki değişken tanımlandığında `E002` (Çift Tanım), tanımsız bir değişken çağrıldığında veya global başlatıcı sırasında ileri başvuru yapıldığında `E001` (Tanımsız İsim), döngüsel struct tanımlandığında ise `E010` hataları DiagnosticEngine tarafından raporlanmalı ve derleme durmalıdır.

*Mühendis Olmayanlar İçin Analiz:* Derleyicinin değişken ve fonksiyon isimlerini anlamlandırmaya başladığı aşamadır. Hatalı kod yazıldığında derleyicinin verdiği akıllı uyarıların temelidir. Karmaşıklığı **Yüksek**, yapım süresi yaklaşık **4-5 gün**dür.
*İmza/Yorum:* Bu aşamada pointer ve scope yapıları çok hassas tasarlanmalıdır; bellek sızıntılarını önlemek için akıllı işaretçiler veya net sahiplik modelleri tercih edilmelidir."""
    },
    {
        "title": "Faz 3 — Semantik Analiz ve Tip Denetimi (Type Checking)",
        "body": """### Giriş (Nedir, Neden Yapıyoruz?)
Bu iş paketinde derleyicinin kurallarını işleten Semantik Analiz (Anlamsal Analiz) ve Tip Denetimi (Type Checking) motorunu yazacağız.
Sözdizimi (Syntax) doğru olan bir kod anlamsal olarak yanlış olabilir (örneğin `int x = \"merhaba\" + 5;` yazmak sözdizimsel olarak geçerlidir ancak mantıksızdır).
Tip Denetimi, ifadelerin veri tiplerinin uyumlu olup olmadığını kontrol eder. Ayrıca dilimizin \"otomatik tip dönüşümü yasaktır\" kuralını işletir. Yapısal doğrulama ise `break`/`continue` deyimlerinin döngü dışında kullanılmasını engeller.

---

### Gelişme (Neyi, Nerede ve Nasıl Yapacaksın?)
Aşağıdaki adımları sırasıyla gerçekleştir:
1.  **Tip Denetleyici:** `src/sema/type_checker.hpp` ve `.cpp` dosyalarını oluştur. AST'yi aşağıdan yukarıya (bottom-up) gezerek her `ExpressionNode` düğümünün tipini (`resolvedType`) belirle.
2.  **Uyuşmazlık Kuralları:** Atama işlemlerinde (`=`), aritmetik operatörlerde ve fonksiyon çağrılarında tip uyumunu denetle. Gizli değişken-değişken dönüşümlerini yasakla (hata: `E003`).
3.  **Literal Uyarlama Kuralı:** Sabit sayı literal'ları için bağlama göre tiplendirme kuralını işlet (Örn: `float x = 1;` geçerli olmalı çünkü `1` kayıpsızca float'a dönüşebilir, ancak `int y = 1.5;` `E003` hatası vermelidir).
4.  **Error Tipi:** Bir ifade hatalıysa tipine `ErrorType` ata. Bu sayede, o ifadeyi kullanan üst ifadelerde tekrar tekrar sahte hatalar üretilmez.
5.  **Yapısal Doğrulayıcı:** `src/sema/structural_validator.hpp` ve `.cpp` dosyalarını oluştur. Ağacı tırmanarak `break`/`continue` ifadelerinin üstünde bir döngü olup olmadığını (`E004`), `return` ifadesinin bir fonksiyon gövdesinde olup olmadığını (`E005` ve dönüş tipi uyumu için `E006`) denetle.

*Geliştirme Dalı (Branch):* `feature/faz3-semantic-analysis`

---

### Sonuç ve Başarı Kriterleri
Bu işin bittiğini ve başarılı olduğunu şu kriterlerle doğrulayacağız:
1.  Geçerli olan `examples/fibonacci.sqt` dosyası semantik analizden sıfır hatayla geçmeli ve her AST ifadesinin tipi JSON çıktısında (`resolvedType`) görünmelidir.
2.  Hatalı bir test dosyası verildiğinde (örneğin döngü dışında `break;` yazıldığında veya `int` değişkene `string` atandığında) derleyici ilgili hata kodlarını (`E003`, `E004` vb.) raporlamalı ve derleme süreci kesilmelidir.

*Mühendis Olmayanlar İçin Analiz:* Bu aşama, derleyicinin yazılan kodun \"anlamlı\" olup olmadığını kontrol ettiği güvenlik kapısıdır. Karmaşıklığı **Orta-Yüksek**, yapım süresi yaklaşık **3-4 gün**dür.
*İmza/Yorum:* Sabit ifadelerde constant folding istisnasını unutmama kuralları burada işletilecektir."""
    },
    {
        "title": "Faz 4 — Optimizasyon Altyapısı ve Kaynak Optimizasyonları (Optimization)",
        "body": """### Giriş (Nedir, Neden Yapıyoruz?)
Bu iş paketinde derleyicinin ürettiği kodu daha verimli hale getiren Optimizasyon (Eniyileme) altyapısını ve ilk optimizasyon adımlarını (pass'leri) yazacağız.
Optimizasyon, yazılan kodun anlamını değiştirmeden daha hızlı çalışacak veya daha az yer kaplayacak şekilde dönüştürülmesidir.
saQut'un \"alet çantası\" felsefesi gereği, optimizasyon orijinal kaynak kod görüntüsünü bozmamalıdır. Bu nedenle optimizasyonlar AST'nin bir kopyası (klon) üzerinde gerçekleştirilir.

---

### Gelişme (Neyi, Nerede ve Nasıl Yapacaksın?)
Aşağıdaki adımları sırasıyla gerçekleştir:
1.  **Optimizasyon Yöneticisi:** `src/opt/` dizinini oluştur. Soyut `OptimizationPass` sınıfını tanımla. `OptimizationManager` sınıfını yaz; bu sınıf `CompilerConfig` üzerinden aktif edilen pass'leri sırayla çalıştırsın.
2.  **Fixpoint Döngüsü:** Pass'lerin birbirini tetiklemesi için (örn. folding sonrası ölü kod oluşması) optimizasyonları bir fixpoint döngüsünde (hiçbir pass değişiklik yapmayana kadar) çalıştır. Monoton olmayan pass'lerin sonsuz döngüye girmemesi için sert bir tur sınırı (`maxFixpointRounds`) ekle.
3.  **Ağaç Klonlama ve Remap:** `ASTNode::clone()` metodunu tüm AST düğümleri için yaz. Kopyalanan düğümlerin `parent` pointer'larını yeni ağaca göre bağla. Klonlanan ağaçtaki Identifier (isim referansı) düğümlerinin sembol tablosu bağlarını, klonlanmış yeni bir sembol tablosuna yönlendir (Remapping).
4.  **Constant Folding Pass:** `src/opt/constant_folding.hpp` dosyasını oluştur. İki sabit terimin işlemini derleme zamanında hesapla (Örn: `1 + 2` → `3`). Sıfıra bölme varsa katlama yapma, `W002` uyarısı ver.
5.  **Dead Code Elimination (DCE) Pass:** `src/opt/dead_code_elim.hpp` dosyasını oluştur. Erişilemez deyimleri (örn. `return` sonrası kodlar, `if(false)` gövdeleri) ve hiç kullanılmayan lokal değişkenleri (`W001`) sil.

*Geliştirme Dalı (Branch):* `feature/faz4-optimization`

---

### Sonuç ve Başarı Kriterleri
Bu işin bittiğini ve başarılı olduğunu şu kriterlerle doğrulayacağız:
1.  `saqut ast examples/fibonacci.sqt --optimized` komutu çalıştırıldığında sabit ifadelerin katlandığı ve varsa ölü kodların silindiği doğrulanmalıdır.
2.  `--optimized` bayrağı verilmeden çağrıldığında orijinal (optimize edilmemiş) AST çıktısı elde edilmelidir (Öncesi/Sonrası karşılaştırması).
3.  Folding ve DCE işlemlerinin ardışık olarak birbirini beslediği (folding sonucu oluşan ölü kodun DCE tarafından temizlendiği) zincirleme senaryolar tek bir komutla doğrulanmalıdır.

*Mühendis Olmayanlar İçin Analiz:* Yazdığınız programın gereksiz kısımlarını temizleyen akıllı temizlikçi aşamasıdır. Kodun boyutunu küçültür. Karmaşıklığı **Yüksek**, yapım süresi yaklaşık **4-5 gün**dür.
*İmza/Yorum:* `ASTNode::clone()` implementasyonu bu fazın en kritik ve hata yapmaya açık kısmıdır, parent pointer bağlarına azami dikkat gösterilmelidir."""
    }
]

for issue in issues_data:
    endpoint = f"{REPO_PATH}/issues"
    res = make_request(endpoint, method="POST", data=issue)
    print(f"Created issue #{res['number']}: {res['title']}")
