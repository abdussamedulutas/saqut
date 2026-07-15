#!/bin/bash
# Debug build
cd "$(dirname "$0")"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
