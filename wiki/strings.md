# String'ler

saQut'ta string'ler **immutable (değişmez) değer-tipi**dir.
İç temsil UTF-8'dir.

## Tanım

```
string selam = "Merhaba";
```

## Birleştirme (Concatenation)

`+` operatörü iki string'i birleştirir, **yeni bir string** üretir:

```
string a = "Merhaba";
string b = " Dünya";
string c = a + b;
print(c);           // Merhaba Dünya
```

`+=` ile de birleştirme yapılabilir:

```
string s = "foo";
s += "bar";
print(s);           // foobar
```

## Eşitlik

saQut'ta string `==` ve `!=` **içerik karşılaştırması** yapar
(Java'dan farklı olarak). İki string'in içeriği aynı mı diye
bakar:

```
string a = "merhaba";
string b = "merhaba";
string c = "dunya";

print(a == b);      // 1 (aynı içerik)
print(a == c);      // 0 (farklı içerik)
print(a != c);      // 1 (farklı)
```

## Kısıtlamalar

String'lerde `<`, `>`, `<=`, `>=` karşılaştırması **derleme hatasıdır**:

```
string a = "a";
string b = "b";
if (a < b) { }      // E003 — string sıralama desteklenmez
```

---

**Sıradaki:** [Global Değişkenler](globals.md)

**Üst:** [Ana Sayfa](home.md)
