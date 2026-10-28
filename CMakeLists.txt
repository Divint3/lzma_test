cmake_minimum_required(VERSION 3.20...3.31)

project(
    easylzma
    VERSION 0.0.8
    DESCRIPTION "A small C wrapper around Igor Pavlov's LZMA implementation"
    LANGUAGES C
)

include(CTest)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(EASYLZMA_BUILD_SHARED "Build the easylzma shared library" ON)
option(EASYLZMA_BUILD_STATIC "Build the easylzma static library" ON)
option(EASYLZMA_BUILD_CLI "Build the elzma and unelzma command-line tools" ON)

if(NOT EASYLZMA_BUILD_SHARED AND NOT EASYLZMA_BUILD_STATIC)
    message(FATAL_ERROR "At least one of EASYLZMA_BUILD_SHARED or EASYLZMA_BUILD_STATIC must be enabled")
endif()

set(EASYLZMA_DIST_NAME "easylzma-${PROJECT_VERSION}")
set(EASYLZMA_OUTPUT_ROOT "${CMAKE_BINARY_DIR}/${EASYLZMA_DIST_NAME}")
set(EASYLZMA_OUTPUT_BIN_DIR "${EASYLZMA_OUTPUT_ROOT}/bin")
set(EASYLZMA_OUTPUT_INCLUDE_DIR "${EASYLZMA_OUTPUT_ROOT}/include")
set(EASYLZMA_OUTPUT_LIB_DIR "${EASYLZMA_OUTPUT_ROOT}/lib")

set(
    EASYLZMA_PUBLIC_HEADERS
    common.h
    compress.h
    decompress.h
)

if(EASYLZMA_BUILD_STATIC)
    set(EASYLZMA_LINK_LIBRARY easylzma_s)
else()
    set(EASYLZMA_LINK_LIBRARY easylzma)
endif()

if(EASYLZMA_BUILD_SHARED)
    set(EASYLZMA_TEST_LIBRARY easylzma)
else()
    set(EASYLZMA_TEST_LIBRARY easylzma_s)
endif()

function(easylzma_enable_warnings target)
    if(MSVC)
        target_compile_options(
            ${target}
            PRIVATE
            /W3
            /wd4130
            /wd4100
            /wd4255
            /wd4668
            /wd4711
            /wd4820
            /wd4996
        )
    else()
        target_compile_options(${target} PRIVATE -Wall)
    endif()
endfunction()

add_subdirectory(src)

if(EASYLZMA_BUILD_CLI)
    add_subdirectory(elzma)
endif()

if(BUILD_TESTING)
    add_subdirectory(test)
endif()

set(EASYLZMA_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/easylzma")

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/easylzmaConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${EASYLZMA_DIST_NAME}/lib/cmake/easylzma/easylzmaConfig.cmake"
    INSTALL_DESTINATION "${EASYLZMA_CMAKE_INSTALL_DIR}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${EASYLZMA_DIST_NAME}/lib/cmake/easylzma/easylzmaConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${EASYLZMA_DIST_NAME}/lib/cmake/easylzma/easylzmaConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${EASYLZMA_DIST_NAME}/lib/cmake/easylzma/easylzmaConfigVersion.cmake"
    DESTINATION "${EASYLZMA_CMAKE_INSTALL_DIR}"
)

install(
    EXPORT easylzmaTargets
    FILE easylzmaTargets.cmake
    NAMESPACE easylzma::
    DESTINATION "${EASYLZMA_CMAKE_INSTALL_DIR}"
)

install(
    DIRECTORY src/easylzma
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING
    PATTERN "*.h"
)

install(
    FILES README ChangeLog INSTALL
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)

message(STATUS "easylzma build artifacts will be written to ${EASYLZMA_OUTPUT_ROOT}")
