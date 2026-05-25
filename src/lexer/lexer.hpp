// ============================================================================
// saQut Compiler — Lexer (Karakter Seviyesinde Tarayıcı)
// ============================================================================
//
// DİZİN:   src/lexer/lexer.hpp
// KATMAN:  Katman 1 — Derleyici pipeline'ının ilk aşaması
// BAĞIMLI: Yok (sadece standart kütüphane)
// KULLANAN: Tokenizer (src/tokenizer/tokenizer.hpp)
//
// AMAÇ:
//   Ham kaynak kodu (std::string) karakter karakter tarayarak:
//   1. Karakter konumunu takip eder (offset)
//   2. Backtracking (geri alma) desteği ile desen eşleme yapar
//   3. Sayısal literal'ları okur ve sınıflandırır (decimal, hex, binary, octal, float)
//   4. Boşluk karakterlerini atlar
//   5. Satır/sütun bilgisi sağlar (hata mesajları için temel)
//
// ADR-006: Neden Kendi Lexer'ımız?
//   - std::istringstream veya regex kullanmak yerine, tam kontrol sağlayan
//     sıfırdan bir lexer yazdık.
//   - Backtracking: offsetMap ile konum yığını tutar, denenen bir eşleşme
//     başarısız olursa geri alınabilir. Bu özellik std::istream'de yoktur.
//   - Performans: Sanal fonksiyon çağrısı yok, her şey inline.
//   - Hata ayıklama: Her karakter okuması kontrol edilebilir.
//
// TASARIM KARARLARI:
//   1. offsetMap (std::vector<int>): İç içe backtracking için yığın.
//      beginPosition() → yığına mevcut konumu ekler
//      acceptPosition() → yığındaki son konumu kalıcı yapar
//      rejectPosition() → yığındaki son konumu atar (geri al)
//      Bu sayede "dene, başarısız olursa geri al" patterni çalışır.
//
//   2. getchar() iki overload:
//      - getchar(): mevcut konumdaki karakteri okur
//      - getchar(int offset): mevcut konum + offset'teki karakteri okur
//      İkincisi özellikle keyword kontrolünde önemlidir:
//      "do" kelimesini gördükten sonra, bunun "double"ın başlangıcı olmadığını
//      kontrol etmek için keyword sonrası karaktere bakılır.
//
//   3. isEnd(): offset >= size ile kontrol. offset her zaman [0, size] aralığında.
//      size konumunda EOF (end of file) anlamına gelir.
//
//   4. readNumeric(): C/C++/Java sayı formatlarını destekler:
//      - Decimal:    42, -3, +7
//      - Hex:        0xFF, 0X1A
//      - Binary:     0b1010, 0B1100
//      - Octal:      0777 (0 ile başlayan ve 8-9 içermeyen)
//      - Float:      3.14, .5, 1e10, 2.5E-3
//      - Negatif/Pozitif: -42, +3 (baştaki işaret)
//
// BİLİNEN SINIRLAMALAR (TODO):
//   TODO: Satır/sütun takibi eklenmeli (şu anda sadece offset var)
//   TODO: Unicode/UTF-8 desteği (şu anda sadece ASCII)
//   TODO: ' char literal'ı okunamıyor
//   TODO: Sayısal alt çizgi (_) ayracı: 1_000_000 formatı
//   TODO: Binary floating point: 0b1.1p10 formatı (C99 hexfloat)
//
// ============================================================================

#ifndef SAQUT_LEXER
#define SAQUT_LEXER

#include <iostream>
#include <string>
#include <vector>

// ============================================================================
// INumber — Ara Sayısal Veri Yapısı
// ============================================================================
//
// Lexer'ın readNumeric() fonksiyonu tarafından döndürülür.
// Tokenizer bu yapıyı NumberToken'a dönüştürür.
//
// Neden ayrı bir struct? Lexer katmanı Token sınıflarından haberdar değil.
// Bağımlılık yönü: Lexer ← Tokenizer. Lexer hiçbir üst katmanı include etmez.
//
// ALANLAR:
//   start, end  : Kaynak koddaki başlangıç/bitiş konumları (offset)
//   token       : Sayının ham string hali (örn: "0xFF", "3.14", "1e10")
//   isFloat     : Ondalıklı sayı mı? (nokta veya epsilon içeriyor mu)
//   hasEpsilon  : Bilimsel gösterim mi? (e/E içeriyor mu)
//   base        : Sayı tabanı: 2, 8, 10, veya 16
//                 - 0x/0X ile başlarsa 16
//                 - 0b/0B ile başlarsa 2
//                 - 0 ile başlayıp 8-9 içermiyorsa 8
//                 - diğer her şey 10
//   positive    : Pozitif mi? (başında - işareti yoksa true)
//
struct INumber {
    int start = 0;           // Kaynak koddaki başlangıç offset'i
    int end   = 0;           // Kaynak koddaki bitiş offset'i
    std::string token;       // Sayının ham metni (örn: "42", "0xFF", "3.14e-2")
    bool isFloat    = false; // true ise float/double literal
    bool hasEpsilon = false; // true ise bilimsel gösterim (örn: 1e10)
    int base        = 10;    // Sayı tabanı: 2, 8, 10, 16
    bool positive   = true;  // false ise sayı negatif
};

// ============================================================================
// Lexer — Karakter Seviyesinde Tarayıcı
// ============================================================================
//
// Derleyici pipeline'ının en alt katmanı. Ham string üzerinde çalışır.
// Üst katmanlara (Tokenizer) karakter okuma ve konum yönetimi hizmeti sunar.
//
// DURUM DEĞİŞKENLERİ:
//   input     : Taranan kaynak kodun tamamı (string kopyası, değişmez)
//   size      : input.length() önbelleği (performans: her seferinde hesaplamaz)
//   offset    : Mevcut okuma konumu. 0 = ilk karakter, size = EOF
//   offsetMap : Backtracking yığını. İç içe beginPosition/acceptPosition/rejectPosition
//
// PERFORMANS NOTU:
//   Tüm metotlar inline tanımlanmıştır. Sanal fonksiyon çağrısı yoktur.
//   offset değişiklikleri O(1)'dir.
//   include() metodu O(n) karakter karşılaştırması yapar (n = kelime uzunluğu).
//
class Lexer {
public:
    // --- Ham Veri ---
    std::string input;       // Kaynak kodun tamamı
    int size   = 0;          // input.length() önbelleği
    int offset = 0;          // Mevcut okuma konumu (0 = başlangıç)
    std::vector<int> offsetMap; // Backtracking yığını

    // --- Pozisyon Yönetimi (Backtracking API) ---
    //
    // Kullanım örneği:
    //   lexer.beginPosition();          // konumu kaydet
    //   if (lexer.include("for", false)) // dene (false = eşleşse de geri al)
    //       lexer.acceptPosition();      // başarılı → kalıcı yap
    //   else
    //       lexer.rejectPosition();      // başarısız → geri al

    void beginPosition();    // Şu anki konumu yığına kaydet
    int  getLastPosition();  // Yığındaki son konumu döndür
    void acceptPosition();   // Yığındaki son konumu kalıcı yap (apply)
    void setLastPosition(int n); // Yığındaki son konumu n olarak değiştir
    void rejectPosition();   // Yığındaki son konumu at (discard)

    // --- Dosya Sonu ve Pozisyon Sorgulama ---
    bool isEnd();            // offset >= size ise true (EOF)
    int* positionRange();    // [start, end] offset aralığı (tahsis eder, silinmeli!)
    std::string getPositionRange(); // Pozisyon aralığındaki metni döndür

    // --- Desen Eşleme ---
    // include(): Belirtilen kelime mevcut konumda başlıyor mu?
    //   accept=true (varsayılan): eşleşirse konum ilerletilir
    //   accept=false: eşleşse bile konum geri alınır (keyword kontrolü için)
    //   Örnek: include("for", false) → "for" ile başlıyor mu? konumu değiştirme.
    bool include(std::string word, bool accept = true);

    // --- Konum Okuma/Yazma ---
    int  getOffset();        // Mevcut offset'i döndür
    int  setOffset(int n);   // Offset'i n olarak ayarla, yeni değeri döndür

    // --- Karakter Okuma ---
    char getchar(int additionalOffset); // offset + ek'teki karakteri oku
    char getchar();          // Mevcut offset'teki karakteri oku
    void nextChar();         // offset'i 1 ilerlet (EOF kontrolü yapar)
    void toChar(int n);      // offset'i n kadar ilerlet

    // --- Üst Seviye İşlemler ---
    void setText(std::string input); // Yeni kaynak kodu yükle
    void skipWhiteSpace();   // Boşluk/sekme/satırsonu karakterlerini atla
    bool isNumeric();        // Mevcut karakter 0-9 aralığında mı?
    INumber readNumeric();   // Sayı literal'ı oku ve INumber olarak döndür
};

// ============================================================================
// GERÇEKLEME (Implementation)
// ============================================================================
// Tüm metotlar inline olarak aşağıda tanımlanmıştır.
// Derleme modeli: header-only. main.cpp bu dosyayı include eder.
// ============================================================================

// --------------------------------------------------------------------------
// beginPosition: Mevcut offset'i yığına kaydet.
// İç içe çağrılabilir: 3 kere beginPosition → 3 elemanlı yığın.
// --------------------------------------------------------------------------
inline void Lexer::beginPosition() {
    offsetMap.push_back(getLastPosition());
}

// --------------------------------------------------------------------------
// getLastPosition: Yığının tepesindeki konumu döndür.
// Yığın boşsa mevcut offset'i döndür (başlangıç durumu).
// --------------------------------------------------------------------------
inline int Lexer::getLastPosition() {
    if (offsetMap.empty()) return offset;
    return offsetMap.back();
}

// --------------------------------------------------------------------------
// acceptPosition: Yığındaki son geçici konumu kalıcı yap.
// Örnek: offsetMap=[5,10], offset=15 → offsetMap.back()=10 olur.
// Bu sayede include() denemesi başarılı olduğunda konum ilerletilmiş olur.
// --------------------------------------------------------------------------
inline void Lexer::acceptPosition() {
    int t = offsetMap.back();
    setLastPosition(t);
}

// --------------------------------------------------------------------------
// setLastPosition: Yığının tepesini veya offset'i değiştir.
// --------------------------------------------------------------------------
inline void Lexer::setLastPosition(int n) {
    if (offsetMap.empty())
        offset = n;
    else
        offsetMap.back() = n;
}

// --------------------------------------------------------------------------
// isEnd: Dosya sonuna gelindi mi? offset >= size.
// --------------------------------------------------------------------------
inline bool Lexer::isEnd() {
    return size <= getOffset();
}

// --------------------------------------------------------------------------
// rejectPosition: Yığındaki son konumu at. Başarısız include() denemesi sonrası.
// --------------------------------------------------------------------------
inline void Lexer::rejectPosition() {
    offsetMap.pop_back();
}

// --------------------------------------------------------------------------
// positionRange: Yığındaki en dış ve en iç konumu [start, end] olarak döndür.
// UYARI: new int[2] ile heap'te tahsis eder. Çağıran sorumludur.
// TODO: std::pair<int,int> veya yapı kullanarak tahsisi kaldır.
// --------------------------------------------------------------------------
inline int* Lexer::positionRange() {
    int len = offsetMap.size();
    if (len == 0)
        return new int[2]{0, offset};
    if (len == 1)
        return new int[2]{offset, offsetMap[0]};
    return new int[2]{offsetMap[len - 2], offsetMap[len - 1]};
}

// --------------------------------------------------------------------------
// getPositionRange: positionRange() aralığındaki metni string olarak döndür.
// --------------------------------------------------------------------------
inline std::string Lexer::getPositionRange() {
    int* a = positionRange();
    std::string mem;
    for (int i = a[0]; i < a[1]; i++)
        mem.push_back(input.at(i));
    return mem;
}

// --------------------------------------------------------------------------
// include: Belirtilen kelime mevcut konumda başlıyor mu?
//
// Algoritma:
//   1. beginPosition() ile konumu kaydet
//   2. Kelimenin her karakterini sırayla karşılaştır
//   3. Eşleşmezse veya EOF olursa → rejectPosition() ve false dön
//   4. Tüm karakterler eşleşirse:
//      - accept=true ise → acceptPosition() (konum kalıcı ilerler)
//      - accept=false ise → rejectPosition() (konum eski haline döner)
//   5. true dön
//
// Neden accept parametresi var?
//   Tokenizer scope() fonksiyonu, keyword'leri kontrol ederken accept=false
//   kullanır. Çünkü bir keyword eşleşmesi, aynı zamanda daha uzun bir
//   keyword'ün parçası olabilir (örn: "do", "double"ın başlangıcı).
//   Eğer include("do", true) kullanılırsa, konum ilerler ve geri alınamaz.
// --------------------------------------------------------------------------
inline bool Lexer::include(std::string word, bool accept) {
    beginPosition();
    for (size_t i = 0; i < word.size(); i++) {
        if (isEnd()) {
            rejectPosition();
            return false;
        }
        if (word[i] != getchar()) {
            rejectPosition();
            return false;
        }
        nextChar();
    }
    if (accept)
        acceptPosition();
    else
        rejectPosition();
    return true;
}

// --------------------------------------------------------------------------
// getOffset / setOffset: Konum erişimcileri.
// --------------------------------------------------------------------------
inline int Lexer::getOffset() {
    return getLastPosition();
}

inline int Lexer::setOffset(int n) {
    setLastPosition(n);
    return getLastPosition();
}

// --------------------------------------------------------------------------
// getchar(additionalOffset): offset + ek kadar ilerideki karakteri oku.
// Sınır kontrolü yapar: target >= size ise '\0' döndürür ve hata mesajı basar.
// Bu metot özellikle keyword sınır kontrolünde kullanılır:
//   "do" eşleşti, sıradaki karakter 'u' ise bu "double" olabilir → keyword değil
// --------------------------------------------------------------------------
inline char Lexer::getchar(int additionalOffset) {
    int target = getOffset() + additionalOffset;
    if (target >= size) {
        std::cerr << "Lexer hatası: sınır aşımı\n";
        return '\0';
    }
    return input.at(target);
}

inline char Lexer::getchar() {
    int target = getOffset();
    if (target >= size) {
        std::cerr << "Lexer hatası: sınır aşımı\n";
        return '\0';
    }
    return input.at(target);
}

// --------------------------------------------------------------------------
// nextChar / toChar: Konum ilerletme.
// EOF kontrolü yapar — dosya sonundaysa ilerlemez.
// --------------------------------------------------------------------------
inline void Lexer::nextChar() {
    if (!isEnd())
        setOffset(getOffset() + 1);
}

inline void Lexer::toChar(int n) {
    if (!isEnd())
        setOffset(getOffset() + n);
}

// --------------------------------------------------------------------------
// setText: Yeni kaynak kodu yükle. input ve size'ı günceller.
// --------------------------------------------------------------------------
inline void Lexer::setText(std::string text) {
    input = text;
    size  = text.length();
}

// --------------------------------------------------------------------------
// skipWhiteSpace: Boşluk, sekme, satırsonu, satırbaşı karakterlerini atla.
// Yorum satırlarını atlamaz — bu Tokenizer'ın işi.
// --------------------------------------------------------------------------
inline void Lexer::skipWhiteSpace() {
    while (!isEnd()) {
        switch (getchar()) {
            case '\r':  // carriage return (Windows satırsonu \r\n)
            case '\n':  // line feed (Unix satırsonu)
            case '\b':  // backspace
            case '\t':  // tab
            case ' ':   // boşluk
                nextChar();
                break;
            default:
                return;
        }
    }
}

// --------------------------------------------------------------------------
// isNumeric: Mevcut karakter bir rakam mı? (0-9)
// Pointer aritmetiği veya ASCII tablosu karşılaştırması yerine basit aralık
// kontrolü. Performans: O(1), branchless (modern derleyiciler optimize eder).
// --------------------------------------------------------------------------
inline bool Lexer::isNumeric() {
    char c = getchar();
    return (c >= '0' && c <= '9');
}

// --------------------------------------------------------------------------
// readNumeric: Tam bir sayı literal'ı oku.
//
// Desteklenen formatlar (öncelik sırasıyla):
//   1. İşaret: -42, +3  (baştaki isteğe bağlı işaret)
//   2. 0x/0X: Hex (0xFF, 0X1A)
//   3. 0b/0B: Binary (0b1010)
//   4. 0 ile başlayan: Octal (0777) veya Float (0.5)
//   5. Ondalık: 42, 3.14
//   6. Bilimsel: 1e10, 2.5E-3, 1.0e+5
//
// Algoritma:
//   1. İsteğe bağlı işareti oku (+ veya -)
//   2. İlk karakter '0' ise → özel durum (hex/bin/octal/float kontrolü)
//   3. Ana döngü: rakamları, hex harflerini (a-f/A-F), nokta (.), epsilon (e/E) oku
//   4. Her karakterde taban uygunluğunu kontrol et (örn: octal'da 8-9 geçersiz)
//   5. İlk karakter '0' değilse → doğrudan decimal
//
// Özel durum: "0" takip eden karakter yoksa → tek haneli sayı, base=10.
//             "0xFF" → hex, "0b10" → binary, "077" → octal, "0.5" → float.
//
// TODO: Hex float desteği (0x1.ffp10) — C99 standardı
// TODO: Sayısal ayraç: 1_000_000 — C++14/Java 7
// --------------------------------------------------------------------------
inline INumber Lexer::readNumeric() {
    INumber num;
    num.start = getLastPosition();

    // --- Adım 1: İsteğe bağlı işaret ---
    if (getchar() == '-') {
        nextChar();
        num.positive = false;
    } else if (getchar() == '+') {
        nextChar();
        num.positive = true;
    } else {
        num.positive = true;
    }

    // --- Adım 2: İlk karakter '0' ise özel format kontrolü ---
    bool nextDot = false;
    if (getchar() == '0') {
        num.token.push_back('0');
        nextChar();
        char c = getchar();
        switch (c) {
            case 'x': case 'X':       // Hex: 0xFF, 0X1A
                num.token.push_back(c);
                num.base = 16;
                nextChar();
                break;
            case 'b': case 'B':       // Binary: 0b1010
                num.token.push_back(c);
                num.base = 2;
                nextChar();
                break;
            case '.':                 // Float: 0.5, 0.0
                num.token.push_back(c);
                num.base   = 10;
                nextDot    = true;
                num.isFloat = true;
                nextChar();
                break;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7':
                // Octal: 0777 — sonraki karakter octal rakam ise devam et
                num.base = 8;
                break;
            default:
                // Sadece "0" — takip eden karakter rakam değil.
                // Hemen dön: base=10 (varsayılan).
                // BUG FIX (commit 438bc0e): Eskiden bu dalda sıradaki karakter
                // token'a ekleniyor ve base=8 yapılıyordu. Bu, "0;" durumunda
                // ';' karakterinin sayıya eklenmesine neden oluyordu.
                num.end = getLastPosition();
                return num;
        }
    } else {
        num.base = 10;
    }

    // --- Adım 3: Ana okuma döngüsü ---
    // Bu döngü, geçerli tabana uygun tüm karakterleri okur.
    // Her karakter tipi için taban uygunluğu kontrol edilir:
    //   - 0-1: tüm tabanlar
    //   - 2-7: base >= 8
    //   - 8-9: base >= 10
    //   - a-f/A-F: base >= 16
    //   - . (nokta): sadece ondalık, sadece bir kere
    //   - e/E: sadece ondalık ve hex (hex'te epsilon yok, direkt okunur)
    while (!isEnd()) {
        char c = getchar();
        switch (c) {
            case '0':
            case '1':
                num.token.push_back(c);
                break;
            case '2': case '3': case '4': case '5':
            case '6': case '7':
                if (num.base >= 8)
                    num.token.push_back(c);
                else {
                    num.end = getLastPosition();
                    return num;
                }
                break;
            case '8': case '9':
                if (num.base >= 10)
                    num.token.push_back(c);
                else {
                    num.end = getLastPosition();
                    return num;
                }
                break;
            case 'a': case 'A': case 'b': case 'B':
            case 'c': case 'C': case 'd': case 'D':
            case 'f': case 'F':
                if (num.base >= 16)
                    num.token.push_back(c);
                else {
                    num.end = getLastPosition();
                    return num;
                }
                break;
            case '.':
                // Nokta: Sadece bir kere izin verilir.
                // .5 gibi başıboş noktalı sayılar için "0." öneki eklenir.
                if (!nextDot) {
                    if (num.token.empty())
                        num.token += "0.";
                    else
                        num.token.push_back('.');
                    nextDot    = true;
                    num.isFloat = true;
                } else {
                    // İkinci nokta → sayı bitti
                    num.end = getLastPosition();
                    return num;
                }
                break;
            case 'e': case 'E':
                // Epsilon (bilimsel gösterim):
                //   - Hex tabanda: epsilon DEĞİL, hex hanesi olarak okunur.
                //   - Decimal tabanda: 1e10, 2.5E-3 formatı.
                if (num.base == 16) {
                    num.token.push_back(c);
                    break;
                }
                if (num.base == 10) {
                    num.hasEpsilon = true;
                    num.token.push_back(c);
                    nextChar();
                    c = getchar();
                    // İsteğe bağlı işaret: e+10, E-3
                    if (c == '+' || c == '-') {
                        num.token.push_back(c);
                        nextChar();
                    }
                    // Epsilon sonrası rakamları oku
                    while (!isEnd()) {
                        c = getchar();
                        if (c >= '0' && c <= '9') {
                            num.token.push_back(c);
                            nextChar();
                        } else {
                            num.end = getLastPosition();
                            return num;
                        }
                    }
                    break;
                }
                num.end = getLastPosition();
                return num;
            default:
                // Tanınmayan karakter → sayı bitti
                num.end = getLastPosition();
                return num;
        }
        nextChar();
    }
    num.end = getLastPosition();
    return num;
}

#endif // SAQUT_LEXER
