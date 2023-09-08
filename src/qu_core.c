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

static struct qu__core const *supported_core_impl_list[] = {

#ifdef QU_WIN32
    &qu__core_win32,
#endif

#ifdef QU_LINUX
    &qu__core_x11,
#endif

#ifdef QU_EMSCRIPTEN
    &qu__core_emscripten,
#endif

};

static struct qu__joystick const *supported_joystick_impl_list[] = {

#ifdef QU_WIN32
    &qu__joystick_win32,
#endif

#ifdef QU_LINUX
    &qu__joystick_linux,
#endif

    &qu__joystick_null,
};

struct qu__core_priv
{
	struct qu__core const *impl;
    struct qu__joystick const *joystick;

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

    int core_impl_count = QU__ARRAY_SIZE(supported_core_impl_list);
    int joystick_impl_count = QU__ARRAY_SIZE(supported_joystick_impl_list);

    if (core_impl_count == 0) {
        QU_HALT("core_impl_count == 0");
    }

    if (joystick_impl_count == 0) {
        QU_HALT("joystick_impl_count == 0");
    }

    for (int i = 0; i < core_impl_count; i++) {
        priv.impl = supported_core_impl_list[i];

        // if (priv.impl->query()) {
            break;
        // }
    }

    QU_HALT_IF(!priv.impl->initialize);
    QU_HALT_IF(!priv.impl->terminate);
    QU_HALT_IF(!priv.impl->process);
    QU_HALT_IF(!priv.impl->present);
    QU_HALT_IF(!priv.impl->get_renderer);
    QU_HALT_IF(!priv.impl->gl_check_extension);
    QU_HALT_IF(!priv.impl->gl_proc_address);
    QU_HALT_IF(!priv.impl->get_gl_multisample_samples);

    for (int i = 0; i < joystick_impl_count; i++) {
        priv.joystick = supported_joystick_impl_list[i];

        // if (priv.joystick->query()) {
            break;
        // }
    }

    QU_HALT_IF(!priv.joystick->initialize);
    QU_HALT_IF(!priv.joystick->terminate);
    QU_HALT_IF(!priv.joystick->process);
    QU_HALT_IF(!priv.joystick->is_connected);
    QU_HALT_IF(!priv.joystick->get_name);
    QU_HALT_IF(!priv.joystick->get_button_count);
    QU_HALT_IF(!priv.joystick->get_axis_count);
    QU_HALT_IF(!priv.joystick->get_button_name);
    QU_HALT_IF(!priv.joystick->get_axis_name);
    QU_HALT_IF(!priv.joystick->is_button_pressed);
    QU_HALT_IF(!priv.joystick->get_axis_value);

	priv.impl->initialize(params);
    priv.joystick->initialize(params);
}

void qu__core_terminate(void)
{
    priv.joystick->terminate();
	priv.impl->terminate();
}

bool qu__core_process(void)
{
    priv.mouse_cursor_delta.x = 0;
    priv.mouse_cursor_delta.y = 0;

    priv.mouse_wheel_delta.x = 0;
    priv.mouse_wheel_delta.y = 0;

    for (int i = 0; i < QU_TOTAL_KEYS; i++) {
        if (priv.keyboard.keys[i] == QU_KEY_RELEASED) {
            priv.keyboard.keys[i] = QU_KEY_IDLE;
        }
    }

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

    priv.joystick->process();

    return true;
}

void qu__core_present(void)
{
    priv.impl->present();
}

enum qu__renderer qu__core_get_renderer(void)
{
    return priv.impl->get_renderer();
}

void *qu__core_get_gl_proc_address(char const *name)
{
    return priv.impl->gl_proc_address(name);
}

int qu__core_get_gl_multisample_samples(void)
{
    return priv.impl->get_gl_multisample_samples();
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

//------------------------------------------------------------------------------
// API entries

qu_keyboard_state const *qu_get_keyboard_state(void)
{
    return &priv.keyboard;
}

qu_key_state qu_get_key_state(qu_key key)
{
    return priv.keyboard.keys[key];
}

bool qu_is_key_pressed(qu_key key)
{
    return (priv.keyboard.keys[key] == QU_KEY_PRESSED);
}

void qu_on_key_pressed(qu_key_fn fn)
{
    priv.key_press_fn = fn;
}

void qu_on_key_repeated(qu_key_fn fn)
{
    priv.key_repeat_fn = fn;
}

void qu_on_key_released(qu_key_fn fn)
{
    priv.key_release_fn = fn;
}

uint8_t qu_get_mouse_button_state(void)
{
    return priv.mouse_buttons & 0xFF; // <-- remove mask later
}

bool qu_is_mouse_button_pressed(qu_mouse_button button)
{
    unsigned int mask = (1 << button);

    return (priv.mouse_buttons & mask) == mask;
}

qu_vec2i qu_get_mouse_cursor_position(void)
{
    return qu__graphics_conv_cursor(priv.mouse_cursor_position);
}

qu_vec2i qu_get_mouse_cursor_delta(void)
{
    return qu__graphics_conv_cursor_delta(priv.mouse_cursor_delta);
}

qu_vec2i qu_get_mouse_wheel_delta(void)
{
    return priv.mouse_wheel_delta;
}

void qu_on_mouse_button_pressed(qu_mouse_button_fn fn)
{
    priv.mouse_button_press_fn = fn;
}

void qu_on_mouse_button_released(qu_mouse_button_fn fn)
{
    priv.mouse_button_release_fn = fn;
}

void qu_on_mouse_cursor_moved(qu_mouse_cursor_fn fn)
{
    priv.mouse_cursor_motion_fn = fn;
}

void qu_on_mouse_wheel_scrolled(qu_mouse_wheel_fn fn)
{
    priv.mouse_wheel_scroll_fn = fn;
}

bool qu_is_joystick_connected(int joystick)
{
    return priv.joystick->is_connected(joystick);
}

char const *qu_get_joystick_id(int joystick)
{
    return priv.joystick->get_name(joystick);
}

int qu_get_joystick_button_count(int joystick)
{
    return priv.joystick->get_button_count(joystick);
}

int qu_get_joystick_axis_count(int joystick)
{
    return priv.joystick->get_axis_count(joystick);
}

char const *qu_get_joystick_button_id(int joystick, int button)
{
    return priv.joystick->get_button_name(joystick, button);
}

char const *qu_get_joystick_axis_id(int joystick, int axis)
{
    return priv.joystick->get_axis_name(joystick, axis);
}

bool qu_is_joystick_button_pressed(int joystick, int button)
{
    return priv.joystick->is_button_pressed(joystick, button);
}

float qu_get_joystick_axis_value(int joystick, int axis)
{
    return priv.joystick->get_axis_value(joystick, axis);
}
