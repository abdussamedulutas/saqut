cmake_minimum_required(VERSION 3.16)
project(saqut VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR NOT CMAKE_BUILD_TYPE)
    add_compile_options(-g -O0)
    message(STATUS "Debug modu aktif")
else()
    add_compile_options(-O3)
    message(STATUS "Release modu aktif")
endif()

# Tüm kaynak dosyaları topla
file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(saqut ${SOURCES})

target_include_directories(saqut PRIVATE src)
