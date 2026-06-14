# Parser Stres Fixture'ları

Buradaki dosyalar **yalnızca parser/AST'yi zorlamak** için yazılmıştır.
**Geçerli saQut programı değildirler** ve semantik analiz / kod üretimi için
fixture olarak **kullanılmamalıdır**.

## `Final.sqt`

Tarihsel olarak parser'ın token/AST kapsamını test etmek için kullanıldı
(289 token, 200+ AST node). Ancak kilitlenmiş dil tasarımını birden çok yerde
**ihlal eder**:

- **Kullanıcıya açık pointer kullanımı** (`struct List*`, `->`) — dilde `*`/`&`
  yoktur (bkz. ADR-014).
- **Sabit boyutlu array** (`int arr[100]`) — array tipi `int[]`'tir, boyut tipin
  parçası değildir (bkz. ADR-010).
- **İki `int main()` tanımı** — aynı scope'ta çift tanım yasaktır (ADR-011, `E002`).
- `printf` format-string semantiği — dilin builtin'i `print`'tir; format'lı
  çıktı bir kütüphane/FFI işidir (bkz. ADR-017).

Geçerli, küçük bir örnek için `examples/fibonacci.sqt`'ye bakın.
