# run_golden.cmake — tek bir golden test çalıştırır ve çıktıyı karşılaştırır.
#
# Kullanım (CMake add_test içinden):
#   cmake -DBINARY=<saqut yolu> -DSOURCE=<.sqt yolu> -DEXPECTED=<.expected yolu>
#         [-DCOMMAND=run] -P run_golden.cmake
#
# COMMAND varsayılanı "run". IR testleri için COMMAND=ir geçilir.

if(NOT DEFINED COMMAND)
    set(COMMAND "run")
endif()

execute_process(
    COMMAND "${BINARY}" "${COMMAND}" "file:${SOURCE}"
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
