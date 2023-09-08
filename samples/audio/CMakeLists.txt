
add_executable(audio audio.c)
target_link_libraries(audio libqu::libqu)

install(TARGETS audio RUNTIME DESTINATION "share/libqu/samples/audio")

if(EMSCRIPTEN)
    target_link_options(audio PRIVATE
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@assets")
else()
    add_custom_command(TARGET audio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:audio>/assets)
    install(DIRECTORY assets DESTINATION "share/libqu/samples/audio")
endif()
