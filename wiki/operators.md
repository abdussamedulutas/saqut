# Operatörler

saQut, C-ailesi operatörlerin neredeyse tamamını destekler. Ayrıştırma
**Pratt parser** ile yapılır — her operatörün bir öncelik seviyesi ve
birleşme yönü vardır.

## Operatör Öncelik Tablosu

Yüksek sayı = önce işlenir.

| Seviye | Kategori | Operatörler | Birleşme |
|-------|----------|-------------|----------|
| 18 | Üye erişimi / çağrı | `.` `->` `[ ]` `( )` | Sol |
| 17 | Postfix | `++` `--` | Sol |
| 16 | Unary prefix | `+` `-` `!` `~` | Sağ |
| 15 | Üs alma | `**` `^` | **Sağ** |
| 14 | Çarpma/Bölme | `*` `/` `%` | Sol |
| 13 | Toplama/Çıkarma | `+` `-` | Sol |
| 12 | Bitsel kaydırma | `<<` `>>` | Sol |
| 11 | İlişkisel | `<` `<=` `>` `>=` | Sol |
| 10 | Eşitlik | `==` `!=` | Sol |
| 9 | Bitsel VE | `&` | Sol |
| 7 | Bitsel VEYA | `\|` | Sol |
| 6 | Mantıksal VE | `&&` | Sol |
| 5 | Mantıksal VEYA | `\|\|` | Sol |
| 4 | Ternary | `?` | **Sağ** |
| 3 | Ternary else | `:` | **Sağ** |
| 2 | Atama | `=` `+=` `-=` `*=` vb. | **Sağ** |
| 1 | Virgül | `,` | Sol |

> **Sağ birleşme:** `a = b = 5` → `a = (b = 5)`
> **Sol birleşme:** `10 - 4 - 3` → `(10 - 4) - 3` = 3

## Pratt Parser — `-2 + -5` Nasıl Çözülür?

Pratt parser her operatöre bir **öncelik** (binding power) atar. Ayrıştırma
şöyle işler:

1. **prefix (null denotation):** `-` görünce "unary minus" olarak tanır
   (çünkü henüz solunda bir ifade yok). `2`'yi okur, `(-2)` düğümü oluşur.
2. **infix (left denotation):** `+` görünce önceliğini kontrol eder
   (seviye 13). Solunda `(-2)` var.
3. **devam:** `5`'i okur, `(-2) + 5` düğümü oluşur. Sonuç = 3.

Sıra farklı olsaydı (örneğin `5 + -2`):
1. Prefix `-` unary olarak `(-2)` yapar
2. `+` solunda `5`, sağında `(-2)` → `5 + (-2)` = 3

Unary `-` seviye 16'da, binary `-` seviye 13'te. Parser bağlama göre
doğru önceliği kullanır.

## Aritmetik Operatörler

```
int a = 10 + 3;    // toplama → 13
int b = 10 - 3;    // çıkarma → 7
int c = 10 * 3;    // çarpma → 30
int d = 10 / 3;    // bölme → 3 (int / int = int)
int e = 10 % 3;    // mod → 1
int f = 2 ** 3;    // üs alma → 8 (2³)
int g = 2 ^ 3;     // üs alma → 8 (^ da üs)
```

```
float x = 10.0 / 3.0;    // float bölme → 3.333...
```

## Karşılaştırma Operatörleri

Sonuç her zaman `1` (doğru) veya `0` (yanlış).

```
print(5 < 3);    // 0
print(5 <= 5);   // 1
print(5 > 3);    // 1
print(5 >= 3);   // 1
print(5 == 5);   // 1
print(5 != 3);   // 1
```

## Mantıksal Operatörler

```
print(!0);        // 1 (değil)
print(1 && 0);    // 0 (ve)
print(1 || 0);    // 1 (veya)
```

### Kısa Devre (Short-Circuit)

`&&` ve `||` kısa devre yapar:

- `false && ...` → sağ taraf **hiç çalıştırılmaz** (zaten false)
- `true || ...` → sağ taraf **hiç çalıştırılmaz** (zaten true)

```
int side() {
    print("S");
    return 1;
}

int main() {
    int f = 0;
    int t = 1;

    if (f && side()) { }     // "S" yazdırılmaz — kısa devre
    if (t || side()) { }     // "S" yazdırılmaz — kısa devre
    if (t && side()) { }     // "S" yazdırılır
    if (f || side()) { }     // "S" yazdırılır
    return 0;
}
```

## Bitsel Operatörler

```
int a = 12 & 10;      // VE → 8   (1100 & 1010 = 1000)
int b = 12 | 10;      // VEYA → 14 (1100 | 1010 = 1110)
int c = 1 << 3;       // sola kaydır → 8
int d = 16 >> 2;      // sağa kaydır → 4
int e = ~0;           // ters çevir → -1
```

## Üs Alma

Hem `**` hem `^` üs alma operatörüdür. Sağ birleşmeli:

```
print(2 ** 3 ** 2);    // 2 ** (3 ** 2) = 2 ** 9 = 512
print(2 ^ 3 ^ 2);      // aynı: 512
```

---

**Sıradaki:** [Kontrol Akışı](control-flow.md)

**Üst:** [Ana Sayfa](home.md)
