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
// qu_core.h: core
//------------------------------------------------------------------------------

#ifndef QU_CORE_H_INC
#define QU_CORE_H_INC

//------------------------------------------------------------------------------

#include <libqu.h>

//------------------------------------------------------------------------------

typedef enum qu_graphics_api
{
    QU_GRAPHICS_API_NULL,
    QU_GRAPHICS_API_GL15,
    QU_GRAPHICS_API_GL33,
    QU_GRAPHICS_API_ES20,
} qu_graphics_api;

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

//------------------------------------------------------------------------------

extern qu_core_impl const qu_android_core_impl;
extern qu_core_impl const qu_emscripten_core_impl;
extern qu_core_impl const qu_win32_core_impl;
extern qu_core_impl const qu_x11_core_impl;

extern qu_joystick_impl const qu_null_joystick_impl;
extern qu_joystick_impl const qu_win32_joystick_impl;
extern qu_joystick_impl const qu_linux_joystick_impl;

//------------------------------------------------------------------------------

void qu_initialize_core(void);
void qu_terminate_core(void);
bool qu_handle_events(void);
void qu_swap_buffers(void);
char const *qu_get_graphics_context_name(void);
void *qu_gl_get_proc_address(char const *name);
int qu_gl_get_samples(void);

//------------------------------------------------------------------------------

#endif // QU_CORE_H_INC

