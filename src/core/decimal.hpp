// ============================================================================
// saQut — DecimalValue (ADR-028)
//
// Ondalık hassasiyet için coefficient×10^exponent temsili.
// Bağımlılık sıfır, taşınabilir (GCC/Clang/MSVC).
//   value = coefficient × 10^exponent
//   ~18 anlamlı ondalık basamak (int64_t aralığı)
//
// 0.1 → {coeff=1, exp=-1}
// 0.2 → {coeff=2, exp=-1}
// 0.1+0.2 → {coeff=3, exp=-1} == 0.3 ✓
// ============================================================================

#ifndef SAQUT_CORE_DECIMAL
#define SAQUT_CORE_DECIMAL

#include <cstdint>

#include "data/decimal_core.hpp"
#include <string>
#include <stdexcept>
#include <cmath>
#include <sstream>
#include <iomanip>

struct DecimalValue {
    int64_t coeff = 0;
    int32_t exp   = 0;

    // ── Factory'ler ──────────────────────────────────────────────────────────

    static DecimalValue zero() { return {0, 0}; }

    static DecimalValue fromInt(int64_t i) {
        DecimalValue d{i, 0};
        d.normalize();
        return d;
    }

    // "1.1", "19.99", "-0.5", "100" gibi string → decimal (kayıpsız)
    static DecimalValue fromString(const std::string& s) {
        if (s.empty()) return zero();

        bool negative = (s[0] == '-');
        size_t start = negative ? 1 : 0;
        std::string abs_s = s.substr(start);

        // Scientific notation (1e10, 1.5e-3) — float'tan gelen dönüşümlerde
        size_t epos = abs_s.find_first_of("eE");
        int32_t extraExp = 0;
        if (epos != std::string::npos) {
            extraExp = std::stoi(abs_s.substr(epos + 1));
            abs_s = abs_s.substr(0, epos);
        }

        size_t dot = abs_s.find('.');
        int32_t e = 0;
        std::string digits;
        if (dot == std::string::npos) {
            digits = abs_s;
            e = 0;
        } else {
            digits = abs_s.substr(0, dot) + abs_s.substr(dot + 1);
            e = -(int32_t)(abs_s.size() - dot - 1);
        }
        if (digits.empty()) return zero();

        // int64 sınırını aşan digit sayısına kırp (18 basamak)
        while (digits.size() > 18) {
            digits.pop_back();
            e++;
        }

        int64_t c = std::stoll(digits);
        if (negative) c = -c;

        DecimalValue d{c, e + extraExp};
        d.normalize();
        return d;
    }

    // double → decimal (string üzerinden geçer — binary hassasiyeti kabul)
    static DecimalValue fromDouble(double v) {
        if (v == 0.0) return zero();
        std::ostringstream oss;
        oss << std::setprecision(15) << v;
        return fromString(oss.str());
    }

    // ── Normalize: sondaki sıfırları kırp ───────────────────────────────────

    void normalize() {
        if (coeff == 0) { exp = 0; return; }
        while (coeff % 10 == 0) { coeff /= 10; exp++; }
    }

    // ── Karşılaştırma (eşit olup olmadığını bulmak için normalize kullanır) ─

    // -1 (a<b), 0 (a==b), 1 (a>b)
    static int compare(const DecimalValue& a, const DecimalValue& b) {
        // #224: hizalama taşma-güvenli 64-bit ile (decimal_core) — __int128 yok.
        return decimal_core::compareScaled(a.coeff, a.exp, b.coeff, b.exp);
    }

    bool operator==(const DecimalValue& o) const { return compare(*this, o) == 0; }
    bool operator!=(const DecimalValue& o) const { return compare(*this, o) != 0; }
    bool operator< (const DecimalValue& o) const { return compare(*this, o) <  0; }
    bool operator<=(const DecimalValue& o) const { return compare(*this, o) <= 0; }
    bool operator> (const DecimalValue& o) const { return compare(*this, o) >  0; }
    bool operator>=(const DecimalValue& o) const { return compare(*this, o) >= 0; }

    // ── Aritmetik ────────────────────────────────────────────────────────────

    // Taşma sentinel: coeff = INT64_MAX ile işaretlenir
    bool isOverflow() const { return coeff == INT64_MAX; }
    static DecimalValue overflow() { return {INT64_MAX, 0}; }

    static DecimalValue add(const DecimalValue& a, const DecimalValue& b) {
        // #224: __int128 yerine taşma-denetimli 64-bit. Davranış birebir korunur.
        if (a.exp == b.exp) {
            int64_t sum;
            if (!decimal_core::addOv(a.coeff, b.coeff, &sum)) return overflow();
            DecimalValue r{sum, a.exp};
            r.normalize();
            return r;
        }
        // Hizala: BÜYÜK eksponentli tarafı küçüğe çek (hassasiyet kaybı olmaz).
        if (a.exp > b.exp) {
            int32_t diff = a.exp - b.exp;
            // 18 basamaktan fazla fark: küçük taraf yutulur (eski davranış).
            if (diff > 18) { DecimalValue r = a; r.normalize(); return r; }
            int64_t ac;
            if (!decimal_core::scale10(a.coeff, diff, &ac)) return overflow();
            int64_t sum;
            if (!decimal_core::addOv(ac, b.coeff, &sum)) return overflow();
            DecimalValue r{sum, b.exp};
            r.normalize();
            return r;
        } else {
            int32_t diff = b.exp - a.exp;
            if (diff > 18) { DecimalValue r = b; r.normalize(); return r; }
            int64_t bc;
            if (!decimal_core::scale10(b.coeff, diff, &bc)) return overflow();
            int64_t sum;
            if (!decimal_core::addOv(a.coeff, bc, &sum)) return overflow();
            DecimalValue r{sum, a.exp};
            r.normalize();
            return r;
        }
    }

    static DecimalValue sub(const DecimalValue& a, const DecimalValue& b) {
        // #224: -INT64_MIN taşar (karşılığı yok) — eskiden UB idi.
        int64_t nb;
        if (!decimal_core::negOv(b.coeff, &nb)) return overflow();
        return add(a, DecimalValue{nb, b.exp});
    }

    static DecimalValue mul(const DecimalValue& a, const DecimalValue& b) {
        // #224: taşma bölme ile önceden denetlenir — __int128 gerekmez.
        int64_t c;
        if (!decimal_core::mulOv(a.coeff, b.coeff, &c)) return overflow();
        DecimalValue r{c, a.exp + b.exp};
        r.normalize();
        return r;
    }

    // bölme: en fazla 18 anlamlı basamak
    static DecimalValue div(const DecimalValue& a, const DecimalValue& b) {
        if (b.coeff == 0) return overflow(); // sıfıra bölme → VM Error'a çevirir
        // #224: eskiden katsayı 10^17 ile büyütülüp __int128'te bölünüyordu.
        // Şimdi uzun bölme: her adımda bir basamak üretilir, ara değer hiçbir
        // zaman 64-bit'i aşmaz. Üretilen basamak sayısı eksponente yansır.
        const int PREC = 17;
        int64_t q;
        int     digits = 0;
        if (!decimal_core::divScaled(a.coeff, b.coeff, PREC, &q, &digits))
            return overflow();
        DecimalValue r{q, a.exp - b.exp - digits};
        r.normalize();
        return r;
    }

    static DecimalValue mod(const DecimalValue& a, const DecimalValue& b) {
        if (b.coeff == 0) return overflow();
        // a % b = a - trunc(a/b)*b
        DecimalValue q = div(a, b);
        if (q.isOverflow()) return overflow();
        // Kesir kısmını kes (sıfıra doğru kırp)
        q = truncate(q);
        DecimalValue prod = mul(q, b);
        if (prod.isOverflow()) return overflow();
        return sub(a, prod);
    }

    static DecimalValue neg(const DecimalValue& a) {
        return {-a.coeff, a.exp};
    }

    // Sıfıra doğru kırpma (cast için de kullanılır)
    static DecimalValue truncate(const DecimalValue& a) {
        if (a.exp >= 0) return a; // zaten tam sayı
        int32_t absExp = -a.exp;
        if (absExp > 18) return zero(); // |a| < 1
        int64_t divisor = 1;
        for (int i = 0; i < absExp; i++) divisor *= 10;
        return {a.coeff / divisor, 0};
    }

    // Tam sayıya dönüştür (taşma kontrolüyle)
    // INT64_MAX dönmesi = taşma
    int64_t toInt64() const {
        DecimalValue t = truncate(*this);
        return t.coeff; // exp=0 → coeff değerin kendisi
    }

    double toDouble() const {
        return (double)coeff * std::pow(10.0, (double)exp);
    }

    // ── Görüntüleme ──────────────────────────────────────────────────────────

    std::string toString() const {
        if (coeff == 0) return "0";

        bool negative = (coeff < 0);
        // abs string (int64_t MIN özel durumu için unsigned kullan)
        uint64_t absCoeff = negative
            ? (coeff == INT64_MIN ? (uint64_t)INT64_MAX + 1 : (uint64_t)(-coeff))
            : (uint64_t)coeff;
        std::string digits = std::to_string(absCoeff);

        std::string result;
        if (exp >= 0) {
            result = digits;
            for (int i = 0; i < exp; i++) result += '0';
        } else {
            int absExp = -exp;
            if ((int)digits.size() <= absExp) {
                std::string zeros(absExp - (int)digits.size(), '0');
                result = "0." + zeros + digits;
            } else {
                int intLen = (int)digits.size() - absExp;
                result = digits.substr(0, intLen) + "." + digits.substr(intLen);
            }
        }
        return negative ? "-" + result : result;
    }

    bool isTruthy() const { return coeff != 0; }
};

#endif // SAQUT_CORE_DECIMAL
