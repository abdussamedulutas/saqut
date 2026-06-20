# Diziler (Array)

Diziler, aynı tipteki değerleri sıralı olarak tutar.

## Tanım

```
<tip>[] <isim> = [<değerler>];
```

```
int[] x = [1, 2, 3];
```

## İndeksleme

Elemanlara `[indeks]` ile erişilir. İndeks **0'dan başlar**.

```
int[] x = [1, 2, 3];
print(x[0]);        // 1
print(x[1]);        // 2
print(x[2]);        // 3

x[0] = 99;          // atama da yapılabilir
print(x[0]);        // 99
```

Sınır dışı erişim çalışma zamanı hatası verir:

```
x[100];             // Çalışma hatası: dizi sınır dışı
```

## Referans Semantiği

Array'ler **referans** tiptir (struct'lar gibi). Bir array'i
başka bir değişkene atarsan veya fonksiyona verirsen,
**aynı nesne** üzerinde işlem yaparsın:

```
void degistir(int[] a) {
    a[0] = 99;          // orijinal array değişir
}

int main() {
    int[] x = [1, 2, 3];
    degistir(x);
    print(x[0]);        // 99
    return 0;
}
```

```
int[] x = [1, 2, 3];
int[] y = x;            // aynı array
print(y == x);          // 1 (aynı nesne)

int[] z = [1, 2, 3];
print(z == x);          // 0 (farklı nesne, aynı içerik)
```

> **Eşitlik (`==`) kimlik kontrolü yapar:** aynı nesne mi?
> İçerik karşılaştırması için `==` kullanma — şu an desteklenmiyor.

---

**Sıradaki:** [String'ler](strings.md)

**Üst:** [Ana Sayfa](home.md)
