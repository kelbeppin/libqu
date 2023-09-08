
add_executable(hello hello.c)
target_link_libraries(hello PUBLIC libqu::libqu)

if(EMSCRIPTEN)
    target_link_options(hello PRIVATE
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@assets")
else()
    add_custom_command(TARGET hello POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:hello>/assets)
endif()
