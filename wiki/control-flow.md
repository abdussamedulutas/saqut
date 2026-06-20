# Kontrol Akışı

saQut, C-ailesi kontrol akışı yapılarının tamamını destekler.

## if / else

```
if (koşul) {
    // koşul doğruysa çalışır
} else {
    // koşul yanlışsa çalışır
}
```

`else` kısmı isteğe bağlıdır:

```
int main() {
    int x = 5;

    if (x > 0) {
        print(1);
    } else {
        print(0);
    }

    if (x == 10) {
        print("on");
    }

    return 0;
}
```

Koşul için **truthy** değer kullanılır: `0` = yanlış, `0` dışı her şey = doğru.

## while

```
while (koşul) {
    // koşul doğru olduğu sürece çalışır
}
```

```
int main() {
    int i = 0;
    while (i < 3) {
        print(i);
        i = i + 1;
    }
    return 0;
}
// Çıktı: 0 1 2
```

## do / while

En az bir kez çalışır, sonra koşulu kontrol eder:

```
int main() {
    int x = 5;
    do {
        print(x);
        x = x + 1;
    } while (x < 0);
    return 0;
}
// Çıktı: 5 (koşul false olsa bile gövde bir kez çalıştı)
```

## for

```
for (başlangıç; koşul; artım) {
    // gövde
}
```

Üç kısım da isteğe bağlıdır:

```
int main() {
    for (int i = 0; i < 5; i = i + 1) {
        print(i);
    }
    return 0;
}
// Çıktı: 0 1 2 3 4
```

## break ve continue

`break`: döngüyü hemen sonlandırır.
`continue`: sonraki iterasyona atlar (koşul tekrar kontrol edilir).

### for'da break

```
for (int i = 1; i <= 5; i = i + 1) {
    if (i == 3) { break; }
    print(i);
}
// Çıktı: 1 2
```

### for'da continue

`continue` **güncelleme adımını atlamaz** — sadece gövdenin kalanını atlar:

```
for (int i = 1; i <= 5; i = i + 1) {
    if (i == 2) { continue; }
    if (i == 4) { continue; }
    print(i);
}
// Çıktı: 1 3 5  (2 ve 4 atlandı, ama i++ yine de çalıştı)
```

### while'da break ve continue

```
int i = 0;
while (i < 5) {
    i = i + 1;
    if (i == 3) { continue; }   // 3'ü atla
    if (i == 4) { break; }      // 4'te çık
    print(i);
}
// Çıktı: 1 2
```

### İç İçe Döngülerde break

`break` sadece **en içteki** döngüyü etkiler:

```
int i = 1;
while (i <= 3) {
    int j = 1;
    while (j <= 3) {
        if (j == 2) { break; }   // sadece içteki döngüyü kırar
        print(i);
        print(j);
        j = j + 1;
    }
    i = i + 1;
}
// Çıktı: 1 1  2 1  3 1
```

---

## switch (Henüz Yok)

`switch`, `case`, `default` keyword'leri **lexer tarafından tanınır**
ancak parser ve sonraki aşamalarda henüz implemente edilmemiştir.
Gelecekte eklenecek bir özelliktir.

---

**Sıradaki:** [Fonksiyonlar](functions.md)

**Üst:** [Ana Sayfa](home.md)
