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

# ── Testler ──────────────────────────────────────────────────────────────────
enable_testing()

set(SAQUT_BINARY "${CMAKE_BINARY_DIR}/saqut")
set(GOLDEN_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/run_golden.cmake")

# Birim testleri (tests/run.sh)
add_test(
    NAME unit_tests
    COMMAND bash "${CMAKE_SOURCE_DIR}/tests/run.sh"
)

# Golden testleri — tests/golden/**/*.sqt + .expected çiftlerini otomatik keşfet
file(GLOB_RECURSE ALL_GOLDEN_SQT CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/tests/golden/*.sqt"
)
foreach(SQT_FILE ${ALL_GOLDEN_SQT})
    # Göreli yoldan test adı üret: fibonacci/fib.sqt → golden_fibonacci_fib
    file(RELATIVE_PATH REL "${CMAKE_SOURCE_DIR}/tests/golden" "${SQT_FILE}")
    string(REPLACE ".sqt" "" BASE "${REL}")
    string(REPLACE "/" "_" TNAME "${BASE}")
    set(EXPECTED_FILE "${CMAKE_SOURCE_DIR}/tests/golden/${BASE}.expected")

    if(EXISTS "${EXPECTED_FILE}")
        add_test(
            NAME "golden_${TNAME}"
            COMMAND ${CMAKE_COMMAND}
                -DBINARY=${SAQUT_BINARY}
                -DSOURCE=${SQT_FILE}
                -DEXPECTED=${EXPECTED_FILE}
                -P "${GOLDEN_SCRIPT}"
        )
    endif()
endforeach()
