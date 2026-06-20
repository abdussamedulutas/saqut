# Optimizasyon

saQut şu an iki optimizasyon geçişi (pass) içerir. `--optimized` bayrağı
ile etkinleştirilir.

```
saqut run program.sqt --optimized
saqut ast program.sqt --optimized
saqut ir  program.sqt --optimized
```

## Constant Folding (Sabit Katlama)

Derleme zamanında hesaplanabilen ifadeleri **doğrudan sabite** çevirir.

```
int main() {
    print(2 + 3 * 4);   // → print(14)
    print(1 + 2 == 3);  // → print(1)
    return 0;
}
```

Optimizasyon **öncesi** IR:

```
LOAD_CONST  s0 = 2
LOAD_CONST  s1 = 3
LOAD_CONST  s2 = 4
MUL         s3 = s1 * s2
ADD         s4 = s0 + s3
CALLHOST    print(s4)
```

Optimizasyon **sonrası** IR:

```
LOAD_CONST  s0 = 14
CALLHOST    print(s0)
```

Boolean ifadeler de katlanır:

```
print(0 && 1);      // → print(0)  (kısa devre derleme zamanında)
print(1 || 0);      // → print(1)
```

## Dead Code Elimination (Ölü Kod Eleme)

`return` sonrasındaki erişilemeyen kodları temizler:

```
int compute() {
    int result = 100 - 6 * 15 + 4;
    return result;
    print(999);          // bu satır silinir
}
```

`if` dallanması sonrası ölü kod da temizlenir:

```
int classify(int n) {
    if (n > 0) {
        return 1;
        print(888);      // silinir
    }
    return 0;
}
```

## Pipeline

Optimizasyonlar **fixpoint döngüsüyle** çalışır. Önce constant folding
uygulanır, ardından DCE. Eğer DCE yeni katlanabilir ifadeler ortaya
çıkarırsa, döngü tekrarlanır (bir üst sınır vardır).

Optimizasyonlar **orijinal AST üzerinde değil**, klon üzerinde yapılır.
`run` ve `ir` komutları yerinde (in-place) optimize eder; `ast` komutu
ise orijinali koruyarak bir klon optimize eder.

---

**Sıradaki:** [Hata Yönetimi (Try/Catch/Throw)](error-handling.md)

**Üst:** [Ana Sayfa](home.md)
