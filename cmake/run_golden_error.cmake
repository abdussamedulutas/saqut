# run_golden_error.cmake — derleme hatası BEKLEYEN golden test.
#
# Parametreler (cmake -D ile geçilir):
#   BINARY   — saqut binary yolu
#   SOURCE   — test .sqt dosyası (tam yol)
#   EXPECTED — beklenen hata içeriği (.compile_error dosyası); stderr bu
#              metni içermeli (regex veya düz dize olarak eşleşir)

execute_process(
    COMMAND "${BINARY}" run "file:${SOURCE}"
    OUTPUT_VARIABLE STDOUT_OUT
    ERROR_VARIABLE  STDERR_OUT
    RESULT_VARIABLE EXIT_CODE
)

if(EXIT_CODE EQUAL 0)
    message(FATAL_ERROR
        "Derleme hatası bekleniyordu ama program başarıyla çalıştı: ${SOURCE}\n"
        "Çıktı: ${STDOUT_OUT}")
endif()

file(READ "${EXPECTED}" EXPECTED_CONTENT)
string(STRIP "${EXPECTED_CONTENT}" EXPECTED_CONTENT)

if(NOT STDERR_OUT MATCHES "${EXPECTED_CONTENT}")
    message(FATAL_ERROR
        "Beklenen '${EXPECTED_CONTENT}' mesajı stderr'de bulunamadı: ${SOURCE}\n"
        "Stderr: ${STDERR_OUT}")
endif()
