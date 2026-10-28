if(NOT DEFINED ELZMA_EXECUTABLE OR NOT DEFINED UNELZMA_EXECUTABLE OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "ELZMA_EXECUTABLE, UNELZMA_EXECUTABLE, and WORK_DIR are required")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(input_file "${WORK_DIR}/sample.txt")
set(expected_file "${WORK_DIR}/expected.txt")
set(compressed_file "${input_file}.lzma")
set(test_data "Easylzma command-line round-trip test.\n")

file(WRITE "${input_file}" "${test_data}")
file(WRITE "${expected_file}" "${test_data}")

execute_process(
    COMMAND "${ELZMA_EXECUTABLE}" --keep "${input_file}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "elzma failed (${result}): ${output}${error}")
endif()

file(REMOVE "${input_file}")

execute_process(
    COMMAND "${UNELZMA_EXECUTABLE}" --keep "${compressed_file}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "unelzma failed (${result}): ${output}${error}")
endif()

file(READ "${expected_file}" expected)
file(READ "${input_file}" actual)

if(NOT expected STREQUAL actual)
    message(FATAL_ERROR "decompressed data does not match the original input")
endif()
