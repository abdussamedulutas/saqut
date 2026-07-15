#!/bin/bash
# Release build (default)
cd "$(dirname "$0")"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
