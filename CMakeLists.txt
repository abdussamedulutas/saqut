cmake_minimum_required(VERSION 3.16)
project(saqut VERSION 0.1 LANGUAGES CXX C)

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

# ── MIR JIT backend (Fast-mode, #80, MIRPLAN.md) ──────────────────────────
# Yalnızca mir.c + mir-gen.c derlenir — c2mir (C->MIR derleyicisi) saQut'ta
# hiç kullanılmıyor (MIRPLAN.md §1: MIR C API üzerinden doğrudan kod
# üretiyoruz, metin/C kaynak yolu yok). mir-gen.c mimariyi kendi içinde
# preprocessor ile seçer (mir-gen-x86_64.c vb.) — ayrı arch dosyası eklemek
# gerekmez.
add_library(mir_core STATIC
    src/mir/vendor/mir.c
    src/mir/vendor/mir-gen.c
)
target_include_directories(mir_core PUBLIC src/mir/vendor)
target_compile_options(mir_core PRIVATE -std=gnu11 -Wno-abi -fsigned-char -fPIC)
include(CheckCCompilerFlag)
check_c_compiler_flag(-fno-tree-sra SAQUT_MIR_HAS_NO_TREE_SRA)
if(SAQUT_MIR_HAS_NO_TREE_SRA)
    target_compile_options(mir_core PRIVATE -fno-tree-sra)
endif()
check_c_compiler_flag(-fno-ipa-cp-clone SAQUT_MIR_HAS_NO_IPA_CP_CLONE)
if(SAQUT_MIR_HAS_NO_IPA_CP_CLONE)
    target_compile_options(mir_core PRIVATE -fno-ipa-cp-clone)
endif()

add_executable(saqut ${SOURCES})

target_include_directories(saqut PRIVATE src)
target_link_libraries(saqut PRIVATE mir_core)

# ── Testler ──────────────────────────────────────────────────────────────────
enable_testing()

set(SAQUT_BINARY "${CMAKE_BINARY_DIR}/saqut")
set(GOLDEN_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/run_golden.cmake")

# Birim testleri (tests/run.sh)
add_test(
    NAME unit_tests
    COMMAND bash "${CMAKE_SOURCE_DIR}/tests/run.sh"
)

# Golden testleri — tests/golden/**/*.sqt dosyalarını otomatik keşfet.
# Her .sqt için iki olası expected dosyası kontrol edilir:
#   BASE.expected      → saqut run  (siyah-kutu çıktı testi)
#   BASE.ir_opt.expected → saqut ir --optimized (beyaz-kutu IR testi)
file(GLOB_RECURSE ALL_GOLDEN_SQT CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/tests/golden/*.sqt"
)
foreach(SQT_FILE ${ALL_GOLDEN_SQT})
    file(RELATIVE_PATH REL "${CMAKE_SOURCE_DIR}/tests/golden" "${SQT_FILE}")
    string(REPLACE ".sqt" "" BASE "${REL}")
    string(REPLACE "/" "_" TNAME "${BASE}")

    # ADR-036 (#76): BASE.flags — --allow-fs vb. gerektiren testler için ekstra
    # CLI bayrakları (satır başına bir bayrak). Yoksa boş.
    set(FLAGS_FILE "${CMAKE_SOURCE_DIR}/tests/golden/${BASE}.flags")
    set(EXTRA_ARGS_JOINED "")
    if(EXISTS "${FLAGS_FILE}")
        file(STRINGS "${FLAGS_FILE}" FLAG_LIST)
        list(JOIN FLAG_LIST "|" EXTRA_ARGS_JOINED)
    endif()

    # Siyah-kutu: saqut run çıktısı
    set(RUN_EXPECTED "${CMAKE_SOURCE_DIR}/tests/golden/${BASE}.expected")
    if(EXISTS "${RUN_EXPECTED}")
        add_test(
            NAME "golden_${TNAME}"
            COMMAND ${CMAKE_COMMAND}
                -DBINARY=${SAQUT_BINARY}
                -DSOURCE=${SQT_FILE}
                -DEXPECTED=${RUN_EXPECTED}
                -DEXTRA_ARGS=${EXTRA_ARGS_JOINED}
                -P "${GOLDEN_SCRIPT}"
        )
    endif()

    # Beyaz-kutu: saqut ir --optimized çıktısı
    set(IR_OPT_EXPECTED "${CMAKE_SOURCE_DIR}/tests/golden/${BASE}.ir_opt.expected")
    if(EXISTS "${IR_OPT_EXPECTED}")
        add_test(
            NAME "golden_${TNAME}_ir_opt"
            COMMAND ${CMAKE_COMMAND}
                -DBINARY=${SAQUT_BINARY}
                -DSOURCE=${SQT_FILE}
                -DEXPECTED=${IR_OPT_EXPECTED}
                -DCOMMAND=ir
                -DOPTIMIZED=1
                -P "${GOLDEN_SCRIPT}"
        )
    endif()

    # Doğruluk kalkanı: saqut run --optimized çıktısı == beklenen golden değer
    # "Optimizasyon oldu" değil, "optimize IR doğru sonuç verdi" testleri.
    set(RUN_OPT_EXPECTED "${CMAKE_SOURCE_DIR}/tests/golden/${BASE}.run_opt.expected")
    if(EXISTS "${RUN_OPT_EXPECTED}")
        add_test(
            NAME "golden_${TNAME}_run_opt"
            COMMAND ${CMAKE_COMMAND}
                -DBINARY=${SAQUT_BINARY}
                -DSOURCE=${SQT_FILE}
                -DEXPECTED=${RUN_OPT_EXPECTED}
                -DOPTIMIZED=1
                -P "${GOLDEN_SCRIPT}"
        )
    endif()

    # Negatif test: .compile_error — derleme hatası beklenen golden testler.
    # Dosya içeriği stderr'de aranır (regex/düz dize).
    set(COMPILE_ERROR_EXPECTED "${CMAKE_SOURCE_DIR}/tests/golden/${BASE}.compile_error")
    if(EXISTS "${COMPILE_ERROR_EXPECTED}")
        add_test(
            NAME "golden_${TNAME}_compile_error"
            COMMAND ${CMAKE_COMMAND}
                -DBINARY=${SAQUT_BINARY}
                -DSOURCE=${SQT_FILE}
                -DEXPECTED=${COMPILE_ERROR_EXPECTED}
                -DEXTRA_ARGS=${EXTRA_ARGS_JOINED}
                -P "${CMAKE_SOURCE_DIR}/cmake/run_golden_error.cmake"
        )
    endif()

    # Negatif test: .runtime_error — çalışma zamanı hatası beklenen golden testler.
    # Aynı altyapı: exit code != 0 + stderr içeriği kontrolü.
    set(RUNTIME_ERROR_EXPECTED "${CMAKE_SOURCE_DIR}/tests/golden/${BASE}.runtime_error")
    if(EXISTS "${RUNTIME_ERROR_EXPECTED}")
        add_test(
            NAME "golden_${TNAME}_runtime_error"
            COMMAND ${CMAKE_COMMAND}
                -DBINARY=${SAQUT_BINARY}
                -DSOURCE=${SQT_FILE}
                -DEXPECTED=${RUNTIME_ERROR_EXPECTED}
                -DEXTRA_ARGS=${EXTRA_ARGS_JOINED}
                -P "${CMAKE_SOURCE_DIR}/cmake/run_golden_error.cmake"
        )
    endif()
endforeach()

# LSP golden testleri — tests/lsp/*.jsonl senaryolarını otomatik keşfet.
# Her senaryonun yanında aynı adla BASE.expected.jsonl bulunmalı.
# "wip_" önekli senaryolar bilinen-bozuk davranışı belgeler: beklenen dosya
# DOĞRU/gelecekteki davranışı tanımlar, bugünkü çıktı onunla uyuşmaz —
# bu yüzden WILL_FAIL ile işaretlenir. İlgili faz düzeltince test
# "beklenmedik şekilde geçti" diye görünür; bu, WILL_FAIL'i kaldırıp
# dosyayı wip_ önekinden kurtarma zamanı geldiğinin sinyalidir.
find_program(PYTHON3_EXECUTABLE python3)
if(PYTHON3_EXECUTABLE)
    file(GLOB ALL_LSP_SCENARIOS CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/tests/lsp/*.jsonl"
    )
    foreach(SCEN_FILE ${ALL_LSP_SCENARIOS})
        get_filename_component(SCEN_NAME "${SCEN_FILE}" NAME)
        if(SCEN_NAME MATCHES "\\.expected\\.jsonl$")
            continue()
        endif()
        string(REGEX REPLACE "\\.jsonl$" "" SCEN_BASE "${SCEN_NAME}")
        set(SCEN_EXPECTED "${CMAKE_SOURCE_DIR}/tests/lsp/${SCEN_BASE}.expected.jsonl")
        if(NOT EXISTS "${SCEN_EXPECTED}")
            continue()
        endif()

        add_test(
            NAME "lsp_${SCEN_BASE}"
            COMMAND "${PYTHON3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tests/lsp/lsp_test_driver.py"
                --binary "${SAQUT_BINARY}"
                --fixtures "${CMAKE_SOURCE_DIR}/tests/lsp/fixtures"
                --scenario "${SCEN_FILE}"
                --expected "${SCEN_EXPECTED}"
        )

        if(SCEN_BASE MATCHES "^wip_")
            set_tests_properties("lsp_${SCEN_BASE}" PROPERTIES WILL_FAIL TRUE)
        endif()
    endforeach()
# DAP golden testleri — tests/dap/*.jsonl senaryolarını otomatik keşfet.
# Format LSP testleriyle aynı. "wip_" önekli senaryolar WILL_FAIL.
file(GLOB ALL_DAP_SCENARIOS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/tests/dap/*.jsonl"
)
foreach(SCEN_FILE ${ALL_DAP_SCENARIOS})
    get_filename_component(SCEN_NAME "${SCEN_FILE}" NAME)
    if(SCEN_NAME MATCHES "\\.expected\\.jsonl$")
        continue()
    endif()
    string(REGEX REPLACE "\\.jsonl$" "" SCEN_BASE "${SCEN_NAME}")
    set(SCEN_EXPECTED "${CMAKE_SOURCE_DIR}/tests/dap/${SCEN_BASE}.expected.jsonl")
    if(NOT EXISTS "${SCEN_EXPECTED}")
        continue()
    endif()

    add_test(
        NAME "dap_${SCEN_BASE}"
        COMMAND "${PYTHON3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/dap/dap_test_driver.py"
            --binary "${SAQUT_BINARY}"
            --fixtures "${CMAKE_SOURCE_DIR}/tests/dap/fixtures"
            --scenario "${SCEN_FILE}"
            --expected "${SCEN_EXPECTED}"
    )

    if(SCEN_BASE MATCHES "^wip_")
        set_tests_properties("dap_${SCEN_BASE}" PROPERTIES WILL_FAIL TRUE)
    endif()
endforeach()

else()
    message(WARNING "python3 bulunamadı — LSP golden testleri (tests/lsp/) atlanıyor")
endif()
