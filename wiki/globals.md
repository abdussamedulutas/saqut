# Global Değişkenler

Değişkenler fonksiyonların **dışında** tanımlanabilir. Bunlara **global
değişkenler** denir — programdaki tüm fonksiyonlar tarafından görülür.

## Tanım

```
<tip> <isim> [= <değer>];
```

```
int counter = 0;
int total = 100;
```

## Kullanım

```
int counter = 0;
int total = 100;

int main() {
    counter = 5;
    total = total + counter;
    print(counter);     // 5
    print(total);       // 105
    return 0;
}
```

## Başlangıç Değeri

Global değişkenler tanımlanırken başlangıç değeri alabilir:

```
int base = 10;
int doubled = 0;

int main() {
    doubled = base * 2;
    print(doubled);     // 20
    return 0;
}
```

## Kapsam

Global değişkenler **tanımlandıkları dosyaya** aittir. Tüm fonksiyonlar
tarafından okunup yazılabilir. Henüz çok dosyalı derleme yoktur
(import/export ileride eklenecek).

---

**Sıradaki:** [Bileşik Atama Operatörleri](compound-assignment.md)

**Üst:** [Ana Sayfa](home.md)
