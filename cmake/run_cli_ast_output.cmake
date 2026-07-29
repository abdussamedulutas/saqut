if(NOT DEFINED BINARY)
    message(FATAL_ERROR "BINARY is required")
endif()
if(NOT DEFINED SOURCE)
    message(FATAL_ERROR "SOURCE is required")
endif()
if(NOT DEFINED EXPECTED)
    message(FATAL_ERROR "EXPECTED is required")
endif()
if(NOT DEFINED TEMP_DIR)
    message(FATAL_ERROR "TEMP_DIR is required")
endif()

file(REMOVE_RECURSE "${TEMP_DIR}")
file(MAKE_DIRECTORY "${TEMP_DIR}")

set(TEXT_OUT "${TEMP_DIR}/ast_output_text.ast.txt")
execute_process(
    COMMAND "${BINARY}" ast --output "${TEXT_OUT}" "file:${SOURCE}"
    OUTPUT_VARIABLE TEXT_STDOUT
    ERROR_VARIABLE  TEXT_STDERR
    RESULT_VARIABLE TEXT_EXIT
)

if(NOT TEXT_EXIT EQUAL 0)
    file(REMOVE_RECURSE "${TEMP_DIR}")
    message(FATAL_ERROR "text ast --output failed with exit ${TEXT_EXIT}:\n${TEXT_STDERR}")
endif()
if(NOT TEXT_STDOUT STREQUAL "")
    file(REMOVE_RECURSE "${TEMP_DIR}")
    message(FATAL_ERROR "text ast --output wrote to stdout:\n${TEXT_STDOUT}")
endif()
if(NOT TEXT_STDERR STREQUAL "")
    file(REMOVE_RECURSE "${TEMP_DIR}")
    message(FATAL_ERROR "text ast --output wrote to stderr:\n${TEXT_STDERR}")
endif()
if(NOT EXISTS "${TEXT_OUT}")
    file(REMOVE_RECURSE "${TEMP_DIR}")
    message(FATAL_ERROR "text ast --output did not create ${TEXT_OUT}")
endif()

file(SIZE "${TEXT_OUT}" TEXT_SIZE)
if(TEXT_SIZE EQUAL 0)
    file(REMOVE_RECURSE "${TEMP_DIR}")
    message(FATAL_ERROR "text ast --output created an empty file")
endif()

file(READ "${TEXT_OUT}" TEXT_CONTENT)
file(READ "${EXPECTED}" EXPECTED_CONTENT)

string(ASCII 27 ESC)
string(REGEX REPLACE "${ESC}\\[[0-9;]*m" "" TEXT_CONTENT "${TEXT_CONTENT}")
string(REGEX REPLACE "${ESC}\\[1m" "" TEXT_CONTENT "${TEXT_CONTENT}")

if(NOT TEXT_CONTENT STREQUAL EXPECTED_CONTENT)
    file(REMOVE_RECURSE "${TEMP_DIR}")
    message(FATAL_ERROR
        "text ast --output content mismatch:\n"
        "--- EXPECTED ---\n${EXPECTED_CONTENT}"
        "--- ACTUAL ---\n${TEXT_CONTENT}"
    )
endif()

set(JSON_OUT "${TEMP_DIR}/ast_output_text.json")
execute_process(
    COMMAND "${BINARY}" ast --json --output "${JSON_OUT}" "file:${SOURCE}"
    OUTPUT_VARIABLE JSON_STDOUT
    ERROR_VARIABLE  JSON_STDERR
    RESULT_VARIABLE JSON_EXIT
)

if(NOT JSON_EXIT EQUAL 0)
    file(REMOVE_RECURSE "${TEMP_DIR}")
    message(FATAL_ERROR "json ast --output failed with exit ${JSON_EXIT}:\n${JSON_STDERR}")
endif()
if(NOT JSON_STDOUT STREQUAL "")
    file(REMOVE_RECURSE "${TEMP_DIR}")
    message(FATAL_ERROR "json ast --output wrote to stdout:\n${JSON_STDOUT}")
endif()
if(NOT JSON_STDERR STREQUAL "")
    file(REMOVE_RECURSE "${TEMP_DIR}")
    message(FATAL_ERROR "json ast --output wrote to stderr:\n${JSON_STDERR}")
endif()
if(NOT EXISTS "${JSON_OUT}")
    file(REMOVE_RECURSE "${TEMP_DIR}")
    message(FATAL_ERROR "json ast --output did not create ${JSON_OUT}")
endif()

file(SIZE "${JSON_OUT}" JSON_SIZE)
if(JSON_SIZE EQUAL 0)
    file(REMOVE_RECURSE "${TEMP_DIR}")
    message(FATAL_ERROR "json ast --output created an empty file")
endif()

file(READ "${JSON_OUT}" JSON_CONTENT)
string(FIND "${JSON_CONTENT}" "\"ast\"" JSON_AST_POS)
string(FIND "${JSON_CONTENT}" "\"analysis\"" JSON_ANALYSIS_POS)
if(JSON_AST_POS EQUAL -1 OR JSON_ANALYSIS_POS EQUAL -1)
    file(REMOVE_RECURSE "${TEMP_DIR}")
    message(FATAL_ERROR "json ast --output missing ast or analysis root fields:\n${JSON_CONTENT}")
endif()

file(REMOVE_RECURSE "${TEMP_DIR}")
