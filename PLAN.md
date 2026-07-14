# saQut — 0.8.0 ve 0.9.0 İş Sırası

> 0.7.0 kapandıktan sonra hazırlandı. Yalnızca mevcut GitHub issue'larının
> sıralı bir özeti — yeni tasarım kararı içermez (kararlar kendi issue'larında
> ve ilgili ADR'lerde).

## 0.7.0 kapanışı (referans — bu dalda tamamlandı)

- #108 IR scope-shadowing düzeltmesi
- #76/ADR-035 capability çekirdeği (`--allow-fs/net/sys`, A+B enforcement)
- #91 `caps::drop`/`caps::has`
- #87 fs modülü
- #90 sys modülü
- #88/ADR-036 date tipi + modülü
- #89 math modülü (+ `PI()`/`E()` sabitleri)
- #107 FFI declaration modeli (ADR-034) — math ile birlikte uçtan uca doğrulandı
- Kod kalitesi: Lexer `positionRange()` leak-prone ham işaretçi → `std::pair`
- `docs/CLI.md`: CLI parametre formatı tasarımı (üç alternatif, hibrit önerisi)

## 0.8.0 — MIR JIT + diferansiyel test

**Sıra gerekçesi:** #92 (diferansiyel test altyapısı) #80'den (MIR JIT) ÖNCE
bitmeli — ADR-032'nin "VM ve JIT aynı IR'de bire bir aynı çıktı vermek
ZORUNDA" kısıtı, JIT kodu yazılmaya başlamadan ÖNCE otomatik doğrulama
iskelesinin hazır olmasını gerektirir; aksi halde JIT'teki sapmalar sessizce
golden testlere karışır.

1. **#92 — Diferansiyel test altyapısı.** Her golden test'in VM ve (henüz
   yazılmamış) JIT çıktısını bayt-bayt karşılaştıran koşum mekanizması.
   JIT henüz yokken bu adım "iskeleti kur + VM'i kendi kendine karşı çalıştır"
   olarak başlar (regresyon bariyeri); #80 ilerledikçe gerçek karşılaştırmaya
   döner.
2. **ArgParser çekirdeği** (`docs/CLI.md`'de tasarlanan hibrit model —
   Alternatif 3). `src/cli/arg_spec.hpp` + yeni `ArgParser`; önce `run`/`ir`
   (en çok bayrak taşıyanlar) yeni şemaya taşınır, sonra basit komutlar
   (`tokens`/`ast`/`symbols`). **Bu adım kullanıcı onayına bağlı** — `CLI.md`
   üç alternatifi sundu, hangisinin seçildiği teyit edilmeden koda geçilmez.
3. **#80 — MIR JIT backend (ADR-032).** ⚠️ **Kullanıcı talimatı: buraya
   gelince DUR ve sor.** Bu belge yalnızca sırayı kaydeder, kod yazımına
   başlanmaz. Ön koşullar: (1) #92'nin gerçek VM↔JIT karşılaştırması yapacak
   hale gelmesi, (2) shadow-stack GC kökü tasarımının ADR-032'de zaten
   çizilen taslaktan koda geçirilmesi.

## 0.9.0 — AOT + net + crypto

**Sıra gerekçesi:** #81 (AOT) MIR JIT'in (#80) IR→makine-kodu yolunu yeniden
kullanır ("gömülü-runtime AOT" modeli `deno compile`'a benzer — runtime
kopyası + IR gömülü tek exe), bu yüzden 0.8.0'ın ürünlerine bağımlı. #93/#94
(net/crypto) capability modeline (ADR-035, 0.7.0'da kuruldu) ve FFI seam'ine
(ADR-034) bağımlı ama JIT'e bağımlı DEĞİL — teorik olarak 0.8.0 ile paralel
de yürüyebilir, ama sürüm numarasına göre 0.9.0'a bırakıldı.

1. **#81 — `saqut build` (gömülü-runtime AOT).** Tek exe paketleme; linker'sız
   (kullanıcı makinesinde sıfır toolchain kısıtı, ADR-032). CLI'ya yeni komut
   ekler (`saqut build`) — `docs/CLI.md`'deki düz-komut kuralına uyar (tek
   eylem, grup gerektirmiyor).
2. **#93 — net modülü.** Ham TCP connect/send/receive/close, `--allow-net`.
   HTTP/TLS kapsam dışı (issue'da açıkça işaretli). fs/sys ile aynı desen:
   `HostContext` üzerinden VM'ye erişim gerekebilir (soket handle'ları için
   — ADR-034 §5'teki "handle YOK, tek atımlık" ilkesinin TCP'ye nasıl
   uyarlanacağı bu issue'nun kendi açık sorusu).
3. **#94 — crypto modülü.** Monocypher vendoring (kripto elle yazılmaz
   ilkesi, ADR-017); blake2b/sha256/hmac/randomBytes/Ed25519. `sys::random`
   (0.7.0) genel amaçlı CSPRNG'den beslenir ama kriptografik garantisi yok —
   bu modül ayrı, kriptografik kaliteli kaynak sağlar (issue'nun kendi
   ayrımı).

## Bu sırada YER ALMAYAN (bilinçli dışarıda bırakıldı)

- **Faz 4 refaktör devamı** (parser.cpp/symbol_collector.cpp dosya bölünmesi):
  0.7.0 kapanışında yalnızca Lexer'daki gerçek bellek-güvenliği sorunu
  düzeltildi; daha büyük dosya-bölme refaktörleri risk/fayda oranı düşük
  görüldüğü için ertelendi. Gerekirse ayrı, dar kapsamlı bir refaktör turu
  olarak 0.8.0/0.9.0 arasına sıkıştırılabilir — MIR JIT'ten önce `ir_generator`/
  `interpreter` CALLHOST dispatch'inin büyümesi izlenmeli (şu an 3 dal:
  `__builtin_method__`/`__ffi__`/host-fn; JIT ikinci bir backend eklerse bu
  dispatch'in iki backend'de de aynı davranması gerekecek — o noktada
  paylaşılan bir dispatch tablosu gerçek bir ihtiyaç haline gelebilir).
- **#106** (heavy-IR idiom tanıma): `fikir` etiketli, versiyon atanmamış —
  kullanıcı talimatı gereği bu tur dışında.
- **#99/#100/#101/#102** (WASM, record-replay, roadmap-meta, editör
  entegrasyonları): `fikir` etiketli veya v1.1.0+ — kapsam dışı.
- **#95/#96/#97/#98** (CI matrix, 1.0 stabilizasyon, website, ekran görüntüleri):
  v0.10.0 etiketli — bu plan yalnızca 0.8.0/0.9.0'ı kapsıyor.
