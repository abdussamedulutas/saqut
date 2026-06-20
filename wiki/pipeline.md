# Derleyici Pipeline'ı

saQut'un "toolbox" felsefesi: derleme sürecinin **her aşaması** dışarıdan
incelenebilir. CLI komutları bu aşamaların her birini ayrı ayrı gösterir.

```
┌──────────┐    ┌───────────┐    ┌──────┐    ┌──────────┐    ┌──────────────┐    ┌─────────┐    ┌──────────┐
│  Kaynak  │ → │  Lexer +  │ → │Parser│ → │  Sembol   │ → │  TypeChecker │ → │ IR Gen  │ → │  Bytecode│
│  Kod     │   │Tokenizer  │   │(Pratt)│   │Collector  │   │  + Validator │   │(3-adres)│   │  VM      │
└──────────┘    └───────────┘    └──────┘    └──────────┘    └──────────────┘    └─────────┘    └──────────┘
                    │                │            │                │                 │              │
               saqut tokens     saqut ast   saqut symbols    saqut check        saqut ir       saqut run
```

## 1. Lexer + Tokenizer

**Giriş:** Kaynak kod (düz metin)
**Çıkış:** Token listesi

Lexer, karakter karakter tarar: boşlukları atlar, sayıları okur
(decimal/hex/binary/octal/float/bilimsel), string literal'larını
çözümler (kaçış dizileriyle), yorumları atlar.

Tokenizer, lexer'ın ürettiği ham veriyi anlamlı token'lara dönüştürür:
operatörler (`+`, `==`, `<<=`), delimiter'lar (`(`, `{`, `;`),
keyword'ler (`if`, `int`, `return`) ve identifier'lar.

```
[kaynak: int x = 42;]
→ [keyword "int"] [identifier "x"] [operator "="] [number "42"] [delimiter ";"]
```

## 2. Parser (Pratt Parser)

**Giriş:** Token listesi
**Çıkış:** AST (Abstract Syntax Tree)

Token'ları hiyerarşik bir ağaca dönüştürür. Pratt parsing sayesinde
operatör öncelik kuralları token tablosunda merkezi olarak tanımlanır.

`2 + 3 * 4` ifadesi şu ağaca dönüşür:

```
    (+)
   /   \
  2     (*)
       /   \
      3     4
```

## 3. Sembol Toplama (Symbol Collector)

**Giriş:** AST
**Çıkış:** Sembol tablosu

İki geçişli çalışır:
1. **İlk geçiş:** Tüm fonksiyon, değişken ve struct isimlerini toplar
   (ileri referanslara izin verir)
2. **İkinci geçiş:** Her identifier'ı sembol tablosundaki tanımına bağlar

Döngüsel struct tespiti bu aşamada yapılır.

## 4. Tip Denetleyici (TypeChecker)

**Giriş:** AST + sembol tablosu
**Çıkış:** AST (tipler atanmış) + hata/uyarı listesi

Her ifadenin tipini kontrol eder:
- `int + int` → int ✓
- `float * float` → float ✓
- `int + float` → **E003** (gizli dönüşüm yok)
- `int = 1.5` → **E003** (daraltma)
- `string < string` → **E003** (string sıralama yok)

## 5. Optimizasyon (opsiyonel)

**Giriş:** AST
**Çıkış:** Optimize edilmiş AST

Fixpoint döngüsünde çalışan iki pas:
- **Constant Folding:** `2 + 3 * 4` → `14`
- **Dead Code Elimination:** `return` sonrası kodları siler

`--optimized` bayrağı ile etkinleştirilir.
Detaylar: [Optimizasyon](optimization.md)

## 6. IR Generator

**Giriş:** AST + sembol tablosu
**Çıkış:** 3-adresli IR (Intermediate Representation)

Her fonksiyon için slot tabanlı talimatlar üretir. Slot = fonksiyonun
yerel değişken ve geçici değerleri için kullandığı sanal kayıt.

```
LOAD_CONST  s0 = 2
LOAD_CONST  s1 = 3
ADD         s2 = s0 + s1
CALLHOST    print(s2)
```

## 7. Bytecode VM

**Giriş:** IR Program
**Çıkış:** Program çıktısı (stdout) + çıkış kodu

IR talimatlarını yorumlayarak çalıştırır:
- `main` fonksiyonunu bulur
- Frame açar, slot'ları tahsis eder
- Talimatları sırayla işletir
- `CALL` ile yeni frame açar
- `RETURN` ile frame'i kapatır
- `CALLHOST` ile C++ fonksiyonlarını çağırır (print)

---

**Sıradaki:** [Ana Sayfa](home.md)

**Üst:** [Ana Sayfa](home.md)
