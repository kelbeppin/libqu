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

#define QU_MODULE "core"

#include "qu.h"

//------------------------------------------------------------------------------
// qu_core.c: Core module
//------------------------------------------------------------------------------

struct qu__core_priv
{
	struct qu__core const *impl;
};

static struct qu__core_priv priv;

//------------------------------------------------------------------------------

void qu__core_initialize(qu_params const *params)
{
	memset(&priv, 0, sizeof(priv));

#if defined(_WIN32)
	priv.impl = &qu__core_win32;
#elif defined(__EMSCRIPTEN__)
	priv.impl = &qu__core_emscripten;
#elif defined(__unix__)
	priv.impl = &qu__core_x11;
#else
	QU_HALT("Everything is broken.");
#endif

	priv.impl->initialize(params);
}

void qu__core_terminate(void)
{
	priv.impl->terminate();
}

bool qu__core_process(void)
{
    return priv.impl->process();
}

void qu__core_present(void)
{
    priv.impl->present();
}

struct qu__graphics const *qu__core_get_graphics(void)
{
    return priv.impl->get_graphics();
}

struct qu__audio const *qu__core_get_audio(void)
{
    return priv.impl->get_audio();
}

void *qu__core_gl_proc_address(char const *name)
{
    return priv.impl->gl_proc_address(name);
}

bool const *qu__core_get_keyboard_state(void)
{
    return priv.impl->get_keyboard_state();
}

bool qu__core_is_key_pressed(qu_key key)
{
    return priv.impl->is_key_pressed(key);
}

uint8_t qu__core_get_mouse_button_state(void)
{
    return priv.impl->get_mouse_button_state();
}

bool qu__core_is_mouse_button_pressed(qu_mouse_button button)
{
    return priv.impl->is_mouse_button_pressed(button);
}

qu_vec2i qu__core_get_mouse_cursor_position(void)
{
    return priv.impl->get_mouse_cursor_position();
}

qu_vec2i qu__core_get_mouse_cursor_delta(void)
{
    return priv.impl->get_mouse_cursor_delta();
}

qu_vec2i qu__core_get_mouse_wheel_delta(void)
{
    return priv.impl->get_mouse_wheel_delta();
}

bool qu__core_is_joystick_connected(int joystick)
{
    return priv.impl->is_joystick_connected(joystick);
}

char const *qu__core_get_joystick_id(int joystick)
{
    return priv.impl->get_joystick_id(joystick);
}

int qu__core_get_joystick_button_count(int joystick)
{
    return priv.impl->get_joystick_button_count(joystick);
}

int qu__core_get_joystick_axis_count(int joystick)
{
    return priv.impl->get_joystick_axis_count(joystick);
}

char const *qu__core_get_joystick_button_id(int joystick, int button)
{
    return priv.impl->get_joystick_button_id(joystick, button);
}

char const *qu__core_get_joystick_axis_id(int joystick, int axis)
{
    return priv.impl->get_joystick_axis_id(joystick, axis);
}

bool qu__core_is_joystick_button_pressed(int joystick, int button)
{
    return priv.impl->is_joystick_button_pressed(joystick, button);
}

float qu__core_get_joystick_axis_value(int joystick, int axis)
{
    return priv.impl->get_joystick_axis_value(joystick, axis);
}

void qu__core_on_key_pressed(qu_key_fn fn)
{
    priv.impl->on_key_pressed(fn);
}

void qu__core_on_key_repeated(qu_key_fn fn)
{
    priv.impl->on_key_repeated(fn);
}

void qu__core_on_key_released(qu_key_fn fn)
{
    priv.impl->on_key_released(fn);
}

void qu__core_on_mouse_button_pressed(qu_mouse_button_fn fn)
{
    priv.impl->on_mouse_button_pressed(fn);
}

void qu__core_on_mouse_button_released(qu_mouse_button_fn fn)
{
    priv.impl->on_mouse_button_released(fn);
}

void qu__core_on_mouse_cursor_moved(qu_mouse_cursor_fn fn)
{
    priv.impl->on_mouse_cursor_moved(fn);
}

void qu__core_on_mouse_wheel_scrolled(qu_mouse_wheel_fn fn)
{
    priv.impl->on_mouse_wheel_scrolled(fn);
}
