=== examples/merhaba.sqt ===
int main() {
    print("Merhaba");
    print("saQut calisiyor");
    return 0;
}

=== examples/fibonacci.sqt ===
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
    print(fibonacci(n));           // recursive
    print(fibonacciIterative(n));  // iterative
    return 0;
}

=== tests/golden/numeric/widths.sqt ===
// ADR-040 (#113): sayısal tip genişlikleri — longint (64-bit), float (32-bit
// single), double (64-bit). VM ≡ JIT birebir (diferansiyel sözleşme, ADR-032).
int main() {
    // longint: 64-bit, int'in taşacağı yerde taşmaz
    longint big = 2147483647;
    print(big + 1);              // 2147483648 (int'te -2147483648 olurdu)
    longint sq = big * big;
    print(sq);                   // 4611686014132420609

    // longint 64-bit tanımlı wrap
    longint max = 9223372036854775807;
    print(max + 1);              // -9223372036854775808 (2's-complement)

    // longint bitwise + kaydırma 64-bit
    print(1 as longint << 40);   // 1099511627776
    longint m = 100;
    print(m / -7);               // -14
    print(m % -7);               // 2

    // int → longint kayıpsız genişletme
    int i = 42;
    longint L = i;
    print(L + big);              // 2147483689

    // float = 32-bit single: precision kaybı gözlemlenir
    float f = 0.1;
    print(f + 0.2);              // 0.300000012 (single)

    // double = 64-bit: temiz
    double d = 0.1;
    print(d + 0.2);              // 0.3

    // float ↔ double dönüşümleri
    float fs = 0.1;
    double widened = fs as double;
    print(widened);              // 0.1000000015 (single'ın double karşılığı)
    double dd = 0.1;
    float narrowed = dd as float;
    print(narrowed);             // 0.100000001 (double → single, veri kaybı)

    // longint / float32 string cast
    print(max as string);        // 9223372036854775807
    float pi = 3.5;
    print(pi as string);         // 3.5
    return 0;
}

sha256sum widths.sqt:
86da94f09545085966913820ce29afde7c432224773f548ea8d54b6fc1ddbf9f  /home/saqut/Masaüstü/saqutcompiler/tests/golden/numeric/widths.sqt
7362ad41552faa697aeb86a7fc9b995d69226f450114532cd458b0d9c2526b45  /tmp/saqut-sq090-ir-baseline-NdIQWO/fixtures/irb5/loadconst.sqt
dd401da31e452a563041c312d3292ebb41649f99eb3214d83733e0d6c3240280  /tmp/saqut-sq090-ir-baseline-NdIQWO/fixtures/irb5/loadlong.sqt
69641a6c8b6c46a3703df4b8ccef71993f8b6bc829ab5df97810e0c41713538b  /tmp/saqut-sq090-ir-baseline-NdIQWO/fixtures/irb5/loadfloat32.sqt
b3ec7af9b1041f4908404ac50e3bf4b5abdf3cab34d422e334d541ec535529a9  /tmp/saqut-sq090-ir-baseline-NdIQWO/fixtures/irb5/cap.sqt
