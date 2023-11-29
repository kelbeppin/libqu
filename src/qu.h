//------------------------------------------------------------------------------
// Copyright (c) 2023 kelbeppin
// 
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
// 
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//------------------------------------------------------------------------------
// qu.h: private header file
//------------------------------------------------------------------------------

#ifndef QU_PRIVATE_H_INC
#define QU_PRIVATE_H_INC

//------------------------------------------------------------------------------

#if !defined(QU_MODULE)
#define QU_MODULE               "?"__FILE__
#endif

//------------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(_UNICODE)
#define _UNICODE
#endif

#if !defined(UNICODE)
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN

#endif // defined(_WIN32)

//------------------------------------------------------------------------------

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <libqu.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "qu_fs.h"
#include "qu_graphics.h"
#include "qu_log.h"
#include "qu_math.h"
#include "qu_platform.h"
#include "qu_resource_loader.h"
#include "qu_util.h"

//------------------------------------------------------------------------------

#define QU_HALT(...) \
    do { \
        qu_log_printf(QU_LOG_LEVEL_ERROR, QU_MODULE, __VA_ARGS__); \
        abort(); \
    } while (0);

#define QU_HALT_IF(condition) \
    do { \
        if (condition) { \
            qu_log_printf(QU_LOG_LEVEL_ERROR, QU_MODULE, "QU_HALT_IF: %s\n", #condition); \
            abort(); \
        } \
    } while (0);

//------------------------------------------------------------------------------

typedef enum qu_result
{
    QU_FAILURE = -1,
    QU_SUCCESS = 0,
} qu_result;

typedef enum qu_event_type
{
    QU_EVENT_TYPE_INVALID = -1,
    QU_EVENT_TYPE_KEY_PRESSED,
    QU_EVENT_TYPE_KEY_RELEASED,
    QU_EVENT_TYPE_MOUSE_BUTTON_PRESSED,
    QU_EVENT_TYPE_MOUSE_BUTTON_RELEASED,
    QU_EVENT_TYPE_MOUSE_CURSOR_MOVED,
    QU_EVENT_TYPE_MOUSE_WHEEL_SCROLLED,
    QU_EVENT_TYPE_ACTIVATE,
    QU_EVENT_TYPE_DEACTIVATE,
    QU_EVENT_TYPE_TOUCH_STARTED,
    QU_EVENT_TYPE_TOUCH_ENDED,
    QU_EVENT_TYPE_TOUCH_MOVED,
    QU_EVENT_TYPE_WINDOW_RESIZE,
} qu_event_type;

typedef enum qu_graphics_api
{
    QU_GRAPHICS_API_NULL,
    QU_GRAPHICS_API_GL15,
    QU_GRAPHICS_API_GL33,
    QU_GRAPHICS_API_ES20,
} qu_graphics_api;

typedef struct qu_keyboard_event
{
    qu_key key;
} qu_keyboard_event;

typedef struct qu_mouse_event
{
    qu_mouse_button button;
    int x_cursor;
    int y_cursor;
    int dx_wheel;
    int dy_wheel;
} qu_mouse_event;

typedef struct qu_touch_event
{
    int index;
    int x;
    int y;
} qu_touch_event;

typedef struct qu_window_resize_event
{
    int width;
    int height;
} qu_window_resize_event;

typedef struct qu_event
{
    qu_event_type type;

    union {
        qu_keyboard_event keyboard;
        qu_mouse_event mouse;
        qu_touch_event touch;
        qu_window_resize_event window_resize;
    } data;
} qu_event;

typedef struct qu_audio_buffer
{
    int16_t *data;      // sample data
    size_t samples;     // number of samples
    intptr_t priv[4];   // implementation-specific data
} qu_audio_buffer;

typedef struct qu_audio_source
{
    int channels;       // number of channels (1 or 2)
    int sample_rate;    // sample rate (usually 44100)
    int loop;           // should this source loop (-1 if yes)
    intptr_t priv[4];   // implementation-specific data
} qu_audio_source;

//------------------------------------------------------------------------------

typedef struct qu_core_impl
{
    qu_result (*precheck)(void);
    qu_result (*initialize)(void);
    void (*terminate)(void);
    bool (*process_input)(void);
    void (*swap_buffers)(void);

    char const *(*get_graphics_context_name)(void);

    void *(*gl_proc_address)(char const *name);
    int (*get_gl_multisample_samples)(void);

    char const *(*get_window_title)(void);
    void (*set_window_title)(char const *title);

    qu_vec2i (*get_window_size)(void);
    void (*set_window_size)(int width, int height);

    int (*get_window_aa_level)(void);
    void (*set_window_aa_level)(int level);
} qu_core_impl;

typedef struct qu_joystick_impl
{
    qu_result (*precheck)(void);
    qu_result (*initialize)(void);
    void (*terminate)(void);
    void (*process)(void);
    bool (*is_connected)(int id);
    char const *(*get_name)(int id);
    int (*get_button_count)(int id);
    int (*get_axis_count)(int id);
    char const *(*get_button_name)(int id, int button);
    char const *(*get_axis_name)(int id, int axis);
    bool (*is_button_pressed)(int id, int button);
    float (*get_axis_value)(int id, int axis);
} qu_joystick_impl;

typedef struct qu_audio_impl
{
    qu_result (*check)(void);
    qu_result (*initialize)(void);
    void (*terminate)(void);

    void (*set_master_volume)(float volume);

    qu_result (*create_source)(qu_audio_source *source);
    void (*destroy_source)(qu_audio_source *source);
    bool (*is_source_used)(qu_audio_source *source);
    qu_result (*queue_buffer)(qu_audio_source *source, qu_audio_buffer *buffer);
    int (*get_queued_buffers)(qu_audio_source *source);
    qu_result (*start_source)(qu_audio_source *source);
    qu_result (*stop_source)(qu_audio_source *source);
} qu_audio_impl;

//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------

extern qu_core_impl const qu_android_core_impl;
extern qu_core_impl const qu_emscripten_core_impl;
extern qu_core_impl const qu_win32_core_impl;
extern qu_core_impl const qu_x11_core_impl;

extern qu_joystick_impl const qu_null_joystick_impl;
extern qu_joystick_impl const qu_win32_joystick_impl;
extern qu_joystick_impl const qu_linux_joystick_impl;

extern qu_audio_impl const qu_null_audio_impl;
extern qu_audio_impl const qu_openal_audio_impl;
extern qu_audio_impl const qu_xaudio2_audio_impl;
extern qu_audio_impl const qu_sles_audio_impl;

//------------------------------------------------------------------------------

void qu_atexit(void (*callback)(void));

void qu_initialize_core(void);
void qu_terminate_core(void);
bool qu_handle_events(void);
void qu_swap_buffers(void);
char const *qu_get_graphics_context_name(void);
void *qu_gl_get_proc_address(char const *name);
int qu_gl_get_samples(void);
void qu_enqueue_event(qu_event const *event);

void qu_initialize_text(void);
void qu_terminate_text(void);

void qu_initialize_audio(void);
void qu_terminate_audio(void);

#if defined(ANDROID)

void np_android_write_log(int level, char const *tag, char const *message);
void *np_android_asset_open(char const *path);
void np_android_asset_close(void *asset);
int64_t np_android_asset_read(void *dst, size_t size, void *asset);
int64_t np_android_asset_tell(void *asset);
int64_t np_android_asset_seek(void *asset, int64_t offset, int whence);

#endif // defined(ANDROID)

// Hidden public functions:

QU_API int QU_CALL qu_get_desired_graphics_api(void);
QU_API void QU_CALL qu_set_desired_graphics_api(char const *api);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif // QU_PRIVATE_H_INC
