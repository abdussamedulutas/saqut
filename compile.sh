#!/bin/bash
# saQut Compiler — build script
# Derleme:  g++ src/main.cpp -Isrc -o saqut

set -e

echo "=== saQut Compiler Build ==="

g++ src/main.cpp \
    -Isrc \
    -std=c++17 \
    -Wall -Wextra \
    -O0 -g \
    -o saqut

echo "Derleme başarılı: ./saqut"
echo "Çalıştırmak için: ./saqut"
