
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

add_subdirectory(hello)
add_subdirectory(mouse)
add_subdirectory(audio)
