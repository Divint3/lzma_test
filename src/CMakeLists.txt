set(
    EASYLZMA_SOURCES
    common_internal.c
    compress.c
    decompress.c
    lzip_header.c
    lzma_header.c
    pavlov/7zBuf.c
    pavlov/7zBuf2.c
    pavlov/7zCrc.c
    pavlov/7zFile.c
    pavlov/7zStream.c
    pavlov/Alloc.c
    pavlov/Bcj2.c
    pavlov/Bra.c
    pavlov/Bra86.c
    pavlov/BraIA64.c
    pavlov/LzFind.c
    pavlov/LzmaDec.c
    pavlov/LzmaEnc.c
    pavlov/LzmaLib.c
)

set(
    EASYLZMA_INTERNAL_HEADERS
    common_internal.h
    lzip_header.h
    lzma_header.h
)

function(easylzma_configure_library target)
    target_sources(
        ${target}
        PRIVATE
        ${EASYLZMA_SOURCES}
        ${EASYLZMA_INTERNAL_HEADERS}
    )

    target_compile_features(${target} PRIVATE c_std_99)
    target_include_directories(
        ${target}
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

    if(WIN32)
        target_compile_definitions(${target} PUBLIC WIN32)
    endif()

    set_target_properties(
        ${target}
        PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    easylzma_enable_warnings(${target})
endfunction()

function(easylzma_copy_public_headers target)
    foreach(header IN LISTS EASYLZMA_PUBLIC_HEADERS)
        add_custom_command(
            TARGET ${target}
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "${EASYLZMA_OUTPUT_INCLUDE_DIR}/easylzma"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${CMAKE_CURRENT_SOURCE_DIR}/easylzma/${header}"
                    "${EASYLZMA_OUTPUT_INCLUDE_DIR}/easylzma/${header}"
            VERBATIM
        )
    endforeach()
endfunction()

if(EASYLZMA_BUILD_SHARED)
    add_library(easylzma SHARED)
    easylzma_configure_library(easylzma)
    add_library(easylzma::easylzma ALIAS easylzma)
    easylzma_copy_public_headers(easylzma)

    target_compile_definitions(easylzma PRIVATE EASYLZMA_SHARED EASYLZMA_BUILD)
    target_compile_definitions(easylzma INTERFACE $<$<PLATFORM_ID:Windows>:EASYLZMA_SHARED>)

    set_target_properties(
        easylzma
        PROPERTIES
        VERSION "${PROJECT_VERSION}"
        SOVERSION "${PROJECT_VERSION_MAJOR}"
        ARCHIVE_OUTPUT_DIRECTORY "$<1:${EASYLZMA_OUTPUT_LIB_DIR}>"
        LIBRARY_OUTPUT_DIRECTORY "$<1:${EASYLZMA_OUTPUT_LIB_DIR}>"
        RUNTIME_OUTPUT_DIRECTORY "$<1:${EASYLZMA_OUTPUT_LIB_DIR}>"
    )

    install(
        TARGETS easylzma
        EXPORT easylzmaTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
endif()

if(EASYLZMA_BUILD_STATIC)
    add_library(easylzma_s STATIC)
    easylzma_configure_library(easylzma_s)
    add_library(easylzma::easylzma_s ALIAS easylzma_s)
    easylzma_copy_public_headers(easylzma_s)

    set_target_properties(
        easylzma_s
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "$<1:${EASYLZMA_OUTPUT_LIB_DIR}>"
    )

    install(
        TARGETS easylzma_s
        EXPORT easylzmaTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
endif()
