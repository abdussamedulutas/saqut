# Hata Yönetimi (Try/Catch/Throw)

saQut, **Swift-tarzı** unchecked hata yönetimini hedefler (henüz deneysel).

> **Uyarı:** Try/Catch/Throw için IR ve VM altyapısı mevcuttur, ancak
> henüz derleme zamanında test edilmemiştir. Aşağıdaki sözdizimi
> **planlanan** kullanımı gösterir.

## try / catch

```
try {
    // hata olabilir
} catch (Error e) {
    // hatayı yakala
}
```

Hatalar **unchecked** (işaretsiz): fonksiyonun `throws` bildirmesi
gerekmez. Java'nın aksine, her fonksiyon her yerden `throw` yapabilir.

## throw

```
throw hata_değeri;
```

## Error Nesnesi

Hatalar bir struct olarak temsil edilir:

| Alan | Açıklama |
|------|----------|
| `line` | Hatanın oluştuğu satır |
| `col` | Hatanın oluştuğu sütun |
| `message` | Hata açıklaması |
| `trace` | Stack trace (ileride) |
| `code` | Hata kodu |

## Çalışma Zamanı Hataları

Aşağıdaki durumlar VM tarafından otomatik hata olarak fırlatılır
(try/catch ile yakalanabilir):

- `/ 0` — sıfıra bölme (kod: `E_DIVZERO`)
- `% 0` — sıfıra mod (kod: `E_DIVZERO`)
- Dizi sınır dışı erişim (kod: `E_OOB`)
- Tip hatası (kod: `E_TYPE`)

---

**Sıradaki:** [Derleyici Pipeline'ı](pipeline.md)

**Üst:** [Ana Sayfa](home.md)
