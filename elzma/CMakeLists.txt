set(ELZMA_SOURCES elzma.c util.c)
set(ELZMA_HEADERS util.h)

add_executable(elzma ${ELZMA_SOURCES} ${ELZMA_HEADERS})
add_executable(unelzma ${ELZMA_SOURCES} ${ELZMA_HEADERS})

foreach(target IN ITEMS elzma unelzma)
    target_link_libraries(${target} PRIVATE ${EASYLZMA_LINK_LIBRARY})
    target_compile_features(${target} PRIVATE c_std_99)
    easylzma_enable_warnings(${target})

    set_target_properties(
        ${target}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "$<1:${EASYLZMA_OUTPUT_BIN_DIR}>"
    )
endforeach()

install(
    TARGETS elzma unelzma
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
