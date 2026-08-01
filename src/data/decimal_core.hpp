// ============================================================================
// saQut — decimal semantik çekirdeği (TAŞINABİLİR)
// ============================================================================
//
// DİZİN:   src/data/decimal_core.hpp
// KATMAN:  data — decimal'in NE YAPTIĞININ tek tanımı
//
// AMAÇ (#224):
//   decimal, saQut'un SENTETİK bir veri tipidir: sonucunu ADR-028 belirler,
//   işlemci veya ortam değil. `2.5 + 0.1` her backend'de aynı çıkmak
//   ZORUNDADIR — VM, JIT, AOT, WASM, hatta bir JS/PHP transpiler'ında.
//
//   Bu yüzden decimal aritmetiği bir CALLHOST (dışarıya sor) DEĞİLDİR.
//   Dışarıda soracak kimse yok: cevabı dilin sözleşmesi tanımlar.
//
// NEDEN __int128 YOK:
//   Önceki gerçekleme 13 yerde `__int128` kullanıyordu. Bu GCC/Clang'e özgü
//   bir genişletmedir; WASM'de, JavaScript'te, PHP'de yoktur. Yani decimal
//   semantiğimiz C++ derleyicisinin bir özelliğine bağlıydı ve o ortamların
//   dışına çıktığımız anda ya tökezleyecek ya da sıfırdan yeniden yazılacaktı
//   — ki yeniden yazım VM'den SAPMA demektir.
//
//   Buradaki her işlem yalnızca int64/uint64 üzerinde çalışır ve taşmayı
//   ELLE denetler. Çıkan kod herhangi bir 64-bit tamsayısı olan ortama
//   mekanik olarak çevrilebilir.
//
// TEMSİL: değer = coeff × 10^exp  (coeff: int64, exp: int32)
//   Taşma sentinel'i: coeff == INT64_MAX (ADR-028; VM bunu Error'a çevirir).
//
// ============================================================================

#ifndef SAQUT_DATA_DECIMAL_CORE
#define SAQUT_DATA_DECIMAL_CORE

#include <cstdint>

namespace decimal_core {

// ── Taşma-güvenli 64-bit primitifler ─────────────────────────────────────────
//
// Hepsi "başarılı mı?" döndürür ve sonucu out-parametreye yazar. __int128'in
// yaptığı işi, taşmayı ÖNCEDEN kontrol ederek yapar.

// a + b; taşarsa false.
inline bool addOv(int64_t a, int64_t b, int64_t* out) {
    // İşaretler aynıysa taşma mümkün; farklıysa asla taşmaz.
    if (b > 0 && a > INT64_MAX - b) return false;
    if (b < 0 && a < INT64_MIN - b) return false;
    *out = a + b;
    return true;
}

// -a; INT64_MIN taşar (karşılığı yok).
inline bool negOv(int64_t a, int64_t* out) {
    if (a == INT64_MIN) return false;
    *out = -a;
    return true;
}

// a * b; taşarsa false. Bölme ile kontrol — her ortamda geçerli.
inline bool mulOv(int64_t a, int64_t b, int64_t* out) {
    if (a == 0 || b == 0) { *out = 0; return true; }
    // INT64_MIN'in mutlak değeri yok; ayrı ele alınır.
    if (a == INT64_MIN || b == INT64_MIN) return false;
    int64_t aa = a < 0 ? -a : a;
    int64_t bb = b < 0 ? -b : b;
    if (aa > INT64_MAX / bb) return false;
    *out = a * b;
    return true;
}

// a × 10^n; taşarsa false. Hizalama için (exp farkını kapatmak).
inline bool scale10(int64_t a, int n, int64_t* out) {
    int64_t r = a;
    for (int i = 0; i < n; ++i)
        if (!mulOv(r, 10, &r)) return false;
    *out = r;
    return true;
}

// ── Yüksek hassasiyetli bölme (128-bit'siz) ──────────────────────────────────
//
// div, katsayıyı 10^17 ile büyütüp bölmek zorunda — bu ara değer int64'e
// sığmaz. __int128 yerine ADIM ADIM bölme kullanılır: her adımda 10 ile
// çarpıp bölerek kalanı taşır, böylece ara değer hiçbir zaman taşmaz.
//
// Uzun bölme (kağıt üstündeki bölme) ile aynı algoritmadır.
//
// Dönüş: sonuç int64'e sığdıysa true; kaç basamak üretildiği *digitsOut'ta.
inline bool divScaled(int64_t num, int64_t den, int precision,
                      int64_t* out, int* digitsUsed) {
    if (den == 0) return false;

    // İşaretleri ayır, mutlak değerlerle çalış (INT64_MIN güvenliği için
    // unsigned kullanılır).
    bool neg = (num < 0) != (den < 0);
    uint64_t n = num < 0 ? (uint64_t)(-(num + 1)) + 1 : (uint64_t)num;
    uint64_t d = den < 0 ? (uint64_t)(-(den + 1)) + 1 : (uint64_t)den;

    uint64_t q   = n / d;
    uint64_t rem = n % d;

    int used = 0;
    const uint64_t kLimit = (uint64_t)INT64_MAX;

    // Uzun bölme: her adımda kalanı 10 ile çarpıp bir basamak daha üret.
    // Bölen büyük olduğunda (rem*10 < d) üretilen basamak 0'dır ve bu SIFIR
    // DA BİR BASAMAKTIR — q'ya 10 ile çarparak yazılmalı, atlanmamalı.
    // Aksi halde sonuç ondalık noktada kayar (ölçüldü: 0.108420... yerine
    // 0.100000011 — 15 kat hata).
    while (used < precision && rem != 0) {
        if (q > kLimit / 10) break;              // bir basamak daha sığmaz

        // rem * 10 uint64'te TAŞABİLİR: rem < d ≤ INT64_MAX olması yetmez,
        // d büyükken rem de büyük olur. (Ölçüldü: 999999999999999999 /
        // 9223372036854775806 sonucu 0.1084... yerine 0.1000000110 çıkıyordu.)
        //
        // Çözüm: rem*10'u 128-bit olarak (hi:lo) taşı ve basamağı hi/lo'dan
        // çıkarımla bul. Kalan TAM hesaplanır — yaklaşıklık yok, aksi halde
        // sonraki bütün basamaklar bozulur (denendi ve ölçüldü).
        uint64_t hi = 0, lo = rem;
        // rem * 10 = rem * 8 + rem * 2, taşan bitler hi'ye
        uint64_t t1 = lo << 3, c1 = lo >> 61;    // ×8
        uint64_t t2 = lo << 1, c2 = lo >> 63;    // ×2
        lo = t1 + t2;
        hi = c1 + c2 + (lo < t1 ? 1u : 0u);

        // (hi:lo) / d — hi < d garantili (rem < d olduğundan hi ≤ 9).
        // Basamağı ve kalanı bul: uzun bölmenin tek adımı.
        uint64_t digit = 0, r = hi;
        for (int bit = 63; bit >= 0; --bit) {
            // r = r*2 + lo'nun bit'i; taşma olmaz çünkü r < d ≤ INT64_MAX
            uint64_t nb = (lo >> bit) & 1u;
            r = (r << 1) | nb;
            if (r >= d) { r -= d; digit |= (uint64_t)1 << bit; }
        }
        if (digit > 9) digit = 9;                // güvenlik (teoride olmaz)

        uint64_t nextQ = q * 10;
        if (nextQ > kLimit - digit) break;
        q   = nextQ + digit;
        rem = r;
        ++used;
    }
    if (q > kLimit) return false;

    *out = neg ? -(int64_t)q : (int64_t)q;
    *digitsUsed = used;
    return true;
}

// ── Karşılaştırma yardımcısı ─────────────────────────────────────────────────
//
// İki değeri ortak eksponente çekmeden, taşmadan karşılaştırır.
// Dönüş: -1 (a<b), 0, 1 (a>b).
inline int compareScaled(int64_t aCoeff, int32_t aExp,
                         int64_t bCoeff, int32_t bExp) {
    if (aExp == bExp)
        return aCoeff < bCoeff ? -1 : (aCoeff > bCoeff ? 1 : 0);

    // Sıfır özel: exp ne olursa olsun 0'dır.
    if (aCoeff == 0 && bCoeff == 0) return 0;
    if (aCoeff == 0) return bCoeff > 0 ? -1 : 1;
    if (bCoeff == 0) return aCoeff > 0 ? 1 : -1;

    // İşaretler farklıysa exp'e bakmaya gerek yok.
    bool aNeg = aCoeff < 0, bNeg = bCoeff < 0;
    if (aNeg != bNeg) return aNeg ? -1 : 1;

    // Aynı işaret: küçük eksponente hizalamayı DENE. Taşarsa, taşan taraf
    // (mutlak değerce) daha büyüktür — işarete göre yön verilir.
    if (aExp > bExp) {
        int diff = aExp - bExp;
        int64_t scaled;
        if (diff > 18 || !scale10(aCoeff, diff, &scaled))
            return aNeg ? -1 : 1;   // |a| çok büyük
        return scaled < bCoeff ? -1 : (scaled > bCoeff ? 1 : 0);
    } else {
        int diff = bExp - aExp;
        int64_t scaled;
        if (diff > 18 || !scale10(bCoeff, diff, &scaled))
            return bNeg ? 1 : -1;   // |b| çok büyük
        return aCoeff < scaled ? -1 : (aCoeff > scaled ? 1 : 0);
    }
}

}  // namespace decimal_core

#endif // SAQUT_DATA_DECIMAL_CORE
