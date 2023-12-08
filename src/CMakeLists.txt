
#-------------------------------------------------------------------------------

add_library(${PROJECT_NAME}
    qu.c
    qu.h
    qu_audio.c
    qu_audio_null.c
    qu_core.c
    qu_fs.c
    qu_graphics.c
    qu_image.c
    qu_joystick_null.c
    qu_log.c
    qu_math.c
    qu_renderer_null.c
    qu_resource_loader.c
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

if(BUILD_SHARED_LIBS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_SHARED)
endif()

if(MATH_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${MATH_LIBRARY})
endif()

if(QU_WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
        qu_audio_xaudio2.cpp
        qu_core_win32.c
        qu_joystick_win32.cpp
        qu_platform_win32.c)

    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE dwmapi XInput)
endif()

if(QU_LINUX)
    target_sources(${PROJECT_NAME} PRIVATE qu_joystick_linux.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_LINUX)
endif()

if(QU_FREEBSD)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_FREEBSD)
endif()

if(QU_EMSCRIPTEN)
    target_sources(${PROJECT_NAME} PRIVATE qu_core_emscripten.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_EMSCRIPTEN)
    target_compile_options(${PROJECT_NAME} PUBLIC -sUSE_SDL)
    target_link_options(${PROJECT_NAME} PUBLIC
        -sINITIAL_MEMORY=67108864
        -sPTHREAD_POOL_SIZE=4
        -sOFFSCREEN_FRAMEBUFFER
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/emscripten-shell.html")
endif()

if(QU_ANDROID)
    target_sources(${PROJECT_NAME} PRIVATE qu_audio_sles.c qu_core_android.c qu_np_android.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_ANDROID)

    find_library(ANDROID_LIBRARY android)
    find_library(LOG_LIBRARY log)
    find_library(EGL_LIBRARY EGL)
    find_library(OPENSLES_LIBRARY OpenSLES)

    target_link_libraries(${PROJECT_NAME} PUBLIC
        ${ANDROID_LIBRARY}
        ${LOG_LIBRARY}
        ${EGL_LIBRARY}
        ${OPENSLES_LIBRARY})
endif()

if(QU_POSIX)
    target_sources(${PROJECT_NAME} PRIVATE qu_platform_posix.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_POSIX)
endif()

if(QU_USE_OPENAL)
    target_sources(${PROJECT_NAME} PRIVATE qu_audio_openal.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_USE_OPENAL)
    target_include_directories(${PROJECT_NAME} PRIVATE ${OPENAL_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENAL_LIBRARY})
endif()

if(QU_USE_GL)
    target_sources(${PROJECT_NAME} PRIVATE qu_renderer_gl1.c qu_renderer_gl3.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_USE_GL)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
endif()

if(QU_USE_ES2)
    target_sources(${PROJECT_NAME} PRIVATE qu_renderer_es2.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_USE_ES2)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_GLESv2_LIBRARY})
endif()

if(QU_USE_X11)
    target_sources(${PROJECT_NAME} PRIVATE qu_core_x11.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QU_USE_X11)
    target_include_directories(${PROJECT_NAME} PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES})
endif()

#-------------------------------------------------------------------------------

if(QU_MSVC)
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

if(QU_CLANG OR QU_GCC)
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
