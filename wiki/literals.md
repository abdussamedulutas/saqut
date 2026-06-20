# Literal'lar

Literal, kaynak kodda doğrudan yazdığın sabit değerlerdir. saQut beş tür
literal'ı destekler.

## Tamsayı Literal'ları

Dört farklı tabanda yazılabilir:

| Format | Örnek | Değer | Açıklama |
|--------|-------|-------|----------|
| Decimal | `42` | 42 | Günlük kullanım |
| Hex | `0xFF` | 255 | `0x` veya `0X` ön eki |
| Binary | `0b1010` | 10 | `0b` veya `0B` ön eki |
| Octal | `0777` | 511 | Başta sıfır (`0`) |

```
int a = 42;         // decimal
int b = 0xFF;       // hexadecimal → 255
int c = 0b1010;     // binary → 10
int d = 0777;       // octal → 511
```

## Float/Double Literal'ları

Ondalık nokta veya bilimsel gösterim (e/E) içerir:

```
float x = 3.14;          // basit ondalık
float y = 0.5;           // sıfırla başlayan
float z = .5;            // .5 de geçerli (lexer otomatik "0.5" yapar)
float w = 1e5;           // bilimsel: 1 × 10⁵ = 100000.0
float v = 2.5e-3;        // 2.5 × 10⁻³ = 0.0025
float u = 0.100e+20;     // üste ek işaret
```

Tamsayı literali **bağlama göre** float'a dönüşebilir:

```
float x = 1;     // geçerli — literal bağlama göre tiplenir
int y = 1.5;     // HATA — ondalık literal int'e atanamaz
```

## String Literal'ları

Çift tırnak içinde yazılır. **Kaçış dizileri** desteklenir:

| Kaçış | Anlamı |
|-------|--------|
| `\\` | Ters bölü işareti |
| `\"` | Çift tırnak |
| `\n` | Yeni satır (line feed) |
| `\t` | Sekme (tab) |
| `\r` | Satır başı (carriage return) |
| `\b` | Geri al (backspace) |

```
string a = "Merhaba";              // basit
string b = "Satir 1\nSatir 2";     // çok satır
string c = "Tırnak\"içinde";       // tırnak kaçışı
string d = "Sekme\tvar";           // sekme
```

Kaçış dizileri tokenizer aşamasında çözülür. `\n` görürsen, bu gerçek bir
newline karakteridir, iki karakter değil.

## Boolean Literal'ları

```
bool dogru  = true;
bool yanlis = false;
```

`bool` aslında `int` olarak saklanır: `true` = 1, `false` = 0.
`print(true)` çıktısı `1` olur.

## Null Literal'ı

```
string? bos = null;  // boş referans
```

---

**Sıradaki:** [Operatörler](operators.md)

**Üst:** [Ana Sayfa](home.md)
