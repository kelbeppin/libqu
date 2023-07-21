
#-------------------------------------------------------------------------------

if(QU_OS_WINDOWS)
    set(QU_PLATFORM_SRC "qu_platform_win32.c")
else()
    set(QU_PLATFORM_SRC "qu_platform_posix.c")
endif()

if(QU_OS_WINDOWS)
    set(QU_CORE_SRC "qu_core_win32.c")
elseif(QU_OS_LINUX)
    set(QU_CORE_SRC "qu_core_x11.c")
elseif(QU_OS_EMSCRIPTEN)
    set(QU_CORE_SRC "qu_core_emscripten.c")
endif()

add_library(${PROJECT_NAME}
    "qu.c"
    "qu.h"
    "qu_array.c"
    "qu_audio_null.c"
    "qu_fs.c"
    "qu_graphics_null.c"
    "qu_image_loader.c"
    "qu_log.c"
    "qu_math.c"
    "qu_sound_reader.c"
    "qu_text.c"
    "qu_util.c"
    ${QU_CORE_SRC}
    ${QU_PLATFORM_SRC}
    $<$<BOOL:${QU_USE_OPENAL}>:qu_audio_openal.c>
    $<$<BOOL:${QU_USE_OPENGL}>:qu_graphics_gl2.c>
    $<$<BOOL:${QU_USE_OPENGL_ES2}>:qu_graphics_es2.c>)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}>
    PRIVATE
        "${PROJECT_SOURCE_DIR}/third-party")

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        QU_BUILD
        $<$<BOOL:${BUILD_SHARED_LIBS}>:QU_SHARED>
        $<$<BOOL:${QU_USE_OPENAL}>:QU_USE_OPENAL>
        $<$<BOOL:${QU_USE_OPENGL}>:QU_USE_GL>
        $<$<BOOL:${QU_USE_OPENGL_ES2}>:QU_USE_ES2>)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        $<$<BOOL:${MATH_LIBRARY}>:${MATH_LIBRARY}>
    PRIVATE
        Threads::Threads
        Freetype::Freetype
        harfbuzz::harfbuzz
        Vorbis::vorbisfile
        $<$<BOOL:${QU_USE_OPENAL}>:OpenAL::OpenAL>
        $<$<BOOL:${QU_USE_OPENGL}>:OpenGL::GL>
        $<$<BOOL:${QU_USE_OPENGL_ES2}>:${OPENGL_GLESv2_LIBRARY}>)

if(MATH_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${MATH_LIBRARY})
endif()

if(QU_OS_WINDOWS)
    target_link_libraries(${PROJECT_NAME} PRIVATE dwmapi shcore XInput)
elseif(QU_OS_LINUX)
    target_include_directories(${PROJECT_NAME} PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GLX ${X11_LIBRARIES})
elseif(QU_OS_EMSCRIPTEN)
    target_compile_options(${PROJECT_NAME} PUBLIC -sUSE_SDL)
    target_link_options(${PROJECT_NAME} PUBLIC -sINITIAL_MEMORY=536870912 -sPTHREAD_POOL_SIZE=4)
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        PREFIX ""
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS ON
        C_VISIBILITY_PRESET hidden)

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
elseif(QU_COMPILER_CLANG OR QU_COMPILER_GNU)
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
