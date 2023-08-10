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

    qu_keyboard_state keyboard;
    uint32_t mouse_buttons;
    qu_vec2i mouse_cursor_position;
    qu_vec2i mouse_cursor_delta;
    qu_vec2i mouse_wheel_delta;

    qu_key_fn key_press_fn;
    qu_key_fn key_repeat_fn;
    qu_key_fn key_release_fn;
    qu_mouse_button_fn mouse_button_press_fn;
    qu_mouse_button_fn mouse_button_release_fn;
    qu_mouse_cursor_fn mouse_cursor_motion_fn;
    qu_mouse_wheel_fn mouse_wheel_scroll_fn;
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
    memset(&priv.keyboard, 0, sizeof(priv.keyboard));

    priv.mouse_buttons = 0;

    priv.mouse_cursor_delta.x = 0;
    priv.mouse_cursor_delta.y = 0;

    priv.mouse_wheel_delta.x = 0;
    priv.mouse_wheel_delta.y = 0;

    if (!priv.impl->process()) {
        return false;
    }

    if (priv.mouse_cursor_delta.x || priv.mouse_cursor_delta.y) {
        if (priv.mouse_cursor_motion_fn) {
            priv.mouse_cursor_motion_fn(priv.mouse_cursor_delta.x, priv.mouse_cursor_delta.y);
        }
    }

    if (priv.mouse_wheel_delta.x || priv.mouse_wheel_delta.y) {
        if (priv.mouse_wheel_scroll_fn) {
            priv.mouse_wheel_scroll_fn(priv.mouse_wheel_delta.x, priv.mouse_wheel_delta.y);
        }
    }

    return true;
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

void *qu__core_get_gl_proc_address(char const *name)
{
    return priv.impl->gl_proc_address(name);
}

qu_keyboard_state const *qu__core_get_keyboard_state(void)
{
    return &priv.keyboard;
}

qu_key_state qu__core_get_key_state(qu_key key)
{
    return priv.keyboard.keys[key];
}

bool qu__core_is_key_pressed(qu_key key)
{
    return (priv.keyboard.keys[key] == QU_KEY_PRESSED);
}

uint8_t qu__core_get_mouse_button_state(void)
{
    return priv.mouse_buttons & 0xFF; // <-- remove mask later
}

bool qu__core_is_mouse_button_pressed(qu_mouse_button button)
{
    unsigned int mask = (1 << button);

    return (priv.mouse_buttons & mask) == mask;
}

qu_vec2i qu__core_get_mouse_cursor_position(void)
{
    return priv.mouse_cursor_position;
}

qu_vec2i qu__core_get_mouse_cursor_delta(void)
{
    return priv.mouse_cursor_delta;
}

qu_vec2i qu__core_get_mouse_wheel_delta(void)
{
    return priv.mouse_wheel_delta;
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

void qu__core_set_key_press_fn(qu_key_fn fn)
{
    priv.key_press_fn = fn;
}

void qu__core_set_key_repeat_fn(qu_key_fn fn)
{
    priv.key_repeat_fn = fn;
}

void qu__core_set_key_release_fn(qu_key_fn fn)
{
    priv.key_release_fn = fn;
}

void qu__core_set_mouse_button_press_fn(qu_mouse_button_fn fn)
{
    priv.mouse_button_press_fn = fn;
}

void qu__core_set_mouse_button_release_fn(qu_mouse_button_fn fn)
{
    priv.mouse_button_release_fn = fn;
}

void qu__core_set_mouse_cursor_motion_fn(qu_mouse_cursor_fn fn)
{
    priv.mouse_cursor_motion_fn = fn;
}

void qu__core_set_mouse_wheel_scroll_fn(qu_mouse_wheel_fn fn)
{
    priv.mouse_wheel_scroll_fn = fn;
}

void qu__core_on_key_pressed(qu_key key)
{
    switch (priv.keyboard.keys[key]) {
    case QU_KEY_IDLE:
        priv.keyboard.keys[key] = QU_KEY_PRESSED;

        if (priv.key_press_fn) {
            priv.key_press_fn(key);
        }
        break;
    case QU_KEY_PRESSED:
        if (priv.key_repeat_fn) {
            priv.key_repeat_fn(key);
        }
        break;
    default:
        break;
    }
}

void qu__core_on_key_released(qu_key key)
{
    if (priv.keyboard.keys[key] == QU_KEY_PRESSED) {
        priv.keyboard.keys[key] = QU_KEY_RELEASED;

        if (priv.key_release_fn) {
            priv.key_release_fn(key);
        }
    }
}

void qu__core_on_mouse_button_pressed(qu_mouse_button button)
{
    unsigned int mask = (1 << button);

    if ((priv.mouse_buttons & mask) == 0) {
        priv.mouse_buttons |= mask;

        if (priv.mouse_button_press_fn) {
            priv.mouse_button_press_fn(button);
        }
    }
}

void qu__core_on_mouse_button_released(qu_mouse_button button)
{
    unsigned int mask = (1 << button);

    if ((priv.mouse_buttons & mask) == mask) {
        priv.mouse_buttons &= ~mask;

        if (priv.mouse_button_release_fn) {
            priv.mouse_button_release_fn(button);
        }
    }
}

void qu__core_on_mouse_cursor_moved(int x, int y)
{
    int x_old = priv.mouse_cursor_position.x;
    int y_old = priv.mouse_cursor_position.y;

    priv.mouse_cursor_position.x = x;
    priv.mouse_cursor_position.y = y;

    priv.mouse_cursor_delta.x = x - x_old;
    priv.mouse_cursor_delta.y = x - y_old;
}

void qu__core_on_mouse_wheel_scrolled(int dx, int dy)
{
    priv.mouse_wheel_delta.x += dx;
    priv.mouse_wheel_delta.y += dy;
}
