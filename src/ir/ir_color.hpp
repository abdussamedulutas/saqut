// ============================================================================
// saQut IR — TTY-aware renk katmanı (#141 SQ-090-IR-TTY-COLOR)
// ============================================================================
//
// DİZİN:   src/ir/ir_color.hpp
// KATMAN:  IR — yalnız `saqut ir` dump çıktısı (ir_function.cpp, ir_program.cpp)
//
// AMAÇ:
//   `saqut ir` renklerini yalnız stdout gerçek bir TTY ise kullanmak;
//   pipe/redirect/dosyaya giden stdout'ta ANSI CSI byte'ı bırakmamak.
//   Semantic text (opcode/operand/sıra/whitespace) TTY durumundan etkilenmez
//   — yalnız renk kod noktaları eklenir veya çıkarılır.
//
//   Kasıtlı olarak yalnız IR dump'ına özel: src/tools.hpp'deki paylaşılan
//   Color:: namespace'i (ast/symbols ve parser node log() çıktıları dahil
//   diğer bütün komutlar) DEĞİŞMEDİ — bu task'ın kapsamı yalnız `saqut ir`.
// ============================================================================

#ifndef SAQUT_IR_COLOR
#define SAQUT_IR_COLOR

#include <cstdio>
#include <unistd.h>
#include "tools.hpp"

namespace IrColor {

inline bool isTty() {
    static const bool v = isatty(fileno(stdout)) != 0;
    return v;
}

inline const char* Reset()       { return isTty() ? Color::Reset       : ""; }
inline const char* Bold()        { return isTty() ? Color::Bold        : ""; }
inline const char* SoftMavi()    { return isTty() ? Color::SoftMavi    : ""; }
inline const char* SoftYesil()   { return isTty() ? Color::SoftYesil   : ""; }
inline const char* SoftTuruncu() { return isTty() ? Color::SoftTuruncu : ""; }
inline const char* SoftMor()     { return isTty() ? Color::SoftMor     : ""; }
inline const char* SoftPembe()   { return isTty() ? Color::SoftPembe   : ""; }
inline const char* SoftTurkuaz() { return isTty() ? Color::SoftTurkuaz : ""; }
inline const char* SoftGri()     { return isTty() ? Color::SoftGri     : ""; }

}  // namespace IrColor

#endif  // SAQUT_IR_COLOR
