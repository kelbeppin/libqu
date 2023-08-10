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
#include <SDL.h>

//------------------------------------------------------------------------------
// qu_core_emscripten.c: Emscripten core module
// TODO: use html5.h API
//------------------------------------------------------------------------------

static struct
{
    SDL_Surface *display;

    struct {
        int dx_wheel, dy_wheel;
    } input;

    struct {
        qu_mouse_wheel_fn on_mouse_wheel_scrolled;
    } callbacks;
} impl;

//------------------------------------------------------------------------------

static qu_key key_conv(SDL_Keysym *sym)
{
    switch (sym->sym) {
    case SDLK_0:                return QU_KEY_0;
    case SDLK_1:                return QU_KEY_1;
    case SDLK_2:                return QU_KEY_2;
    case SDLK_3:                return QU_KEY_3;
    case SDLK_4:                return QU_KEY_4;
    case SDLK_5:                return QU_KEY_5;
    case SDLK_6:                return QU_KEY_6;
    case SDLK_7:                return QU_KEY_7;
    case SDLK_8:                return QU_KEY_8;
    case SDLK_9:                return QU_KEY_9;
    case SDLK_a:                return QU_KEY_A;
    case SDLK_b:                return QU_KEY_B;
    case SDLK_c:                return QU_KEY_C;
    case SDLK_d:                return QU_KEY_D;
    case SDLK_e:                return QU_KEY_E;
    case SDLK_f:                return QU_KEY_F;
    case SDLK_g:                return QU_KEY_G;
    case SDLK_h:                return QU_KEY_H;
    case SDLK_i:                return QU_KEY_I;
    case SDLK_j:                return QU_KEY_J;
    case SDLK_k:                return QU_KEY_K;
    case SDLK_l:                return QU_KEY_L;
    case SDLK_m:                return QU_KEY_M;
    case SDLK_n:                return QU_KEY_N;
    case SDLK_o:                return QU_KEY_O;
    case SDLK_p:                return QU_KEY_P;
    case SDLK_q:                return QU_KEY_Q;
    case SDLK_r:                return QU_KEY_R;
    case SDLK_s:                return QU_KEY_S;
    case SDLK_t:                return QU_KEY_T;
    case SDLK_u:                return QU_KEY_U;
    case SDLK_w:                return QU_KEY_W;
    case SDLK_x:                return QU_KEY_X;
    case SDLK_y:                return QU_KEY_Y;
    case SDLK_z:                return QU_KEY_Z;
    case SDLK_BACKQUOTE:        return QU_KEY_GRAVE;
    case SDLK_QUOTE:            return QU_KEY_APOSTROPHE;
    case SDLK_MINUS:            return QU_KEY_MINUS;
    case SDLK_EQUALS:           return QU_KEY_EQUAL;
    case SDLK_LEFTBRACKET:      return QU_KEY_LBRACKET;
    case SDLK_RIGHTBRACKET:     return QU_KEY_RBRACKET;
    case SDLK_COMMA:            return QU_KEY_COMMA;
    case SDLK_PERIOD:           return QU_KEY_PERIOD;
    case SDLK_SEMICOLON:        return QU_KEY_SEMICOLON;
    case SDLK_SLASH:            return QU_KEY_SLASH;
    case SDLK_BACKSLASH:        return QU_KEY_BACKSLASH;
    case SDLK_SPACE:            return QU_KEY_SPACE;
    case SDLK_ESCAPE:           return QU_KEY_ESCAPE;
    case SDLK_BACKSPACE:        return QU_KEY_BACKSPACE;
    case SDLK_TAB:              return QU_KEY_TAB;
    case SDLK_RETURN:           return QU_KEY_ENTER;
    case SDLK_F1:               return QU_KEY_F1;
    case SDLK_F2:               return QU_KEY_F2;
    case SDLK_F3:               return QU_KEY_F3;
    case SDLK_F4:               return QU_KEY_F4;
    case SDLK_F5:               return QU_KEY_F5;
    case SDLK_F6:               return QU_KEY_F6;
    case SDLK_F7:               return QU_KEY_F7;
    case SDLK_F8:               return QU_KEY_F8;
    case SDLK_F9:               return QU_KEY_F9;
    case SDLK_F10:              return QU_KEY_F10;
    case SDLK_F11:              return QU_KEY_F11;
    case SDLK_F12:              return QU_KEY_F12;
    case SDLK_UP:               return QU_KEY_UP;
    case SDLK_DOWN:             return QU_KEY_DOWN;
    case SDLK_LEFT:             return QU_KEY_LEFT;
    case SDLK_RIGHT:            return QU_KEY_RIGHT;
    case SDLK_LSHIFT:           return QU_KEY_LSHIFT;
    case SDLK_RSHIFT:           return QU_KEY_RSHIFT;
    case SDLK_LCTRL:            return QU_KEY_LCTRL;
    case SDLK_RCTRL:            return QU_KEY_RCTRL;
    case SDLK_LALT:             return QU_KEY_LALT;
    case SDLK_RALT:             return QU_KEY_RALT;
    case SDLK_LSUPER:           return QU_KEY_LSUPER;
    case SDLK_RSUPER:           return QU_KEY_RSUPER;
    case SDLK_MENU:             return QU_KEY_MENU;
    case SDLK_PAGEUP:           return QU_KEY_PGUP;
    case SDLK_PAGEDOWN:         return QU_KEY_PGDN;
    case SDLK_HOME:             return QU_KEY_HOME;
    case SDLK_END:              return QU_KEY_END;
    case SDLK_INSERT:           return QU_KEY_INSERT;
    case SDLK_DELETE:           return QU_KEY_DELETE;
    case SDLK_PRINTSCREEN:      return QU_KEY_PRINTSCREEN;
    case SDLK_PAUSE:            return QU_KEY_PAUSE;
    case SDLK_CAPSLOCK:         return QU_KEY_CAPSLOCK;
    case SDLK_SCROLLLOCK:       return QU_KEY_SCROLLLOCK;
    case SDLK_NUMLOCK:          return QU_KEY_NUMLOCK;
    case SDLK_KP_0:             return QU_KEY_KP_0;
    case SDLK_KP_1:             return QU_KEY_KP_1;
    case SDLK_KP_2:             return QU_KEY_KP_2;
    case SDLK_KP_3:             return QU_KEY_KP_3;
    case SDLK_KP_4:             return QU_KEY_KP_4;
    case SDLK_KP_5:             return QU_KEY_KP_5;
    case SDLK_KP_6:             return QU_KEY_KP_6;
    case SDLK_KP_7:             return QU_KEY_KP_7;
    case SDLK_KP_8:             return QU_KEY_KP_8;
    case SDLK_KP_9:             return QU_KEY_KP_9;
    case SDLK_KP_MULTIPLY:      return QU_KEY_KP_MUL;
    case SDLK_KP_PLUS:          return QU_KEY_KP_ADD;
    case SDLK_KP_MINUS:         return QU_KEY_KP_SUB;
    case SDLK_KP_PERIOD:        return QU_KEY_KP_POINT;
    case SDLK_KP_DIVIDE:        return QU_KEY_KP_DIV;
    case SDLK_KP_ENTER:         return QU_KEY_KP_ENTER;
    default:                    return QU_KEY_INVALID;
    }
}

static qu_mouse_button mb_conv(Uint8 button)
{
    switch (button) {
        case SDL_BUTTON_LEFT:   return QU_MOUSE_BUTTON_LEFT;
        case SDL_BUTTON_MIDDLE: return QU_MOUSE_BUTTON_MIDDLE;
        case SDL_BUTTON_RIGHT:  return QU_MOUSE_BUTTON_RIGHT;
        default:
            break;
    }

    return QU_MOUSE_BUTTON_INVALID;
}

//------------------------------------------------------------------------------

static void initialize(qu_params const *params)
{
    memset(&impl, 0, sizeof(impl));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) == -1) {
        QU_HALT("Failed to initialize SDL.\n");
    }

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    impl.display = SDL_SetVideoMode(
        params->display_width,
        params->display_height,
        32,
        SDL_OPENGL | SDL_RESIZABLE
    );

    if (!impl.display) {
        QU_HALT("Failed to initialize display.\n");
    }

    SDL_WM_SetCaption(params->title, NULL);
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

    QU_INFO("Emscripten core module initialized.\n");
}

static void terminate(void)
{
    SDL_Quit();
    QU_INFO("Emscripten core module terminated.\n");
}

static bool process(void)
{
    impl.input.dx_wheel = 0;
    impl.input.dy_wheel = 0;

    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                return false;
            case SDL_KEYDOWN:
                qu__core_on_key_pressed(key_conv(&event.key.keysym));
                break;
            case SDL_KEYUP:
                qu__core_on_key_released(key_conv(&event.key.keysym));
                break;
            case SDL_MOUSEBUTTONDOWN:
                qu__core_on_mouse_button_pressed(mb_conv(event.button.button));
                break;
            case SDL_MOUSEBUTTONUP:
                qu__core_on_mouse_button_released(mb_conv(event.button.button));
                break;
            case SDL_MOUSEMOTION:
                qu__core_on_mouse_cursor_moved(event.motion.x, event.motion.y);
                break;
            case SDL_MOUSEWHEEL: {
                impl.input.dx_wheel = event.wheel.x;
                impl.input.dy_wheel = event.wheel.y;

                if (impl.callbacks.on_mouse_wheel_scrolled) {
                    impl.callbacks.on_mouse_wheel_scrolled(
                        impl.input.dx_wheel, impl.input.dy_wheel
                    );
                }
                break;
            }
            default:
                break;
        }
    }

    return true;
}

static void present(void)
{
    SDL_GL_SwapBuffers();
}

static struct qu__graphics const *get_graphics(void)
{
    return &qu__graphics_es2;
}

static struct qu__audio const *get_audio(void)
{
    return &qu__audio_openal;
}

static bool gl_check_extension(char const *name)
{
    return false;
}

static void *gl_proc_address(char const *name)
{
    return NULL;
}

//------------------------------------------------------------------------------

static qu_vec2i get_mouse_wheel_delta(void)
{
    return (qu_vec2i) { impl.input.dx_wheel, impl.input.dy_wheel };
}

//------------------------------------------------------------------------------

static bool is_joystick_connected(int joystick)
{
    return false;
}

static char const *get_joystick_id(int joystick)
{
    return NULL;
}

static int get_joystick_button_count(int joystick)
{
    return 0;
}

static int get_joystick_axis_count(int joystick)
{
    return 0;
}

static char const *get_joystick_button_id(int joystick, int button)
{
    return NULL;
}

static char const *get_joystick_axis_id(int joystick, int axis)
{
    return NULL;
}

static bool is_joystick_button_pressed(int joystick, int button)
{
    return false;
}

static float get_joystick_axis_value(int joystick, int axis)
{
    return 0.f;
}

//------------------------------------------------------------------------------

static void on_mouse_wheel_scrolled(qu_mouse_wheel_fn fn)
{
    impl.callbacks.on_mouse_wheel_scrolled = fn;
}

//------------------------------------------------------------------------------

struct qu__core const qu__core_emscripten = {
    .initialize = initialize,
    .terminate = terminate,
    .process = process,
    .present = present,
    .get_graphics = get_graphics,
    .get_audio = get_audio,
    .gl_check_extension = gl_check_extension,
    .gl_proc_address = gl_proc_address,
    .get_mouse_wheel_delta = get_mouse_wheel_delta,
    .is_joystick_connected = is_joystick_connected,
    .get_joystick_id = get_joystick_id,
    .get_joystick_button_count = get_joystick_button_count,
    .get_joystick_axis_count = get_joystick_axis_count,
    .is_joystick_button_pressed = is_joystick_button_pressed,
    .get_joystick_axis_value = get_joystick_axis_value,
    .get_joystick_button_id = get_joystick_button_id,
    .get_joystick_axis_id = get_joystick_axis_id,
    .on_mouse_wheel_scrolled = on_mouse_wheel_scrolled,
};
