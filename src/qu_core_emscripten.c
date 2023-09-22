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

#define QU_MODULE "emscripten"

#include "qu.h"

#include <emscripten.h>
#include <emscripten/html5.h>

//------------------------------------------------------------------------------
// qu_core_emscripten.c: Emscripten core module
//------------------------------------------------------------------------------

#define CHECK_EMSCRIPTEN(call, error) \
    do { \
        EMSCRIPTEN_RESULT result = call; \
        if (result != EMSCRIPTEN_RESULT_SUCCESS) { \
            QU_HALT(error); \
        } \
    } while (0);

//------------------------------------------------------------------------------

struct event
{
    int type;

    union {
        EmscriptenKeyboardEvent keyboard;
        EmscriptenMouseEvent mouse;
        EmscriptenWheelEvent wheel;
    };
};

struct event_buffer
{
    struct event *array;
    size_t length;
    size_t capacity;
};

struct priv
{
    struct event_buffer events;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gl;
};

//------------------------------------------------------------------------------

static struct priv priv;

//------------------------------------------------------------------------------

static qu_key key_conv(char const *code)
{
    // Letter keys

    if (strncmp(code, "Key", 3) == 0) {
        char sym = code[3];

        if (sym >= 'A' && sym <= 'Z') {
            return QU_KEY_A + (sym - 'A');
        }
    }
    
    // Digit keys

    if (strncmp(code, "Digit", 5) == 0) {
        char digit = code[5];

        if (digit >= '0' && digit <= '9') {
            return QU_KEY_0 + (digit - '0');
        }
    }
    
    // F1-F12

    if (code[0] == 'F' && code[1] >= '1' && code[1] <= '9') {
        if (code[2] == '\0') {
            return QU_KEY_F1 + (code[1] - '1');
        } else if (code[2] >= '0' && code[2] <= '2') {
            if (code[1] == '1') {
                return QU_KEY_F10 + (code[2] - '0');
            }
        }
    }

    // Alt, Shift, Control, Meta

    if (strncmp(code, "Alt", 3) == 0) {
        if (strcmp(&code[3], "Left") == 0) {
            return QU_KEY_LALT;
        }

        if (strcmp(&code[3], "Right") == 0) {
            return QU_KEY_RALT;
        }
    }

    if (strncmp(code, "Shift", 5) == 0) {
        if (strcmp(&code[5], "Left") == 0) {
            return QU_KEY_LSHIFT;
        }

        if (strcmp(&code[5], "Right") == 0) {
            return QU_KEY_RSHIFT;
        }
    }

    if (strncmp(code, "Control", 7) == 0) {
        if (strcmp(&code[7], "Left") == 0) {
            return QU_KEY_LCTRL;
        }

        if (strcmp(&code[7], "Right") == 0) {
            return QU_KEY_RCTRL;
        }
    }

    if (strncmp(code, "Meta", 4) == 0) {
        if (strcmp(&code[4], "Left") == 0) {
            return QU_KEY_LSUPER;
        }

        if (strcmp(&code[4], "Right") == 0) {
            return QU_KEY_RSUPER;
        }
    }

    if (strncmp(code, "OS", 2) == 0) {
        if (strcmp(&code[2], "Left") == 0) {
            return QU_KEY_LSUPER;
        }

        if (strcmp(&code[2], "Right") == 0) {
            return QU_KEY_RSUPER;
        }
    }

    // Numpad

    if (strncmp(code, "Numpad", 6) == 0) {
        if (code[6] >= '0' && code[6] <= '9' && code[7] == '\0') {
            return QU_KEY_KP_0 + (code[6] - '0');
        }

        if (strcmp(&code[6], "Multiply") == 0) {
            return QU_KEY_KP_MUL;
        }

        if (strcmp(&code[6], "Add") == 0) {
            return QU_KEY_KP_ADD;
        }

        if (strcmp(&code[6], "Subtract") == 0) {
            return QU_KEY_KP_SUB;
        }

        if (strcmp(&code[6], "Decimal") == 0) {
            return QU_KEY_KP_POINT;
        }

        if (strcmp(&code[6], "Multiply") == 0) {
            return QU_KEY_KP_DIV;
        }

        if (strcmp(&code[6], "Enter") == 0) {
            return QU_KEY_KP_ENTER;
        }
    }

    // Other keys...

    if (strcmp(code, "Backquote") == 0) {
        return QU_KEY_GRAVE;
    }

    if (strcmp(code, "Quote") == 0) {
        return QU_KEY_APOSTROPHE;
    }

    if (strcmp(code, "Minus") == 0) {
        return QU_KEY_MINUS;
    }

    if (strcmp(code, "Equal") == 0) {
        return QU_KEY_EQUAL;
    }

    if (strncmp(code, "Bracket", 7) == 0) {
        if (strcmp(&code[7], "Left") == 0) {
            return QU_KEY_LBRACKET;
        }

        if (strcmp(&code[7], "Right") == 0) {
            return QU_KEY_RBRACKET;
        }
    }

    if (strcmp(code, "Comma") == 0) {
        return QU_KEY_COMMA;
    }

    if (strcmp(code, "Period") == 0) {
        return QU_KEY_PERIOD;
    }

    if (strcmp(code, "Semicolon") == 0) {
        return QU_KEY_SEMICOLON;
    }

    if (strcmp(code, "Slash") == 0) {
        return QU_KEY_SLASH;
    }

    if (strcmp(code, "Backslash") == 0) {
        return QU_KEY_BACKSLASH;
    }

    if (strcmp(code, "Space") == 0) {
        return QU_KEY_SPACE;
    }

    if (strcmp(code, "Escape") == 0) {
        return QU_KEY_ESCAPE;
    }

    if (strcmp(code, "Backspace") == 0) {
        return QU_KEY_BACKSPACE;
    }

    if (strcmp(code, "Tab") == 0) {
        return QU_KEY_TAB;
    }

    if (strcmp(code, "Enter") == 0) {
        return QU_KEY_ENTER;
    }

    if (strncmp(code, "Arrow", 5) == 0) {
        if (strcmp(&code[5], "Up") == 0) {
            return QU_KEY_UP;
        }

        if (strcmp(&code[5], "Down") == 0) {
            return QU_KEY_DOWN;
        }

        if (strcmp(&code[5], "Left") == 0) {
            return QU_KEY_LEFT;
        }

        if (strcmp(&code[5], "Right") == 0) {
            return QU_KEY_RIGHT;
        }
    }

    if (strcmp(code, "ContextMenu") == 0) {
        return QU_KEY_MENU;
    }

    if (strncmp(code, "Page", 4) == 0) {
        if (strcmp(&code[4], "Up") == 0) {
            return QU_KEY_PGUP;
        }

        if (strcmp(&code[4], "Down") == 0) {
            return QU_KEY_PGDN;
        }
    }

    if (strcmp(code, "Home") == 0) {
        return QU_KEY_HOME;
    }

    if (strcmp(code, "End") == 0) {
        return QU_KEY_END;
    }

    if (strcmp(code, "Insert") == 0) {
        return QU_KEY_INSERT;
    }
    
    if (strcmp(code, "Delete") == 0) {
        return QU_KEY_DELETE;
    }

    if (strcmp(code, "Pause") == 0) {
        return QU_KEY_PAUSE;
    }

    if (strcmp(code, "CapsLock") == 0) {
        return QU_KEY_CAPSLOCK;
    }

    if (strcmp(code, "ScrollLock") == 0) {
        return QU_KEY_SCROLLLOCK;
    }

    if (strcmp(code, "NumLock") == 0) {
        return QU_KEY_NUMLOCK;
    }

    return QU_KEY_INVALID;
}

static qu_mouse_button mouse_button_conv(int button)
{
    switch (button) {
    case 0:
        return QU_MOUSE_BUTTON_LEFT;
    case 1:
        return QU_MOUSE_BUTTON_MIDDLE;
    case 2:
        return QU_MOUSE_BUTTON_RIGHT;
    }

    return QU_MOUSE_BUTTON_INVALID;
}

//------------------------------------------------------------------------------

static void handle_keyboard_event(int type, EmscriptenKeyboardEvent const *event)
{
    qu_key key = key_conv(event->code);

    if (key == QU_KEY_INVALID) {
        return;
    }

    switch (type) {
    case EMSCRIPTEN_EVENT_KEYDOWN:
        qu__core_on_key_pressed(key);
        break;
    case EMSCRIPTEN_EVENT_KEYUP:
        qu__core_on_key_released(key);
        break;
    }
}

static void handle_mouse_event(int type, EmscriptenMouseEvent const *event)
{
    if (type == EMSCRIPTEN_EVENT_MOUSEMOVE) {
        qu__core_on_mouse_cursor_moved((int) event->targetX, (int) event->targetY);
        return;
    }

    qu_mouse_button button = mouse_button_conv(event->button);

    if (button == QU_MOUSE_BUTTON_INVALID) {
        return;
    }

    switch (type) {
    case EMSCRIPTEN_EVENT_MOUSEDOWN:
        qu__core_on_mouse_button_pressed(button);
        break;
    case EMSCRIPTEN_EVENT_MOUSEUP:
        qu__core_on_mouse_button_released(button);
        break;
    }
}

static void handle_wheel_event(int type, EmscriptenWheelEvent const *event)
{
    double scale;

    // These values are arbitrary.

    switch (event->deltaMode) {
    default:
    case DOM_DELTA_PIXEL:
        scale = 0.25f;
        break;
    case DOM_DELTA_LINE:
        scale = 1.f;
        break;
    case DOM_DELTA_PAGE:
        scale = 8.f;
        break;
    }

    int dx = (int) (event->deltaX * scale);
    int dy = (int) (event->deltaY * scale);

    qu__core_on_mouse_wheel_scrolled(dx, dy);
}

//------------------------------------------------------------------------------
// Custom event buffer

static void push_event(struct event_buffer *buffer, struct event *event)
{
    if (buffer->length == buffer->capacity) {
        buffer->capacity *= 2;

        if (buffer->capacity == 0) {
            buffer->capacity = 256;
        }

        buffer->array = realloc(buffer->array, sizeof(struct event) * buffer->capacity);

        if (!buffer->array) {
            QU_HALT("Out of memory: unable to grow Emscripten event buffer.");
        }
    }

    memcpy(&buffer->array[buffer->length++], event, sizeof(struct event));
}

static void execute_events(struct event_buffer *buffer)
{
    for (size_t i = 0; i < buffer->length; i++) {
        struct event *event = &buffer->array[i];

        switch (event->type) {
        case EMSCRIPTEN_EVENT_KEYDOWN:
        case EMSCRIPTEN_EVENT_KEYUP:
            handle_keyboard_event(event->type, &event->keyboard);
            break;
        case EMSCRIPTEN_EVENT_MOUSEMOVE:
        case EMSCRIPTEN_EVENT_MOUSEDOWN:
        case EMSCRIPTEN_EVENT_MOUSEUP:
            handle_mouse_event(event->type, &event->mouse);
            break;
        case EMSCRIPTEN_EVENT_WHEEL:
            handle_wheel_event(event->type, &event->wheel);
            break;
        }
    }

    buffer->length = 0;
}

//------------------------------------------------------------------------------
// Emscripten callbacks

static EM_BOOL keyboard_callback(int type, EmscriptenKeyboardEvent const *event, void *data)
{
    push_event(data, &(struct event) {
        .type = type,
        .keyboard = *event,
    });

    return EM_TRUE;
}

static EM_BOOL mouse_callback(int type, EmscriptenMouseEvent const *event, void *data)
{
    push_event(data, &(struct event) {
        .type = type,
        .mouse = *event,
    });

    return EM_TRUE;
}

static EM_BOOL wheel_callback(int type, EmscriptenWheelEvent const *event, void *data)
{
    push_event(data, &(struct event) {
        .type = type,
        .wheel = *event,
    });

    return EM_TRUE;
}

//------------------------------------------------------------------------------

static void initialize(qu_params const *params)
{
    memset(&priv, 0, sizeof(priv));

    EM_ASM({
        specialHTMLTargets["!canvas"] = Module.canvas;
    });

    char const *document = EMSCRIPTEN_EVENT_TARGET_DOCUMENT;
    char const *canvas = "!canvas";

    // Set keyboard event handlers.

    CHECK_EMSCRIPTEN(emscripten_set_keydown_callback(document, &priv.events, EM_TRUE, keyboard_callback),
        "Failed to set event callback: keydown");

    CHECK_EMSCRIPTEN(emscripten_set_keyup_callback(document, &priv.events, EM_TRUE, keyboard_callback),
        "Failed to set event callback: keyup");

    // Set mouse event handlers.

    CHECK_EMSCRIPTEN(emscripten_set_mousemove_callback(document, &priv.events, EM_TRUE, mouse_callback),
        "Failed to set event callback: mousemove");

    CHECK_EMSCRIPTEN(emscripten_set_mousedown_callback(document, &priv.events, EM_TRUE, mouse_callback),
        "Failed to set event callback: mousedown");

    CHECK_EMSCRIPTEN(emscripten_set_mouseup_callback(document, &priv.events, EM_TRUE, mouse_callback),
        "Failed to set event callback: mouseup");

    CHECK_EMSCRIPTEN(emscripten_set_wheel_callback(document, &priv.events, EM_TRUE, wheel_callback),
        "Failed to set event callback: wheel");

    // Create WebGL context.

    priv.gl = emscripten_webgl_create_context(canvas, &(EmscriptenWebGLContextAttributes) {
        .alpha = EM_TRUE,
        .depth = EM_TRUE,
        .stencil = EM_TRUE,
        .antialias = EM_TRUE,
        .premultipliedAlpha = EM_TRUE,
        .preserveDrawingBuffer = EM_FALSE,
        .powerPreference = EM_WEBGL_POWER_PREFERENCE_DEFAULT,
        .failIfMajorPerformanceCaveat = EM_FALSE,
        .majorVersion = 2,
        .minorVersion = 0,
        .enableExtensionsByDefault = EM_FALSE,
        .explicitSwapControl = EM_TRUE,
        .renderViaOffscreenBackBuffer = EM_TRUE,
        .proxyContextToMainThread = EMSCRIPTEN_WEBGL_CONTEXT_PROXY_DISALLOW,
    });

    if (!priv.gl) {
        QU_HALT("Failed to create WebGL context.");
    }

    CHECK_EMSCRIPTEN(emscripten_webgl_make_context_current(priv.gl),
        "Unable to activate WebGL context.");

    // Enable required WebGL extensions.

    if (!emscripten_webgl_enable_extension(priv.gl, "OES_vertex_array_object")) {
        QU_INFO("OES_vertex_array_object is not available.\n");
    }

    // Set proper canvas size.

    int width = params->display_width;
    int height = params->display_height;

    CHECK_EMSCRIPTEN(emscripten_set_canvas_element_size(canvas, width, height),
        "Unable to set HTML canvas size.");

    // Done.

    QU_INFO("Initialized.\n");
}

static void terminate(void)
{
    emscripten_webgl_destroy_context(priv.gl);
    free(priv.events.array);

    QU_INFO("Terminated.\n");
}

static bool process(void)
{
    execute_events(&priv.events);

    return true;
}

static void present(void)
{
    emscripten_webgl_commit_frame();
}

static enum qu__renderer get_renderer(void)
{
    return QU__RENDERER_ES2;
}

static void *gl_proc_address(char const *name)
{
    QU_HALT("Requesting GL procedure address is not allowed in Emscripten. Fix the library code.");
    return NULL;
}

static int get_gl_multisample_samples(void)
{
    return 1;
}

//------------------------------------------------------------------------------

struct qu__core const qu__core_emscripten = {
    .initialize = initialize,
    .terminate = terminate,
    .process = process,
    .present = present,
    .get_renderer = get_renderer,
    .gl_proc_address = gl_proc_address,
    .get_gl_multisample_samples = get_gl_multisample_samples,
};
