# ADR-038 — Determinizm ve Sürüm Uyumluluğu Sözleşmesi

> **Durum (2026-07-25): Kısmen supersede edildi — ADR-042.**
> §2'deki minor sürümde sıfır yeni özellik/iki yönlü uyumluluk, zorunlu
> backport+yank; §3'te yeni public işlevin doğrudan major olması ve exact
> insan-okunur diagnostic metninin donması artık yürürlükte değildir. Major
> sürümün ayrı ürün olması, iç temsil serbestliği ve açıkça sürümlenen
> gözlemlenebilir sözleşmeler korunur. Ayrıntı:
> `ADR-042-v1-feedback-mvp-ve-surumleme.md`.

İlgili: ADR-032 (backend kararı: VM referans + MIR JIT, diferansiyel test zorunlu),
ADR-037 (JIT Value ABI — kutulama farkı "gözlemlenen davranışa uygulanır" ilkesi),
LICENSE.md (ürün kimliği: v1 bir ürün / v2 başka ürün), #92 (in-process
diferansiyel test), #111 (paket yöneticisi / yank altyapısı).

## Bağlam

saQut'un kimliği "cam kutu + kararlı zemin": derleme sürecinin her aşaması
incelenebilir, ve üstüne elle yazılan araç ekosistemi (LSP istemcileri, harici
tool'lar, CLI çıktısını ayrıştıran script'ler) doğrudan **gözlemlenen davranışa**
bağımlıdır. Aynı zamanda iki çalıştırma yolu var (VM referans backend + MIR JIT,
ADR-032) ve bunlar aynı IR'de **aynı sonucu** vermek zorunda. Bu iki baskı birlikte
bir uyumluluk sözleşmesi gerektiriyor: neyin donuk, neyin serbest olduğu net olmalı
ki hem iki backend, hem ardışık sürümler birbirini bozmasın.

## Karar

### 1. Sözleşme gözlemlenen davranıştır — iç temsil değil

Bağlayıcı olan: **stdout, dönüş değerleri, hata/tanı çıktıları, serileştirme
sonuçları** (gözlemlenen davranış). Bağlayıcı OLMAYAN: bellek düzeni (heap/stack),
değer kutulama, GC mekanizması, register tahsisi, IR'in iç biçimi. Bunlar hem iki
backend arasında (ADR-037: VM string'i inline, JIT'te `StringObject` kutulu —
kasıtlı fark), hem sürümden sürüme serbestçe değişebilir. **Tek şart: gözlemlenen
sonucu değiştirmemek.** Diferansiyel test (#92) bunu zorlar.

### 2. Üç sürüm ekseni (SemVer, 1.0.0'dan itibaren bağlayıcı)

**PATCH (x.y.Z) — bugfix.** Yanlış sonuç veren bir hata "sözleşme dışıdır":
yanlış davranışa hiçbir zaman uyumluluk sözü verilmedi. Bu yüzden anında
düzeltilir ve **etkilenen tüm minor serilerine backport edilir** (bir hata
0.6/0.5/0.4'te varsa → 0.6.1 / 0.5.1 / 0.4.1). Düzeltme gözlemlenen çıktıyı
"yanlıştan doğruya" değiştirebilir. Hatalı `.0` sürümü **yank**'lenir: paket
dağıtımından çekilir (kaynak repo'da kalır). Böylece uyumluluk garantisi fiilen
"yayında olan sürümler" kümesi için geçerli olur.

**MINOR (x.Y.0) — donuk yüzey, iki yönlü uyumluluk.** Bir major serisi (1.x)
boyunca **gözlemlenebilir yüzey değişmez**: syntax, builtin/FFI imzaları, bir
fonksiyonun var/yok oluşu, CLI çıktı formatları. Sonuç: **her iki yön de
koşulsuz çalışır** — 1.7'de yazılan kod 1.3'te de, 1.3 kodu 1.7'de de. Minor'da
değişebilen **yalnızca**: GC iç mekanizması, performans (aynı sonuç, daha hızlı),
sonucu değiştirmeyen iç yeniden yazımlar. Bu, endüstri normundan (SemVer'de
"minor = geriye-uyumlu **yeni** işlevsellik") **bilinçli bir ayrılıktır:** minor'da
**sıfır yeni gözlemlenebilir özellik** olur.

**MAJOR (X.0.0) — yeni ürün.** Syntax, dil kimliği (OOP olmaması dahil), hatta bu
determinizm kuralının kendisi bile değişebilir. Uyumluluk sözü yoktur (Python 2/3
modeli: v1 bir ürün, v2 başka ürün). Major aralıkları kasıtlı olarak uzundur;
tüm dil + stdlib yeniliği burada toplanır.

### 3. Uyumluluğu bozan → doğrudan major (minor'da yasak)

Örnekler: bir builtin/FFI imzasının değişmesi (ör. `void`→`int` dönüş); **yeni bir
fonksiyonun var olması** (o fonksiyonu kullanan yeni kod eski sürümde çalışmaz →
ileri yönü bozar); CLI çıktısının biçim/içerik değişmesi (elle yazılmış tool ve
LSP'leri kırar). Bir şeyi elemek/değiştirmek gerekiyorsa: önceki minor'larda
**deprecated** uyarısı ("gelecek major'da kalkacak") basılır; gerçek kaldırma
yalnızca major'da yapılır.

### 4. Serileştirme çıktısı = API sözleşmesi

Dil içi serialize/deserialize (`struct::toJson()` vb.) aynı girdi için aynı sonucu
üretmek zorunda; determinizm performanstan önce gelir (kabul edilen yavaşlık). Bir
kez yayınlanan format **donar**; iyileştirme ancak yeni isimle (`toJson2()`) veya
major sürümle gelir. Bir stdlib fonksiyonunun çıktı biçimi, imzası kadar bağlayıcı
bir arayüzdür (Hyrum Yasası politika hâline getirilmiştir).

## Açık Maddeler (ayrıca karara bağlanacak — bu ADR omurgayı sabitler)

- **Platform-arası determinizm kapsamı.** Tek platformda bit-özdeşlik hedeftir.
  Platformlar arası (farklı CPU/OS) için karara bağlanacak: (a) serileştirmede
  **endianness** sabitleme (little/big — biri seçilmeli), (b) float format spec
  (`-0.0`, NaN payload, denormal), (c) **transandantal libm** (`sin`/`cos`/`exp` —
  `sqrt` IEEE-tam olduğundan hariç) sonuçları platform kütüphanesine bağlıdır →
  ya kendi deterministik implementasyon ya "son bit platforma bağlı" dokümante
  istisnası.
- **Gözlemlenemez kalması gerekenler** (baştan yasakla, sonradan sızıntı avlama):
  nesne adresleri (struct `==` kimlik ama adres asla basılmaz); GC zamanlaması
  (finalizer yok → gözlemlenemez, `defer` gelince korunmalı); ileride bir map/dict
  eklenirse iterasyon sırası; zaman/rastgelelik (`date::now()`, RNG — efektli
  fonksiyon ayrımı gerekebilir).
- **0.x fazı.** Uyumluluk garantisi 1.0.0'dan itibaren bağlayıcıdır; 0.x
  geliştirme fazında API stabil değildir, ancak bugfix/backport/yank hijyeni
  şimdiden uygulanır.

## Sonuç / Kapsam

Bu ADR sözleşmeyi kaydeder; **mekanizması** ayrı bileşenlerdedir: diferansiyel test
(#92) iki backend'in aynı sonucu verdiğini, golden testler çıktının sürümler arası
sabitliğini doğrular; yank/backport pratiği paket yöneticisi (#111) altyapısını
gerektirir. ADR-037'deki string kutulama farkı bu sözleşmenin ilk uygulamasıdır
(iç temsil farklı, gözlemlenen davranış aynı).
