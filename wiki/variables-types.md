# Değişkenler ve Veri Tipleri

saQut'ta her değişkenin bir **tipi** vardır. Tipler iki ana kategoriye ayrılır.

## Tipler

| Tip | Açıklama | Boyut |
|-----|----------|-------|
| `int` | 32-bit işaretli tamsayı | 4 bayt |
| `float` | 32-bit ondalıklı sayı | 4 bayt |
| `double` | 64-bit ondalıklı sayı | 8 bayt |
| `bool` | Mantıksal değer (`true`/`false`) | 4 bayt (`int` olarak saklanır) |
| `char` | 8-bit karakter | 1 bayt |
| `string` | Metin (immutable değer-tipi, UTF-8) | Değişken |
| `void` | Değer yok (sadece fonksiyon dönüş tipi) | — |

### Değer ve Referans Semantiği

saQut bir **JavaScript/Java/C# modeli** kullanır:

- **Değer tipleri:** `int`, `float`, `double`, `bool`, `char` — atama yapınca **kopyalanır**
- **Referans tipleri:** `string`, `struct`, `array` — atama yapınca **aynı nesneyi gösterir**

```
int a = 5;
int b = a;      // b = 5, a'dan bağımsız
b = 10;         // a hâlâ 5

int[] x = [1, 2, 3];
int[] y = x;    // y, x ile AYNI diziyi gösterir
y[0] = 99;      // x[0] da 99 olur!
```

## Değişken Tanımlama

```
<tip> <isim> [= <başlangıç değeri>];
```

```
int x;             // tanımlı ama değeri 0
int y = 10;        // tanımlı ve 10'a ayarlı
float pi = 3.14;
bool dogru = true;
string selam = "Merhaba";
```

Başlangıç değeri vermezsen, değişken **sıfır değeriyle** başlar:
- `int` → `0`
- `float`/`double` → `0.0`
- `bool` → `false` (yani `0`)
- `string` → `""` (boş metin)

### İsimlendirme Kuralları

- Harf (`a-z`, `A-Z`), rakam (`0-9`), alt çizgi (`_`) ve dolar (`$`) içerebilir
- **Rakamla başlayamaz**
- Büyük-küçük harf duyarlıdır: `toplam` ≠ `Toplam`

```
int yas;          // geçerli
int _sayac;       // geçerli
int 2_defa;       // GEÇERSİZ — rakamla başlar
int toplam;       // geçerli
int toplam;       // GEÇERSİZ — aynı scope'ta iki kez tanımlanamaz
```

---

**Sıradaki:** [Literal'lar](literals.md) — sayılar, metinler ve özel değerler.

**Üst:** [Ana Sayfa](home.md)
