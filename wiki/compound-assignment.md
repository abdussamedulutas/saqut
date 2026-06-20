# Bileşik Atama Operatörleri

Bir işlem yapıp sonucu aynı değişkene atamak için kullanılır.

```
x = x + 5;    // yerine
x += 5;       // aynı şey
```

## Tüm Bileşik Atamalar

| Operatör | Anlamı | Örnek |
|----------|--------|-------|
| `+=` | Topla ve ata | `x += 5` → `x = x + 5` |
| `-=` | Çıkar ve ata | `x -= 3` → `x = x - 3` |
| `*=` | Çarp ve ata | `x *= 2` → `x = x * 2` |
| `/=` | Böl ve ata | `x /= 4` → `x = x / 4` |
| `%=` | Mod al ve ata | `x %= 3` → `x = x % 3` |
| `&=` | Bitsel VE ve ata | `x &= 6` → `x = x & 6` |
| `\|=` | Bitsel VEYA ve ata | `x \|= 8` → `x = x \| 8` |
| `^=` | XOR ve ata | `x ^= 3` → `x = x ^ 3` |
| `<<=` | Sola kaydır ve ata | `x <<= 2` → `x = x << 2` |
| `>>=` | Sağa kaydır ve ata | `x >>= 1` → `x = x >> 1` |

## Örnek

Bileşik atamalar zinciri:

```
int main() {
    int x = 20;
    x += 5;     // 25
    x -= 3;     // 22
    x *= 2;     // 44
    x /= 4;     // 11
    x %= 3;     // 2
    print(x);   // 2
    return 0;
}
```

Bitsel bileşikler:

```
int x = 15;
x &= 6;         // 6   (1111 & 0110 = 0110)
x |= 8;         // 14  (0110 | 1000 = 1110)
x <<= 2;        // 56  (1110 << 2 = 111000)
x >>= 1;        // 28  (111000 >> 1 = 11100)
```

---

**Sıradaki:** [Optimizasyon](optimization.md)

**Üst:** [Ana Sayfa](home.md)
