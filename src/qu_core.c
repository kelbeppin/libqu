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
// qu_core.c: Core module
//------------------------------------------------------------------------------

#define QU_MODULE "core"

//------------------------------------------------------------------------------

#include "qu.h"

//------------------------------------------------------------------------------

#define WINDOW_TITLE_LENGTH         256

//------------------------------------------------------------------------------

static qu_core_impl const *core_impl_list[] = {

#ifdef QU_WIN32
    &qu_win32_core_impl,
#endif

#ifdef QU_USE_X11
    &qu_x11_core_impl,
#endif

#ifdef QU_EMSCRIPTEN
    &qu_emscripten_core_impl,
#endif

#ifdef QU_ANDROID
    &qu_android_core_impl,
#endif
};

//------------------------------------------------------------------------------

static qu_joystick_impl const *joystick_impl_list[] = {

#ifdef QU_WIN32
    &qu_win32_joystick_impl,
#endif

#ifdef QU_LINUX
    &qu_linux_joystick_impl,
#endif

    &qu_null_joystick_impl,
};

//------------------------------------------------------------------------------

struct core_params
{
    char window_title[WINDOW_TITLE_LENGTH];
    qu_vec2i window_size;
};

struct event_buffer
{
    qu_event *array;
    size_t length;
    size_t capacity;
};

struct core_clock
{
    bool initialized;
    uint32_t start_mediump;
    uint64_t start_highp;
};

struct core_priv
{
    bool initialized;

	qu_core_impl const *impl;
    qu_joystick_impl const *joystick;

    bool window_active;
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

    int touch_state[QU_MAX_TOUCH_INPUTS];
    qu_vec2i touch_position[QU_MAX_TOUCH_INPUTS];
    qu_vec2i touch_delta[QU_MAX_TOUCH_INPUTS];

    struct core_params params;
    struct event_buffer event_buffer;
    struct core_clock clock;
};

//------------------------------------------------------------------------------

static struct core_priv priv;

//------------------------------------------------------------------------------

static bool is_core_impl_valid(qu_core_impl const *core)
{
    return core->initialize
        && core->terminate
        && core->process_input
        && core->swap_buffers
        && core->get_graphics_context_name
        && core->gl_proc_address
        && core->get_gl_multisample_samples
        && core->set_window_title
        && core->set_window_size;
}

static bool is_joystick_impl_valid(qu_joystick_impl const *joystick)
{
    return joystick->initialize
        && joystick->terminate
        && joystick->process
        && joystick->is_connected
        && joystick->get_name
        && joystick->get_button_count
        && joystick->get_axis_count
        && joystick->get_button_name
        && joystick->get_axis_name
        && joystick->is_button_pressed
        && joystick->get_axis_value;
}

static void initialize_window(void)
{
    if (priv.impl) {
        return;
    }

    int impl_count = QU_ARRAY_SIZE(core_impl_list);

    if (impl_count == 0) {
        QU_HALT("core_impl_count == 0");
    }

    for (int i = 0; i < impl_count; i++) {
        priv.impl = core_impl_list[i];

        if (priv.impl->precheck(NULL) == QU_SUCCESS) {
            break;
        }
    }

    if (!is_core_impl_valid(priv.impl)) {
        QU_HALT("Core module implementation is invalid.");
    }

	if (priv.impl->initialize(NULL) != QU_SUCCESS) {
        QU_HALT("Failed to initialize core module.");
    }

    // These functions always return valid values even if priv.params
    // is still zeroed. So use them instead of directly taking values from
    // priv.params.

    char const *title = qu_get_window_title();
    qu_vec2i size = qu_get_window_size();

    priv.impl->set_window_title(title);
    priv.impl->set_window_size(size.x, size.y);
}

static void initialize_joystick(void)
{
    if (priv.joystick) {
        return;
    }

    int impl_count = QU_ARRAY_SIZE(joystick_impl_list);

    if (impl_count == 0) {
        QU_HALT("No joystick implementation found.");
    }

    for (int i = 0; i < impl_count; i++) {
        priv.joystick = joystick_impl_list[i];

        if (priv.joystick->precheck(NULL) == QU_SUCCESS) {
            break;
        }
    }

    if (!is_joystick_impl_valid(priv.joystick)) {
        QU_HALT("Joystick module implementation is invalid.");
    }

    if (priv.joystick->initialize(NULL) != QU_SUCCESS) {
        QU_HALT("Failed to initialize joystick module.");
    }
}

static void initialize_clock(void)
{
    if (priv.clock.initialized) {
        return;
    }

    priv.clock.initialized = true;
    priv.clock.start_mediump = pl_get_ticks_mediump();
    priv.clock.start_highp = pl_get_ticks_highp();
}

static void handle_key_press(qu_keyboard_event const *event)
{
    if (event->key == QU_KEY_INVALID) {
        return;
    }

    switch (priv.keyboard.keys[event->key]) {
    case QU_KEY_IDLE:
        priv.keyboard.keys[event->key] = QU_KEY_PRESSED;

        if (priv.key_press_fn) {
            priv.key_press_fn(event->key);
        }
        break;
    case QU_KEY_PRESSED:
        if (priv.key_repeat_fn) {
            priv.key_repeat_fn(event->key);
        }
        break;
    default:
        break;
    }
}

static void handle_key_release(qu_keyboard_event const *event)
{
    if (event->key == QU_KEY_INVALID) {
        return;
    }

    if (priv.keyboard.keys[event->key] == QU_KEY_PRESSED) {
        priv.keyboard.keys[event->key] = QU_KEY_RELEASED;

        if (priv.key_release_fn) {
            priv.key_release_fn(event->key);
        }
    }
}

static void handle_mouse_button_press(qu_mouse_event const *event)
{
    if (event->button == QU_MOUSE_BUTTON_INVALID) {
        return;
    }

    unsigned int mask = (1 << event->button);

    if ((priv.mouse_buttons & mask) == 0) {
        priv.mouse_buttons |= mask;

        if (priv.mouse_button_press_fn) {
            priv.mouse_button_press_fn(event->button);
        }
    }
}

static void handle_mouse_button_release(qu_mouse_event const *event)
{
    if (event->button == QU_MOUSE_BUTTON_INVALID) {
        return;
    }
    
    unsigned int mask = (1 << event->button);

    if ((priv.mouse_buttons & mask) == mask) {
        priv.mouse_buttons &= ~mask;

        if (priv.mouse_button_release_fn) {
            priv.mouse_button_release_fn(event->button);
        }
    }
}

static void handle_mouse_cursor_motion(qu_mouse_event const *event)
{
    int x_old = priv.mouse_cursor_position.x;
    int y_old = priv.mouse_cursor_position.y;

    priv.mouse_cursor_position.x = event->x_cursor;
    priv.mouse_cursor_position.y = event->y_cursor;

    priv.mouse_cursor_delta.x = event->x_cursor - x_old;
    priv.mouse_cursor_delta.y = event->y_cursor - y_old;
}

static void handle_mouse_wheel_scroll(qu_mouse_event const *event)
{
    priv.mouse_wheel_delta.x += event->dx_wheel;
    priv.mouse_wheel_delta.y += event->dy_wheel;
}

static void release_all_inputs(void)
{
    for (int i = 0; i < QU_TOTAL_KEYS; i++) {
        if (priv.keyboard.keys[i] == QU_KEY_PRESSED) {
            priv.keyboard.keys[i] = QU_KEY_RELEASED;

            if (priv.key_release_fn) {
                priv.key_release_fn(i);
            }
        }
    }

    for (int i = 0; i < QU_TOTAL_MOUSE_BUTTONS; i++) {
        int mask = (1 << i);

        if (priv.mouse_buttons & mask) {
            priv.mouse_buttons &= ~mask;

            if (priv.mouse_button_release_fn) {
                priv.mouse_button_release_fn(i);
            }
        }
    }
}

static void handle_window_activation(bool active)
{
    if (priv.window_active == active) {
        return;
    }

    if (active) {
        // ...
    } else {
        release_all_inputs();
    }

    priv.window_active = active;
}

static void handle_touch_motion(qu_touch_event const *event)
{
    int x_old = priv.touch_position[event->index].x;
    int y_old = priv.touch_position[event->index].y;

    priv.touch_position[event->index].x = event->x;
    priv.touch_position[event->index].y = event->y;

    priv.touch_delta[event->index].x = event->x - x_old;
    priv.touch_delta[event->index].y = event->y - y_old;
}

//------------------------------------------------------------------------------
// Internal API

void qu_initialize_core(qu_params const *params)
{
    initialize_window();

    priv.initialized = true;

    QU_LOGI("Initialized.");

    // Temporary:
    priv.window_active = true;
}

void qu_terminate_core(void)
{
    pl_free(priv.event_buffer.array);

    if (priv.joystick) {
        priv.joystick->terminate();
    }

    if (priv.impl) {
	    priv.impl->terminate();
    }

    memset(&priv, 0, sizeof(priv));
}

bool qu_handle_events(void)
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

    if (!priv.impl->process_input()) {
        return false;
    }

    struct event_buffer *buffer = &priv.event_buffer;

    for (size_t i = 0; i < buffer->length; i++) {
        qu_event *event = &buffer->array[i];

        switch (event->type) {
        case QU_EVENT_TYPE_KEY_PRESSED:
            handle_key_press(&event->data.keyboard);
            break;
        case QU_EVENT_TYPE_KEY_RELEASED:
            handle_key_release(&event->data.keyboard);
            break;
        case QU_EVENT_TYPE_MOUSE_BUTTON_PRESSED:
            handle_mouse_button_press(&event->data.mouse);
            break;
        case QU_EVENT_TYPE_MOUSE_BUTTON_RELEASED:
            handle_mouse_button_release(&event->data.mouse);
            break;
        case QU_EVENT_TYPE_MOUSE_CURSOR_MOVED:
            handle_mouse_cursor_motion(&event->data.mouse);
            break;
        case QU_EVENT_TYPE_MOUSE_WHEEL_SCROLLED:
            handle_mouse_wheel_scroll(&event->data.mouse);
            break;
        case QU_EVENT_TYPE_ACTIVATE:
            handle_window_activation(true);
            break;
        case QU_EVENT_TYPE_DEACTIVATE:
            handle_window_activation(false);
            break;
        case QU_EVENT_TYPE_TOUCH_STARTED:
            priv.touch_state[event->data.touch.index] = 1;
            priv.touch_position[event->data.touch.index].x = event->data.touch.x;
            priv.touch_position[event->data.touch.index].y = event->data.touch.y;
            priv.touch_delta[event->data.touch.index].x = 0;
            priv.touch_delta[event->data.touch.index].y = 0;
            break;
        case QU_EVENT_TYPE_TOUCH_ENDED:
            priv.touch_state[event->data.touch.index] = 0;
            break;
        case QU_EVENT_TYPE_TOUCH_MOVED:
            handle_touch_motion(&event->data.touch);
            break;
        case QU_EVENT_TYPE_WINDOW_RESIZE:
            qu_event_window_resize(
                event->data.window_resize.width,
                event->data.window_resize.height
            );
            break;
        }
    }

    buffer->length = 0;

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

    if (priv.joystick) {
        priv.joystick->process();
    }

    return true;
}

void qu_swap_buffers(void)
{
    priv.impl->swap_buffers();
}

char const *qu_get_graphics_context_name(void)
{
    return priv.impl->get_graphics_context_name();
}

void *qu_gl_get_proc_address(char const *name)
{
    return priv.impl->gl_proc_address(name);
}

int qu_gl_get_samples(void)
{
    return priv.impl->get_gl_multisample_samples();
}

void qu_enqueue_event(qu_event const *event)
{
    struct event_buffer *buffer = &priv.event_buffer;

    if (buffer->length == buffer->capacity) {
        size_t next_capacity = buffer->capacity == 0 ? 256 : buffer->capacity * 2;
        void *next_array = pl_realloc(buffer->array, sizeof(*(buffer->array)) * next_capacity);

        if (!next_array) {
            QU_HALT("Out of memory: unable to grow event buffer.");
        }

        buffer->array = next_array;
        buffer->capacity = next_capacity;
    }

    memcpy(&buffer->array[buffer->length++], event, sizeof(*event));
}

//------------------------------------------------------------------------------
// Public API

char const *qu_get_window_title(void)
{
    if (!priv.initialized) {
        if (priv.params.window_title[0] == '\0') {
            return "libqu application";
        }

        return priv.params.window_title;
    }

    return priv.impl->get_window_title();
}

void qu_set_window_title(char const *title)
{
    if (!priv.initialized) {
        strncpy(priv.params.window_title, title, WINDOW_TITLE_LENGTH);
        return;
    }

    priv.impl->set_window_title(title);
}

qu_vec2i qu_get_window_size(void)
{
    if (!priv.initialized) {
        if (!priv.params.window_size.x || !priv.params.window_size.y) {
            return (qu_vec2i) { 1280, 720 };
        }

        return priv.params.window_size;
    }

    return priv.impl->get_window_size();
}

void qu_set_window_size(int width, int height)
{
    if (!priv.initialized) {
        priv.params.window_size.x = width;
        priv.params.window_size.y = height;
        return;
    }

    priv.impl->set_window_size(width, height);
}

bool qu_is_window_active(void)
{
    return priv.window_active;
}

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
    return qu_convert_window_pos_to_canvas_pos(priv.mouse_cursor_position);
}

qu_vec2i qu_get_mouse_cursor_delta(void)
{
    return qu_convert_window_delta_to_canvas_delta(priv.mouse_cursor_delta);
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

bool qu_is_touch_pressed(int index)
{
    return priv.touch_state[index];
}

qu_vec2i qu_get_touch_position(int index)
{
    if (index < 0 || index >= QU_MAX_TOUCH_INPUTS) {
        return (qu_vec2i) { -1, -1 };
    }

    if (!priv.touch_state[index]) {
        return (qu_vec2i) { -1, -1 };
    }

    qu_vec2i position = { priv.touch_position[index].x, priv.touch_position[index].y };

    return qu_convert_window_pos_to_canvas_pos(position);
}

bool qu_is_joystick_connected(int joystick)
{
    if (!priv.joystick) {
        initialize_joystick();
    }

    return priv.joystick->is_connected(joystick);
}

char const *qu_get_joystick_id(int joystick)
{
    if (!priv.joystick) {
        initialize_joystick();
    }

    return priv.joystick->get_name(joystick);
}

int qu_get_joystick_button_count(int joystick)
{
    if (!priv.joystick) {
        initialize_joystick();
    }

    return priv.joystick->get_button_count(joystick);
}

int qu_get_joystick_axis_count(int joystick)
{
    if (!priv.joystick) {
        initialize_joystick();
    }

    return priv.joystick->get_axis_count(joystick);
}

char const *qu_get_joystick_button_id(int joystick, int button)
{
    if (!priv.joystick) {
        initialize_joystick();
    }

    return priv.joystick->get_button_name(joystick, button);
}

char const *qu_get_joystick_axis_id(int joystick, int axis)
{
    if (!priv.joystick) {
        initialize_joystick();
    }

    return priv.joystick->get_axis_name(joystick, axis);
}

bool qu_is_joystick_button_pressed(int joystick, int button)
{
    if (!priv.joystick) {
        initialize_joystick();
    }

    return priv.joystick->is_button_pressed(joystick, button);
}

float qu_get_joystick_axis_value(int joystick, int axis)
{
    if (!priv.joystick) {
        initialize_joystick();
    }

    return priv.joystick->get_axis_value(joystick, axis);
}

float qu_get_time_mediump(void)
{
    if (!priv.clock.initialized) {
        initialize_clock();
    }

    return (pl_get_ticks_mediump() - priv.clock.start_mediump) / 1000.f;
}

double qu_get_time_highp(void)
{
    if (!priv.clock.initialized) {
        initialize_clock();
    }

    return (pl_get_ticks_highp() - priv.clock.start_highp) / 1000000000.0;
}

qu_date_time qu_get_date_time(void)
{
    qu_date_time date_time = { 0 };

    pl_get_date_time(&date_time);

    return date_time;
}
