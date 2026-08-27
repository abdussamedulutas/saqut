# saQut Repository Agent Rules

Bu dosya repository kökünün tamamı için bağlayıcı yerel ajan sözleşmesidir.
Bütün AI ajanları işe başlamadan önce bu dosyayı tamamen okumalıdır.

Rol sistemi yoktur. Tek bir çalışma disiplini vardır ve bu dosyada tanımlıdır.
Ajan, kendisine verilen görevi bu disiplinle yapar; ayrı bir rol prompt'u
okumaz, rol üstlenmez, rol devretmez.

## 1. Ürün otoritesi ve aktif sürümler

Ürün sahibi saQut'u kullanacak ilk kişidir. GC ergonomisi, concurrency modeli,
CLI yüzeyi, FFI kapsamı, platform desteği ve dil davranışı gibi kullanıcıya
görünen kararlar ürün sahibine aittir. Ajan seçenek, kanıt ve risk sunar; ürün
kararını kendisi vermez.

- `0.8.0` yayınlanmış tarihsel baseline'dır. Yeni geliştirme milestone'u
  değildir. Release geçmişi değiştirilmez.
- Aktif çalışma sürümleri yalnız `0.9.x` ve `1.0.0`'dır.
- `0.9.x`, VM ve veri-işleme geribildirimi için doğrulanabilir preview'dur.
- `1.0.0`, production-ready iddiası olmayan Feedback MVP'dir.
- v1'in tek stabil ve normatif backend'i VM'dir.
- MIR JIT her yerde `[EXPERIMENTAL]` ve v1 ürün sözleşmesinin dışındadır.
- AOT, `saqut build` ve tek executable v1 gereksinimi değildir.
- v1 public sözleşmesinde thread, fiber, actor veya async runtime yoktur.

Bağlayıcı kapsam:

- `docs/v1.0-kapsam-bildirgesi.md`
- `docs/adr/ADR-042-v1-feedback-mvp-ve-surumleme.md`
- `docs/v0.9-v1.0-yol-haritasi.md`
- `docs/v1.0-issue-disposition.md`

## 2. Oturum başlangıcı

Yeni bir oturum, değişiklik yapmadan önce:

1. Bu `AGENTS.md` dosyasını tamamen okur.
2. `knowledge-base/` altındaki bütün dosyaları okur.
3. `docs/saqut-compiler-audit-metodolojisi.md` dosyasını okur (§4'ün uzun formu,
   somut vaka çalışmalarıyla).
4. §1'deki dört bağlayıcı kapsam belgesini okur.
5. `git status --short` ve `git rev-parse HEAD` ile çalışma ağacını kaydeder.

Belleği korunan uzun bir oturum sonraki görevlerde bütün bilgi tabanını tekrar
okumak zorunda değildir; ancak her yeni görevde `AGENTS.md`'yi ve varsa görevin
issue'sunu yeniden okur, `git status --short` / `git rev-parse HEAD` kaydını
tazeler.

## 3. Otorite ve kanıt ayrımı

Normatif karar önceliği:

1. Kullanıcının mevcut görevdeki açık talimatı
2. Bu `AGENTS.md`
3. ADR-042 ve v1.0 Kapsam Bildirgesi
4. Yürürlükteki, supersede edilmemiş ADR'ler
5. Spec ve public dokümantasyon
6. Issue yorumları, roadmap, wiki ve eski toplantı notları

Gerçek uygulama durumu için kanıt önceliği:

1. Release artifact'ına bağlanan tekrarlanabilir test kanıtı
2. Güncel revizyondan temiz build ile çalıştırılmış tracked test
3. Aktif kaynak ve build tanımı
4. ADR/spec beklentisi
5. Issue veya rapor iddiası

Bir ADR "olması gerekeni", kaynak "şu anda olanı" anlatabilir. İkisi
çatışıyorsa ajan çatışmayı açıkça raporlar; ADR'yi uygulanmış saymaz ve kaynağı
istenen davranış ilan etmez.

Bir karar düzeltilecekse eski kayıt silinmez veya sessizce yeniden yazılmaz;
yeni kayıt neyin yerini aldığını açıkça söyler.

## 4. Çalışma yöntemi — şüpheci denetim

Bu bölüm zorunlu çalışma yöntemidir. Uzun formu, somut vaka çalışmaları ve
faz tabloları `docs/saqut-compiler-audit-metodolojisi.md` dosyasındadır.

### 4.1 Sıra

**Önce mimariyi anla, sonra kod yaz.** Doğrulanmamış hipoteze dayanarak
düzeltme yapılmaz.

1. **Görevi yeniden ifade et.** Kullanıcının açık sözüyle dolaylı hedefini
   ayır. "Tek bir bug" mu, "bug sınıfı" mı çözülecek — tek cümleyle yaz.
   Belirsizse sor.
2. **Anchor'la haritala.** Her katmanın giriş noktasını ve veri yapısını çapala
   (tip sistemi, lexer, parser, semantic, IR, VM temsil, GC, test). Bütün
   dosyaları okumaya kalkma. Katman ihlali ara: debug/LSP/DAP araçlarının
   runtime iç structlarına doğrudan erişimi.
3. **Spesifikasyonu çıkar.** Testleri spec olarak oku; ADR/docs'u kural olarak
   al; kodu kural değil "şu an olan" olarak al. İkisi çatışırsa raporla.
4. **Yatay dilim al.** Bir özelliği tek bağlamda doğrulama. Aynı tipi/özelliği
   şu bağlamların hepsinde ayrı ayrı kanıtla: tanım, atama, parametre, **dönüş**,
   array elemanı, struct alanı, karşılaştırma, aritmetik, cast, serialization,
   GC-scan, native sınır, const-fold. **"Bir bağlamda destekliyse diğerinde de
   destekli" varsayımı yanlıştır** — parser genelde ayrı kod yolları kullanır.
5. **Tip-erasür sınırını ara.** "Bilgi hangi katmandan itibaren siliniyor?"
   saQut'ta gözlenen bug'ların çoğunun ortak kökü budur: bilgi upstream'da
   hesaplanıp downstream'de tüketilmiyor veya yeniden keşfediliyor.
6. **Hipotez üret — alternatifiyle birlikte.** Her önemli çıkarım için:
   gözlem, kaynak (`file:satır`), teknik yorum, **alternatif açıklama**,
   doğrulama yöntemi, güven seviyesi (yüksek/orta/düşük).
7. **En ucuz deneyi yap.** Kod değiştirmeden doğrula: katman katman `grep`,
   `sizeof` ölçümü, smoke programı, `git log -- <file>` (temsil ne zaman
   değişti), golden test kapsam denetimi. Perf hipotezi için log-log süre
   eğrisi ve `--gc-stats`.
8. **Düzelt — yalnız doğrulanmış problemi.** En küçük diff. Aynı hatanın
   **tüm sitelerini** bul; tek site düzeltip sınıfı kaçırma. Shared helper
   tercih et. Kapsamı genişletme, "fırsat bulmuşken" refactor yapma.
9. **Regresyon koruması ekle.** Hatayı değil, **hata sınıfını** yakalayan test
   yaz. Negatif ve false-reject testleri dahil et.
10. **Yan etkiyi tara.** Değiştirilen sembolün tüm tüketicilerini yeniden çek.
    "Derledi" = "güvenli" değildir.
11. **Perf ile correctness'i karıştırma.** Perf bug'ını correctness fix'i gibi
    çözme; tersi de.
12. **Bilinmeyenleri yaz.** Doğrulanmamış her şeyi "doğrulanmadı" /
    "muhtemelen" / "bilinmiyor" olarak açıkça listele. Varsayımı gizleme.

### 4.2 İtiraz protokolü

Şu durumlarda kullanıcıya itiraz et: dil semantiği bozuluyorsa; test geçse bile
mimari tutarsızlık varsa; lokal düzeltme sistemik problemi gizliyorsa; perf
problemi correctness gibi ele alınıyorsa; backward-compat kırılıyorsa; yanlış
katmanda değişiklik öneriliyorsa; hızlı çözüm teknik borç yaratıyorsa; spec
belirsizse; iki davranış da mantıklı ama karar dil tasarımına bağlıysa.

Her itiraz şunları içerir: (1) itiraz edilen varsayım, (2) neden riskli,
(3) koddan kanıt, (4) alternatif, (5) alternatifin maliyeti, (6) öneri,
(7) kararın kullanıcıya ait olup olmadığı.

**Her şeye itiraz etme; sadece kanıtlı riskte.** Risk maddesi yazmadan önce
mevcut korumanın olup olmadığını oku — kendi risk listene de kanıt sınıfını
uygula.

### 4.3 Öğretici davranış

Karşılaşılan her önemli kavram için standart adını söyle (Pratt parsing, SSA,
tricolor marking, precise/conservative GC, safepoint, NaN-boxing, tagged
pointer, generational GC, write barrier, escape analysis, ...), kısa tanım +
bu projedeki karşılığı + neden önemli + araştırma anahtar kelimeleri. Yalnız
kodla bağlantılı olduğunda.

### 4.4 Yasaklar

- Uydurma dosya/sembol/satır referansı.
- Doğrulanmamışı kesin gibi yazmak.
- Vendor kodundaki eşleşmeyi (`mir/vendor/`, `nlohmann/json.hpp`) saQut'un
  kodu saymak.
- Kanıtsız başarı sözleri (§5).
- Perf ile correctness'i karıştırmak.
- Tek site düzeltip hata sınıfını kaçırmak.
- Kullanıcıya sormadan dil tasarımı kararı vermek.
- Yöntemi, kanıtı, alternatifi ve doğrulamayı gizlemek.

## 5. Dört aşamalı Definition of Done

Her özellik tam olarak bir kanıt durumundadır:

1. **Tasarlandı:** Kabul edilmiş karar ve ölçülebilir kabul kriteri var.
2. **Uygulandı:** Aktif kaynak yoluna entegre; diff kapsamı belli.
3. **Test Edildi:** Tanımlı revizyon/build üzerinde tracked test kanıtı var.
4. **Release Edildi:** Aynı kanıt zincirinden sürümlü artifact yayımlandı ve
   smoke doğrulandı.

Durumlar atlanamaz. Şu ifadeler tek başına yasak başarı kanıtlarıdır:

- "tamamlandı"
- "entegre edildi"
- "bug çözüldü"
- "tüm testler geçti"
- "hızlandı"
- "production-ready"
- "VM/JIT parity sağlandı"
- "leak yok"

Bu ifadeler kullanılacaksa hemen yanında commit, build, komut, test ve gözlenen
sonuç bulunmalıdır. Kanıt yoksa doğru ifade "uygulama iddiası", "test edilmedi"
veya "yeniden doğrulanmalı"dır.

Ajan kendi işini "Test Edildi" veya "Release Edildi" sayamaz; kanıtı sunar,
kabulü ürün sahibi verir.

## 6. Görev takibi

İş GitHub issue'da izlenir. Canlı durum panosu pinli issue'dur.

- GitHub issue/Project/PR state için authenticated yerel `gh` CLI canonical
  kaynaktır. GitHub eklentisi/app/connector/MCP, web fetch, tarayıcı, HTML
  scraping veya anonim API fallback olarak kullanılmaz.
- Bir görev bittiğinde sonuç ilgili issue'ya yorum olarak yazılır: incelenen
  kanıt, yapılan değişiklik, çalıştırılan komutlar, DoD durumu,
  **kanıtlanmayanlar**, riskler ve sonraki adım. Hiç değişiklik yapılmadıysa
  veya test çalıştırılmadıysa bu açıkça yazılır.
- "Gerekli her şeyi yap", "ilgili yerleri düzelt", "mükemmel hale getir" bir
  görev tanımı değildir; ajan tahminle çalışmaz, sorar.
- Issue kapatmak bug'ın çözüldüğü anlamına gelmez; kapanış gerekçesi kanıt,
  supersession, kapsam dışı bırakma veya duplicate durumunu açıkça söyler.

Uzun mimari kararlar ADR olarak `docs/adr/` altında yaşar. Büyük raw evidence
veya GitHub yorum sınırını aşan veri için `tasks/<TASK-ID>/` kullanılabilir ve
exact yol issue'da linklenir; binary veya dependency evidence olarak commit
edilmez.

## 7. Değişiklik güvenliği

- Başlangıçtaki dirty worktree kullanıcıya aittir; ilgisiz değişikliklere
  dokunulmaz.
- GitHub kimliği görev boyunca değişmezdir. Salt-okunur `gh auth status` ve
  `gh api user` serbesttir; kullanıcının mevcut turdaki açık talimatı olmadan
  `gh auth switch/login/logout/refresh` veya token/credential/keyring
  değişikliği yapılamaz. Access denied bu yetkiyi doğurmaz; state mutasyonu
  yapmadan durulur ve raporlanır.
- Dosya silme, release veya dependency ekleme yalnız açık yetkiyle yapılır.
- `git add .`, `git add -A` ve `git commit -am` dirty worktree'de kullanılmaz;
  stage yalnız görevin exact path'leriyle yapılır ve commit öncesi
  `git diff --cached --name-only` ile doğrulanır.
- `git reset --hard`, `git checkout --`, geniş recursive silme ve kullanıcı
  değişikliğini ezen işlemler yasaktır.
- Uygulama işi mümkünse exact base revision'dan ayrı bir worktree ve
  `issue-<numara>-<kısa-ad>` branch'inde yapılır. Sürüm dalları birbirine
  merge edilmez.
- Görev dışı formatlama, dosya taşıma veya toplu yeniden adlandırma yapılmaz.
- Generated/vendor dosyaları görev açıkça gerektirmedikçe değiştirilmez.
- Testi geçirmek için test zayıflatılmaz, kaldırılmaz veya davranış sessizce
  "beklenen" ilan edilmez.
- Stub, boş başarı, sessiz fallback ve sahte telemetry kabul edilmez.
- Davranış değiştiğinde etkilenen kaynak, `--help` metni, fixture, doküman ve
  ADR tek tek sayılır; sessiz geçilmez.
- İnternet kaynağı veya üçüncü taraf iddiası kullanılırsa kaynak ve erişim
  tarihi kaydedilir; ölçülmemiş benchmark kopyalanmaz.

## 8. Derleyiciye özgü kırmızı çizgiler

- Parser başarısı semantic/IR/runtime başarısı sayılmaz.
- Kaynakta fonksiyon bulunması CLI'dan erişilebilir olduğunu kanıtlamaz.
- VM davranışı JIT davranışını, JIT davranışı VM davranışını kanıtlamaz.
- **JIT hata-yakalama kapsamı artık kapalıdır:** MIR JIT backend'i
  `try/catch`/`throw`/yakalanabilir runtime error ("division by zero", cast,
  array out-of-range) dilimini 0.9.4'ten itibaren uygulamaya başladı (MIRPLAN
  §7/Dilim 5, iş #110). VM davranışı bu dilimde normatif kalır; JIT bunu
  VM ile aynı stdout/exit ile vermek zorundadır ve diferansiyel (VM≡JIT)
  suite'inde bu dilimdeki eşitsizlik artık regresyon sayılır. Hâlâ geçerli
  kısıt: JIT `[EXPERIMENTAL]`dır ve deterministik stacktrace alanı (ADL-025
  `trace`) diferansiyel fixture'larda gözlemlenmiyor — tam satır-düzeyi iz
  paritesi bu doğrulamanın dışındadır.
- "Embedded runtime" AOT değildir.
- Capability kontrolü process sandbox değildir.
- Mark-sweep kodunun varlığı kök doğruluğu, leak-free çalışma veya bounded
  pause kanıtlamaz.
- LSP/DAP sürecinin açılması protokol yeteneklerinin çalıştığını kanıtlamaz.
- Exit code, stdout ve stderr ayrı sözleşmelerdir.
- Eski binary ile üretilmiş sonuç güncel kaynak için kanıt değildir.
- Unit test sayısı gerçek program, protocol ve release kanıtının yerine geçmez.
- Golden test stdout black-box'tır; tek katman proxy'sidir. Hatalı binary'den
  üretilmiş expected self-fulfilling olur.

## 9. 0.9.x ve 1.0.0 kapsam koruması

0.9.x öncelikleri:

- normatif VM doğruluğu;
- `run/check/ast/ir/symbols` temel yüzeyi;
- JSON/XML ve CPU ağırlıklı doğrulama programları;
- sürekli açık ve gözlenebilir basit VM GC;
- eski iddiaların güncel binary ile yeniden doğrulanması;
- LSP/DAP v1 yetenek matrislerinin tasarlanması ve preview kanıtı.

1.0.0 öncelikleri:

- yedi zorunlu CLI/tooling yüzeyinin ilan edilen matrisinin tamamı;
- stub yüzeylerin kaldırılması;
- ürün sahibince seçilmiş dar host/FFI yüzeyi;
- dosya/klasör, SQLite, sıkıştırma ve blocking-TCP kanıt programları;
- platform/artifact kararının kanıtlanması;
- commit → build → test → artifact release izi.

Şunlar bu iki sürüme gizlice eklenemez: stabil JIT, AOT, concurrency, sandbox,
WASM, record/replay, production Redis/SQLite uyumluluğu, generic native library
loading, kapsamlı optimizer veya 500+ self-hosted stdlib migrasyonu.

## 10. Kod yazma standardı

Bu bölüm uygulayıcı (AI veya insan) için bağlayıcı kod kurallarıdır. Hedef
profil: performans odaklı, okunabilir, derin C/C++ uzmanlığı gerektirmeyen
kod.

**Karmaşıklık tavanı:** bir çözüm uzman düzeyinde C++ bilgisi gerektiriyorsa
(template metaprogramming, SFINAE, lock-free senkronizasyon, custom
allocator, standart dışı/UB-kenarı numaralar) o çözüm uygulanmaz; rapora
gerekçeyle yazılır ve karar mimara/ürün sahibine bırakılır. Akıllı değil
öngörülebilir olan tercih edilir; anlaşılması için açıklama gereken kod,
açıklamaya ihtiyaç duymayan koda yenik sayılır.

### 10.1 Yorum disiplini

- Dokunulan dosyanın BAŞLIK yorumu da güncellenir; yalnız iç satırlar
  değildir. Silinen sembol/fonksiyona atıf yapan hiçbir yorum kalmaz.
- Başlık yorumu geçmiş süreci değil MİMARİYİ anlatır ("adım 2'de şunu
  yapacağız" değil, "bu katman şunu yapar"). Süreç anlatısı commit mesajına
  aittir, dosya başlığına değil.
- Yorum bir kısıtı anlatır (neden böyle, neyin değişmeyeceği); kodun bir
  sonraki satırını tekrar etmez.

### 10.2 Tek tanım ve dürüst adlar

- Aynı veri iki yapıda tutulmaz: alan-kopyası struct yerine alias
  (`using X = Y`) veya tek struct kullanılır.
- Ad dürüst olur: bir yapının içeriği/nitelendişi değiştiyse eski ad veya
  eski sıfat ("legacy", "eski tablo", "geçici") düzeltilir; ad koddaki
  gerçeği yalan söylemez.
- Elle senkron gereken çapraz tablo eklenmez; tek kaynak + türetme ilkesi
  geçerlidir (kayıt birliği, #229, bu ilkenin uygulamasıdır).

### 10.3 Performans kuralları (basit ve öngörülebilir tercih)

- Sıcak yolda (talimat işleyici, dispatch, çağrı köprüsü): çağrı başına
  heap tahsisi yok; yeniden kullanılan scratch/tampon; string
  karşılaştırmasıyla dispatch yok — indeks/enum ile çözülür.
- Değişmeyen tablolar bir kez kurulur (statik init) ve `const&` ile döner.
- Erken return, düz döngü, bitişik veri tercih edilir. Mikro-optimizasyon
  ölçüm ister (§4.1); ölçülmeden karmaşıklık eklenmez.

### 10.4 Rapor dürüstlüğü

- Görev tanımındaki bir şart yerine getirilemiyorsa veya farklı
  karşılanıyorsa raporda AÇIKÇA yazılır; sessiz yarı-karşılanma olmaz.
- Doğrulanamayan her davranış "doğrulanamadı" olarak listelenir (§4.1.12
  zaten bunu ister; bu madde uygulayıcıya hatırlatmadır).
- Uygulayıcı kendi işini "Test Edildi" ilan edemez (§5); kanıtı sunar,
  kabulü mimar/ürün sahibi verir.
