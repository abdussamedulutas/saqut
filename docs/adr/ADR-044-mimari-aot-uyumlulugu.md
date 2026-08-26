# ADR-044 — Mimari AOT-Uyumluluğu Disiplini

İlgili: ADR-032, ADR-037, ADR-041, ADR-042, ADR-043, #81, #101, #115.

## Durum

**Kabul edildi (2026-08-25, ürün sahibi kararı).** Hiçbir kaydın yerini
almaz. ADR-042 ve v1.0 Kapsam Bildirgesi aynen yürürlüktedir: AOT, `saqut
build` ve tek-executable v1.0 gereksinimi olmaya devam eder ve 0.9.x/1.0.0'a
gizlice eklenmez (AGENTS.md §9). Bu ADR bir implementasyon kararı değildir;
yürürlükte bir **mimari kısıt** tanımlar:

> Bugün eklenen veya düzenlenen hiçbir yapı, ileride yazılacak bir AOT
> backend'inin maliyetini artırmamalıdır.

Ürün sahibi 2026-08-25'te sunulan AOT tasarım metnini (statik runtime
arşivi, derleyici içi sembol çözümü, doğrudan relocation, jump table, ELF/PE
üretimi) **tasarım hedefi** olarak değerlendirmiş; implementasyonu acil
görmemiştir. Bu ADR o hedefe köprüdür: implementasyon gelmeden önce
korunması gereken dikişleri bugünden taahhüt eder.

## Bağlam — bugünkü kanıt durumu

Repoda AOT/native-object üretimi için kod yoktur (ELF/PE yazıcı, object
parser, relocation işleme, `.a` bağlama yok; CLI'da `build` komutu yok —
`src/main.cpp:41-79`). Tek iz `src/mir/mir_backend.cpp:1317`'deki bir TODO
yorumudur. ADR-032 uzun vadeli yön olarak kabul edilmiş, v1 ürün iddiaları
ADR-042 ile supersede edilmiştir.

Buna karşılık mevcut mimari, AOT'yi zorlaştıran üç klasik hata sınıfından
bilinçli olarak kaçınmıştır:

1. **Host çağrı dikişi:** `rt_host_call` tek `extern "C"` giriş noktasıdır;
   "bir backend'i host çağrılarına açmak = bu tek fonksiyonu import etmek"
   (`src/ffi/host_registry.hpp:62-71`). LLVM/gccjit eklense de aynı imza
   geçerli kılınmıştır.
2. **GC kökleri:** JIT referansları shadow stack protokolü ile görünürdür
   (`rt_jit_shadow_enter/set/leave`, `src/mir/mir_backend.cpp:265-275`);
   kök doğruluğu makine kodundan precise stack map çıkarımı gerektirmez.
3. **Sembol bağlama:** üretilen kodun tüm dış sembolleri tek noktadan
   bağlanır (MIR import bildirimleri + `MIR_load_external`,
   `src/mir/mir_backend.cpp:1189-1193, 2661`); feature kodunda dağınık
   `dlsym`/`dlopen` yoktur.

Faz C (FFI/CALLHOST yeniden düzenlemesi) tam bu üç dikişün üzerine
dokunacaktır. Yanlış bir düzenleme (ör. backend başına ayrı çağrı yolları,
dikişe sızan C++ tipleri, bağlamayı bypass eden hızlı yollar) gelecekteki
AOT'yi mimari değil sosyal zorluğa çevirir: her backend'i yeniden yazmak
gerekir.

## Karar — altı kural

Aşağıdaki kurallar 0.9.x/1.0.0 süresince her yeni veya düzenlenen
runtime-yüzlü yapı için bağlayıcıdır. "Runtime-yüzlü yapı": üretilen kodun
(VM komut işleyici, JIT lowering/trampoline, host/builtin kaydı, GC kökü,
çalışma zamanı veri tablosu) okuduğu veya çağırdığı her şey.

**K1 — Dikiş `extern "C"` kalır.** Üretilen kodun çağrdığı her runtime
hizmeti, kararlı adlı bir `rt_*` C sembolüdür. C++ mangling'li sembol,
template, lambda veya C++ sınıf referansı içeren imza üretilen koda
açılamaz. Bugünkü örnekler: `rt_host_call`, `rt_jit_shadow_*`. AOT'de bu
semboller statik runtime arşivinden link-time'da çözülür.

**K2 — Dış sembol bağlama tek tabloda.** Üretilen kodün kullandığı tüm dış
semboller tek kayıt/bağlama noktasından geçer (bugün: MIR import listesi +
`MIR_load_external` çağrıları). Feature kodunda doğrudan `dlsym`/
`GetProcAddress`/`dlopen` eklenmez. AOT bu tabloyu ikili içine gömülü bir
bağlama/jump-table katmanına çevirir; tablo temsili değişirse dönüşüm tek
yerden yapılır.

**K3 — GC kökleri açık protokolle.** Her backend referans değerleri shadow
stack protokolü (veya eşdeğer, kod-agnosis açık kök yapısı) üzerinden GC'ye
görünür kılar. Dil semantiği, kökleri yalnız makine kodundan precise stack
map çıkarımıyla bulunabilir kılacak şekilde genişletilmez. Host çağrısının
dönüşünde üretilen referans/sahiplik gerektiren değerler bugünkü modelde
çağrı-ömürlüdür (`HostRetOwner`, `src/ffi/host_bridge.hpp:61-78`); kalıcı
çözüm (ör. GC-yönetimli dönüş değeri) backend-nötr tasarlanmak zorundadır.

**K4 — Gömülü sayısal kimlikler kararlı, sembolik ad tek kaynak.** Host
registry ve data registry indeksleri IR'ye gömülür; blok tabanları sabittir
(`kHostFnBase=0`, `kBuiltinBase=256`, `kCoreBase=512`,
`src/ffi/host_registry.hpp:43-48`), yeni kayıtlar sona eklenir, sembolik ad
tek kaynaktır ve drift hatayla değil sessizce yanlış dispatch ile değil
bildirilir (`src/data/data_registry.hpp:36-41`). AOT'de bu indeksler ikiliye
gömülü tablonun indeksiyle aynı anlamı taşır. Eski IR ↔ yeni runtime tablosu
sürüm uyumu ayrı bir format/sürüm ADR'sinin konusudur; bu ADR onu taahhüt
etmez.

**K5 — Runtime verisi ikiliye gömülebilir olmalı.** Runtime'ın okuduğu
tablolar (hata mesajları, tip metadata, sabit havuzları) derleyici
sürecinden veri olarak taşınabilir olmalıdır; program çalışma zamanı
derleyici sürecine, derleyici global state'ine veya derleyici binary'sinde
bulunan kaynaklara geri dönüp soramaz. "Derleyici yanında çalışıyor"
varsayımı dil davranışına giremez.

**K6 — Ağır bağlama bağımlılığı bilinçli karar gerektirir.** Runtime C++
koduna TLS, dinamik initializer'lar, statik ctor sırası bağımlılığı veya
geniş exception kullanımı gibi statik bağlamada ek maliyet getiren
bağımlılıklar ancak gerekçeli ve bu ADR'ye atıfla eklenir. Bu bir ekleme
yasağı değil, görünürlük kuralıdır: maliyet sessiz birikmez.

## Faz C (FFI/CALLHOST) uygulaması

Faz C bu disiplinin ilk sınavıdır ve şu ek taahhütlere bağlanır:

- CALLHOST düzenlemesi `rt_host_call` ABI'sını korur; yeni çağrı aileleri
  varolan indeks uzayına **yeni blok olarak** eklenir, mevcut indeksler
  kaydırılmaz (K4).
- JIT'e hızlı yol (inline trampoline, doğrudan çağrı) eklenirse bu yol tablo
  kaydının semantiğini birebir taşır; ayrı davranış çatallanması değildir.
  VM≡JIT parity suite'i bunu zaten zorunlu kılar; AOT≡VM aynı suite'ten
  geçecektir.
- Host dönüş tiplerine `Ref` eklenirse K3 sahiplik kuralı uygulanır.

## Denetim ve itiraz

Her runtime-yüzlü görevin issue raporunda **"AOT-uyumluluk denetimi"**
maddesi bulunur: K1-K6'dan hangilerine dokunulduğu ve kuralın korunduğu ya
da ihlal edildiği açıkça yazılır. İhlal şüphesi AGENTS.md §4.2 itiraz
protokolü ile ürün sahibine gider; ajan ihlali kendi başına çözemez.

## Kanıtlanmayanlar ve kapsam dışı

- Bu ADR hiçbir kod değişikliği iddia etmez; kabulüyle yalnız
  `docs/adr/ADR-044-mimari-aot-uyumlulugu.md` ve yol haritası notu eklenmiştir.
- AOT'nin hangi sürümde, hangi kapsamla implement edileceği ayrı ve açık bir
  ürün kararına kalır (o geldiğinde yeni bir ADR, ADR-032/042 ile ilişkiyi
  açıkça kurar).
- Statik runtime arşivi, relocation motoru, ELF/PE emission ve jump-table
  tasarımı bu ADR'nin konusu değildir; onlar ADR-032'nin uzun vadeli yönüne
  ve gelecek tasarım ADR'lerine aittir.
- K1-K6'nın mevcut koddaki örnekleri bu ADR'de kaynak konumlarıyla
  listelenmiştir; kural ihlali olup olmadığının sistemik denetimi (kodometri,
  CI kapısı) henüz yoktur ve bu ADR onu taahhüt etmez.

## Sonuç

AOT implementasyonu ertelenmiş bir ürün kararidir; AOT-uyumluluğu bugünden
yürürlüğe giren bir mimari kısıttır. K1-K6 korunduğu sürece gelecekteki AOT
backend'i yalnızca "bağlama + üretim" katmanı olarak eklenir: dikişler
aynıdır, tablo aynıdır, kökler aynıdır. Bu, 0.9.x'in VM doğruluk odağından
tek bir adım geri atmadan sağlanabilir.
