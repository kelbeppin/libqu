
add_executable(qu_array_test qu_array_test.c)
target_link_libraries(qu_array_test PRIVATE libqu)

add_test(NAME qu_array_test COMMAND $<TARGET_FILE:qu_array_test>)
