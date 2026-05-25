#!/bin/bash
# ============================================================================
# saQut Compiler — Derleme Betiği
# ============================================================================
#
# AMAÇ: Projeyi tek komutla derlemek.
#
# KULLANIM:
#   ./compile.sh          → saqut binary'sini üret
#   ./saqut               → derleyiciyi çalıştır
#
# DERLEME SÜRECİ:
#   Tek bir .cpp dosyası (src/main.cpp) tüm header-only kütüphaneleri
#   include eder. Harici bağımlılık yoktur.
#
#   g++ parametreleri:
#     -Isrc              : include path (header'lar src/ altında)
#     -std=c++17         : C++17 standardı (std::variant, constexpr, vb.)
#     -Wall -Wextra      : Tüm uyarıları aç
#     -O0 -g             : Optimizasyon kapalı, debug sembolleri açık
#     -o saqut           : Çıktı binary adı
#
# GELECEK:
#   - Makefile veya CMakeLists.txt ile daha esnek build
#   - Release modu: -O2 -DNDEBUG
#   - Test modu: ayrı bir test binary'si
#
# ============================================================================

set -e  # Hata durumunda dur

echo "=== saQut Compiler Build ==="

g++ src/main.cpp \
    -Isrc \
    -std=c++17 \
    -Wall -Wextra \
    -O0 -g \
    -o saqut

echo "Derleme başarılı: ./saqut"
echo "Çalıştırmak için: ./saqut"
