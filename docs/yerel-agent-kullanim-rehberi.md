# GitHub-Merkezli Yerel Ajan Kullanım Rehberi

Bu düzende ürün sahibi ajanlar arasında rapor ve prompt taşımaz. Her atomik iş
tek GitHub issue'da yaşar; karar, contract, implementation, PR, validation ve
review aynı tarihsel zincirde görünür.

Ana pano:

https://github.com/orgs/saqutlang/projects/2

## 1. Sabit roller

| Rol | Sorumluluk |
|---|---|
| Başmimar/Mimar | Sorunu tanımlar, karar ve acceptance kriterini yönetir |
| Teslimat Yöneticisi | Kararı implementation ve validation contract yorumuna çevirir |
| Uygulayıcı | Contract kapsamındaki kod/test diff'ini üretir ve draft PR açar |
| Muhalif Testçi | Bağımsız oracle ile PR revision'ını doğrular |
| Başmimar | PR + validation kanıtını kabul/reddeder, merge/kapanışı yönetir |

Her oturum tek role sabittir. Uygulayıcı ve Testçi aynı model olabilir ama aynı
oturum olamaz.

## 2. Project akışı ve sahipleri

```text
Triage — Başmimar/Mimar
  → Todo — Teslimat Yöneticisi
  → In Progress — Uygulayıcı
  → Validation — Muhalif Testçi
  → Architect Review — Başmimar
  → Done — Başmimar
```

- Triage: karar, kapsam veya ürün onayı eksik.
- Todo: karar onaylı; PM contract üretir.
- In Progress: coder contract'a göre branch/PR üretir.
- Validation: tester frozen oracle ile ölçer.
- Architect Review: Başmimar bütün tarihsel zinciri ve PR'ı inceler.
- Done: Başmimar kanıtı kabul etmiştir; bu durum release anlamına gelmez.

## 3. Ürün sahibinin vereceği kısa komutlar

Teslimat Yöneticisi:

```text
Sen Teslimat Yöneticisisin. AGENTS.md ve kendi rol prompt'unu oku.
Şu issue'yu işle: https://github.com/saqutlang/saqut/issues/NNN
```

Todo kuyruğunun tamamı:

```text
Sen Teslimat Yöneticisisin. AGENTS.md ve kendi rol prompt'unu oku.
Şu Project'teki `Todo — Teslimat Yöneticisi` issue'larının tamamını ayrı ayrı işle:
https://github.com/orgs/saqutlang/projects/2
```

Bu komut issue'ları birleştirmez. PM her issue için ayrı contract/BLOCKED
yorumu ve ayrı Project geçişi üretir.

Uygulayıcı:

```text
Sen Uygulayıcısın. AGENTS.md ve kendi rol prompt'unu oku.
Şu issue'yu işle: https://github.com/saqutlang/saqut/issues/NNN
```

Muhalif Testçi:

```text
Sen Muhalif Testçisin. AGENTS.md ve kendi rol prompt'unu oku.
Şu issue'yu işle: https://github.com/saqutlang/saqut/issues/NNN
```

Başka prompt taşınmaz. Rol, kapsam, dosya allowlist'i, git yetkisi ve test
oracle'ı issue'dan okunur. Eksikse ajan çalışmak yerine BLOCKED yazar.

Vardiya başlangıcında her kalıcı rol oturumuna aynı Project URL'si verilebilir.
Her rol bütün panoyu okur ama yalnız kendi sütununda eylem yapar. Kendi
sütununda iş yoksa GitHub/repository durumuna dokunmadan
`Benlik iş yok; gelirse söyle.` der. Issue başına yeni oturum açılmaz; rol
başına kalıcı oturum korunur. Uygulayıcı ve Testçi yine ayrı oturumlardır.

## 4. Canonical yorum başlıkları

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

Yorum silinmez. Düzeltme yeni amendment yorumudur ve eski comment URL'sini
`SUPERSEDES` alanında gösterir.

## 5. Label anlamları

- `karar-gerekli`: ürün veya mimari karar bekleniyor.
- `akış:sözleşme-hazır`: PM contract'ları yazdı; coder başlayabilir.
- `akış:doğrulama-bekliyor`: implementation revision'ı tester bekliyor.
- `akış:bloklu`: görev mevcut rol tarafından ilerletilemiyor.

Label tek başına yetki değildir; Project Status + Sorumlu rol + canonical yorum
birlikte eşleşmelidir.

## 6. Issue ve PR ilişkisi

- Bir issue tek atomik davranış veya invariant taşır.
- Coder ayrı worktree/branch kullanır; ana dirty worktree'yi commit'e katmaz.
- Contract exact izin vermedikçe git yazma yetkisi yoktur.
- Stage yalnız exact path'lerle yapılır; `git add .`, `git add -A` ve
  `git commit -am` yasaktır.
- Draft PR body `Refs #NNN` yazar. PR'ın açılması issue'nun doğrulandığı anlamına
  gelmez.
- Testçi exact PR/head SHA'yı fresh build ile ölçer.
- Yalnız Başmimar review sonrasında merge/Done/close yapar.

## 7. Tester izolasyonu

Issue zinciri herkes için görünürdür; bu tester'ın coder reasoning'ini oracle
yapacağı anlamına gelmez. Testçi önce issue body, Başmimar kararı ve PM
validation contract'ından plan üretip `MUHALİF TESTÇİ — TEST PLAN` yorumuyla
dondurur. Sonra coder yorumundan yalnız provenance/PR/build bilgisini kullanır.

## 8. Yerel task artifact'ları

`tasks/<TASK-ID>/` artık normal handoff yolu değildir. Şunlarda kullanılabilir:

- uzun ve kalıcı mimari karar;
- GitHub yorumuna sığmayan raw evidence;
- release artifact provenance;
- özel mevzuat/üçüncü taraf kayıtları.

Issue bu dosyanın exact yolunu ve rolünü linkler. Aynı contract'ın issue ve
dosyada iki ayrı normatif kopyası tutulmaz.

## 9. Geri dönüşler

- PM karar boşluğu bulur → Triage.
- Coder contract boşluğu bulur → Todo.
- Coder mimari karar ihtiyacı bulur → Triage.
- Tester sonucu ne olursa olsun → Architect Review.
- Başmimar implementation kusuru bulur → In Progress.
- Başmimar contract kusuru bulur → Todo.
- Başmimar yeniden ölçüm ister → Validation.

Her geri dönüş rol-prefiksli yorumda exact neden ve beklenen sonraki eylemi
taşır. Böylece hatanın hangi halkada doğduğu tarihsel olarak görülür.

## 10. Definition of Done

Project sütunu DoD değildir:

1. Tasarlandı: kabul edilmiş karar + ölçülebilir kriter.
2. Uygulandı: exact diff/commit/PR.
3. Test Edildi: exact revision üzerinde bağımsız tracked kanıt.
4. Release Edildi: aynı zincirden sürümlü artifact + smoke.

Issue ancak Başmimar review yorumunda kanıt zinciri yazıldıktan sonra kapanır.
