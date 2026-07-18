# ADR-041 — Self-Hosted Standard Library & Thin Runtime Architecture

İlgili: ADR-032 (MIR JIT + gömülü-runtime AOT), ADR-034 (FFI declaration modeli +
sayısal host dispatch), ADR-037 (JIT Value ABI), ADR-038 (determinizm + sürüm
uyumluluğu), ADR-017 (batteries = sınır/FFI problemi; kripto elle yazılmaz),
ADR-033 (UFCS builtin sözdizimi), ADR-035 (capability modeli), #119 (string
yönetimi), #92 (diferansiyel test).

Durum: **Kabul edildi — kilitli karar.** Bu ADR'den sonra "builtin/FFI davranışı
nerede yaşar" sorusu yoruma açık değildir; §3'teki karar ağacı mekanik yanıt verir.

---

## Bağlam

saQut'un bugün iki, yakında üç (VM referans + MIR JIT + gömülü-runtime AOT),
2.0 ufkunda dört+ (WASM, olası LLVM) backend'i olacak. Her builtin/FFI'nin
**davranışı** bugün her backend'de ayrı yazılıyor:

- `string::substring`, `string::upper`, `string::split` vb. **VM tarafında**
  `src/vm/interpreter.cpp` içindeki dev switch-case'te (ör. `case 16:
  string::substring`) host `std::string` çağrılarıyla implement edilmiş.
- **JIT tarafında** (`src/mir/mir_backend.cpp`) bu builtin'lerin çoğu **hiç yok**:
  `isSupportedCallhost()` yalnızca **tek-argümanlı `print`**'i geçiriyor
  (satır ~277). `str.upper()` içeren herhangi bir program `wholeProgramSupported()`
  tarafından tümüyle VM'e düşürülüyor (ikili whole-program gate).
- JIT'in desteklediği az sayıda işlem bile **ikinci bir bağımsız implementasyon**:
  `rt_jit_print_int`, `rt_jit_decimal_to_str`, `rt_jit_print_decimal` gibi
  `extern "C"` trampolinler `interpreter.cpp`'deki karşılıklarıyla **hiç kod
  paylaşmıyor**, elle senkron tutuluyor.
- `src/ffi/root_sqt.hpp`'de **44 `ffi` bildiriminin yalnızca 13'ü `requires`**
  (gerçek capability-gated dünya-teması). Kalan 31'in çoğu — `abs`, `min`, `max`,
  `date::addDays`, `date::year`, `date::month`, `floor`, `ceil` — capability
  istemiyor ama yine de **C++ host fonksiyonu**. `addDays` = epoch-ms üstünde saf
  aritmetik; dünyaya dokunmuyor, ama gereksiz yere host sınırının ötesinde yaşıyor.

Maliyet modeli bugün **çarpımsal**: `N_backend × N_builtin`. Şu an ~20 builtin ×
küçük backend sayısı yönetilebilir. Ama stdlib dalgaları (fs/net/crypto + string/
array genişletme + base64/json/regex/…) builtin sayısını **500+**'a taşıyacak.
O noktada:

1. Yeni bir builtin eklemek/düzeltmek her backend'e ayrı dokunmayı gerektirir.
2. Yeni bir backend (WASM) eklemek 500 builtin'i o backend için **yeniden**
   yazmayı gerektirir.
3. İki bağımsız implementasyon determinizm sözleşmesini (ADR-038: VM ≡ JIT
   gözlemlenen davranış birebir) sürekli tehdit eder; senkron yalnızca
   diferansiyel test disipliniyle korunur.

Bu, bakım hızını projeyi durduracak seviyeye çekebilecek bir **mimari borçtur**.
Sorun determinizm, okunabilirlik veya ham performans değil; **genişletme ve
backend-ekleme maliyetinin builtin sayısıyla çarpımsal büyümesidir.**

---

## Karar

Maliyeti çarpımdan **toplama** indiriyoruz: `N_backend + N_builtin`. Bunu dört
katmanlı bir mimari ve keskin bir "davranış nerede yaşar" sınırıyla sağlıyoruz.

### 1. Dört katman

```
┌─ Katman 3 — Self-hosted stdlib (saQut KAYNAĞIYLA yazılır, derleyiciye gömülü) ─┐
│  substring · split · trim · replace · upper/lower · base64 · crc32 · json ·    │
│  utf8 · path(manipülasyon) · uuid(format) · seeded-PRNG · regex …              │
│  → normal pipeline'dan geçer (parse → tip → IR)                                 │
│  BACKEND'E GÖRÜNMEZ: yalnızca Katman-1 opcode + Katman-2 CALLHOST üretir.       │
│  Backend maliyeti: SIFIR. (300 fonksiyon eklesen backend'e 0 satır.)            │
└───────────────────────────────┬─────────────────────────────────────────────────┘
                                │ yalnızca şunları kullanabilir ↓
┌─ Katman 2 — Host FFI seam (dünyaya dokunan atomlar; C++ ZORUNLU) ──────────────┐
│  fs.readFile · net.connect · sys.random · date.now · console.write · crypto …  │
│  → TEK jenerik host-çağrı ABI (CALLHOST + sayısal HostFnId, ADR-034)           │
│  Capability tek kapıda enforce (interpreter.cpp:~1204, instr.requiredCap).      │
│  Backend maliyeti: SABİT. Backend tek köprü yazar; 500 FFI o köprüden geçer.    │
└───────────────────────────────┬─────────────────────────────────────────────────┘
                                │ ve şu atomların üstünde durur ↓
┌─ Katman 1 — Çekirdek intrinsic opcode'lar (dilin ifade edemedikleri) ──────────┐
│  byte-get/byte-len/bytes↔string · array new/get/set/len · struct field ·       │
│  aritmetik · karşılaştırma · kontrol akışı → KAPALI, KÜÇÜK küme (~50 opcode)    │
│  Backend maliyeti: her backend BUNLARI implement eder — ama sabit ve küçük.     │
└───────────────────────────────┬─────────────────────────────────────────────────┘
                                ▼
              Katman 0 — Backend (VM / MIR JIT / AOT / WASM / …)
              register allocation, calling convention — tamamen backend-özel
```

**Bel kemiği cümlesi:** Bir backend'in bilmesi gereken **yalnızca Katman 1
(kapalı, küçük opcode seti) + Katman 2 (tek host-çağrı ABI)**'dir. Katman 3'ün
yüzlerce fonksiyonu backend için görünmez — o sadece Katman 1'e derlenmiş IR'dir.
Backend IR çalıştırmayı zaten Katman 1 için biliyor; stdlib "önceden yazılmış IR"
olduğundan bedava gelir.

### 2. Dört sınıf

Her builtin/FFI/stdlib fonksiyonu **tam olarak bir** sınıfa girer:

| Sınıf | Katman | Nerede yaşar | Örnek |
|---|---|---|---|
| **Intrinsic** | 1 | her backend'de codegen (küçük, kapalı küme) | byte-get, array-set, `+` |
| **Host FFI** | 2 | C++ host fonksiyonu + tek `ffi` bildirimi | `fs.readFile`, `crypto.sha256` |
| **Self-hosted stdlib** | 3 | `std/*.sqt` saQut kaynağı, gömülü | `substring`, `json.parse` |
| **Intrinsic-terfi adayı** | 3 (+ opsiyonel 1) | stdlib olarak yaşar; profiling kanıtıyla backend'de özel-kodlanabilir | `utf8.decode`, `regex` motoru |

Dördüncü sınıf bir **durum**tır, ayrı bir yer değil: fonksiyon Katman 3'te
yaşamaya devam eder; yalnızca ölçülmüş bir hot-path için bir veya daha çok
backend onu intrinsic olarak özel-kodlar (§4).

### 3. Objektif karar ağacı (mekanik — "hissettirdiği için" yasak)

Bir fonksiyonun sınıfı şu ağaçtan **tek** yanıt alır. Sırayla uygulanır; ilk
eşleşen kazanır:

```
1. Syscall / dış durum / entropi / sistem saati gerektiriyor mu?
   (dosya, soket, süreç, ortam, gerçek-rastgelelik, now())
      EVET → Host FFI (Katman 2)

2. Güvenlik-kritik mi? (kriptografik hash/şifre/imza/MAC)
      EVET → Host FFI (Katman 2), vendored kütüphane.
             GEREKÇE: ADR-017 — kripto asla elle yazılmaz. Saf hesaplama OLSA
             DA timing-safe + audit edilmiş implementasyon zorunlu.

3. Olgun, audit-edilmiş kütüphanesi olan + doğruluk/güvenlik-kritik +
   yeniden-yazma-değeri olmayan bir alan mı? (sıkıştırma, TLS)
      EVET → Host FFI (Katman 2), vendored.
             GEREKÇE: ADR-017 — "zlib'i yeniden yazma". Saf olsa da bozuk
             implementasyon veri/güvenlik kaybı; olgun kütüphane elle-yazımdan
             üstün.

4. Dilin mevcut primitifleriyle ifade edilebilir mi?
   ATOMİKLİK TESTİ: fonksiyonu yazmak için kendisine-benzer daha alt bir
   yeteneğe (byte erişimi, ham bellek, temsil oluşturma) zaten ihtiyaç var mı?
      HAYIR (ifade edilemez / temsil-sahibi) → Intrinsic (Katman 1)

5. Buraya gelen her şey: saf + ifade-edilebilir → Self-hosted stdlib (Katman 3)
   5a. Büyük statik veri gerektiriyor mu? (Unicode/timezone tabloları)
         EVET → stdlib + gömülü-veri seam (1.0 sonrasına ertelenebilir).
   5b. Profiling'de kanıtlanmış hot-path mı?
         EVET → "intrinsic-terfi adayı" işaretle — ama ÖNCE stdlib yaz, ÖLÇ (§4).
```

**Neden bu sıra?** Adım 1 (dünya-teması) mutlak sınır: saQut'ta ifade edilemez,
tartışma yok. Adımlar 2-3 saf-hesaplama-ama-yine-de-FFI istisnalarını (kripto,
sıkıştırma) önce eler; aksi halde "saf → stdlib" kuralı bunları yanlış yakalardı.
Adım 4 atomiklik testi Katman 1'i objektif çizer (ifade-edilebilirlik ölçülebilir:
yazabildin mi yazamadın mı). Adım 5 varsayılan: kalan her şey stdlib.

### 4. Intrinsic-terfi kriteri (objektif, geri-döndürülebilir)

Bir stdlib fonksiyonunu bir backend'de intrinsic'e terfi etmek **yalnızca**:

- **Profiling kanıtı** varsa (öznel "sıcak hissettiriyor" değil; ölçülmüş darboğaz).
- Terfi **davranışı değiştirmez** (aynı gözlemlenen sonuç, ADR-038); yalnızca o
  backend o fonksiyonu elle kodlar.
- Terfi bir **optimizasyondur**, mimari zorunluluk değil: her backend'de ayrı
  yazılır → çarpımsal maliyet geri gelir, o yüzden yalnızca ölçülmüş kazanç için.
- **Geri-döndürülebilir**: intrinsic kaldırılıp stdlib'e dönülebilir (davranış aynı
  kaldığından fark gözlemlenmez).

Kural: **önce saQut'ta yaz → ölç → gerekirse terfi ettir.** Erken intrinsic yok.

### 5. 25 fonksiyon — kesin sınıflandırma

Her satır: sınıf · gerekçe (karar ağacı adımı) · başka katmanda olsaydı kayıp.

| Fonksiyon | Sınıf | Gerekçe | Başka katmanda olsaydı kayıp |
|---|---|---|---|
| **substring** | Stdlib | §5 — byte-get + bytes→string kompozisyonu | Intrinsic olsa: her backend'de tekrar (patlama). FFI olsa: gereksiz host sınırı |
| **split** | Stdlib | §5 — substring + indexOf döngüsü | aynı |
| **replace** | Stdlib | §5 — indexOf + concat döngüsü | aynı |
| **trim** | Stdlib | §5 — byte inceleme + substring | aynı |
| **upper/lower** | Stdlib (ASCII); Unicode case → 5a veri-gerektiren | §5 — ASCII saf tablo | Unicode case-folding büyük tablo ister → veri-seam |
| **base64** | Stdlib | §5 — saf bit-ops + 64-karakter sabit tablo (minik, kaynak-içi) | Intrinsic: absürt. FFI: gereksiz |
| **sha256** | **Host FFI** | §2 — kriptografik hash | Stdlib olsa: audit edilmemiş kripto = güvenlik açığı (timing/doğruluk) |
| **crc32** | Stdlib | §5 — checksum (GÜVENLİK DEĞİL), saf tablo | FFI: gereksiz. (Kriptodan farkı: güvenlik iddiası yok) |
| **lz4** | **Host FFI** | §3 — sıkıştırma, olgun kütüphane, doğruluk-kritik | Stdlib: bozuk sıkıştırma = veri kaybı; yeniden-yazma değersiz (ADR-017) |
| **gzip** | **Host FFI** | §3 — zlib vendored (ADR-017'nin tam örneği) | aynı |
| **json** | Stdlib | §5 — parser/serializer, dilin tipleriyle ifade edilir | FFI: esneklik kaybı. Glass-box için ideal (IR incelenebilir) |
| **regex** | Stdlib (+ 5b terfi adayı) | §5 — NFA/DFA saf hesaplama | Intrinsic: erken. Motor hot-path olursa terfi. ⚠️ ReDoS için determinizm dikkati |
| **utf8** (decode/encode) | Stdlib (+ 5b terfi adayı) | §5 — saf bit-manipülasyon, byte-get üstünde | Çok sıcak (her string işlemi) → muhtemelen İLK terfi adayı |
| **unicode normalization** | Stdlib + 5a veri-gerektiren | §5a — algoritma saf, NFC/NFD tabloları büyük | 1.0 sonrası (#119'da ertelendi). Veri-seam olmadan yazılamaz |
| **grapheme** | Stdlib + 5a veri-gerektiren | §5a — segmentation tabloları | aynı, 1.0 sonrası |
| **timezone** | Karma: hesap Stdlib + tz-DB (5a veya FFI) | §1/§5a — IANA veritabanı dış/güncellenen | tz-DB gömülü-veri ya da OS FFI; date'in ötesi, ertelenebilir |
| **path** (dirname/basename/join/normalize) | Stdlib | §5 — saf string manipülasyonu | path'in FS'e ÇÖZÜMÜ (realpath/exists) ayrı: o §1 → fs FFI |
| **date** | Karma: çoğu Stdlib, `now()` FFI | §5 (aritmetik) + §1 (`now()`) | `addDays/year/month` = epoch-ms aritmetik → Stdlib. ŞU AN yanlışlıkla FFI (root_sqt.hpp) → düzeltilecek |
| **random** | Karma: `sys.random()` FFI + seeded-PRNG Stdlib | §1 (entropi) + §5 (deterministik PRNG) | Gerçek entropi = FFI (CSPRNG). Deterministik xorshift(seed) = Stdlib (test/tekrar için) |
| **file** | **Host FFI** | §1 — syscall | Katman-2 tanımı gereği; başka yerde imkânsız |
| **socket** | **Host FFI** | §1 — syscall (#93 net) | aynı |
| **process** | **Host FFI** | §1 — syscall (fork/exec/env) | aynı |
| **thread** | **Kapsam dışı** | determinizm — hiçbir katmanda değil | Concurrency 1.0 sonrası izolasyon modeli (#116/#118); "her state'in tek mutator'ı" |
| **crypto** (hmac/ed25519/…) | **Host FFI** | §2 — vendored (Monocypher, #94) | Stdlib: elle-yazım güvenlik felaketi (ADR-017) |
| **uuid** | Karma: format Stdlib + entropi FFI | §5 (format) + §1 (random bytes) | v4 formatlama saf → Stdlib; rastgele baytlar sys.random FFI'den gelir |

**Bu tablodan çıkan üç istisna** ("saf hesaplama = stdlib" basit kuralının
kenar durumları — bunlar bilinçli, yoruma açık değil):

1. **Güvenlik-kritik saf hesaplama** (sha256, crypto) → **FFI**, çünkü elle-yazım
   güvenlik açığı (ADR-017, §2).
2. **Olgun-kütüphane + doğruluk-kritik saf hesaplama** (lz4, gzip) → **FFI**,
   çünkü yeniden-yazma değersiz ve riskli (ADR-017, §3).
3. **Büyük-statik-veri gerektiren saf hesaplama** (unicode, timezone) → **stdlib
   ama veri-seam ile** (§5a), 1.0 sonrasına ertelenebilir.

### 6. Katman 3'ün sınırları (stdlib sandbox sözleşmesi)

**İlke: stdlib, ayrıcalıklı primitiflere erişebilen ama kullanıcı koduyla aynı
güvenlik kurallarına tabi koddur. Sandbox içindedir — ama duvarını o örer.**

- **FFI çağırabilir mi?** Evet, ama bir modül **yalnızca kendi sorumluluğundaki**
  FFI atomunu sarar (`fs` modülü `FS_READ_FILE`'ı; `string` modülü hiçbir FFI'yi).
- **Capability?** Stdlib capability'yi **tüketmez, iletir.** `fs.readLines` (saQut)
  → `fs.readFile` (FFI, requires fs) → kontrol yine kullanıcının `--allow-fs`'ine
  bakar. Enforce CALLHOST'ta (interpreter.cpp:~1204) olduğundan stdlib içinden
  çağrı da **aynı kapıdan** geçer; stdlib bypass **edemez** (mekanik garanti).
- **Başka stdlib modülü?** Evet, ama **asiklik (DAG)** — modül döngü tespiti
  (ADR-031/#78) stdlib'e de uygulanır.
- **Reflection?** Hayır — dilde yok.
- **Thread?** Hayır — determinizm (§5, thread satırı).
- **Unsafe?** saQut'ta "unsafe" kavramı yok; en alt erişim bile bounds-checked
  (OOB → yakalanabilir hata, ADR-025). Stdlib'in kullanıcıdan tek farkı Katman-1
  atomlarına + kendi FFI atomunu sarma iznine erişimdir.

Alt-karar (uygulama sırasında netleşir): Katman-1 atomlarının hangileri kullanıcıya
`public`, hangileri `stdlib-only`? İlke **minimum ayrıcalık**: dil güvenli olduğundan
mümkün olduğunca çok atom public; yalnızca temsil-bozabilecek olanlar (ör.
sıfırlanmamış kapasiteyle array alloc) stdlib-only.

### 7. Paketleme: (A) kaynak-gömülü, derleme-zamanı derlenir

stdlib kaynağı derleyiciye gömülüdür (`root_sqt.hpp`'nin genişlemesi — orada zaten
gömülü kaynak seam'i var) ve her kullanıcı programı derlenirken normal pipeline'dan
geçer (süreç-içi cache mümkün; ModuleLoader root.sqt için zaten "bir kez parse"
yapıyor).

Reddedilen alternatif (B) "önceden IR'e serialize edilmiş gömülü" ile beş eksende
karşılaştırma:

| Eksen | (A) kaynak-gömülü | (B) önceden-IR | Kazanan |
|---|---|---|---|
| Performans | her derlemede parse+IR (stdlib küçükken ~ms, cache'lenebilir) | startup'ta parse atlanır | (B) — ama sadece stdlib büyürse; ve bugün-YOK altyapı ister |
| Diferansiyel test | tek frontend, VM/JIT aynı IR — doğal | serialize/deserialize round-trip'i **kendisi** test yüzeyi | **(A)** |
| Debugging | `saqut ir std/string.sqt`, gerçek satır no, stacktrace stdlib'e iner | serialize IR'de kaynak bağı/satır tablosu zayıflar | **(A) ezici** |
| Bootstrap | frontend stdlib'e bağımlı değil (Katman-1+2 yeter); çembersiz, tek-aşama | önceden-IR için çalışan derleyici gerekir = iki-aşamalı build + IR-format versiyonlama | **(A)** |
| Bakım | `.sqt` düzelt, bitti | düzelt + yeniden-serialize + blob + format uyumu | **(A)** |

**Karar: (A).** Beş eksenden dördünde net üstün; (B) yalnızca startup-performansında
teorik üstün, o da bugün olmayan IR-serialization gerektiriyor ve stdlib büyümeden
anlamsız.

**(B)'ye giden kapı açık ve tek-yönlü/kayıpsız bırakılır:** AOT gömülü-runtime (#81)
zaten IR'i binary'ye gömme altyapısını gerektiriyor. Startup profiling'de gerçekten
darboğaz çıkarsa, (B) o altyapının doğal uzantısı olarak eklenir — **aynı kaynak,
yalnızca paketleme değişir.** Bugün (B) yapmak erken; sırası AOT'nin arkası.

En güçlü (A) argümanı kimliksel: saQut "programlanabilir ve incelenebilir derleyici"
iddiasındaysa, stdlib'in de saQut'ta yazılıp `saqut ast/ir` ile incelenebilir olması
tezin doğal sonucudur. C++ switch-case kara kutu; `std/string.sqt` cam kutu.

### 8. Migrasyon: dikey dilim, ikame (ikili-yazım değil)

Sıra: **String** (pilot — #119 zaten orada + en yoğun küme) → **Array** → **Date**
(yarı-yanlış-sınıflandırılmış; `addDays/year/month` saQut'a, yalnızca `now()` FFI
kalır) → sonrası (base64/json/…) doğrudan Katman-3 **doğar**, hiç C++'a uğramaz.

Geçiş sırasında iki sistem bir arada yaşar, ama disiplinli:

- Migrasyon **ikame**dir: bir modül saQut'a taşındığında C++ karşılığı **silinir**.
  Aynı anda iki implementasyon tutmak zaten şu anki hastalık, tekrar üretilmez.
- Ara durum ("string saQut'ta, array hâlâ C++'ta") sorun değil: dispatch aynı
  (ikisi de IR çağrısı), kullanıcı farkı görmez.
- **Modül-atomikliği**: bir modül ya *tamamen* saQut ya *tamamen* C++; yarım modül
  yasak (yarı-taşınıp unutulma riskini keser). Her dilim bir issue ile takip edilir.

Bir dilimin "bitti" sayılması için **üç şart birden**:
1. O modülün golden testleri yeşil (davranış bit-bit korundu — ADR-038 sözleşmesi),
2. VM ≡ JIT diferansiyel geçti (#92),
3. C++ implementasyonu **silindi**.

**Güvence:** Bir builtin'i taşımadan **önce** mevcut C++ davranışının golden testi
olmalı; taşıdıktan sonra aynı golden geçmeli. Bu, taşımanın yanlış çıktı
üretmediğinin mekanik kanıtıdır.

### 9. Backend-bağımsızlık invariant'ı (formal + CI-testable)

**İddia:** Yeni backend maliyeti = `f(|Katman-1|) + g(1 host-ABI) + h(backend-altyapısı)`.
**Hiçbir terim `|Katman-3|` (builtin sayısı) içermez.**

| Katman | Yeni backend eklenince | Maliyet |
|---|---|---|
| 3 — stdlib | **Sıfır** yeniden implementasyon; tamamen yeniden kullanılır (IR'dir) | 0 — builtin sayısından bağımsız |
| 2 — FFI seam | Backend başına **tek** host-call köprüsü | Sabit — FFI sayısından bağımsız |
| 1 — intrinsic | Backend başına tam yeniden impl — ama kapalı, küçük (~50 opcode) | Sabit (küçük) |
| 0 — altyapı | RA/calling-convention — backend-özel | Backend-özel (zaten öyle) |

Yani bugün 20 builtin, yarın 500 builtin — WASM eklemenin maliyeti **değişmez**.

**Mekanik garanti (davranışa değil mekanizmaya dayanır):** "stdlib'in ürettiği IR
yalnızca Katman-1 opcode + Katman-2 CALLHOST içerir" bir **CI invariant'ı** olur.
Biri stdlib'e backend-özel bir sembol sızdırırsa CI patlar. `saqut ir` çıktısı
üstünde bir kontrol kadar basit. Bu, backend-bağımsızlığın kağıt-üstü vaadi değil
derleme-zamanı garantisidir.

---

## Reddedilen alternatifler

- **(B) önceden-IR paketleme** — §7'de beş eksende elendi; AOT sonrası opsiyonel
  optimizasyon olarak kapı açık bırakıldı.
- **Her backend'de builtin (mevcut durum)** — çarpımsal maliyetin ta kendisi; bu
  ADR'nin çözdüğü sorun.
- **Tam self-hosting (FFI dahil her şey saQut'ta)** — imkânsız: syscall/entropi/
  sistem-saati dilin primitifleriyle ifade edilemez (karar ağacı §1). Katman-2
  indirgenemez bir çekirdektir; hedef onu **daraltmak**, yok etmek değil.
- **C transpile ile stdlib** — ADR-032'de zaten elendi (kullanıcı makinesinde C
  toolchain isterdi); burada da geçersiz.
- **Güvenlik-kritik/sıkıştırma kodunu da self-host etmek** — ADR-017 ihlali;
  karar ağacı §2-3 bunları FFI'de tutar.

---

## Sonuçlar (bu mimari hangi problemleri çözer)

- **Backend patlaması engellenir:** §9 invariant'ı — yeni backend maliyeti builtin
  sayısından bağımsız. WASM/LLVM/AOT eklemek Katman-1 (~50 opcode) + tek host-ABI
  yazmaktır; stdlib bedava gelir.
- **500+ builtin'de bakım maliyeti artmaz:** her yeni saf-hesaplama builtin'i tek
  bir `.sqt` fonksiyonu; backend'e sıfır dokunuş, C++ tarafı büyümez. Karmaşıklık
  `src/` C++'ından `std/` saQut'una (dilin kendisi, herkesin okuyabildiği) kayar.
  Bu senin "src 253→1000 dosya" kaygını **tersine çevirir**: yüzlerce C++ builtin
  dosyası hiç oluşmaz.
- **WASM/LLVM/AOT'de yeniden-implementasyon gerekmez:** Katman 3 IR'dir; backend
  onu Katman-1 için nasılsa çalıştırır (§9).
- **Determinizm korunur:** stdlib tek IR üretir → VM ve JIT **aynı** IR'i çalıştırır;
  "iki bağımsız implementasyonu senkron tutma" sorunu Katman-3'te **ortadan kalkar**
  (ADR-038 sözleşmesi bir kod hattında toplanır). Kalan diferansiyel yükü yalnızca
  Katman-1+2'ye iner.
- **Diferansiyel test kolaylaşır:** #92'nin karşılaştırması Katman-3 için gereksiz
  (tek IR); yalnızca ~50 intrinsic + tek host-ABI'yi doğrular. Test yüzeyi builtin
  sayısıyla büyümez.
- **Cam-kutu (glass-box) felsefesi güçlenir:** kullanıcı `saqut tokens/ast/ir` ile
  stdlib'in **kendisini** de inceleyebilir. `json.parse`'ın IR'ini görmek, C++
  switch-case'in içine bakamamaktan niteliksel olarak farklıdır. stdlib artık
  ürünün "programlanabilir + incelenebilir" tezinin bir parçası, dışında değil.

---

## Açık maddeler / bu ADR'nin doğurduğu gelecek işler

- **Katman-1 çekirdek setinin string bölümü #119'a bağlı:** byte/codepoint erişim
  atomları #119'un çözümüyle tanımlanır (aynı karar; ayrı verilirse çelişir).
  Katman-1-core (array/struct/aritmetik) #119'dan bağımsız, önce dondurulabilir.
- **Katman-1 görünürlük tablosu** (public / stdlib-only) — §6 alt-kararı.
- **Gömülü-veri seam** (Unicode/timezone tabloları için read-only `byte[]` resource)
  — §5a; 1.0 sonrası.
- **Tip-temsili backend patlaması (ADR-037 genişlemesi):** Bu ADR *davranış*
  eksenini çözer ama *tip temsili* eksenini çözmez — her yeni tip hâlâ VM inline
  `Value` + JIT box'ta ayrı ele alınır (self-hosting'in ikiz kardeşi). Ayrı issue.
- **Diagnostic mesaj merkezileştirme:** 1.0 freeze (ADR-038) öncesi tek katalog.
  Ayrı issue.
