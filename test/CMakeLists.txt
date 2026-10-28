add_executable(
    easylzma_test
    easylzma_test.c
    simple.c
    simple.h
)

target_link_libraries(easylzma_test PRIVATE ${EASYLZMA_TEST_LIBRARY})
target_compile_features(easylzma_test PRIVATE c_std_99)
easylzma_enable_warnings(easylzma_test)

add_test(NAME easylzma.api COMMAND easylzma_test)

add_executable(easylzma_ota_stream_test ota_stream_test.c)
target_link_libraries(easylzma_ota_stream_test PRIVATE ${EASYLZMA_TEST_LIBRARY})
target_compile_features(easylzma_ota_stream_test PRIVATE c_std_99)
easylzma_enable_warnings(easylzma_ota_stream_test)

set(
    EASYLZMA_OTA_FIRMWARE
    ""
    CACHE FILEPATH
    "OTA firmware image used by the streaming decompression test"
)

if(EASYLZMA_OTA_FIRMWARE)
    if(NOT EXISTS "${EASYLZMA_OTA_FIRMWARE}")
        message(FATAL_ERROR "EASYLZMA_OTA_FIRMWARE does not exist: ${EASYLZMA_OTA_FIRMWARE}")
    endif()

    add_test(
        NAME easylzma.ota-stream
        COMMAND
            easylzma_ota_stream_test
            "${EASYLZMA_OTA_FIRMWARE}"
            "${CMAKE_CURRENT_BINARY_DIR}/ota-stream-output.bin"
    )
    set_tests_properties(easylzma.ota-stream PROPERTIES LABELS "streaming;integration")
endif()

if(TARGET elzma AND TARGET unelzma)
    add_test(
        NAME easylzma.cli-roundtrip
        COMMAND
            "${CMAKE_COMMAND}"
            "-DELZMA_EXECUTABLE=$<TARGET_FILE:elzma>"
            "-DUNELZMA_EXECUTABLE=$<TARGET_FILE:unelzma>"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-roundtrip"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cli_roundtrip.cmake"
    )
endif()
