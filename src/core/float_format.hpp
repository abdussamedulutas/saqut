// ============================================================================
// saQut Core — Float biçimlendirme tek kaynak (#114)
// ============================================================================
//
// DİZİN:   src/core/float_format.hpp
// KATMAN:  Core — backend-agnostic değer biçimlendirme sözleşmesi
//
// AMAÇ:
//   Float/double/float32 değerlerinin stdout'a ve string'e yazılış biçimi
//   ADR-038 gereği gözlemlenen davranıştır (diferansiyel test VM≢JIT eşleşmesi
//   buna bağlı). Bu biçim eskiden 4 ayrı yerde elle senkron tutuluyordu
//   (Value::toString, rt_jit_print_float, rt_jit_print_float32,
//   rt_jit_float32_to_str). Biri değişirse diğeri sessizce sapar.
//
//   Bu başlık tek sözleşmedir: tüm üretim noktaları buradan okur.
//   Biçim değişikliği tek yerde yapılır, VM ve JIT aynı anda etkilenir.
//
// SÖZLEŞME (değiştirme — ADR-038 freeze):
//   - double : setprecision(10), tam sayıysa ".0" eklenir
//   - float32: setprecision(9) (float'a daraltılarak), tam sayıysa ".0" eklenir
//   - cast   : double → default precision (cast sözleşmesi print'ten farklıdır)
//
// ============================================================================

#ifndef SAQUT_CORE_FLOAT_FORMAT
#define SAQUT_CORE_FLOAT_FORMAT

#include <iomanip>
#include <sstream>
#include <string>

// double → print biçimi (setprecision(10) + ".0" kuralı)
inline std::string formatDoublePrint(double v) {
    std::ostringstream oss;
    oss << std::setprecision(10) << v;
    std::string s = oss.str();
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos)
        s += ".0";
    return s;
}

// float32 → print biçimi (single'a daralt, setprecision(9) + ".0" kuralı)
inline std::string formatFloat32Print(double v) {
    std::ostringstream oss;
    oss << std::setprecision(9) << (float)v;
    std::string s = oss.str();
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos)
        s += ".0";
    return s;
}

// double → cast biçimi (default precision — CAST_FLOAT_TO_STR sözleşmesi)
inline std::string formatDoubleCast(double v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

// float32 → cast biçimi (single'a daralt, setprecision(9))
inline std::string formatFloat32Cast(float v) {
    std::ostringstream oss;
    oss << std::setprecision(9) << v;
    return oss.str();
}

#endif // SAQUT_CORE_FLOAT_FORMAT
