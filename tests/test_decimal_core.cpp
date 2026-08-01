// ============================================================================
// #224: decimal semantik çekirdeği — taşınabilirlik ve doğruluk sözleşmesi
//
// decimal saQut'un SENTETİK bir tipidir: sonucunu ADR-028 belirler, ortam
// değil. Bu yüzden aritmetiği C++'a özgü hiçbir şeye (özellikle __int128)
// dayanamaz — WASM'de, JS/PHP transpiler'ında aynı sonucu vermek zorundadır.
//
// Hata SINIFI: taşma-güvenli 64-bit gerçekleme, __int128'li olandan SESSİZCE
// farklı sonuç verirse VM≡JIT≡AOT sözleşmesi kırılır ve bu ancak kullanıcının
// para hesabı tutmadığında fark edilir.
// ============================================================================

#include <cassert>
#include <cstdio>

#include "core/decimal.hpp"
#include "data/decimal_core.hpp"

using decimal_core::addOv;
using decimal_core::mulOv;
using decimal_core::negOv;
using decimal_core::scale10;
using decimal_core::divScaled;

static DecimalValue D(int64_t c, int32_t e) { return DecimalValue{c, e}; }

int main() {
    // ── 1) Taşma-güvenli primitifler ─────────────────────────────────────────
    {
        int64_t r;
        assert(addOv(1, 2, &r) && r == 3);
        assert(addOv(INT64_MAX, 0, &r) && r == INT64_MAX);
        assert(!addOv(INT64_MAX, 1, &r));            // taşma yakalanmalı
        assert(!addOv(INT64_MIN, -1, &r));
        assert(addOv(INT64_MAX, INT64_MIN, &r));     // işaretler farklı → güvenli

        assert(mulOv(3, 4, &r) && r == 12);
        assert(mulOv(0, INT64_MAX, &r) && r == 0);
        assert(!mulOv(INT64_MAX, 2, &r));
        assert(!mulOv(INT64_MIN, -1, &r));           // |INT64_MIN| temsil edilemez
        assert(mulOv(-3, 4, &r) && r == -12);

        assert(negOv(5, &r) && r == -5);
        assert(!negOv(INT64_MIN, &r));               // karşılığı yok

        assert(scale10(7, 3, &r) && r == 7000);
        assert(!scale10(INT64_MAX, 1, &r));
    }

    // ── 2) Uzun bölme: hassasiyet kaybı OLMAMALI ─────────────────────────────
    //
    // Bu vaka geliştirme sırasında gerçek bir hata yakaladı: rem*10 uint64'te
    // sessizce sarıyordu ve sonuç 0.10842021724855044 yerine 0.1000000110
    // çıkıyordu (15 kat hata). 128-bit ara değer elle taşınarak düzeltildi.
    {
        int64_t q; int digits;
        assert(divScaled(999999999999999999LL, 9223372036854775806LL, 17, &q, &digits));
        assert(q == 10842021724855044LL && digits == 17);
    }
    {
        int64_t q; int digits;
        assert(divScaled(1, 3, 17, &q, &digits));
        assert(q == 33333333333333333LL && digits == 17);   // 1/3
    }
    {
        int64_t q; int digits;
        assert(divScaled(10, 2, 17, &q, &digits));
        assert(q == 5 && digits == 0);                       // tam bölünme
    }
    {
        int64_t q; int digits;
        assert(!divScaled(1, 0, 17, &q, &digits));           // sıfıra bölme
    }

    // ── 3) ADR-028 çekirdek davranışı ────────────────────────────────────────
    {
        // 0.1 + 0.2 == 0.3 — decimal'in var olma sebebi
        DecimalValue s = DecimalValue::add(DecimalValue::fromString("0.1"),
                                           DecimalValue::fromString("0.2"));
        assert(s == DecimalValue::fromString("0.3"));
        assert(s.toString() == "0.3");
    }
    {
        // Para hesabı: 19.99 * 3
        DecimalValue p = DecimalValue::mul(DecimalValue::fromString("19.99"),
                                           DecimalValue::fromInt(3));
        assert(p.toString() == "59.97");
    }
    {
        // Sıfır her eksponentte sıfırdır ve normalize edilir.
        assert(D(0, 5) == D(0, -5));
        assert(DecimalValue::compare(D(0, 0), D(0, 99)) == 0);
    }

    // ── 4) DÜZELTİLEN HATA: sıfır ile çok küçük sayı karşılaştırması ─────────
    //
    // Eski compare() `diff > 18` dalında sıfırı özel ele almıyor ve yalnızca
    // a.coeff'in işaretine bakıyordu: 0 > 0.0000000000000000001 diyordu.
    {
        DecimalValue zero = D(0, 0);
        DecimalValue tiny = D(1, -19);            // 1e-19
        assert(DecimalValue::compare(zero, tiny) == -1);   // 0 < 1e-19
        assert(DecimalValue::compare(tiny, zero) ==  1);
        DecimalValue negTiny = D(-1, -19);
        assert(DecimalValue::compare(zero, negTiny) ==  1); // 0 > -1e-19
        assert(DecimalValue::compare(negTiny, zero) == -1);
    }

    // ── 5) DÜZELTİLEN HATA: temsil edilebilir bölme sonucu OVF sayılıyordu ───
    //
    // Eski div() katsayıyı önce 10^17 ile büyütüp __int128'te bölüyordu; büyük
    // bölünenlerde ara değer taşıyor ve sonuç temsil edilebilir olduğu halde
    // taşma sentinel'i dönüyordu. 999999999999999999 / 0.01 = 9.99999...E+19
    // decimal olarak pekâlâ temsil edilebilir.
    {
        DecimalValue r = DecimalValue::div(D(999999999999999999LL, 0), D(1, -2));
        assert(!r.isOverflow());
        assert(r.coeff == 999999999999999999LL && r.exp == 2);
    }
    {
        DecimalValue r = DecimalValue::div(D(999999999999999999LL, 0), D(1, -18));
        assert(!r.isOverflow());
    }

    // ── 6) Gerçek taşmalar HÂLÂ yakalanmalı ──────────────────────────────────
    //
    // Düzeltmeler taşma denetimini gevşetmemeli.
    {
        assert(DecimalValue::add(D(INT64_MAX - 1, 0), D(INT64_MAX - 1, 0)).isOverflow());
        assert(DecimalValue::mul(D(INT64_MAX - 1, 0), D(INT64_MAX - 1, 0)).isOverflow());
        assert(DecimalValue::div(D(1, 0), D(0, 0)).isOverflow());       // sıfıra bölme
        // -INT64_MIN temsil edilemez → sub taşma vermeli (eskiden UB idi)
        assert(DecimalValue::sub(D(0, 0), D(INT64_MIN, 0)).isOverflow());
    }

    // ── 7) Taşınabilirlik: çekirdek C++'a özgü tip kullanmamalı ──────────────
    //
    // Bu bir çalışma zamanı iddiası değil, kaynak sözleşmesi: decimal_core.hpp
    // yalnızca int64/uint64 üzerinde çalışır. Denetimi tests/run.sh yapar
    // (grep __int128). Burada yalnızca sonuçların doğruluğu sabitlenir.

    std::printf("test_decimal_core: TUM TESTLER GECTI\n");
    return 0;
}
