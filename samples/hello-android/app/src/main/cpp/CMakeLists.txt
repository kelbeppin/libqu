
cmake_minimum_required(VERSION 3.22.1)
project("libquackandroidsample")

add_library(native-lib SHARED native-lib.c)

set(TOP_LEVEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(LIBQUACK_DIR "${TOP_LEVEL_DIR}/../..")

add_subdirectory(${LIBQUACK_DIR} ${CMAKE_CURRENT_BINARY_DIR}/libqu)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

target_link_libraries(native-lib libqu)
