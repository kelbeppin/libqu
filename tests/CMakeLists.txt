
set(EXECUTABLE_LIST "01-hello" "02-audio")

foreach(EXECUTABLE ${EXECUTABLE_LIST})
    add_executable(${EXECUTABLE} ${EXECUTABLE}.c)
    target_link_libraries(${EXECUTABLE} libqu::libqu)

    add_custom_command(TARGET ${EXECUTABLE} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${EXECUTABLE}>/assets)

    # if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
    #    add_custom_command(TARGET ${EXECUTABLE} POST_BUILD
    #        COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #            $<TARGET_RUNTIME_DLLS:${EXECUTABLE}>
    #            $<TARGET_FILE_DIR:${EXECUTABLE}>
    #        COMMAND_EXPAND_LISTS)
    # endif()
endforeach()
