
#-------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.14...3.26)

cmake_policy(SET CMP0048 NEW)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
    cmake_policy(SET CMP0135 NEW)
endif()

#-------------------------------------------------------------------------------

set(QU_VERSION_REGEX "#define QU_VERSION[ \t]+\"(.+)\"")

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/libqu/libqu.h"
    QU_VERSION_STRING REGEX ${QU_VERSION_REGEX})

string(REGEX REPLACE ${QU_VERSION_REGEX} "\\1" QU_VERSION_STRING "${QU_VERSION_STRING}")

project(libqu
    VERSION ${QU_VERSION_STRING}
    DESCRIPTION "2D game library written in C99"
    LANGUAGES C)

#-------------------------------------------------------------------------------

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(QU_USE_ASAN "Use AddressSanitizer" OFF)

#-------------------------------------------------------------------------------
# (checked in config.cmake.in)

if(BUILD_SHARED_LIBS)
    set(QU_STATIC 0)
else()
    set(QU_STATIC 1)
endif()

#-------------------------------------------------------------------------------
# Detect the target platform

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(QU_OS_WINDOWS 1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(QU_OS_LINUX 1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    set(QU_OS_EMSCRIPTEN 1)
else()
    message(FATAL_ERROR "This system is not supported by ${PROJECT_NAME}: ${CMAKE_SYSTEM_NAME}.")
endif()

#-------------------------------------------------------------------------------
# Detect the compiler

if(MSVC)
    set(QU_COMPILER_MSVC 1)

    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(QU_COMPILER_CLANG_CL 1)
    endif()
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(QU_COMPILER_CLANG 1)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
    set(QU_COMPILER_GNU 1)
else()
    message(WARNING "This compiler is not supported by ${PROJECT_NAME}: ${CMAKE_C_COMPILER_ID}.")
endif()

#-------------------------------------------------------------------------------
# Set USE flags

if(QU_OS_WINDOWS OR QU_OS_LINUX)
    set(QU_USE_OPENGL 1)
else()
    set(QU_USE_OPENGL 0)
endif()

if(QU_OS_EMSCRIPTEN)
    set(QU_USE_OPENGL_ES2 1)
else()
    set(QU_USE_OPENGL_ES2 0)
endif()

set(QU_USE_OPENAL 1)

#-------------------------------------------------------------------------------
# Global compile options, flags, etc.

set(CMAKE_POSITION_INDEPENDENT_CODE 1)

if(QU_OS_EMSCRIPTEN)
    add_compile_options(-pthread -Wno-cast-function-type-strict)
    add_link_options(-pthread)
endif()

#-------------------------------------------------------------------------------
# CPM

set(CPM_DOWNLOAD_VERSION 0.38.1)

if(CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD
        https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION})
endif()

include(${CPM_DOWNLOAD_LOCATION})

#-----------------------------------------------------------
# -lm

find_library(MATH_LIBRARY m)

#-----------------------------------------------------------
# Threads

find_package(Threads REQUIRED)

#-----------------------------------------------------------
# X11

if(QU_OS_LINUX)
    find_package(X11 REQUIRED) # X11_INCLUDE_DIR, X11_LIBRARIES
endif()

#-----------------------------------------------------------
# OpenGL & OpenGL ES

if(QU_USE_OPENGL)
    find_package(OpenGL REQUIRED)
endif()

if(QU_USE_OPENGL_ES2)
    find_library(OPENGL_GLESv2_LIBRARY GLESv2 REQUIRED)
endif()

#-----------------------------------------------------------
# OpenAL-soft

if(QU_USE_OPENAL)
    CPMAddPackage(NAME OpenAL
        VERSION 1.22.2
        OPTIONS
            "ALSOFT_UTILS OFF"
            "ALSOFT_EXAMPLES OFF"
            "ALSOFT_BUILD_IMPORT_LIB OFF"
        URL https://openal-soft.org/openal-releases/openal-soft-1.22.2.tar.bz2
        URL_HASH SHA256=ae94cc95cda76b7cc6e92e38c2531af82148e76d3d88ce996e2928a1ea7c3d20
        EXCLUDE_FROM_ALL ON)
endif()

#-----------------------------------------------------------
# FreeType

CPMAddPackage(NAME Freetype
    VERSION 2.13
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "FT_DISABLE_BZIP2 ON"
        "FT_DISABLE_HARFBUZZ ON"
        "FT_DISABLE_BROTLI ON"
    URL https://downloads.sourceforge.net/project/freetype/freetype2/2.13.0/freetype-2.13.0.tar.xz
    URL_HASH SHA256=5ee23abd047636c24b2d43c6625dcafc66661d1aca64dec9e0d05df29592624c
    EXCLUDE_FROM_ALL ON)

# No alias
if(Freetype_ADDED)
    add_library(Freetype::Freetype ALIAS freetype)
endif()

#-----------------------------------------------------------
# harfbuzz

CPMAddPackage(NAME harfbuzz
    VERSION 7.1.0
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "HB_HAVE_FREETYPE ON"
        "HB_BUILD_SUBSET OFF"
    URL https://github.com/harfbuzz/harfbuzz/releases/download/7.1.0/harfbuzz-7.1.0.tar.xz
    URL_HASH SHA256=f135a61cd464c9ed6bc9823764c188f276c3850a8dc904628de2a87966b7077b
    EXCLUDE_FROM_ALL ON)

# No alias
if(harfbuzz_ADDED)
    add_library(harfbuzz::harfbuzz ALIAS harfbuzz)
endif()

#-----------------------------------------------------------
# xiph/ogg

CPMAddPackage(NAME Ogg
    VERSION 1.3.5
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "BUILD_TESTING OFF"
    URL http://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.gz
    URL_HASH SHA256=0eb4b4b9420a0f51db142ba3f9c64b333f826532dc0f48c6410ae51f4799b664
    EXCLUDE_FROM_ALL ON)

#-----------------------------------------------------------
# xiph/vorbis

CPMAddPackage(NAME Vorbis
    VERSION 1.3.7
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
    URL http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.xz
    URL_HASH SHA256=b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b
    EXCLUDE_FROM_ALL ON)

# No aliases
if(Vorbis_ADDED)
    add_library(Vorbis::vorbis ALIAS vorbis)
    add_library(Vorbis::vorbisenc ALIAS vorbisenc)
    add_library(Vorbis::vorbisfile ALIAS vorbisfile)
endif()

#-------------------------------------------------------------------------------

add_subdirectory(src)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

#-------------------------------------------------------------------------------

include(GNUInstallDirs)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(EXPORT ${PROJECT_NAME}-targets
    NAMESPACE ${PROJECT_NAME}::
    FILE ${PROJECT_NAME}-targets.cmake
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
    VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
    COMPATIBILITY SameMajorVersion)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

# Install OpenAL shared library alongside libqu if it was built here.
if(TARGET OpenAL)
    get_target_property(OPENAL_TYPE OpenAL TYPE)

    if(${OPENAL_TYPE} STREQUAL "SHARED_LIBRARY")
        install(TARGETS OpenAL
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()

# Install static dependencies.
if(QU_STATIC)
    install(
        TARGETS
            OpenAL
            freetype
            harfbuzz
            ogg
            vorbis
            vorbisfile
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

#-------------------------------------------------------------------------------

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

include(CPack)

