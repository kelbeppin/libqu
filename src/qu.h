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

#include "qu_graphics.h"
#include "qu_log.h"
#include "qu_math.h"
#include "qu_platform.h"
#include "qu_text.h"
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

//------------------------------------------------------------------------------

void qu_atexit(void (*callback)(void));

void qu_enqueue_event(qu_event const *event);

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

#endif // QU_PRIVATE_H_INC
