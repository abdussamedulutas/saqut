# Vendored MIR — pin bilgisi

- Üst akım: https://github.com/vnmakarov/mir
- Etiket: v1.0.0
- Commit: 477d820e7b3054980ea1b936ecb2945c0e6465e8 ("Make code size calculation for MIR more clear.")
- Klonlama tarihi: bu commit'in yazıldığı tarih (bkz. git log)
- Neden vendored (submodule değil): saQut'un "kullanıcı makinesinde sıfır
  harici toolchain" kısıtı (ADR-032) build zamanında ağ bağımlılığı
  istemiyor; kaynak doğrudan repoya gömülü. Güncelleme = bu dizini yeni bir
  MIR sürümüyle değiştirip bu dosyayı güncellemek.
- saQut'un kendi entegrasyon kodu BURADA değil, `src/mir/` (bir üst dizin)
  altında — bu dizin (`vendor/`) yalnızca üst akım kaynağı, elle
  değiştirilmez.
