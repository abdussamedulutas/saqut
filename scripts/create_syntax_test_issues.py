#!/usr/bin/env python3
"""Sozdizimi/davranis golden-test senaryolari + CLI/UX + kalite-mimari fikirleri.

"[Test]" basliklilar: somut .sqt kodu + beklenen cikti/davranis iceren
golden-test taslaklaridir. Derleyici (Faz 0-4 + IR/VM) tamamlandiginda bu
dosyalar `examples/tests/` altina yerlestirilip regresyon testi olarak
kullanilabilir; ayni zamanda "syntax X soyle calismali" spesifikasyonudur -
calismiyorsa ilgili faz/pass'te bugfix konusu acar.

"[Fikir]" basliklilar onceki script'lerle aynisekilde fikir/backlog kayitlaridir.
"""
from gitea import make_request, REPO_PATH

L_FIKIR = 66
L_TEST = 73
L_CLIUX = 74
L_KALITE = 75

issues_data = [
    # ---------------- Golden-test senaryolari ----------------
    {
        "title": "[Test] Temel Aritmetik, Operatör Önceliği ve `print` Çıktısı",
        "labels": [L_TEST],
        "body": """### Giriş (Nedir, Neden Önemli?)
Bu issue, derleyicinin **en temel** davranışını sabitleyen bir golden-test taslağıdır: sayı literalleri, dört işlem, operatör önceliği (precedence) ve `print` builtin'i. "Fibonacci çalıştı" hedefinden önce bile bu örneğin doğru çalışması gerekir — Pratt parser'ın (zaten yapılı) önceliği doğru uyguladığının kanıtıdır.

---

### Test Kodu (`examples/tests/aritmetik.sqt`)
```c
int main() {
    print(1 + 2 * 3);     // çarpma toplamadan önce
    print((1 + 2) * 3);   // parantez önceliği
    print(10 - 4 - 3);    // soldan-sağa birliktelik (left-assoc)
    print(10 / 3);        // tamsayı bölmesi (truncation)
    print(10 % 3);        // mod
    print(2 + 3 * 4 - 6 / 2);
    return 0;
}
```

### Beklenen Çıktı (`saqut run examples/tests/aritmetik.sqt`)
```
7
9
3
3
1
11
```

---

### Açık Sorular / Bağlı Fazlar
- Bu test, `saqut run` (IR + VM, ADR-015) tamamlanmadan **AST** seviyesinde de kısmen doğrulanabilir: `saqut ast --optimized` ile constant folding (Faz 4) çalıştığında her `print` argümanı doğrudan tek bir `Literal` düğümüne katlanmalı (örn. `1 + 2 * 3` → `Literal(7)`).
- `10 / 3` → `3` (tamsayı bölmesi) davranışı tip denetiminde (Faz 3) açıkça test edilmeli; `int / int = int` (kayıp olabilir ama bu saQut'ta hata değil, sadece kesme/truncation — ADR-010'daki "kayıpsız literal" kuralından farklı bir konu, karıştırılmamalı).

*İmza/Yorum:* Bu dosya, "derleyici bitti mi?" sorusuna verilecek **ilk** somut cevaplardan biri olmalı — `examples/fibonacci.sqt`'den önce, daha küçük bir adım."""
    },
    {
        "title": "[Test] Döngüler — `while` / `do-while` / `for` Davranış Farkları",
        "labels": [L_TEST],
        "body": """### Giriş (Nedir, Neden Önemli?)
Üç döngü yapısının (`while`, `do-while`, `for`) **birbirinden farklı** davrandığı noktalar — özellikle `do-while`'ın gövdeyi **en az bir kez** çalıştırması — derleyicide kolayca karıştırılabilecek köşe durumlardır (edge case). Bu issue, bu farkları somut kodla sabitler.

---

### Test Kodu 1 — Üç döngü de aynı sayıları üretmeli (`examples/tests/donguler_temel.sqt`)
```c
int main() {
    int i = 0;
    while (i < 3) {
        print(i);
        i = i + 1;
    }

    int j = 0;
    do {
        print(j);
        j = j + 1;
    } while (j < 3);

    for (int k = 0; k < 3; k = k + 1) {
        print(k);
    }
    return 0;
}
```

### Beklenen Çıktı
```
0
1
2
0
1
2
0
1
2
```

---

### Test Kodu 2 — `do-while` koşulu en baştan yanlışsa bile gövde bir kez çalışır (`examples/tests/do_while_en_az_bir.sqt`)
```c
int main() {
    int x = 5;
    do {
        print(x);
        x = x + 1;
    } while (x < 0);   // baştan yanlış!
    return 0;
}
```

### Beklenen Çıktı
```
5
```
(Eğer çıktı **boş** ise — yani `do-while`, `while` gibi davranıyorsa — bu bir **bug**'dır: `do-while`'ın AST düğümü/IR alçaltması `while` ile aynı koddan üretilmiş ve gövdeyi koşuldan **sonra** kontrol etmiyor olabilir.)

---

### Açık Sorular / Bağlı Fazlar
- `for` döngüsünün üç parçası (`init; condition; step`) ayrı ayrı `AST` düğümlerinde mi tutuluyor, yoksa `while`'a "alçaltılıyor" (desugar) mu? İkinci durumda, `for (int k = 0; ...)` içindeki `k`'nın scope'u (sadece `for` bloğuna özel mi?) Faz 2'de doğru kurulmalı.
- `break`/`continue` (E004 ile ilişkili) bu üç döngü türünde de test edilmeli — ayrı bir test dosyası (`donguler_break_continue.sqt`) eklenebilir.

*İmza/Yorum:* `do-while`/`while` karışıklığı, "derleyici neredeyse bitti ama syntax X yanlış davranıyor" tarzı klasik bug'lardandır — bu test erken yazılırsa erken yakalanır."""
    },
    {
        "title": "[Test] Bit Düzeyi (Bitwise) İşlemler — `&`, `|`, `^`, `~`, `<<`, `>>`",
        "labels": [L_TEST],
        "body": """### Giriş (Nedir, Neden Önemli?)
Tokenizer'da `&`, `|`, `^`, `~`, `<<`, `>>` token'ları zaten tanımlı (`src/tokenizer/tokenizer.hpp`). Ama bu operatörlerin **Pratt parser'da doğru öncelik** ile ayrıştırıldığı ve **tip denetiminde** (Faz 3, sadece `int` üzerinde tanımlı) doğru ele alındığı henüz doğrulanmadı. Bu issue, "karmaşık bit işlemleri" için bir golden-test taslağıdır.

---

### Test Kodu (`examples/tests/bit_islemleri.sqt`)
```c
int main() {
    int a = 12;   // 0b1100
    int b = 10;   // 0b1010

    print(a & b);    // bitwise AND
    print(a | b);    // bitwise OR
    print(a ^ b);    // bitwise XOR
    print(~a);       // bitwise NOT (iki's complement)
    print(a << 2);   // sola kaydır
    print(a >> 2);   // sağa kaydır

    // Operatör önceliği: << ve >>, + ve - 'dan düşük öncelikli olmalı (C kuralı)
    print(1 << 2 + 1);   // (2+1)=3 -> 1 << 3 = 8, yoksa (1<<2)+1=5 olur
    print(a & b | b);    // & , | 'dan önce: (a&b) | b = 8 | 10 = 10
    return 0;
}
```

### Beklenen Çıktı
```
8
14
6
-13
48
3
8
10
```

---

### Açık Sorular / Bağlı Fazlar
- `~a` için `-13` beklentisi, `int`'in **iki's complement (ikiye tümleyen) imzalı 32-bit** temsiline dayanır — bu, `src/core/type.hpp`'de `int`'in tam temsilinin (bit genişliği) **şimdiden** dokümante edilmesini gerektirir (Faz 0'a not).
- `1 << 2 + 1` örneği **kasıtlı olarak kafa karıştırıcı** — C'de `<<`/`>>` aritmetik operatörlerden (`+`/`-`) daha düşük önceliklidir. saQut Pratt parser'ı bu sırayı C ile aynı mı tutacak, yoksa daha "şaşırtmasız" bir öncelik mi tanımlayacak? **Karar ne olursa olsun docs'a yazılmalı**, çünkü bu tip kararlar "sessiz" kalırsa en çok şikayet edilen bug kaynağı olur.
- Bu operatörler sadece `int` için tanımlı olmalı; `bool & bool` veya `float << 1` gibi kullanımlar `E003` üretmeli (Faz 3'e not).

*İmza/Yorum:* "Karmaşık byte işlemleri" testi olarak istenen tam da bu — bit-manipülasyonu doğru çalışmayan bir derleyici, gerçek programlar (hash, sıkıştırma, bayrak/flag kümeleri) için kullanılamaz hale gelir."""
    },
    {
        "title": "[Test] Builtin Fonksiyonların Kullanım Örnekleri (`print`, `len`, `toString`, `parseInt`, ...)",
        "labels": [L_TEST],
        "body": """### Giriş (Nedir, Neden Önemli?)
`print` dışındaki builtin'ler henüz yazılmadı (bkz. "[Fikir] Builtin Fonksiyon Kataloğu", #89), ama bu issue **hedefin son hâlini** somut kodla göstermek için var — yani "builtin kataloğu" tamamlandığında bu dosyanın **aynen bu çıktıyı** vermesi beklenir. Erken yazmanın faydası: builtin imzaları (parametre/dönüş tipleri) bu örnekten geriye doğru türetilebilir.

---

### Test Kodu (`examples/tests/builtin_fonksiyonlar.sqt`)
```c
int main() {
    // print: farklı tiplerde çalışmalı
    print(42);
    print(3.14);
    print("merhaba");
    print(true);

    // toString: sayıdan string'e
    print(toString(123));        // "123"

    // parseInt / parseFloat: string'den sayıya
    print(parseInt("45") + 5);   // 50
    print(parseFloat("2.5") * 2.0); // 5.0

    // len: string ve array uzunluğu
    print(len("saqut"));         // 5

    int[] sayilar = {10, 20, 30};
    print(len(sayilar));         // 3

    return 0;
}
```

### Beklenen Çıktı
```
42
3.14
merhaba
true
123
50
5.0
5
3
```

---

### Açık Sorular / Bağlı Fazlar
- `print(true)` çıktısı `true`/`false` mi yoksa `1`/`0` mı olmalı? saQut'ta `bool` ayrı bir tip olduğu için (ADR-010, gizli int↔bool dönüşümü de yok varsayımıyla) `true`/`false` **daha tutarlı** görünüyor — ama bu kararın `print`'in builtin tablosuna (#89) not edilmesi gerekir.
- `int[] sayilar = {10, 20, 30};` — array literal sözdizimi (`{...}`) şu an dilde **tanımlı mı**? Eğer değilse, bu issue aynı zamanda "array literal sözdizimi eklenmeli" şeklinde bir alt-konu açar (Faz 1/parser'a not).
- `parseFloat("2.5") * 2.0` → `5.0` çıktısının `5` değil `5.0` (ondalık nokta korunarak) basılması, `print`'in `float` biçimlendirmesi için bir kural gerektirir — bu kural baştan netleşmeli (örn. her zaman en az bir ondalık basamak).

*İmza/Yorum:* Bu dosya, builtin kataloğu (#89) ve minimal stdlib (#90) issue'larının "kabul testi" (acceptance test) gibi düşünülebilir."""
    },
    {
        "title": "[Test] Struct + Array Birlikte Kullanım Senaryosu",
        "labels": [L_TEST],
        "body": """### Giriş (Nedir, Neden Önemli?)
`struct` ve `int[]` dil kimliğinde "var" olarak işaretli ama bu ikisinin **birlikte** kullanıldığı (struct içinde array, array of struct) bir örnek henüz yazılmadı. Bu kombinasyon, value-semantics (değer semantiği) kopyalama davranışını (bkz. #79 "Array ve Struct'ların Runtime Bellek Düzeni") en çok zorlayan senaryodur.

---

### Test Kodu (`examples/tests/struct_array.sqt`)
```c
struct Ogrenci {
    string ad;
    int notlar[3];
}

int toplaNotlar(Ogrenci o) {
    int toplam = 0;
    for (int i = 0; i < 3; i = i + 1) {
        toplam = toplam + o.notlar[i];
    }
    return toplam;
}

int main() {
    Ogrenci ali;
    ali.ad = "Ali";
    ali.notlar[0] = 70;
    ali.notlar[1] = 85;
    ali.notlar[2] = 90;

    print(ali.ad);
    print(toplaNotlar(ali));

    // value semantics: kopya degisse de orijinal degismemeli
    Ogrenci kopya = ali;
    kopya.notlar[0] = 0;
    print(toplaNotlar(ali));    // değişmemeli: 245
    print(toplaNotlar(kopya));  // değişmeli: 175

    return 0;
}
```

### Beklenen Çıktı
```
Ali
245
245
175
```

---

### Açık Sorular / Bağlı Fazlar
- `int notlar[3];` sözdizimi (struct alanı olarak sabit-boyutlu array) — parser bunu destekliyor mu, yoksa `int[3] notlar;` mı olmalı? Dil kimliğinde array sözdizimi `int[]` olarak yazılmış ama boyutlu hali (`[3]`) henüz örneklenmemiş — bu issue, sözdizimini netleştirme ihtiyacını da gösterir.
- `Ogrenci kopya = ali;` satırının **gerçekten derin kopya** yapması (struct içindeki array dahil) — bu, #79'daki "struct kopyalama IR'de alan-alan mı genişler" sorusunun **somut test vakasıdır**. Eğer `kopya.notlar[0] = 0;` satırı `ali.notlar[0]`'ı da değiştirirse (yüzeysel kopya/shallow copy), bu **value semantics kuralının ihlalidir** — kritik bir bug.

*İmza/Yorum:* Bu test, "value semantics" kilitli kararının (dil kimliği tablosu) en sıkı sınandığı yer — diğer tüm testler geçse bile bu test başarısızsa, dilin temel garantisi bozulmuş demektir."""
    },
    # ---------------- Optimizasyon test senaryolari ----------------
    {
        "title": "[Test] Optimizasyon — Constant Folding Doğrulama (`--optimized` Öncesi/Sonrası)",
        "labels": [L_TEST],
        "body": """### Giriş (Nedir, Neden Önemli?)
Faz 4'ün constant folding pass'i, sabit ifadeleri derleme zamanında hesaplayıp `Literal` düğümüyle değiştirmeli (bkz. roadmap Faz 4). Bu issue, "öncesi/sonrası" AST karşılaştırmasının **tam olarak nasıl görünmesi gerektiğini** somutlaştırır.

---

### Test Kodu (`examples/tests/opt_constant_folding.sqt`)
```c
int main() {
    int x = 2 + 3 * 4;       // sabit ifade -> 14
    int y = (10 - 4) / 2;    // sabit ifade -> 3
    int z = x + y;           // x, y sabit olduğu için z de katlanabilir -> 17
    int w = 10 / 0;          // sıfıra bölme: KATLANMAZ, W002 uyarısı
    print(z);
    print(w);
    return 0;
}
```

### Beklenen Davranış
- `saqut ast examples/tests/opt_constant_folding.sqt` (bayraksız): orijinal AST — `x`'in initializer'ı `BinaryExpression(2, +, BinaryExpression(3, *, 4))` olarak **değişmeden** görünür.
- `saqut ast examples/tests/opt_constant_folding.sqt --optimized`: klonlanmış AST'de:
  - `x`'in initializer'ı → `Literal(14)`
  - `y`'nin initializer'ı → `Literal(3)`
  - `z`'nin initializer'ı → `Literal(17)` (**zincirleme**: `x` ve `y` önce katlanmalı, sonra `z = x + y` de katlanabilmeli — bu, fixpoint döngüsünün gerekliliğinin kanıtıdır)
  - `w`'nin initializer'ı → **değişmez** (`10 / 0` kalır), ve diagnostic çıktısında `W002` (sıfıra bölme) uyarısı görünür.
- `saqut run examples/tests/opt_constant_folding.sqt` çıktısı (VM tamamlandığında): `17` ve ardından `10 / 0` çalışma-zamanı hatası (bu son satır, IR/VM tasarımına `E0xx`/runtime-error kategorisi notu olarak düşülmeli).

---

### Açık Sorular / Bağlı Fazlar
- "Zincirleme katlama" (`z = x + y`, `x` ve `y` kendileri katlanmış sabitlerden geliyor) tek bir fixpoint turunda mı yoksa birden fazla turda mı gerçekleşmeli — bu, ADR-009'daki fixpoint/iterasyon-tavanı kararının **somut test sayısı** olabilir (örn. "bu örnek 2 turda kararlı hale gelmeli").
- `10 / 0` ifadesinin **derleme zamanında uyarı, çalışma zamanında hata** ayrımı IR/VM tasarımına (#74-78) açıkça not edilmeli.

*İmza/Yorum:* Bu test dosyası, Faz 4'ün "bitti" tanımının resmi kanıtı olabilir — roadmap'teki "Doğrulama" bölümüne doğrudan eklenebilir."""
    },
    {
        "title": "[Test] Optimizasyon — Dead Code Elimination Doğrulama (Ölü Kod Temizliği)",
        "labels": [L_TEST],
        "body": """### Giriş (Nedir, Neden Önemli?)
Faz 4'ün DCE (Dead Code Elimination, Ölü Kod Eleme) pass'i, erişilemez kodu ve kullanılmayan değişkenleri temizlemeli. Bu issue, "ölü kod" sayılan **farklı durumların** her birini ayrı ayrı örnekler — DCE'nin sadece "return sonrası kod" ile sınırlı kalmaması gerektiğini gösterir.

---

### Test Kodu (`examples/tests/opt_dead_code.sqt`)
```c
int hesapla(int n) {
    int kullanilmayan = 99;   // hiç kullanılmıyor -> W001

    if (n > 0) {
        return n * 2;
        print(n);             // return sonrası -> erişilemez, W003
    }

    return -1;

    int olu = 5;              // fonksiyonun erişilemez kuyruğu -> W003
    return olu;
}

int main() {
    print(hesapla(5));
    return 0;
}
```

### Beklenen Davranış
- `saqut ast examples/tests/opt_dead_code.sqt --optimized` çıktısında:
  - `int kullanilmayan = 99;` satırı **silinmiş** olmalı (referans sayısı 0 → Faz 2'nin `references` listesi boş).
  - `print(n);` (return sonrası) **silinmiş** olmalı.
  - `int olu = 5; return olu;` (fonksiyonun son `return -1;`'inden sonraki erişilemez kuyruk) **silinmiş** olmalı.
- Diagnostic çıktısında **3 uyarı** görünmeli: `W001` (kullanılmayan değişken), iki adet `W003` (erişilemez kod) — konum bilgileriyle (satır/sütun) birlikte.
- `saqut ast examples/tests/opt_dead_code.sqt` (bayraksız, optimize edilmemiş): **hiçbir satır silinmemiş**, ama uyarılar yine de raporlanabilir (uyarı raporlama optimizasyondan bağımsız olabilir — bu ayrım netleştirilmeli).
- `saqut run examples/tests/opt_dead_code.sqt` çıktısı (optimize edilsin/edilmesin aynı olmalı — **DCE anlamı değiştirmez**, sadece temizler): `10`

---

### Açık Sorular / Bağlı Fazlar
- "Kullanılmayan değişken" (`W001`) tespiti Faz 2'nin `references` sayacına dayanır — ama bu sayaç **optimize edilmemiş orijinal AST'de** mi tutulur, yoksa her fixpoint turunda klon üzerinde yeniden mi hesaplanır (ADR-009, "flow-bound analiz her turda klon üzerinde yeniden hesaplanır")? Bu örnek, o kararın doğru uygulanıp uygulanmadığının testi.
- Fonksiyonun "erişilemez kuyruğu" (son `return`'den sonraki kod) ile "if içindeki return sonrası kod" aynı `isReachable=false` mekanizmasıyla mı işaretleniyor — yoksa farklı kod yolları mı?

*İmza/Yorum:* "deadcode elimination eklenecek" değil, tam olarak kullanıcının istediği gibi: "optimizasyonlu/optimizasyonsuz çıktı karşılaştırıldığında gereksiz kod gözlemlenene kadar" çalışılacak somut hedef budur."""
    },
    # ---------------- CLI / UX ----------------
    {
        "title": "[Fikir] CLI'da AST Görüntüleme — Ağaç (Tree), Tablo ve `--format=dot` Seçenekleri",
        "labels": [L_FIKIR, L_CLIUX],
        "body": """### Giriş (Nedir, Neden Önemli?)
Şu an `saqut ast` ham JSON basıyor. JSON, makineler için ideal ama bir insanın "bu kod nasıl bir ağaca dönüşmüş" sorusuna hızlı cevap vermesi için **okunması zor**. "Programlanabilir derleyici" felsefesi hem makine-okur (JSON) hem **insan-okur** çıktıyı hak ediyor.

---

### Gelişme (Olası Yaklaşımlar)
- `saqut ast file --format=json` (mevcut, varsayılan kalabilir), `--format=tree`: terminalde girintili/kutu-çizgili bir ağaç (örn. `tree` komutunun çıktısına benzer: `├──`, `└──`).
- `--format=dot`: Graphviz DOT formatı — `saqut ast file --format=dot | dot -Tpng -o ast.png` ile görsel AST diyagramı üretilebilir (#43'te zaten "AST görselleştirme" fikri vardı, bu onun somut hâli).
- `--format=table`: her düğümü satır olarak basan, `tip | konum | değer` kolonlarına sahip düz bir tablo — `grep`/`awk` ile script'lenmesi kolay bir ara format.
- Renkli terminal çıktısı (`--color`): düğüm tipine göre (Expression mavi, Statement sarı, Literal yeşil gibi) ANSI renk kodları — `--optimized` ile birlikte kullanılırsa, **silinen düğümler kırmızı/üstü çizili** gösterilebilir (DCE'nin görsel kanıtı).

---

### Açık Sorular
- `--format=tree`/`table`/`dot` çıktıları AST'nin **tam** bilgisini mi taşır (her alan), yoksa "özet" mi (sadece tip + konum + kısa değer)? Tam JSON her zaman `--format=json` ile erişilebilir kalmalı — diğerleri "insan için özet" olabilir.
- Bu format seçenekleri `saqut symbols` ve ileride `saqut ir` için de **aynı bayrak isimleriyle** tutarlı olmalı mı (örn. her komut `--format=tree/json/dot/table` desteklesin — CLI'da tek bir ortak altyapı, `src/cli/format.hpp` gibi)?

*İmza/Yorum:* `--format=dot` özellikle düşük efor / yüksek "vitrin" değeri taşıyor — bir blog yazısında/README'de "işte saQut'un AST'si" diye gösterilebilecek bir görsel üretir."""
    },
    {
        "title": "[Fikir] CLI Komut Seti Genişletme Önerileri (`check`, `explain`, `watch`)",
        "labels": [L_FIKIR, L_CLIUX],
        "body": """### Giriş (Nedir, Neden Önemli?)
Mevcut/planlanan komutlar (`tokens`, `ast`, `symbols`, `run`, ileride `ir`, `fmt`, `lsp`) hep "bir aşamanın çıktısını göster" mantığında. Bu issue, geliştirici deneyimini iyileştirecek **birleşik/yardımcı** komutları tartışır.

---

### Gelişme (Olası Yaklaşımlar)
- **`saqut check file`**: sadece derleme hatalarını (Faz 0-3 diagnostic'leri) raporlar, **hiçbir çıktı üretmez/çalıştırmaz** — "bu kod derlenir mi?" sorusuna hızlı cevap. CI'larda (otomatik test sistemlerinde) kullanışlı. Çıkış kodu (exit code) `0` = hatasız, `1` = hata var.
- **`saqut explain E003`**: bir hata kodunun ne anlama geldiğini, **neden** var olduğunu (ilgili ADR'ye atıfla) ve örnek doğru/yanlış kod gösteren bir açıklama basar — hata kataloğunun (roadmap'teki tablo) CLI'dan erişilebilir hâli. "[Fikir] Akıllı Diagnostic" (#98) ile doğrudan ilişkili.
- **`saqut watch file --run`**: dosya her kaydedildiğinde otomatik olarak `check`/`run` çalıştırır, sonucu terminalde gösterir — hızlı geri-bildirim döngüsü (`cargo watch` benzeri). v0 için basit bir dosya-değişikliği izleme (polling veya `inotify`) yeterli.
- **`saqut --version` / `saqut --help`**: temel ama unutulmaması gereken iskelet komutlar — her CLI aracının beklediği standart.

---

### Açık Sorular
- `saqut explain` kataloğu, hata mesajlarının içine gömülü mü olacak (örn. her hata "detaylar için: `saqut explain E003`" diye bitiyor), yoksa ayrı statik bir JSON/Markdown kataloğundan mı okunacak?
- `saqut watch`, LSP (#91) ile fonksiyonel olarak örtüşüyor — ikisi de "kodu değiştirdikçe geri bildirim" sağlıyor. `watch` LSP'den **önce**, basit/terminal-tabanlı bir "ara çözüm" olarak mı konumlanmalı?

*İmza/Yorum:* `saqut check` ve `saqut explain`, düşük maliyetli ama günlük kullanımda en çok hissedilecek iyileştirmeler — Faz 0'ın `DiagnosticEngine`'i bittiği anda `check` neredeyse bedavaya gelir."""
    },
    # ---------------- Kalite / Mimari tavsiyeler ----------------
    {
        "title": "[Fikir] Performans için C Tarzı Tasarım Kararları (Hız Önceliksiz Ama Bilinçli)",
        "labels": [L_FIKIR, L_KALITE],
        "body": """### Giriş (Nedir, Neden Önemli?)
ADR-015 "öncelik determinizm + incelenebilirlik, ham hız değil" diyor — bu **doğru** bir karar. Ama "hız öncelik değil" ile "gereksiz yere yavaş" aynı şey değildir. Bu issue, **mimariyi karmaşıklaştırmadan** (yeni soyutlama eklemeden) elde edilebilecek "bedava" performans kazanımlarını listeler — C'nin "ne kadar az iş, o kadar hızlı" felsefesinden ilham alarak.

---

### Gelişme (Tavsiyeler)
- **Gereksiz kopyalama'dan kaçın:** AST düğümleri ve `Type`/`Symbol` nesneleri `std::string` gibi alanlar taşıyor — bunlar fonksiyonlara **referans/`const&`** ile geçilmeli, değer ile değil. Bu, "header-only" (ADR-003) ile çatışmaz, sadece dikkatli imza yazımı gerektirir.
- **`std::unique_ptr` ile sahiplik (ownership) net olsun** (#44'te bahsedilen issue tipi) — bellek sahipliği belirsizse hem bug hem performans kaybı (gereksiz `shared_ptr`/kopya) olur.
- **Tokenizer/Lexer tek geçişte (single-pass) çalışıyor mu** — kaynak dosya birden fazla kez taranıyorsa (örn. önce tokenize, sonra tekrar karakter-karakter konum hesaplama), bu birleştirilebilir.
- **IR/VM tasarımında (ileride):** yığın-tabanlı VM'de (#76) gereksiz push/pop zincirleri, basit "peephole" (göz ucu) optimizasyonlarla (örn. `push X; pop` → hiçbir şey) IR seviyesinde temizlenebilir — bu, Faz 4'ün constant folding'ine **benzer** ama IR'e özel, küçük bir ek pass olabilir.
- **Derleme zamanı (`saqut` binary'sinin kendi derlenme hızı):** header-only + agresif template kullanımı derleme süresini şişirebilir — `-Wall -Wextra` yanında zaman zaman derleme süresi de (örn. `time cmake --build`) izlenmeli.

---

### Açık Sorular
- "Bedava" optimizasyonlar (kopya azaltma, peephole) ile "erken optimizasyon yapma" ilkesi (roadmap'teki "önce dikey dilim") arasındaki çizgi nerede? Önerim: **mimariyi değiştirmeyen, sadece imza/kopya düzeltmeleri** her zaman yapılabilir; **yeni pass/altyapı gerektirenler** (peephole gibi) Faz 4 sonrasına bırakılır.

*İmza/Yorum:* "C gibi hızlı olmak", saQut için "C kadar hızlı VM" anlamına gelmek zorunda değil — "gereksiz iş yapmayan, dikkatli yazılmış C++ kodu" anlamına gelebilir; bu, ADR-015 ile çatışmaz."""
    },
    {
        "title": "[Fikir] Güvenlik ve Stabilite için Java Tarzı Tasarım Kararları (Sınır Kontrolü, Hata İzolasyonu)",
        "labels": [L_FIKIR, L_KALITE],
        "body": """### Giriş (Nedir, Neden Önemli?)
saQut'ta kullanıcıya açık pointer yok (dil kimliği) — bu, C'nin en büyük güvenlik açığı kaynağı olan "pointer aritmetiği"ni baştan eliyor. Ama Java'nın asıl gücü bundan da fazlası: **her hata kontrollü bir şekilde yakalanır, programın geri kalanı çökmeden devam edebilir** (exception/hata izolasyonu) ve **dizi sınırları her zaman kontrol edilir**. Bu issue, bu ilkelerin saQut'a nasıl yansıyacağını tartışır.

---

### Gelişme (Tavsiyeler)
- **Array sınır kontrolü (bounds checking):** `int[3] notlar; notlar[5] = 1;` gibi bir erişim — eğer boyut derleme zamanında biliniyorsa (`E009` zaten "array boyutu sabit değil" hatasını kapsıyor, ama "sabit boyut + sınır-dışı indeks" ayrı bir konu) **derleme zamanında** `E0xx` ile yakalanmalı; eğer indeks çalışma zamanı değeriyse (`notlar[i]`, `i` döngü değişkeni), **VM çalışma zamanında kontrol etmeli** ve kontrollü bir "runtime error" üretmeli (C'deki sessiz bellek bozulmasının tam tersi).
- **Çalışma zamanı hatalarının kategorize edilmesi:** sıfıra bölme (Faz 4'te `W002` derleme-zamanı uyarısı var, ama çalışma zamanında gerçek bir bölme `10 / x` ile `x=0` olursa ne olur?), dizi sınır taşması, derinlik taşması (call stack overflow, #78) — bunların hepsi **aynı "RuntimeError" ailesinde**, konum bilgisiyle (hangi `print`/satırda patladı) raporlanmalı; VM **segfault** ile çökmemeli.
- **"Programın bir kısmı patlasın, derleyici/VM çökmesin":** VM'in kendi C++ kodunda `assert`/exception kullanımı, kullanıcı programındaki bir hatayı (örn. sınır-dışı erişim) **VM crash'ine** dönüştürmemeli — bu, VM geliştirilirken her runtime-error noktasında "bu durum kullanıcıya nasıl raporlanır" sorusunun sorulması demektir.
- **Tip sisteminin sıkılığı (ADR-010) zaten "Java tarzı" bir güvenlik katmanı** — gizli dönüşüm yok kuralı, C'deki "implicit int→pointer" gibi klasik hata sınıflarını zaten önlüyor. Bu issue bunu **devam ettirme** çağrısıdır, yeni bir şey eklemiyor.

---

### Açık Sorular
- Çalışma zamanı hataları (array sınır taşması vb.) için **yeni bir diagnostic kategorisi** (`R001`, `R002`... "Runtime" öneki ile, `E`/`W`'den ayrı) mı açılmalı — IR/VM tasarım issue'larına (#74-78) not edilmeli.
- Sınır kontrolü **her zaman** mı açık olacak, yoksa "release modu"nda (varsa böyle bir kavram) kapatılabilir mi — ADR-015'in "determinizm > hız" ilkesi düşünülürse, **her zaman açık** olması daha tutarlı görünüyor.

*İmza/Yorum:* "Güvenli dil" derken kastedilen genelde "bellek güvenliği" — saQut bunu zaten pointer'sızlıkla büyük ölçüde kazandı; kalan iş, **çalışma zamanı hatalarını zarif bir şekilde raporlamak**."""
    },
    {
        "title": "[Fikir] Modernlik için Go Tarzı Tasarım Kararları (Basitlik, Hızlı Geri-Bildirim, Tek Binary)",
        "labels": [L_FIKIR, L_KALITE],
        "body": """### Giriş (Nedir, Neden Önemli?)
Go'nun başarısının çoğu **dil özelliklerinden değil, geliştirici deneyiminden** geliyor: çok hızlı derleme, yerleşik formatter (`gofmt`), tek-binary dağıtım, anlaşılır hata mesajları, "az ama öz" bir standart kütüphane. saQut zaten küçük/basit bir dil — bu issue, Go'nun **araç zinciri (toolchain) felsefesini** saQut'a nasıl taşıyabileceğimizi listeler.

---

### Gelişme (Tavsiyeler)
- **Tek binary, sıfır bağımlılık:** `saqut` CLI'sı tüm komutları (`tokens`, `ast`, `symbols`, ileride `run`, `fmt`, `lsp`) **tek bir çalıştırılabilir dosyada** barındırır — kullanıcı hiçbir ek paket/runtime kurmaz. Header-only C++ (ADR-003) bu hedefe zaten doğal olarak uygun; bu issue sadece **bunu bir hedef olarak yazıya dökmek**.
- **Hızlı derleme = hızlı geri-bildirim:** Go'nun derleme hızı, "kodu yaz → hemen çalıştır" döngüsünü mümkün kılıyor. saQut için bu, `saqut run` (IR+VM) ile birleşince **"saQut programını derleyip çalıştırmak, C++ programını derlemekten çok daha hızlı olmalı"** gibi somut bir hedef olabilir — VM yorumlayıcı olduğu için bu zaten doğal bir avantaj.
- **`gofmt` zaten bir issue olarak var (#93, `saqut fmt`)** — Go'nun "tartışmasız tek doğru format" felsefesi: format ayarlanabilir **olmamalı** (yapılandırma dosyası yok), bu da formatter'ın tasarımını basitleştirir.
- **Anlaşılır hata mesajları (#98 ile ilişkili):** Go'nun hata mesajları kısa ve doğrudandır (`undefined: x`). saQut'un hata kataloğu (E001 vb.) zaten bu yönde — bu issue, mesaj **dilinin** (Türkçe/İngilizce) ve **tonunun** (kısa cümle + örnek) standartlaştırılmasını önerir.
- **Yerleşik test çalıştırıcı (#96 ile ilişkili)** ve **modül sistemi (#81-83)** — Go'nun `go test` ve `go mod`'u, dilin "etrafına" değil **içine** gömülü araçlar. saQut'un CLI'sı da bu felsefeyle büyümeli: her yeni özellik (test, format, modül) **ayrı bir 3. parti araç değil, `saqut` alt-komutu** olmalı.

---

### Açık Sorular
- "Tek binary" hedefi, FFI (#88) ile gerilim yaratabilir mi — kullanıcı kendi C++ host fonksiyonlarını eklemek isterse, bu "tek binary"yi nasıl etkiler (dinamik kütüphane yükleme mi, yeniden derleme mi)?
- Hata mesajı dili: şu anki dokümantasyon Türkçe — CLI çıktısı da Türkçe mi olacak, yoksa İngilizce + Türkçe `explain` (#issue) mi? Bu, uluslararası kullanıcı kitlesi hedefleniyorsa önemli bir erken karar.

*İmza/Yorum:* saQut'un "alet çantası" kimliği ile Go'nun "az şey, ama hepsi birlikte ve iyi çalışıyor" felsefesi doğal bir eşleşme — bu issue, var olan diğer issue'ları (#91-98) tek bir "araç zinciri vizyonu" şemsiyesi altında toplama denemesidir."""
    },
]

for issue in issues_data:
    endpoint = f"{REPO_PATH}/issues"
    res = make_request(endpoint, method="POST", data=issue)
    print(f"Created issue #{res['number']}: {res['title']}")
