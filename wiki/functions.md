# Fonksiyonlar

## Fonksiyon Tanımı

```
<dönüş_tipi> <isim>(<parametreler>) {
    <gövde>
}
```

```
int topla(int a, int b) {
    return a + b;
}
```

- Fonksiyonlar en üst seviyede tanımlanır (iç içe fonksiyon yok)
- Parametreler ve dönüş tipi **zorunludur**
- Parametre tipi belirtilir (tip çıkarımı yok)

## return

Fonksiyon bir değer döndürecekse `return` kullanılır:

```
int kare(int x) {
    return x * x;
}
```

`void` fonksiyonlar `return` gerektirmez:

```
void selamla() {
    print("Merhaba");
}
```

## Çağrı

```
int main() {
    int sonuc = topla(3, 4);
    print(sonuc);       // 7
    return 0;
}
```

### print() — Host Fonksiyon

`print` özel bir fonksiyondur. C++ tarafında implemente edilmiştir.
Herhangi bir türde tek argüman alır ve stdout'a yazdırır.

```
print(42);             // 42
print(3.14);           // 3.14
print("Merhaba");      // Merhaba
print(true);           // 1
print(false);          // 0
```

## Recursion (Özyineleme)

Fibonacci — saQut'un referans programı:

```
int fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main() {
    print(fibonacci(10));     // 55
    return 0;
}
```

## Parametre Geçişi

saQut parametreleri **değerle** alır (kopyalar):

```
void degistir(int x) {
    x = 100;        // sadece yerel kopya değişir
}

int main() {
    int a = 5;
    degistir(a);
    print(a);       // 5 — değişmedi
    return 0;
}
```

Ancak **referans tipleri** (array, struct) için durum farklıdır.
Detaylar için [Arrays](arrays.md) ve [Structs](structs.md) sayfalarına bak.

## Fonksiyonlarda Sık Yapılan Hatalar

```
int topla(int a, int b) {
    return a + b;
}

int main() {
    topla(1, 2, 3);     // E008 — yanlış argüman sayısı
    return 0;
}
```

```
int foo() {
    return 1.5;         // E003 — return tipi uyuşmazlığı
}
```

---

**Sıradaki:** [Struct'lar](structs.md)

**Üst:** [Ana Sayfa](home.md)
