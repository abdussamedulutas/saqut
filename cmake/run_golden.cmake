# run_golden.cmake — tek bir golden test çalıştırır ve çıktıyı karşılaştırır.
#
# Parametreler (cmake -D ile geçilir):
#   BINARY    — saqut binary yolu
#   SOURCE    — test .sqt dosyası (tam yol)
#   EXPECTED  — beklenen çıktı dosyası (tam yol)
#   COMMAND   — "run" (varsayılan) veya "ir"
#   OPTIMIZED — 1 ise --optimized bayrağı eklenir

if(NOT DEFINED COMMAND)
    set(COMMAND "run")
endif()

set(EXTRA_FLAGS "")
if(OPTIMIZED)
    list(APPEND EXTRA_FLAGS "--optimized")
endif()

execute_process(
    COMMAND "${BINARY}" "${COMMAND}" ${EXTRA_FLAGS} "file:${SOURCE}"
    OUTPUT_VARIABLE ACTUAL
    ERROR_VARIABLE  STDERR_OUT
    RESULT_VARIABLE EXIT_CODE
)

if(NOT EXIT_CODE EQUAL 0)
    message(FATAL_ERROR
        "saqut ${COMMAND} başarısız (exit ${EXIT_CODE}):\n${STDERR_OUT}")
endif()

file(READ "${EXPECTED}" EXPECTED_CONTENT)

if(NOT ACTUAL STREQUAL EXPECTED_CONTENT)
    message(FATAL_ERROR
        "Çıktı uyuşmuyor: ${SOURCE}\n"
        "--- BEKLENEN ---\n${EXPECTED_CONTENT}"
        "--- GERÇEK ---\n${ACTUAL}"
    )
endif()
