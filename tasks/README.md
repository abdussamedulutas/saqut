# saQut Task Artifact Directory

Bu dizin yalnız kabul edilmiş, benzersiz görevlerin denetlenebilir handoff
artifact'ları içindir. Global rol dosyaları kullanılmaz.

## Dizin adı

```text
tasks/SQ-090-<kisa-konu>/
tasks/SQ-100-<kisa-konu>/
```

## İzin verilen artifact'lar

```text
decision.md
implementation-contract.md
validation-contract.md
implementation-report.md
validation-report.md
release-evidence.md
evidence/
```

`evidence/` yalnız exact komutla ilişkilendirilmiş raw build logu, stdout,
stderr, exit-code ve provenance kayıtları içindir. Binary, dependency veya
geniş generated çıktı commit edilmez.

## Yaşam döngüsü

1. Ağır mimar taslağı üretir; ürün sahibi onaylarsa `decision.md` ve DoD
   `Tasarlandı` olur.
2. Hafif teslimat yöneticisi kabul edilmiş karardan iki ayrı contract üretir.
3. Hafif uygulayıcı yalnız implementation contract'ı uygular ve
   `implementation-report.md` üretir. DoD en fazla `Uygulandı`.
4. Yeni ve izole hafif testçi oturumu validation contract'ı black-box doğrular ve
   `validation-report.md` üretir.
5. `Test Edildi` ancak bütün acceptance kriterleri kanıtlandığında kabul edilir.
6. `Release Edildi` yalnız artifact provenance ve `release-evidence.md` ile
   ürün sahibi tarafından ilan edilir.

Validation-only baseline görevinde `implementation-contract.md` ve
`implementation-report.md` oluşturulmaz. Teslimat yöneticisi yalnız
`validation-contract.md`, testçi yalnız `validation-report.md` ve gerekli raw
`evidence/` kayıtlarını üretir.

Bir görevin dosyaları başka göreve şablon diye kopyalanmaz. Eski karar geçersiz
olursa silinmez; başına supersession notu ve yerine geçen task/ADR eklenir.
