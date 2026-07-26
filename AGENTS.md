# saQut Repository Agent Rules

Bu dosya repository kökünün tamamı için bağlayıcı yerel ajan sözleşmesidir.
Bütün AI ajanları işe başlamadan önce bu dosyayı tamamen okumalıdır. Model veya
rol prompt'u bu kuralları daraltabilir; gevşetemez.

## 1. Ürün otoritesi ve aktif sürümler

Ürün sahibi saQut'u kullanacak ilk kişidir. GC ergonomisi, concurrency modeli,
CLI yüzeyi, FFI kapsamı, platform desteği ve dil davranışı gibi kullanıcıya
görünen kararlar ürün sahibine aittir. Ajan seçenek, kanıt ve risk sunar; ürün
kararını kendisi vermez.

- `0.8.0` yayınlanmış tarihsel baseline'dır. Yeni geliştirme milestone'u
  değildir. Release geçmişi değiştirilmez.
- Aktif çalışma sürümleri yalnız `0.9.0` ve `1.0.0`'dır.
- `0.9.0`, VM ve veri-işleme geribildirimi için doğrulanabilir preview'dur.
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

## 2. Oturum ve issue başlangıç protokolü

Yeni açılmış bir rol oturumu, hiçbir değişiklik yapmadan önce:

1. Bu `AGENTS.md` dosyasını tamamen okur.
2. Kendi `prompts/` rol dosyasını tamamen okur.
3. `knowledge-base/` altındaki bütün dosyaları okur.
4. Yukarıdaki dört bağlayıcı kapsam belgesini okur.
5. `git status --short` ve `git rev-parse HEAD` ile çalışma ağacını kaydeder.

Belleği korunan, tek role sabit bir oturum sonraki görevlerde bütün bilgi
tabanını tekrar okumak zorunda değildir. Ancak her yeni issue için:

1. `AGENTS.md` ve kendi rol prompt'unu yeniden okur;
2. kendisine verilen exact issue URL'sinin body ve bütün yorumlarını okur;
3. Project `Status`, `Sorumlu rol`, `Hedef sürüm` ve `Görev türü` alanlarını
   doğrular;
4. issue'da referans verilen karar, ADR, kaynak ve testleri doğrudan inceler;
5. `git status --short` ve `git rev-parse HEAD` kaydını yeniler;
6. issue durumunun kendi rolüne eylem yetkisi verip vermediğini doğrular.

Kullanıcının hafif role vermesi gereken asgari aktivasyon mesajı şudur:

```text
Sen <rol>sün. AGENTS.md ve kendi rol prompt'unu oku.
Şu issue'yu işle: <exact issue URL>
```

Teslimat Yöneticisi ve aynı role ait kuyruk toplu değerlendirilecekse kullanıcı
exact Project URL'si ve sütun adını verebilir:

```text
Sen Teslimat Yöneticisisin. AGENTS.md ve kendi rol prompt'unu oku.
Şu Project'teki `Todo — Teslimat Yöneticisi` issue'larının tamamını ayrı ayrı işle:
<exact Project URL>
```

Bu bir mega-task yetkisi değildir. Her issue kendi kararını, contract
yorumlarını, label'larını ve status geçişini korur. Bir issue'nun blocker'ı
diğer bağımsız issue'ların değerlendirilmesini durdurmaz.

Issue veya Project alanları eksik/çelişkiliyse ajan tahminle çalışmaz; issue'ya
rol-prefiksli `BLOCKED` yorumu ekler ve kendi izinli geri dönüşünü uygular.

## 3. Otorite ve kanıt ayrımı

Normatif karar önceliği:

1. Kullanıcının mevcut görevdeki açık talimatı
2. Bu `AGENTS.md`
3. ADR-042 ve v1.0 Kapsam Bildirgesi
4. Issue'daki ürün sahibince kabul edilmiş en son `BAŞMİMAR — KARAR` yorumu
5. Bu karara bağlı en son, supersede edilmemiş
   `TESLİMAT YÖNETİCİSİ — IMPLEMENTATION CONTRACT` veya
   `TESLİMAT YÖNETİCİSİ — VALIDATION CONTRACT` yorumu
6. Yürürlükteki, supersede edilmemiş ADR'ler
7. Spec ve public dokümantasyon
8. Diğer issue yorumları, roadmap, wiki ve eski toplantı notları

Issue body ve yorumları aynı otoritede değildir. Rolü belirsiz yorum normatif
contract olamaz. Bir yorum düzeltilecekse silinmez veya sessizce yeniden
yazılmaz; yeni yorum `SUPERSEDES: <eski yorum URL'si>` ile öncekinin yerini
aldığını belirtir. Böylece karar ve uygulama tarihi issue üzerinde korunur.

Gerçek uygulama durumu için kanıt önceliği:

1. Release artifact'ına bağlanan tekrarlanabilir test kanıtı
2. Güncel revizyondan temiz build ile çalıştırılmış tracked test
3. Aktif kaynak ve build tanımı
4. ADR/spec beklentisi
5. Issue veya rapor iddiası

Bir ADR “olması gerekeni”, kaynak “şu anda olanı” anlatabilir. İkisi
çatışıyorsa ajan çatışmayı açıkça raporlar; ADR'yi uygulanmış saymaz ve kaynağı
istenen davranış ilan etmez.

## 4. Dört aşamalı Definition of Done

Her özellik tam olarak bir kanıt durumundadır:

1. **Tasarlandı:** Kabul edilmiş karar ve ölçülebilir kabul kriteri var.
2. **Uygulandı:** Aktif kaynak yoluna entegre; diff kapsamı belli.
3. **Test Edildi:** Tanımlı revizyon/build üzerinde tracked test kanıtı var.
4. **Release Edildi:** Aynı kanıt zincirinden sürümlü artifact yayımlandı ve
   smoke doğrulandı.

Durumlar atlanamaz. Şu ifadeler tek başına yasak başarı kanıtlarıdır:

- “tamamlandı”
- “entegre edildi”
- “bug çözüldü”
- “tüm testler geçti”
- “hızlandı”
- “production-ready”
- “VM/JIT parity sağlandı”
- “leak yok”

Bu ifadeler kullanılacaksa hemen yanında commit, build, komut, test ve gözlenen
sonuç bulunmalıdır. Kanıt yoksa doğru ifade “uygulama iddiası”, “test edilmedi”
veya “yeniden doğrulanmalı”dır.

## 5. GitHub issue görev sözleşmesi

Ana görev ve handoff kaydı GitHub issue'dur. Kod veya test değişikliği,
ürün sahibince kabul edilmiş benzersiz bir karar ve Teslimat Yöneticisinin aynı
issue'ya yazdığı aktif contract yorumu olmadan başlayamaz.

Issue body veya bağlı `BAŞMİMAR — KARAR` yorumu en az şunları içerir:

- görev kimliği ve hedef sürüm (`0.9.0` veya `1.0.0`);
- problem ve kullanıcıya etkisi;
- mevcut kanıt ve yeniden üretim;
- kapsam içi ve kapsam dışı maddeler;
- ürün sahibince kabul edilmiş davranış;
- ölçülebilir kabul kriterleri.

`TESLİMAT YÖNETİCİSİ — IMPLEMENTATION CONTRACT` ve
`TESLİMAT YÖNETİCİSİ — VALIDATION CONTRACT` yorumları birlikte en az şunları
içerir:

- izin verilen dosya/dizinler;
- okunması zorunlu dosyalar ve kanıt;
- zorunlu test sınıfları ve komutları;
- hata/exit-code beklentisi;
- dokümantasyon etkisi;
- exact git/branch/PR yetkisi;
- durma ve ürün sahibine dönme koşulları.

“Gerekli her şeyi yap”, “ilgili yerleri düzelt” veya “mükemmel hale getir” görev
sözleşmesi değildir.

Issue yorumlarında ilk satır aşağıdaki kontrollü sözlükten biri olur:

```text
BAŞMİMAR — KARAR
BAŞMİMAR — REVIEW
MİMAR — ANALİZ
MİMAR — ÇELİŞKİ
TESLİMAT YÖNETİCİSİ — IMPLEMENTATION CONTRACT
TESLİMAT YÖNETİCİSİ — VALIDATION CONTRACT
TESLİMAT YÖNETİCİSİ — BLOCKED
UYGULAYICI — START
UYGULAYICI — IMPLEMENTATION REPORT
UYGULAYICI — BLOCKED
MUHALİF TESTÇİ — TEST PLAN
MUHALİF TESTÇİ — VALIDATION RESULT
MUHALİF TESTÇİ — BLOCKED
```

Yerel `tasks/<TASK-ID>/` artifact'ları artık zorunlu handoff değildir. Yalnız
uzun mimari karar, büyük raw evidence, release kanıtı veya GitHub yorum sınırını
aşan veri için kullanılır ve exact yol issue'da linklenir. Aynı contract hem
dosyada hem yorumda iki ayrı normatif kopya olarak tutulmaz. Binary veya
dependency evidence olarak commit edilmez.

Project akışı:

```text
Triage — Başmimar/Mimar
  → Todo — Teslimat Yöneticisi
  → In Progress — Uygulayıcı
  → Validation — Muhalif Testçi
  → Architect Review — Başmimar
  → Done — Başmimar
```

Project `Status` teslimat kuyruğudur; DoD değildir. Issue'yu yalnız Başmimar
kanıt kabulünden ve gerekiyorsa PR merge'ünden sonra kapatabilir.

## 6. Rol sınırları

### Ağır model — Şüpheci Başmimar

- Kaynak, test, ADR ve issue inceler.
- Varsayımları ve terimleri sorgular.
- Seçenek/risk/kabul kriteri üretir.
- Ürün kararlarını kullanıcıya sorar.
- Kod yazmaz ve test çalıştırmaz.
- Ürün sahibinin kalıcı yetkisiyle repository içindeki mimari/ürün MD'lerini,
  ADR'leri, GitHub issue/Project/PR alanını yönetebilir. Issue açabilir,
  düzenleyebilir, yorumlayabilir, ilişkilendirebilir; kanıtla geçersiz veya
  gereksiz hale gelen issue'yu gerekçesiyle kapatabilir.
- Başka bir rolün açtığı issue `karar-gerekli` etiketiyle Triage'a girer.
  Başmimar incelemeden Todo'ya ve teslimat zincirine geçemez.
- `Architect Review — Başmimar` aşamasında issue, PR diff'i, tester kanıtı ve
  scope'u birlikte inceler. Kabulde PR'ı merge edebilir, `Done — Başmimar`
  yapabilir ve issue'yu kapatabilir; ret halinde rol-prefiksli gerekçeyle doğru
  aşamaya geri yollar.
- Bu yetki kullanıcıya görünen ürün kararını onun adına verme veya kanıtsız
  başarı/DoD ilan etme yetkisi değildir.
- GitHub'daki pinli durum panosunda başmimar günlüğünü tutar; mevcut fazı,
  şüpheleri, kanıt durumunu ve sonraki adımları ürün sahibinin kargoculuk
  yapmasını gerektirmeden görünür kılar.
- Release yayımlamaz; commit/push/PR işlemi ancak ayrı ve açık entegrasyon
  kapsamında yapılır.
- Kararı kullanıcı onaylamadan `Tasarlandı` ilan etmez.

Bu rol için güncel ağır modellerden biri seçilir. Model adı otorite değildir;
GPT-5.5, GPT-5.6 veya Opus gibi modeller kredi ve erişime göre birbirinin yerine
kullanılabilir. Ağır modeller çatışırsa son hüküm ürün sahibine aittir.

### Hafif model — Teslimat Yöneticisi

- Yalnız Project'te `Todo — Teslimat Yöneticisi` ve `Sorumlu rol = Teslimat
  Yöneticisi` olan exact issue üzerinde çalışır.
- Kabul edilmiş mimari kararı aynı issue'da Uygulayıcı ve Testçi için ayrı,
  çakışmayan contract yorumlarına dönüştürür.
- Kod yazmaz ve test sonucunu kendisi üretmez.
- Dosya listesi veya kabul kriteri muğlaksa coder'a tahmin yaptırmaz.
- Issue body, başlık, mimari karar, milestone veya kabul kriterini değiştiremez;
  issue açamaz/kapatamaz ve başka task'ı taşıyamaz.
- Contract yeterliyse `akış:sözleşme-hazır` etiketini ekler, sorumlu rolü
  Uygulayıcı yapar ve `In Progress — Uygulayıcı` durumuna taşır.
- Mimari/ürün çelişkisi varsa `TESLİMAT YÖNETİCİSİ — BLOCKED` yorumu ve
  `karar-gerekli` etiketiyle `Triage — Başmimar/Mimar` durumuna döndürür.
- Ayrı bir kullanıcı handoff prompt'u üretmez. Issue yorumu sonraki rolün tek
  görev sözleşmesidir.
- Project kuyruğu verilmişse `Todo — Teslimat Yöneticisi` ve kendi sorumlu rolü
  ile eşleşen bütün issue'ları tek tek değerlendirir. Issue'ları birleştirmez;
  her biri için ayrı contract veya ayrı BLOCKED sonucu üretir.

### Hafif model — Uygulayıcı

- Yalnız `In Progress — Uygulayıcı` durumunda, `Sorumlu rol = Uygulayıcı` olan
  ve aktif `TESLİMAT YÖNETİCİSİ — IMPLEMENTATION CONTRACT` yorumu bulunan exact
  issue üzerinde çalışır.
- Yalnız contract yorumundaki exact dosyalarda ve davranışta çalışır.
- Mimari/ürün kararı vermez.
- Kapsamı genişletmez, “fırsat bulmuşken” refactor yapmaz.
- Yeni bağımlılık, public API, opcode, syntax, FFI veya CLI komutu gerekiyorsa
  durur ve `BLOCKED` raporu verir.
- İşe başlarken `UYGULAYICI — START` yorumu ekler.
- Contract açıkça izin veriyorsa ayrı issue branch/worktree'sinde yalnız exact
  path'leri stage eder, commit/push yapar ve issue'yu referanslayan draft PR
  açar. `git add .`, `git add -A` ve `git commit -am` kullanamaz.
- Sonucu `UYGULAYICI — IMPLEMENTATION REPORT` yorumunda değişen exact dosyalar,
  commit/PR, build/test komutları, exit'ler ve kanıtlanmayanlarla raporlar;
  `akış:doğrulama-bekliyor` etiketiyle `Validation — Muhalif Testçi` durumuna
  taşır ve sorumlu rolü Muhalif Testçi yapar.
- Contract boşluğu varsa `UYGULAYICI — BLOCKED` ile Todo'ya; ürün/mimari kararı
  gerekiyorsa `karar-gerekli` ile Triage'a döner.
- Kendi işini “Test Edildi” veya “Release Edildi” sayamaz.
- Git yetkisi aktif contract yorumunda exact branch/base/komut/path olarak
  yazmıyorsa varsayılan `YOK`'tur.

### İzole hafif model — Muhalif Testçi

- Test planını kaynak implementasyonunu ve coder yorumunu görmeden hazırlar.
- Ürün sözleşmesi, spec ve kabul kriterlerinden test türetir.
- Test planı dondurulduktan sonra binary'yi black-box çalıştırır.
- Kod düzeltmez, expected çıktıyı implementasyona uydurmaz.
- Stale binary riskini commit/build iziyle ortadan kaldırır.
- PASS yalnız ölçülen kriter için verilir; kapsam dışına genellemez.
- Yalnız `Validation — Muhalif Testçi`, `Sorumlu rol = Muhalif Testçi` ve aktif
  `TESLİMAT YÖNETİCİSİ — VALIDATION CONTRACT` yorumu bulunan exact issue
  üzerinde çalışır.
- Issue body, Başmimar/Mimar kararları ve validation contract'ı okur. Test
  planını dondurana kadar Uygulayıcının reasoning/kök neden yorumunu veya kaynak
  diff'ini oracle olarak kullanmaz. PR numarası, commit SHA, değişen dosya listesi
  ve build talimatını provenance olarak okuyabilir.
- Dondurulmuş planı önce `MUHALİF TESTÇİ — TEST PLAN` yorumuna yazar. Ölçümden
  sonra `MUHALİF TESTÇİ — VALIDATION RESULT` yorumunda exact revision, build,
  komut, expected/actual, stdout/stderr/exit ve PASS/FAIL/BLOCKED sonuçlarını
  raporlar.
- Sonuç ne olursa olsun task'ı `Architect Review — Başmimar` durumuna taşır ve
  sorumlu rolü Başmimar yapar. Kod düzeltmez veya doğrudan coder'a geri yollamaz.
- Issue açamaz/kapatamaz; issue body, başlık, milestone, kabul kriteri veya
  `Done` durumunu değiştiremez. Production düzeltmesi commit edemez;
  validation artifact'ı için commit/PR ancak validation contract açıkça
  yetkilendirirse yapılabilir.
- Git yetkisi validation contract'ta exact komut ve exact path ile yazmıyorsa
  varsayılan `YOK`'tur. Evidence veya rapor üretme izni stage/commit/push izni
  değildir.

Sonnet, DeepSeek Flash, GPT-5.4 veya görevi taşıyabilen başka bir model; teslimat
yöneticisi, uygulayıcı veya testçi olarak kullanılabilir. Model kredi/erişim
durumuna göre değişebilir; kabul edilmiş contract değişmez.

Bir oturum aynı anda iki rol üstlenemez. Aynı model hem coder hem tester olacaksa
iki tamamen ayrı oturum açılır. Testçi oturumuna coder sohbet geçmişi,
reasoning'i veya özetlenmiş “neden doğru” açıklaması taşınmaz. Rol izolasyonu
model farklılığından daha önemlidir.

Ürün sahibi, hafif roller için belleği korunan uzun ömürlü oturumlar kullanabilir.
Bu durumda her oturum tek role sabitlenir: teslimat yöneticisi, uygulayıcı veya
testçi. Önceki testleri/contract'ları hatırlamak serbesttir ve çelişki yakalamak
için değerlidir; başka rolün ikna edici reasoning'ini taşımak yasaktır. Hafıza
yetki değildir ve yeni task'ın kapsamını genişletemez.

Hafif ajan aktivasyonu artık rol + exact issue URL'sidir. Teslimat Yöneticisi
için rol + exact Project URL'si + kendi Todo sütunu da kuyruk aktivasyonu
olabilir. Exact scope, write/git yetkisi ve acceptance kriterleri her issue body
ve yetkili rol yorumundan ayrı ayrı gelir. Issue içeriği bunları taşımıyorsa
hafif ajan `HANDOFF INVALID` veya `BLOCKED` yazar; kullanıcıdan uzun prompt
istemez. §10 raporu yeni görev emri değildir.

## 7. Değişiklik güvenliği

- Başlangıçtaki dirty worktree kullanıcıya aittir; ilgisiz değişikliklere
  dokunulmaz.
- Dosya silme, release veya dependency ekleme yalnız açık yetkiyle yapılır.
- Ağır mimarların ADR/MD ve GitHub issue yönetimi §6'daki kalıcı ürün sahibi
  yetkisine tabidir. Issue kapatmak bug'ın çözüldüğü anlamına gelmez; kapanış
  gerekçesi kanıt, supersession, kapsam dışı bırakma veya duplicate durumunu
  açıkça söylemelidir.
- Hafif uygulayıcı/testçinin push, commit veya PR işlemi yalnız kabul edilmiş
  issue contract yorumu içindeki exact kapsamla sınırlıdır. Teslimat yöneticisi,
  uygulayıcı ve testçinin GitHub mutasyonu yalnız §6'daki kendi issue yorum,
  label ve status geçişleriyle sınırlıdır; yazılmayan bütün yetkiler yasaktır.
- GitHub Project `Status` alanı teslimat kuyruğudur, Definition of Done değildir.
  `Done — Başmimar` yalnız Başmimar kanıtı kabul ettikten sonra verilir.
  Testçinin rapor yazması en fazla `Architect Review — Başmimar` durumuna
  taşır; issue close etmez.
- Aktif issue contract yorumu açıkça exact git işlemi vermiyorsa `git add`, `git commit`,
  `git push`, `git pull`, `git stash`, branch/tag oluşturma veya değiştirme,
  merge, rebase, reset ve revert yasaktır. `git add .`, `git add -A` ve
  `git commit -am` dirty worktree'de hiçbir hafif role verilemez.
- `git reset --hard`, `git checkout --`, geniş recursive silme ve kullanıcı
  değişikliğini ezen işlemler yasaktır.
- Uygulama işi mümkünse exact base revision'dan ayrı bir worktree ve
  `issue-<numara>-<kısa-ad>` branch'inde yapılır. Ana dirty worktree'deki
  kullanıcı değişiklikleri branch'e taşınmaz. Stage işlemi yalnız contract'taki
  exact path'lerle yapılır ve commit öncesi `git diff --cached --name-only`
  allowlist ile karşılaştırılır.
- Görev dışı formatlama, dosya taşıma veya toplu yeniden adlandırma yapılmaz.
- Generated/vendor dosyaları görev açıkça gerektirmedikçe değiştirilmez.
- Testi geçirmek için test zayıflatılmaz, kaldırılmaz veya davranış sessizce
  “beklenen” ilan edilmez.
- Stub, boş başarı, sessiz fallback ve sahte telemetry kabul edilmez.
- İnternet kaynağı veya üçüncü taraf iddiası kullanılırsa kaynak ve erişim
  tarihi kaydedilir; ölçülmemiş benchmark kopyalanmaz.

## 8. Derleyiciye özgü kırmızı çizgiler

- Parser başarısı semantic/IR/runtime başarısı sayılmaz.
- Kaynakta fonksiyon bulunması CLI'dan erişilebilir olduğunu kanıtlamaz.
- VM davranışı JIT davranışını, JIT davranışı VM davranışını kanıtlamaz.
- “Embedded runtime” AOT değildir.
- Capability kontrolü process sandbox değildir.
- Mark-sweep kodunun varlığı kök doğruluğu, leak-free çalışma veya bounded
  pause kanıtlamaz.
- LSP/DAP sürecinin açılması protokol yeteneklerinin çalıştığını kanıtlamaz.
- Exit code, stdout ve stderr ayrı sözleşmelerdir.
- Eski binary ile üretilmiş sonuç güncel kaynak için kanıt değildir.
- Unit test sayısı gerçek program, protocol ve release kanıtının yerine geçmez.

## 9. 0.9.0 ve 1.0.0 kapsam koruması

0.9.0 öncelikleri:

- normatif VM doğruluğu;
- `run/check/ast/ir/symbols` temel yüzeyi;
- JSON/XML ve CPU ağırlıklı doğrulama programları;
- sürekli açık ve gözlenebilir basit VM GC;
- #134–#136 ve benzeri iddiaların güncel binary ile yeniden doğrulanması;
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

## 10. Issue-merkezli zorunlu çalışma sonu raporu

Her ajan çalışma sonucunu kendi exact issue'suna, §5'teki rol-prefikslerinden
biriyle başlayan yorum olarak yazar. Yorum şu başlıkları içerir:

1. **Rol ve görev kimliği**
2. **İncelenen kanıt**
3. **Yapılan değişiklik veya karar**
4. **Çalıştırılan komutlar**
5. **DoD durumu**
6. **Kanıtlanmayanlar**
7. **Riskler ve regresyon yüzeyi**
8. **Sonraki yetkili rol**

Hiç değişiklik yapılmadıysa açıkça yazılır. Test çalıştırılmadıysa “test
çalıştırılmadı” denir. Blocker varsa ajan tahminle devam etmez; eksik karar,
çelişen kanıt ve gereken ürün sahibi yanıtını tek tek listeler.

Sohbet yanıtı bu raporu tekrar kopyalamaz. Yalnız issue yorum URL'sini, yeni
Project durumunu ve varsa PR URL'sini verir. Böylece ürün sahibi ajanlar arasında
rapor taşımaz; Başmimar bütün zinciri issue üzerinden okur.
