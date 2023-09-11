
add_executable(mouse mouse.c)
target_link_libraries(mouse PUBLIC libqu::libqu)

install(TARGETS mouse RUNTIME DESTINATION "share/libqu/samples/mouse")
