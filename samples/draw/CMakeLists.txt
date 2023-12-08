
set(TARGET "draw")
set(SOURCES "draw.c")

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} PUBLIC libqu::libqu)

install(TARGETS ${TARGET} RUNTIME DESTINATION "share/libqu/samples/${TARGET}")

