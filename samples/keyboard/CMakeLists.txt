
add_executable(keyboard keyboard.c)
target_link_libraries(keyboard PUBLIC libqu::libqu)

install(TARGETS keyboard RUNTIME DESTINATION "share/libqu/samples/keyboard")

if(EMSCRIPTEN)
    target_link_options(keyboard PRIVATE
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@assets")
else()
    add_custom_command(TARGET keyboard POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:keyboard>/assets)
    install(DIRECTORY assets DESTINATION "share/libqu/samples/keyboard")
endif()
