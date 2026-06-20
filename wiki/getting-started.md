# Başlarken

saQut, **programlanabilir ve incelenebilir** bir derleyicidir. Token'dan bytecode'a
kadar her aşamayı tek tek görebilir, inceleyebilirsin.

## Derleyiciyi Derleme

Proje CMake + Ninja kullanır. Build zaten hazır:

```bash
cd build/
cmake -G Ninja ..
ninja
```

Başarılı derleme sonunda `build/saqut` ikilisi oluşur.

Denemek için:

```bash
./build/saqut
```

Yardım metnini görmelisin.

## İlk Program

Bir dosya oluştur: `merhaba.sqt`

```
int main() {
    print("Merhaba dunya");
    return 0;
}
```

Çalıştırmak için:

```bash
./build/saqut run merhaba.sqt
```

Çıktı:

```
Merhaba dunya
```

Tebrikler, ilk saQut programını çalıştırdın!

## Program Yapısı

Bir saQut programı **prosedürel**dir. `class` ya da `main` isimli özel bir
fonksiyon kalıbı yoktur — en üst seviyede fonksiyon tanımları ve global
değişkenler bulunur. Çalıştırma `main()` fonksiyonundan başlar.

```
<fonksiyon tanımları>
<global değişkenler>

int main() {
    <kod>
    return 0;
}
```

Her ifade `;` ile biter. Bloklar `{ }` ile oluşturulur.

## print() Fonksiyonu

saQut'ta çıktı almak için `print()` kullanılır. Bir **host fonksiyonudur** —
C++ tarafında implemente edilmiştir. İstediğin tipte tek bir argüman alır:

```
print(42);           // tamsayı
print(3.14);         // ondalık
print("metin");      // metin
print(true);         // boolean → "1" veya "0"
```

---

**Sıradaki:** [CLI komutları](cli-commands.md) — `saqut` aracının tüm komutları.

**Üst:** [Ana Sayfa](home.md)
