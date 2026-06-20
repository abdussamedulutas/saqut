# CLI Komutları

`saqut` aracı 6 ana komut sunar. Her biri derleme pipeline'ının farklı bir
aşamasını görüntüler.

## Kullanım

```
saqut <komut> <dosya> [seçenekler]
```

### Seçenekler

| Seçenek | Açıklama |
|---------|----------|
| `-o, --output <dosya>` | Çıktıyı bir dosyaya yaz |
| `--format <json\|text>` | Çıktı formatı (varsayılan: text) |
| `--compact` | JSON'u boşluksuz yazdır |
| `--optimized` | Optimizasyon (constant folding + DCE) uygula |
| `-h, --help` | Yardım metni |

---

## saqut run

**En sık kullanılan komut.** Kaynak dosyayı uçtan uca derler ve çalıştırır.

Pipeline: `tokenize → parse → sembol topla → tip denetle → IR üret → VM çalıştır`

```
int main() {
    print(2 + 3);
    return 0;
}
```

```bash
$ saqut run program.sqt
5
```

`--optimized` bayrağı ile optimizasyonlar açık çalıştırma:

```bash
$ saqut run program.sqt --optimized
```

---

## saqut tokens

Kaynak dosyayı tokenize eder ve token listesini gösterir.

Her satırda `[token_tipi] "değer"` formatı vardır.

```bash
$ saqut tokens merhaba.sqt
Tokenler (15 adet):
  [keyword] "int"
  [identifier] "main"
  [delimiter] "("
  [delimiter] ")"
  [delimiter] "{"
  [keyword] "print"
  [delimiter] "("
  [string] "Merhaba"
  ...
```

---

## saqut ast

Kaynak kodun **Abstract Syntax Tree** (AST) çıktısını JSON olarak verir.
Token'ların nasıl bir ağaca dönüştüğünü görmek için idealdir.

```bash
$ saqut ast program.sqt
```

`--optimized` ile optimizasyon **sonrası** AST:

```bash
$ saqut ast program.sqt --optimized
```

Her düğüm tipi, kaynak konumu ve alt düğümleri JSON içinde görünür.
`--format json` ile saf JSON, varsayılan ile hiyerarşik metin alırsın.

---

## saqut symbols

Sembol tablosunu JSON olarak gösterir: fonksiyonlar, değişkenler, struct'lar
— her birinin tipi, tanımlandığı konum ve referans sayısı.

```bash
$ saqut symbols program.sqt
```

Çıktıda her sembol için:
- `name`: Sembol adı
- `kind`: `function`, `variable`, `struct` vb.
- `type`: `int`, `float`, `int[]` vb.
- `definition`: Tanımlandığı satır/sütun
- `referenceCount`: Kaç yerde kullanıldığı

---

## saqut check

Sadece semantik analiz (tip denetimi + yapısal doğrulama) yapar. Programı
çalıştırmaz, hataları raporlar.

```bash
$ saqut check program.sqt
```

Başarılı: `{"diagnostics": {"errors": [], "warnings": []}}`
Hatalı: hata kodlarıyla birlikte JSON.

---

## saqut ir

3-adresli **Intermediate Representation** (IR) çıktısını gösterir. Derleyicinin
kodu bytecode'a nasıl dönüştürdüğünü görmek için kullanılır.

```bash
$ saqut ir program.sqt
```

Örnek çıktı:

```
IR DUMP

NAME=main PARAMS=0 SLOTS=5
    0  LOAD_CONST  s0 = 2
    1  LOAD_CONST  s1 = 3
    2  ADD         s2 = s0 + s1
    3  CALLHOST    print(s2)
    4  LOAD_CONST  s3 = 0
    5  RETURN      s3

END
```

`--optimized` ile optimizasyon sonrası IR (constant folding sonucu `2+3` yerine
doğrudan `5`):

```
    0  LOAD_CONST  s0 = 5
    1  CALLHOST    print(s0)
    2  LOAD_CONST  s1 = 0
    3  RETURN      s1
```

---

## Gelecek Komutlar (TODO)

| Komut | Açıklama |
|-------|----------|
| `compile` | Kaynak kodu derle |
| `parse` | IR üret |
| `transpile` | C koduna çevir |

---

**Sıradaki:** [Değişkenler ve Veri Tipleri](variables-types.md)

**Üst:** [Ana Sayfa](home.md)
