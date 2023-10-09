
add_executable(clock clock.c)
target_link_libraries(clock PUBLIC libqu::libqu)

install(TARGETS clock RUNTIME DESTINATION "share/libqu/samples/clock")

if(EMSCRIPTEN)
    target_link_options(clock PRIVATE
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@assets")
else()
    add_custom_command(TARGET clock POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:clock>/assets)
    install(DIRECTORY assets DESTINATION "share/libqu/samples/clock")
endif()

if(NOT ${CMAKE_VERSION} VERSION_LESS "3.21")
    add_custom_command(TARGET clock POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:clock> $<TARGET_RUNTIME_DLLS:clock>
        COMMAND_EXPAND_LISTS)
endif()
