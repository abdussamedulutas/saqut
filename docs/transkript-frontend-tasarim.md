# saQut Frontend Tasarım Oturumu — Transkript

> Bu belge, frontend (symbol table + semantic analiz + optimizasyon) mimarisinin
> kararlaştırıldığı tasarım oturumunun **sadık bir transkriptidir.** Alınan
> kararların gerekçeleri ADR formatında `docs/adr-frontend-analiz.md`'de,
> uygulama planı `docs/roadmap-frontend.md`'de yer alır. Bu belge ise tartışmanın
> **akışını ve düşünce sürecini** korur — bir kararın nasıl olgunlaştığı, hangi
> itirazların nasıl çözüldüğü buradadır.

---

## 0. Başlangıç durumu

Oturum öncesi tamamlananlar:
- Aşama 0 (SourceLocation / konum takibi) bitirildi: her token ve AST node'u
  dosya/satır/sütun/offset taşıyor.
- İki bug giderildi: (1) `location.file` boş geliyordu → `Tokenizer::scan` artık
  dosya yolu alıyor; (2) `"kind": true` bug'ı → `JsonObject::add`'a `const char*`
  overload'u eklendi.
- AST JSON çıktısındaki fazla boşluk/girinti hataları temizlendi.
- Bu işler commit'lendi, master'a merge edildi ve push edildi.

---

## 1. "Rotamız ne? IR/JIT'e geçebilir miyiz?"

**Kullanıcı:** AST ve parser hazır mı? IR ve JIT adımına geçebilir miyiz?

**Asistan (özet):** Henüz değil, doğrudan atlamak riskli. Roadmap durumu:
- Aşama 0 (konum) ✅
- Aşama 1 (CLI/REPL) 🟡 yarım
- Aşama 2 (AST bellek canavarı) 🟡 yarım
- Aşama 3 (Symbol Table) ❌ yok (`src/symbol/` klasörü bile yok)
- Aşama 4 (Feature Toggle) ❌ yok
- Aşama 5 (IR + Backend) ❌ embriyonik (IR'de sadece 5 opcode: mathadd/sub/mul/div,
  declare; backend yok)

JIT'e atlamamak için iki engel: (1) Symbol table yok — IR üretimi değişken
çözümlemesi ister; (2) IR'de kontrol akışı/fonksiyon/bellek opcode'ları yok, yani
if/while/fonksiyon derlenemez. Ayrıca ADR-001'e göre strateji: C transpile →
QBE → custom/JIT. JIT "uzun vade".

Önerilen sıra: Symbol Table → IR güçlendirme → C transpile → sonra JIT/QBE.

---

## 2. Kullanıcının vizyonu: 2-pass parser ve AST'yi derinleştirme

**Kullanıcı (özet):** Parser 2 aşamalı olacak. İlk pass syntax'ı çözüp basit
AST'yi bırakacak (tokenler, yerleri, türleri). İkinci pass'te symbol table
kurulacak (değişkenler, türleri, yerleri, kapsamları, kaç kez/nerede kullanıldıkları).
Sonraki tüm operasyonlar AST üzerinden döneceği için AST'de çok fazla ham/işlenmiş
veri olmalı; optimizasyon ve analizler tamamen burada olacak. 2. pass'te ilk AST
ve symbol table kullanılarak: kullanılmayan değişkenler, return sonrası kod,
bir kez atanıp bir kez kullanılan değişkenler, literal-operatör-literal (`1+2`)
kapatılacak (constant folding). Yorumlar/eleştiriler isteniyor.

**Asistan — 4 itiraz/uyarı:**

1. **"AST'yi derinleştirmek" felsefeyle çelişebilir.** "Hiçbir bilgi atılmaz"
   prensibi var. `1+2`'yi yıkıcı şekilde `3` yaparsan orijinali silersin →
   `saqut ast` artık kullanıcının yazdığını göstermez. Çözüm: **annotation**
   (node'a `foldedValue:3` ekle, ağacı bozma) vs **transformation** (asıl silmeyi
   AST→IR indirmede yap).

2. **"2. pass" tek pass değil, içine 3 iş tıkıyorsun:** (a) sembol toplama/isim
   çözümleme, (b) semantik doğrulama, (c) optimizasyon. Bunlar farklı bağımlılık
   sırasına sahip; optimizasyon semantik analiz bitince anlamlı; optimizasyonlar
   birbirini tetikler (fixpoint gerekir); ayrı test edilmeli.

3. **"Bir kez atanıp bir kez kullanılan" optimizasyonu ağır.** Def-use chain /
   dataflow ister; basit tree-walk değil. Ertele, IR'de yap.

4. **Tip kontrolünü atlamamış ol.** Symbol table tip tutacaksa, `int x = "..."`
   hatasını kim yakalar? Her expression node'una sonuç tipi annotate etmek değerli.

İki soru soruldu: constant folding yıkıcı mı annotation mı? Scope kuralı
(forward reference) ne?

---

## 3. Kullanıcının netleştirmesi: pass = derleyici aşaması; optimizasyon nerede?

**Kullanıcı (özet):** "2-pass" sadece parser içindi, optimizasyonu kastetmedim.
Sıralama: 1. pass AST kur, 2. pass symbol table, 3. pass AST'yi derinleştir.
Ölü kodu 3'te sadece işaretleyelim demiştim ama haklısın, optimizasyon kendi
içinde tekrar çalışmalı — belki 5, belki 10 pass; her pass öncekinden rahat.

**Önemli soru:** Optimizasyonları tam derleme/IR üretme zamanından önce
yapmamız gerekmiyor mu? Neden AST aşamasında optimize edeceğimizi düşündün?
Parametre olarak AST isteniyorsa optimizasyon halini isteğe bağlı vermeliyiz;
kullanıcı AST'nin/symbol table'ın optimizasyon öncesi/sonrası halini görmek
isteyebilir.

**Scope:** Bunu JIT veya compiler frontend olarak düşünüyorum; arka planda Java
gibi forward reference olan ama C syntax'lı bir dil. Başta OOP yok, JS gibi düşün.

**Asistan:**
- "Pass" düzeltmesi kabul: "5-10 pass" değil **fixpoint** (değişiklik durana
  kadar). Her **tur** daha az iş yapar; analiz pass'leri bir kez çalışır.
- "Neden AST'de optimize?" sorusuna dürüst cevap: Ben "optimize" değil
  "annotate" demiştim. **Analiz (gerçekler) vs optimizasyon (değişiklik)** ayrımı.
  Frontend/middle-end/backend katmanlaması tanıtıldı. Basit kaynak-seviyesi
  optimizasyonlar AST'de (backend-bağımsız, incelenebilir); ağır dataflow
  optimizasyonları IR'de. Backend'e koyarsak 3 backend'de 3 kez yazarız.
- **Kullanıcının "öncesi/sonrası görebilmeli" gereksinimi mimariyi belirledi:**
  optimizasyon orijinali ASLA bozmamalı → klon üret. `saqut ast` öncesi,
  `saqut ast --optimized` sonrası.
- Scope kararı önerildi: üst seviye tam forward reference (2 alt-geçiş), lokal
  declare-before-use. "Local hoisting istiyor musun?" sorusu soruldu.

---

## 4. Tipli dil, hoisting yok; kalan tasarım sorunları

**Kullanıcı (özet):** JS'i sadece syntax basitliği için örnek verdim, kötü
yanlarını almıyoruz — null/undefined karmaşası yok. Lokal'de hoisting olmasın,
tipli dil olacak. Pass = derleyicinin kendi aşamaları (lexing/parsing/optimizing/
compiling), bunları çoğaltabiliriz. Scope: tam forward reference olsun.
Başka sorun var mı?

**Asistan — "pass = aşama" doğrulandı,** her aşama bağımsız çalışan modül; CLI
komutları bunun yansıması. Sonra **3 gerçek mesele + küçük kararlar:**

1. **Tip sistemi hiç tanımlanmadı.** `varType` şu an sadece string. Minimal ama
   genişletilebilir `Type` sınıfı gerek. Tip çıkarımı (auto)? Implicit conversion?
2. **Diagnostic altyapısı şimdi gelmeli.** Semantik analiz hata üretecek ama
   koyacak yer yok. Aşama 6'nın tamamı değil ama minimal `Diagnostic` toplayıcı
   bu işle birlikte gelmeli.
3. **Annotation'lar node'larda nerede yaşayacak?** `ExpressionNode`/`StatementNode`
   ara tabanları önerisi.

Küçük kararlar: overloading yok; scope granülerliği (Program/FunctionDecl/Block/
for-while); recursive struct → pointer gelince; aritmetik semantiği (5/2 int mi).

---

## 5. Kullanıcının kapsam kararları + sorular

**Kullanıcı (özet):** En baştan: pointer sistemi yok, tipleme basit, tuple
(`<T,U>`) yok, class yok struct var, array'ler define sırasında `[]` ile —
ama dinamik mi statik bellek mi karar veremedim. Fonksiyonlar tipli. `class`
gibi keyword'leri dikkate alma, `auto` yok. Otomatik float↔int dönüşümü yok,
her şey açık; gizli dönüşüm istemiyorum, sadece constant'larda (`5/2`→`2`).
Minimal Type sınıfına katılıyorum ama ileride geliştirilebilir olmalı — bu dilin
geleceğini bilmiyorum, popüler de olabilir, tozlanabilir de. Diagnostic için tüm
hataları baştan belirlemeliyiz; sistemi durdurup tüm hataları kullanıcıya
söylemeliyiz; hata raporlama fonksiyonları ayrı bir yerde olsun. Recursive struct
define yok ama `struct StructA { StructB BVar }` olabilir. Scope granülerliği
onaylandı. Duplicate'i tamamen yok et. For döngüsü içindeki objeler dışında
silinebilir — ama symbol table'ı karmaşıklaştırır, her katmanda namespace
tutmalıyız; üst katmanda hem öncesi hem sonrası aranacak — symbol table da mı 2
geçiş olacak? AST'nin temiz kalması neyi zorlaştırır bilmiyorum, sen karar ver.
AST node cpp dosyaları neredeyse boş, oraları kullanalım, o class'ları dolduralım.

**ExpressionNode konusunu tam anlamadım, açar mısın?** Ben tüm analizi AST'de
yapar, her node'da anlamlı veri tutar, ref'leri kendi içimde pointer olarak
tutarım diye düşünüyordum.

**Asistan cevapları:**
- **Array:** Pointer yoksa dinamik array kuramazsın (heap+gizli pointer ister).
  Öneri: statik sabit boyut. *(Bu karar bir sonraki turda revize edildi — bkz. §6.)*
- **ExpressionNode = aslında aynı fikirdeyiz.** Sorum felsefi değil, C++ detayı:
  `resolvedType`'ı hangi node'a koyacağız? Tipli dilde sadece ifadelerin tipi var
  (`5+3`→int; `if`→tipsiz). İki ara taban (`ExpressionNode`/`StatementNode`)
  alanları doğru yere koyar + "burası ifade olmalı" kontrolü sağlar. Analiz yine
  AST'de yaşar; bu sadece alanları doğru sınıflara dağıtmak.
- **Symbol table 2 geçiş?** Evet ama sadece üst seviyede. Geçiş 1: global'leri
  hoist et. Geçiş 2: gövdelere in, çöz. Fonksiyon içi tek geçiş (lokal'de forward
  ref yok). Öncesi/sonrası derdi sadece global'ler için, onu Geçiş 1 çözer.
- **AST temiz vs her şey AST'de → karar: her şey AST'de** (senin modelin).
  Optimizasyon klonda dönüştürür. **Önemli:** "kaç kez kullanıldı" Symbol'da
  yaşar (değişkene ait), node'da değil; IdentifierNode Symbol'a pointer tutar.
- **Hoisting açıklaması** (kullanıcı "local hoisting ne demek, global=local değil
  mi?" diye sordu): Hoisting = tanımın yazıldığı satırdan önce de görünmesi.
  Global'de forward ref güvenli (gövde tanımda çalışmaz, çağrılınca çalışır).
  Lokal'de declare-before-use (değişkenin sırası/değeri var). Asimetri =
  tutarsızlık değil, tam olarak Java'nın davranışı.

---

## 6. Pointer netleştirmesi + son onaylar

**Kullanıcı (özet):** Yanlış anlama düzeltmesi: pointer derleyici **içinde** tabii
ki var; programlama **dilinde** `*`/`&` yok. Pointer sadece derleyicinin içinde.
Bu, değişken listelerini destekler — JS'te pointer syntax'ı yok ama array var.
Kullanıcıya pointer yok, arka planda A'dan Z'ye pointer var. Ayrıca: local
hoisting senin dediğin gibi (declare-before-use). Array'ler `int[]` olsun.
**Ama bu backend'de dinamik bellek zorunluluğu getirmez mi? JIT'te?**
ExpressionNode → evet ekle. Her şey temiz, anlaşılır, yorum satırlarıyla
takip edilebilir olsun yeter.

**Asistan:**
- **Array kararı revize edildi:** Pointer arka planda olduğuna göre dinamik
  array mümkün. Frontend için `Type` sadece "array of T" temsil etsin, boyut
  tipte olmasın. Runtime bellek modeli (statik/heap/free) backend kararı, ertelendi.
- **Dinamik bellek/JIT yükümlülüğü cevabı:** Evet ileride yükümlülük getirir ama
  frontend'i bloklamaz. C transpile'da malloc/realloc/free ile neredeyse bedava;
  JIT'te minik runtime kütüphanesine call emit edilir; yönetim stratejisi en
  basiti scope-tabanlı ownership, GC gerekmez.
- Tüm kararlar kilitlendi; dosya-dosya yol haritası sunuldu (Faz 0…4).

---

## 7. Belgeleme talebi

**Kullanıcı:** Konuştuklarımızı ve kararları ADR formatında, neden öyle yaptığımızı
da belgele; hiçbir şey boş geçmesin; transkript de yazabilirsin; en son yol
haritasını oluştur; her şeyden önce yeni bir branch çık.

**Asistan:** `feature/frontend-analysis` branch'i açıldı. Üç belge üretildi:
- `docs/adr-frontend-analiz.md` (ADR-006…014)
- `docs/transkript-frontend-tasarim.md` (bu belge)
- `docs/roadmap-frontend.md` (uygulama planı)
