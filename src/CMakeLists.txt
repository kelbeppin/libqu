
#-------------------------------------------------------------------------------

add_library(${PROJECT_NAME}
    qu.c
    qu.h
    qu_array.c
    qu_audio.c
    qu_audio_null.c
    qu_core.c
    qu_fs.c
    qu_graphics.c
    qu_image_loader.c
    qu_joystick_null.c
    qu_log.c
    qu_math.c
    qu_renderer_null.c
    qu_sound_reader.c
    qu_text.c
    qu_util.c)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}>
    PRIVATE
        "${PROJECT_SOURCE_DIR}/third-party")

target_compile_definitions(${PROJECT_NAME} PRIVATE QU_BUILD)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Threads::Threads
        Freetype::Freetype
        harfbuzz::harfbuzz
        Vorbis::vorbisfile)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        PREFIX ""
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS ON
        C_VISIBILITY_PRESET hidden
        CXX_STANDARD 11
        CXX_EXTENSIONS OFF
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR})

#-------------------------------------------------------------------------------

if(QU_OS_WINDOWS)
    target_sources(${PROJECT_NAME} PRIVATE
        qu_audio_xaudio2.cpp
        qu_core_win32.c
        qu_joystick_win32.cpp
        qu_platform_win32.c)

    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE dwmapi XInput)
endif()

if(QU_OS_LINUX)
    target_sources(${PROJECT_NAME} PRIVATE
        qu_core_x11.c
        qu_joystick_linux.c
        qu_platform_posix.c)

    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_LINUX)
    target_include_directories(${PROJECT_NAME} PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GLX ${X11_LIBRARIES})
endif()

if(QU_OS_EMSCRIPTEN)
    target_sources(${PROJECT_NAME} PRIVATE qu_core_emscripten.c qu_platform_posix.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_EMSCRIPTEN)
    target_compile_options(${PROJECT_NAME} PUBLIC -sUSE_SDL)
    target_link_options(${PROJECT_NAME} PUBLIC -sINITIAL_MEMORY=536870912 -sPTHREAD_POOL_SIZE=4)
endif()

#-------------------------------------------------------------------------------

if(BUILD_SHARED_LIBS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_SHARED)
endif()

if(MATH_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${MATH_LIBRARY})
endif()

if(QU_USE_OPENAL)
    target_sources(${PROJECT_NAME} PRIVATE qu_audio_openal.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_USE_OPENAL)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenAL::OpenAL)
endif()

if(QU_USE_OPENGL)
    target_sources(${PROJECT_NAME} PRIVATE qu_renderer_gl1.c qu_renderer_gl3.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_USE_GL)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
endif()

if(QU_USE_OPENGL_ES2)
    # TODO: implement
    # target_sources(${PROJECT_NAME} PRIVATE qu_renderer_es2.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_USE_ES2)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_GLESv2_LIBRARY})
endif()

#-------------------------------------------------------------------------------

if(QU_COMPILER_MSVC)
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
            _CRT_SECURE_NO_WARNINGS)

    target_compile_options(${PROJECT_NAME}
        PRIVATE
            /W4
            /wd4100
            /wd4152
            /wd4244)

    if(QU_USE_ASAN)
        target_compile_options(${PROJECT_NAME} PRIVATE /fsanitize=address)
    endif()
endif()

if(QU_COMPILER_CLANG OR QU_COMPILER_GNU)
    target_compile_options(${PROJECT_NAME}
        PRIVATE
            -Wall
            -Wextra
            -Wno-switch
            -Wno-switch-default
            -Wno-unused-parameter
            -Wno-unused-variable)

    if(QU_USE_ASAN)
        target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
        target_link_libraries(${PROJECT_NAME} PUBLIC -fsanitize=address)
    endif()
endif()
