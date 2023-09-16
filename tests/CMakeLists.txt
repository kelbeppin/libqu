
add_executable(qu_array_test qu_array_test.c)
target_link_libraries(qu_array_test PRIVATE libqu)

add_executable(qu_fs_test qu_fs_test.c)
target_link_libraries(qu_fs_test PRIVATE libqu)

add_custom_command(TARGET qu_fs_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/data.bin
        $<TARGET_FILE_DIR:qu_fs_test>)

add_test(NAME qu_array_test COMMAND $<TARGET_FILE:qu_array_test>)
add_test(NAME qu_fs_test COMMAND $<TARGET_FILE:qu_fs_test>)
