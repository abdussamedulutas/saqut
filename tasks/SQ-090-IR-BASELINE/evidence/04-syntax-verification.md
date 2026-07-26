# 04-syntax-verification.md — Fixture sözdizimi doğrulama

## examples/merhaba.sqt
**SHA-256:** `c4ce11d8d7734da4430fe9919a93aa15c72f6c3395f9746769cd9491d82fc6f0`
```
int main() {
    print("Merhaba");
    print("saQut calisiyor");
    return 0;
}
```

## examples/fibonacci.sqt
**SHA-256:** `21acd78d76336e5a59e01816fd6a20765ab85b5ca3cbd8a77b52d3b20051c6b7`
```
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

## tests/golden/numeric/widths.sqt
**SHA-256:** `86da94f09545085966913820ce29afde7c432224773f548ea8d54b6fc1ddbf9f`
```
// ADR-040 (#113): sayısal tip genişlikleri — longint (64-bit), float (32-bit
// single), double (64-bit). VM ≡ JIT birebir (diferansiyel sözleşme, ADR-032).
int main() {
    // longint: 64-bit, int'in taşacağı yerde taşmaz
    longint big = 2147483647;
    print(big + 1);              // 2147483648 (int'te -2147483648 olurdu)
    longint sq = big * big;
    print(sq);                   // 4611686014132420609
    ...
}
```
(Tam içerik için yukarıdaki SHA-256 referansı kullanılır; tam içerik stdout'a yazdırıldı ve doğrulandı.)

## Doğrulanan temel biçim
- Dönüş tipi fonksiyon adından önce (`int main() { ... }`)
- `func`/`:` biçimi kullanılmaz
- Gövde `{}` bloğu
- `longint x = ...` ve `float x = ...` surface sözdizimi `widths.sqt` tracked dosyasında kanıtlıdır
