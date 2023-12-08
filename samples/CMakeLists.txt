
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

add_subdirectory(hello)
add_subdirectory(mouse)
add_subdirectory(keyboard)
add_subdirectory(audio)
add_subdirectory(clock)
add_subdirectory(image)
add_subdirectory(draw)

