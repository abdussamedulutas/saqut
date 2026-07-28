# ADR-042 — v1 Feedback MVP, Backend ve Sürümleme Sözleşmesi

**Durum:** Kabul edildi  
**Tarih:** 2026-07-25  
**Karar verenler:** Ürün sahibi, şüpheci başmimar  
**Ürün sözleşmesi:** `docs/v1.0-kapsam-bildirgesi.md`

**Kısmen supersede eder:** ADR-032, ADR-038, ADR-041

## Bağlam

Önceki planlar v1'e aynı anda stabil JIT, “AOT”, geniş stdlib, katı iki yönlü
minor uyumluluğu, sandbox çağrışımı ve production düzeyinde beklentiler
yüklemiştir. Bu hedeflerin bir bölümü kaynakta yoktur, bir bölümü test kanıtından
yoksundur, bir bölümü de ürün sahibinin ilk kullanım amacından önce mimariyi
kilitlemektedir.

v1'in amacı nihai runtime mimarisini kanıtlamak değil; gerçek programlarla dil,
modül, FFI ve tooling ergonomisi hakkında geribildirim üretmektir.

## Karar

1. v1.0 bir **Feedback MVP**'dir; production-ready iddiası taşımaz.
2. VM, v1.0'ın tek stabil ve normatif backend'idir.
3. MIR JIT `[EXPERIMENTAL]` kalır ve v1.0 ürün sözleşmesinin dışındadır.
4. AOT ve tek executable v1.0 gereksinimi değildir. Gömülü runtime paketleme,
   yapılırsa “bundled-runtime executable packaging” olarak adlandırılır.
5. Basit VM GC sürekli açık ve gözlenebilir olur; leak-free, bounded-pause,
   server uptime veya JIT GC garantisi verilmez.
6. v1 public sözleşmesinde concurrency yoktur. Deneysel kod gizli ve varsayılan
   olarak kapalıdır.
7. `run`, `check`, `ir`, `ast`, `symbols`, `lsp`, `dap` v1 release yüzeyidir.
   Stub komutlar public CLI ve yardımdan kaldırılır.
8. LSP/DAP kalitesi, sürümlü desteklenen yetenek matrisi ve protocol-level
   kanıtlarla tanımlanır; protokolün tamamı varsayılmaz.
9. Major sürüm yeni ürün neslidir ve cross-major uyumluluk sözü vermez. Minor
   sürüm v1 ailesine geriye uyumlu işlev ekleyebilir. Patch yanlış davranışı
   düzeltebilir.
10. Özellik durumu `Tasarlandı → Uygulandı → Test Edildi → Release Edildi`
    zinciriyle raporlanır. Kanıtsız hiçbir aşama atlanamaz.

## Önceki ADR'lere etkisi

### ADR-038

Şu maddeler supersede edilmiştir:

- §2'deki “MINOR = sıfır yeni gözlemlenebilir özellik” ve iki yönlü uyumluluk;
- §3'te yeni bir public fonksiyonun doğrudan major gerektirmesi;
- bütün minor serilerine zorunlu backport ve hatalı `.0` artifact'ini otomatik
  yank etme zorunluluğu;
- insan-okunur diagnostic metninin birebir donmuş CLI sözleşmesi sayılması.

Şunlar yürürlükte kalır:

- iç temsilin public sözleşme olmaması;
- VM'in aynı sürüm ve tanımlı ortamda deterministik gözlemlenen davranış hedefi;
- açıkça public ilan edilen serileştirme formatlarının sürümlenmesi;
- yanlış davranışın patch ile düzeltilebilmesi;
- major sürümün ayrı ürün nesli olması.

JIT experimental olduğu sürece VM≡JIT eşitliği v1 release kapısı değildir.
Experimental JIT'in sessiz semantik sapması yine bug'dır; desteklenen yüzeyi
ayrıca test edilir.

### ADR-032

MIR seçimi gelecekteki deneysel backend yönü olarak korunur. ADR-032'nin JIT
olgunluğu, performans rakamları, platform kapsamı ve gömülü-runtime paketleme
takvimi v1 için kanıtlanmış ürün kararı değildir. “Gömülü-runtime AOT” terimi
v1 belgelerinde kullanılmaz; klasik AOT ile karıştırılmamalıdır.

### ADR-041

Self-hosted stdlib + thin runtime, değerlendirmeye değer bir mimari hipotezdir;
“kilitli karar” ve v1 release migrasyonu statüsü kaldırılmıştır. Katman sınırları
v1 doğrulama programlarından gelecek ölçüm ve bakım verisiyle yeniden
değerlendirilir. #122–#129 v1 release zinciri değildir.

## Sonuçlar

- v1 daha dar fakat doğrulanabilir bir ürün sözleşmesine sahiptir.
- Deneysel kodun varlığı, o özelliği v1 desteği yapmaz.
- FFI ve platform yüzeyi ürün sahibiyle ayrı kararlar gerektirir.
- v2; VM'i, syntax'ı veya runtime modelini geriye uyumsuz biçimde
  değiştirebilir.
- Eski belgeler silinmez; üstlerine bu ADR'ye yönlendiren statü notu eklenir.

## Uygulama durumu

Bu ADR'nin kendisi **Tasarlandı** durumundadır. CLI, belgeler, testler, issue
statüleri ve release süreci bu karara göre güncellenip kanıtlanmadan karar
“Uygulandı” veya “Test Edildi” sayılamaz.

## Doğrulama

- `docs/v1.0-kapsam-bildirgesi.md` release kapıları ölçülebilir olmalıdır.
- Her zorunlu tooling yüzeyi için ayrı yetenek matrisi ve izlenen test kanıtı
  bulunmalıdır.
- Issue panosu kanıt durumu ile ürün önceliğini birbirine karıştırmamalıdır.
- Release notu, commit/build/test/artifact zincirini göstermelidir.
