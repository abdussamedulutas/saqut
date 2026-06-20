# Struct'lar

Struct, birden çok değeri tek bir birimde gruplamaya yarar.

## Tanım

```
struct <isim> {
    <tip> <alan1>;
    <tip> <alan2>;
    ...
}
```

```
struct Point {
    int x;
    int y;
}
```

Noktalı virgül **yok** — struct tanımı `}` ile biter.

## Kullanım

```
struct Point {
    int x;
    int y;
}

int main() {
    Point p;          // yeni Point nesnesi
    p.x = 10;         // alanlara erişim
    p.y = 20;
    print(p.x);       // 10
    print(p.y);       // 20
    return 0;
}
```

## Referans Semantiği

saQut'ta struct'lar **referans** tiptir (Java/C# gibi). Bir struct'ı
bir fonksiyona parametre olarak verdiğinde aslında **aynı nesneyi**
değiştirirsin:

```
struct Point {
    int x;
    int y;
}

void setX(Point p, int val) {
    p.x = val;         // aynı nesneyi değiştirir
}

int main() {
    Point p;
    p.x = 10;
    setX(p, 99);
    print(p.x);        // 99 — değişti!
    return 0;
}
```

Atama da referansı kopyalar:

```
Point a;
a.x = 5;
Point b = a;        // aynı Point'i gösterir
b.x = 10;
print(a.x);         // 10 — a da değişti
```

## Sınırlamalar

- Struct içinde fonksiyon/metot tanımlanamaz (OOP yok)
- Struct içinde dizi varsa, dizi referans olarak tutulur

---

**Sıradaki:** [Diziler (Array)](arrays.md)

**Üst:** [Ana Sayfa](home.md)
