
add_executable(audio audio.c)
target_link_libraries(audio libqu::libqu)

add_custom_command(TARGET audio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:audio>/assets)
