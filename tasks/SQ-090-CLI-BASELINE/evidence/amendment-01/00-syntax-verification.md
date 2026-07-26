# SQ-090-CLI-BASELINE-AMENDMENT-01 — Syntax Verification

## Kaynak 1: examples/merhaba.sqt (tam içerik)

```
int main() {
    print("Merhaba");
    print("saQut calisiyor");
    return 0;
}
```

## Kaynak 2: examples/fibonacci.sqt (tam içerik)

```
// saQut — geçerli örnek program (semantik analiz + kod üretimi fixture'ı)
//
// Kilitlenmiş tasarıma uyar: prosedürel, value semantics, kullanıcıya açık
// pointer yok, tek main, struct/array gerektirmez. Birinci kilometre taşının
// ("fibonacci'yi derle ve çalıştır") referans programıdır.
//
// İlk ifade doğrudan bir fonksiyon tanımı olabilir; zorunlu class/main
// boilerplate'i yoktur (Java'nın aksine).

int fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int fibonacciIterative(int n) {
    int first = 0;
    int second = 1;
    for (int i = 0; i < n; i = i + 1) {
        int next = first + second;
        first = second;
        second = next;
    }
    return first;
}

int main() {
    int n = 10;
    print(fibonacci(n));
    print(fibonacciIterative(n));
    return 0;
}
```

## Kaynak 3: knowledge-base/02_Language.md, §10 Syntax — Lines 117-119

```
Type name [= expression] [, name [= expression] ...] ;
Type function(Type param, Type[] param, ...) { statements }
```

## Consistent conclusion

Her üç kaynak aynı sözdizimini gösterir:

- `Type function(Type params...) { body }` — dönüş tipi fonksiyon adından **önce**
- Sözdizimi `func` keyword'ü veya `:` ayracı **kullanmaz**
- `main` fonksiyonu: `int main() { ... }`
- Değişken tanımı: `Type name = value;` (iki nokta kullanılmaz, `let` kullanılmaz)

**Tutarlılık:** TAMAMEN TUTARLI.

**Sonuç:** §3'teki düzeltilmiş F1–F10 fixture'ları bu sözdizimiyle uyumludur. Fixture yazılabilir.
