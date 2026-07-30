```text
Görev: saQut derleyicisini gerçekçi bir programla doğrulamak. Yazdığın programı
saQut ile derleyip çalıştıracak ve sonucu bağımsız beklemediğin çıktıyla
karşılaştıracaksın. SADECE bir .sqt programı yazıp çalıştır; derleyici kaynağına
DOKUNMA. Programı examples/algorithm/ altına koy (bu klasör bu tür denemeler için
var; /tmp KULLANMA).

BAĞLAM:
saQut, C benzeri statik tipli bir dil. Son düzeltmeler: hex/ikili/oktal sayı
literalleri (0x80, 0b1010, 0777) artık doğru; fonksiyon array dönüş tipleri
(int[] f()) artık parse ediliyor. Amacımız: float32, byte shifting, array ve
switch/toggle içeren bilinen bir algoritmayla residual (kalan) hata yakalamak.

ALGORİTMA GEREKSİNİMİ:
Bilinen, deterministik bir algoritma seç (öneri: Xorshift32 veya Xorshift128
PRNG; istersen RC4 KSA+PRGA veya Pearson hash). Programın MUTLAKA şu dörtünü
anlamlı şekilde (sadece dokunmak için değil, çıktıyı etkileyecek biçimde)
kullanması gerek:
  1. byte shifting: bir byte/int değeri üzerinde <<, >>, ^ ile kaydırma/xor
     (xorshift çekirdeği bunu doğal yapar).
  2. float32: en az bir çıktıyı float tipinde üret ve onu sonucu etkileyen bir
     hesapta kullan (örn. keystream'i 2^32'ye bölüp [0,1) normalize et; float
     32-bit single, double 64-bit — precision farkı gözlenebilmeli).
  3. array: durumu/buffer'ı bir byte[] veya int[] içinde tut, indeksle oku/yaz.
  4. switch/toggle: bir mod değişkeniyle switch yap (örn. 0=ham int çıktı,
     1=byte çıktı, 2=float32 normalize çıktı) ve her dalı en az bir kez çalıştır.

Beklenen çıktıyı BAĞIMSIZ olarak hesapla (Python/elle) ve yaz — algoritma
deterministik olduğu için ilk ~5 çıktıyı referansla. Böylece saQut'un ürettiği
sonuçla karşılaştırabilirsin.

saQut SÖZDİZİMİ REFERANSI (doğrulanmış):
  - Giriş:    int main() { ... return 0; }
  - Fonksiyon: <dönüşTipi> ad(<paramlar>) { ... return x; }
      dönüş tipleri: int longint float double byte bool char string void
                     ve array: int[]  byte[]  (array dönüş artık destekli)
  - print(x);  -> YENİ SATIR EKLEMEZ. Değerler arasına print("\n") koy.
      int/float/double/string/bool basar.
  - Tipler ve genişlikler: int=32b, longint=64b, float=32b single,
      double=64b, byte=0..255, bool, char, string, decimal, date.
  - Sayı literalleri: 123 (onluk), 0x80 (hex), 0b1010 (ikili), 0777 (oktal);
      float: 1.5 (çıplak float literal -> float32); string "..."
  - Array:   int[] a = [1,2,3];   a[i]   a[i] = v;   (indeks atama destekli)
      çok boyut: int[][]  m[];  eleman tipi byte[] de olabilir.
      Boyutlu boş array gerekiyorsa literalle doldur ([0,0,0,...]) — `new`
      sözdiziminin varlığını denetle, emin değilsen literal kullan.
  - Cast:   x as int   x as float   x as byte   x as string
      başarısız olabilecek cast:  s as int?  (nullable; başarısızsa null)
      byte aritmetikte int'e terfi eder, sonuç asla byte olmaz.
  - Bitwise/kaydırma:  & | ^ ~ << >>   (int/longint üzerinde; byte->int olur)
  - Atama/artırma:  i = i + 1  kullan.  ++ / += / -=  desteklenmeyebilir;
      emin değilsen açık form yaz (i = i + 1).
  - Kontrol:  if/else,  while,  for (i = 0; i < n; i = i + 1),
      do { } while (cond);  switch (x) {
        case 1: ...; break;
        case 3, 4: ...; break;     // çok-değerli case
        default: ...;
      }
      switch case değeri float olabilir (case 1.5:). Fallthrough davranışını
      doğrulamak için HER case'e explicit break koy.
  - Karşılaştırma/mantık:  == != < <= > >=   && || !
  - Nullable:  int? x = ...;  null;   if (x == null) { ... }

ÇALIŞTIRMA KOMUTLARI (derleyici hazır, /home/saqut/Masaüstü/saqutcompiler altında):
  cd /home/saqut/Masaüstü/saqutcompiler
  ./build/saqut run        examples/algorithm/<dosya>.sqt   # VM (normatif) — ana test
  ./build/saqut run --jit  examples/algorithm/<dosya>.sqt   # JIT (deneysel; desteklenmeyen
                                                            #   opcode varsa atlar, normal)
  ./build/saqut check      examples/algorithm/<dosya>.sqt   # sadece semantic kontrol
  ./build/saqut ir         examples/algorithm/<dosya>.sqt   # IR dökümü (gerekirse)
  Eğer binary yeniden derlenmeliyse: cmake --build build

RAPOR FORMATI (bu yapıyı kullan, sade tut):
  1. SEÇİLEN ALGORİTMA: adı + neden seçildi.
  2. PROGRAM: tam .sqt kaynağı (kod bloğu olarak).
  3. BEKLENEN ÇIKTI: bağımsız hesaplama yöntemi + ilk ~5 değer.
  4. GERÇEK ÇIKTI (VM):  ./build/saqut run  -> stdout, stderr, exit code.
  5. JIT DİFERANSİYELİ:  ./build/saqut run --jit çıktısı VM ile birebir mi?
     (fark varsa detay; JIT "desteklenmeyen opcode" atladıysa belirt).
  6. ÖZELLİK DOĞRULAMASI: byte shifting / float32 / array / switch — her biri
     beklenen gibi mi çalıştı? float32'nin 32-bit precision'ı gözlemlenebildi mi?
  7. ANOMALİLER: yanlış sonuç, çökme, askıda kalma, geçerli kodda parse/type
     hatası, sessiz yanlış hesap. YOKSA "anomali yok" yaz.
  8. ŞÜPHELER: emin olamadığın ama test etmeye değer davranışlar.

Önemli: "çalıştı" demek için çıktıyı BEKLENEN ile karşılaştır. Sadece exit=0
yetmez; değerler doğru mu kontrol et. Yanlış değeri "anomali" olarak raporla.
```
