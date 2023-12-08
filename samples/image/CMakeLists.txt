
set(TARGET "image")
set(SOURCES "image.c")

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} PUBLIC libqu::libqu)

install(TARGETS ${TARGET} RUNTIME DESTINATION "share/libqu/samples/${TARGET}")

