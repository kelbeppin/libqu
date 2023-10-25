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
// libqu.h: public header file
//------------------------------------------------------------------------------

#ifndef LIBQU_H
#define LIBQU_H

//------------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

//------------------------------------------------------------------------------
// Version

#define QU_VERSION              "0.5.0-dev"

//------------------------------------------------------------------------------
// Exported function attribute

#if defined(QU_SHARED)

#if defined(_MSC_VER)

#if defined(QU_BUILD)
#define QU_API                  __declspec(dllexport)
#else
#define QU_API                  __declspec(dllimport)
#endif

#elif defined(__GNUC__)

#define QU_API                  __attribute__((visibility("default")))

#endif

#endif // defined(QU_SHARED)

#ifndef QU_API
#define QU_API
#endif

//------------------------------------------------------------------------------
// Calling conventions

#if defined(_WIN32)
#define QU_CALL                 __cdecl
#else
#define QU_CALL
#endif

//------------------------------------------------------------------------------
// Non-returning function attribute

#if defined(_MSC_VER)
#define QU_NO_RET               __declspec(noreturn)
#elif defined(__GNUC__)
#define QU_NO_RET               __attribute__((__noreturn__))
#else
#define QU_NO_RET
#endif

//------------------------------------------------------------------------------

/**
 * C++ lacks C99's compound literals. However, since C++11, plain structures
 * can be initialized with slightly different syntax:
 * `xyz_person { "John", "Doe" }`
 *
 * The same thing, but without parentheses. Sadly, this doesn't support
 * designated initializers.
 */

#ifdef __cplusplus
#define QU_COMPOUND(type)       type
#else
#define QU_COMPOUND(type)       (type)
#endif

//------------------------------------------------------------------------------

/**
 * @defgroup macros Macros
 * @brief Preprocessor macros.
 * @{
 */

/**
 * Approximate value of Pi.
 */
#define QU_PI                   (3.14159265358979323846)

/**
 * Maximum number of touch points.
 */
#define QU_MAX_TOUCH_INPUTS     (16)

/**
 * Predefined blending mode: none.
 * No blending is done, dst is overwritten by src.
 */
#define QU_BLEND_MODE_NONE \
    QU_COMPOUND(qu_blend_mode) { \
        QU_BLEND_ONE, QU_BLEND_ZERO, QU_BLEND_ADD, \
        QU_BLEND_ONE, QU_BLEND_ZERO, QU_BLEND_ADD, \
    }

/**
 * Predefined blending mode: alpha.
 */
#define QU_BLEND_MODE_ALPHA \
    QU_COMPOUND(qu_blend_mode) { \
        QU_BLEND_SRC_ALPHA, QU_BLEND_ONE_MINUS_SRC_ALPHA, QU_BLEND_ADD, \
        QU_BLEND_SRC_ALPHA, QU_BLEND_ONE_MINUS_SRC_ALPHA, QU_BLEND_ADD, \
    }

/**
 * Predefined blending mode: add.
 */
#define QU_BLEND_MODE_ADD \
    QU_COMPOUND(qu_blend_mode) { \
        QU_BLEND_SRC_ALPHA, QU_BLEND_ONE, QU_BLEND_ADD, \
        QU_BLEND_SRC_ALPHA, QU_BLEND_ONE, QU_BLEND_ADD, \
    }

/**
 * Predefined blending mode: multiply.
 */
#define QU_BLEND_MODE_MUL \
    QU_COMPOUND(qu_blend_mode) { \
        QU_BLEND_ZERO, QU_BLEND_SRC_COLOR, QU_BLEND_ADD, \
        QU_BLEND_ZERO, QU_BLEND_SRC_ALPHA, QU_BLEND_ADD, \
    }

/**
 * Shortcut macro to quickly define custom blend mode.
 */
#define QU_DEFINE_BLEND_MODE(src_factor, dst_factor) \
    QU_COMPOUND(qu_blend_mode) { \
        (src_factor), (dst_factor), QU_BLEND_ADD, \
        (src_factor), (dst_factor), QU_BLEND_ADD, \
    }

/**
 * Shortcut macro to quickly define custom blend mode with custom equation.
 */
#define QU_DEFINE_BLEND_MODE_EX(src_factor, dst_factor, equation) \
    QU_COMPOUND(qu_blend_mode) { \
        (src_factor), (dst_factor), (equation), \
        (src_factor), (dst_factor), (equation), \
    }

/**
 * Convert degrees to radians.
 *
 * @param deg Angle in degrees.
 */
#define QU_DEG2RAD(deg)         ((deg) * (QU_PI / 180.0))

/**
 * Convert radians to degrees.
 *
 * @param rad Angle in radians.
 */
#define QU_RAD2DEG(rad)         ((rad) * (180.0 / QU_PI))

/**
 * Get maximum of two values.
 *
 * @param a First value.
 * @param b Second value.
 */
#define QU_MAX(a, b)            ((a) > (b) ? (a) : (b))

/**
 * Get minimum of two values.
 *
 * @param a First value.
 * @param b Second value.
 */
#define QU_MIN(a, b)            ((a) < (b) ? (a) : (b))

/**
 * Get color value from individual RGB components.
 *
 * @param red Red component (in range 0-255).
 * @param green Green component (in range 0-255).
 * @param blue Blue component (in range 0-255).
 */
#define QU_COLOR(red, green, blue) \
    (0xff000000 | (red) << 16 | (green) << 8 | (blue))

/**
 * Get color value from individual RGBA components.
 *
 * @param red Red component (in range 0-255).
 * @param green Green component (in range 0-255).
 * @param blue Blue component (in range 0-255).
 * @param alpha Alpha component (in range 0-255).
 */
#define QU_RGBA(red, green, blue, alpha) \
    ((alpha) << 24 | (red) << 16 | (green) << 8 | (blue))

/**@}*/

//------------------------------------------------------------------------------

/**
 * @defgroup types Types
 * @brief Definitions of custom types, including enumerations and structures.
 * @{
 */

/**
 * Color type.
 *
 * Assumed to hold components in ARGB order.
 */
typedef uint64_t qu_color;

/**
 * Keys of keyboard.
 */
typedef enum qu_key
{
    QU_KEY_INVALID = -1,        /*!< Special value for invalid key */
    QU_KEY_0 = 0,               /*!< '0' key */
    QU_KEY_1,                   /*!< '1' key */
    QU_KEY_2,                   /*!< '2' key */
    QU_KEY_3,                   /*!< '3' key */
    QU_KEY_4,                   /*!< '4' key */
    QU_KEY_5,                   /*!< '5' key */
    QU_KEY_6,                   /*!< '6' key */
    QU_KEY_7,                   /*!< '7' key */
    QU_KEY_8,                   /*!< '8' key */
    QU_KEY_9,                   /*!< '9' key */
    QU_KEY_A,                   /*!< 'A' key */
    QU_KEY_B,                   /*!< 'B' key */
    QU_KEY_C,                   /*!< 'C' key */
    QU_KEY_D,                   /*!< 'D' key */
    QU_KEY_E,                   /*!< 'E' key */
    QU_KEY_F,                   /*!< 'F' key */
    QU_KEY_G,                   /*!< 'G' key */
    QU_KEY_H,                   /*!< 'H' key */
    QU_KEY_I,                   /*!< 'I' key */
    QU_KEY_J,                   /*!< 'J' key */
    QU_KEY_K,                   /*!< 'K' key */
    QU_KEY_L,                   /*!< 'L' key */
    QU_KEY_M,                   /*!< 'M' key */
    QU_KEY_N,                   /*!< 'N' key */
    QU_KEY_O,                   /*!< 'O' key */
    QU_KEY_P,                   /*!< 'P' key */
    QU_KEY_Q,                   /*!< 'Q' key */
    QU_KEY_R,                   /*!< 'R' key */
    QU_KEY_S,                   /*!< 'S' key */
    QU_KEY_T,                   /*!< 'T' key */
    QU_KEY_U,                   /*!< 'U' key */
    QU_KEY_V,                   /*!< 'V' key */
    QU_KEY_W,                   /*!< 'W' key */
    QU_KEY_X,                   /*!< 'X' key */
    QU_KEY_Y,                   /*!< 'Y' key */
    QU_KEY_Z,                   /*!< 'Z' key */
    QU_KEY_GRAVE,               /*!< Backtick (tilde) key */
    QU_KEY_APOSTROPHE,          /*!< Quote key */
    QU_KEY_MINUS,               /*!< Minus (underscore) key */
    QU_KEY_EQUAL,               /*!< Equal (plus) key */
    QU_KEY_LBRACKET,            /*!< Left square bracket key */
    QU_KEY_RBRACKET,            /*!< Right square bracket key */
    QU_KEY_COMMA,               /*!< Comma key */
    QU_KEY_PERIOD,              /*!< Period key */
    QU_KEY_SEMICOLON,           /*!< Semicolon key */
    QU_KEY_SLASH,               /*!< Slash key */
    QU_KEY_BACKSLASH,           /*!< Backslash key */
    QU_KEY_SPACE,               /*!< Space key */
    QU_KEY_ESCAPE,              /*!< Escape key */
    QU_KEY_BACKSPACE,           /*!< Backspace key */
    QU_KEY_TAB,                 /*!< Tab key */
    QU_KEY_ENTER,               /*!< Enter key */
    QU_KEY_F1,                  /*!< F1 key */
    QU_KEY_F2,                  /*!< F2 key */
    QU_KEY_F3,                  /*!< F3 key */
    QU_KEY_F4,                  /*!< F4 key */
    QU_KEY_F5,                  /*!< F5 key */
    QU_KEY_F6,                  /*!< F6 key */
    QU_KEY_F7,                  /*!< F7 key */
    QU_KEY_F8,                  /*!< F8 key */
    QU_KEY_F9,                  /*!< F9 key */
    QU_KEY_F10,                 /*!< F10 key */
    QU_KEY_F11,                 /*!< F11 key */
    QU_KEY_F12,                 /*!< F12 key */
    QU_KEY_UP,                  /*!< Up arrow key */
    QU_KEY_DOWN,                /*!< Down arrow key */
    QU_KEY_LEFT,                /*!< Left arrow key */
    QU_KEY_RIGHT,               /*!< Right arrow key */
    QU_KEY_LSHIFT,              /*!< Left Shift key */
    QU_KEY_RSHIFT,              /*!< Right Shift key */
    QU_KEY_LCTRL,               /*!< Left Control key */
    QU_KEY_RCTRL,               /*!< Right Control key */
    QU_KEY_LALT,                /*!< Left Alt key */
    QU_KEY_RALT,                /*!< Right Alt (AltGr) key */
    QU_KEY_LSUPER,              /*!< Left OS (Windows) key */
    QU_KEY_RSUPER,              /*!< Right OS (Windows) key */
    QU_KEY_MENU,                /*!< Context Menu key */
    QU_KEY_PGUP,                /*!< PageUp key */
    QU_KEY_PGDN,                /*!< PageDown key */
    QU_KEY_HOME,                /*!< Home key */
    QU_KEY_END,                 /*!< End key */
    QU_KEY_INSERT,              /*!< Insert key */
    QU_KEY_DELETE,              /*!< Delete key */
    QU_KEY_PRINTSCREEN,         /*!< PrintScreen key */
    QU_KEY_PAUSE,               /*!< Pause (Break) key */
    QU_KEY_CAPSLOCK,            /*!< Caps Lock key */
    QU_KEY_SCROLLLOCK,          /*!< Scroll Lock key */
    QU_KEY_NUMLOCK,             /*!< Num Lock key */
    QU_KEY_KP_0,                /*!< Numpad 0 key */
    QU_KEY_KP_1,                /*!< Numpad 1 key */
    QU_KEY_KP_2,                /*!< Numpad 2 key */
    QU_KEY_KP_3,                /*!< Numpad 3 key */
    QU_KEY_KP_4,                /*!< Numpad 4 key */
    QU_KEY_KP_5,                /*!< Numpad 5 key */
    QU_KEY_KP_6,                /*!< Numpad 6 key */
    QU_KEY_KP_7,                /*!< Numpad 7 key */
    QU_KEY_KP_8,                /*!< Numpad 8 key */
    QU_KEY_KP_9,                /*!< Numpad 9 key */
    QU_KEY_KP_MUL,              /*!< Numpad Multiply key */
    QU_KEY_KP_ADD,              /*!< Numpad Add key */
    QU_KEY_KP_SUB,              /*!< Numpad Subtract key */
    QU_KEY_KP_POINT,            /*!< Numpad Decimal Point key */
    QU_KEY_KP_DIV,              /*!< Numpad Divide key */
    QU_KEY_KP_ENTER,            /*!< Numpad Enter */
    QU_TOTAL_KEYS,              /*!< Total count of supported keys */
} qu_key;

/**
 * State of a single key.
 */
typedef enum qu_key_state
{
    QU_KEY_IDLE,                /*!< Not pressed */
    QU_KEY_PRESSED,             /*!< Being hold down */
    QU_KEY_RELEASED,            /*!< Released just now (during last frame) */
} qu_key_state;

/**
 * Mouse buttons.
 */
typedef enum qu_mouse_button
{
    QU_MOUSE_BUTTON_INVALID = -1, /*!< Special value for invalid button */
    QU_MOUSE_BUTTON_LEFT = 0,   /*!< Left mouse button */
    QU_MOUSE_BUTTON_RIGHT,      /*!< Right mouse button */
    QU_MOUSE_BUTTON_MIDDLE,     /*!< Middle mouse button */
    QU_TOTAL_MOUSE_BUTTONS,     /*!< Total count of supported mouse buttons */
} qu_mouse_button;

/**
 * Bitmasks of mouse buttons.
 */
typedef enum qu_mouse_button_bits
{
    QU_MOUSE_BUTTON_LEFT_BIT = (1 << QU_MOUSE_BUTTON_LEFT),
    QU_MOUSE_BUTTON_RIGHT_BIT = (1 << QU_MOUSE_BUTTON_RIGHT),
    QU_MOUSE_BUTTON_MIDDLE_BIT = (1 << QU_MOUSE_BUTTON_MIDDLE),
} qu_mouse_button_bits;

/**
 * Blend mode factors.
 */
typedef enum qu_blend_factor
{
    QU_BLEND_ZERO,              /*!< `(0, 0, 0, 0)` */
    QU_BLEND_ONE,               /*!< `(1, 1, 1, 1)` */
    QU_BLEND_SRC_COLOR,         /*!< `(src.r, src.g, src.b, src.a)` */
    QU_BLEND_ONE_MINUS_SRC_COLOR, /*!< `(1 - src.r, 1 - src.g, 1 - src.b, 1 - src.a)` */
    QU_BLEND_DST_COLOR,         /*!< `(dst.r, dst.g, dst.b, dst.a)` */
    QU_BLEND_ONE_MINUS_DST_COLOR, /*!< `(1 - dst.r, 1 - dst.g, 1 - dst.b, 1 - dst.a)` */
    QU_BLEND_SRC_ALPHA,         /*!< `(src.a, src.a, src.a, src.a)` */
    QU_BLEND_ONE_MINUS_SRC_ALPHA, /*!< `(1 - src.a, 1 - src.a, 1 - src.a, 1 - src.a)` */
    QU_BLEND_DST_ALPHA,         /*!< `(dst.a, dst.a, dst.a, dst.a)` */
    QU_BLEND_ONE_MINUS_DST_ALPHA, /*!< `(1 - dst.a, 1 - dst.a, 1 - dst.a, 1 - dst.a)` */
} qu_blend_factor;

/**
 * Blend mode equations.
 */
typedef enum qu_blend_equation
{
    QU_BLEND_ADD,               /*!< `src * sfactor + dst * dfactor` */
    QU_BLEND_SUB,               /*!< `src * sfactor - dst * dfactor` */
    QU_BLEND_REV_SUB,           /*!< `dst * dfactor - src * sfactor` */
} qu_blend_equation;

/**
 * Two-dimensional vector of floating-point values.
 */
typedef struct qu_vec2f
{
    float x;                    /*!< X-component of the vector */
    float y;                    /*!< Y-component of the vector */
} qu_vec2f;

/**
 * Two-dimensional vector of integer values.
 */
typedef struct qu_vec2i
{
    int x;                      /*!< X-component of the vector */
    int y;                      /*!< Y-component of the vector */
} qu_vec2i;

/**
 * Texture handle.
 */
typedef struct qu_texture
{
    int32_t id;                 /*!< Identifier */
} qu_texture;

/**
 * Surface handle.
 */
typedef struct qu_surface
{
    int32_t id;                 /*!< Identifier */
} qu_surface;

/**
 * Font handle.
 */
typedef struct qu_font
{
    int32_t id;                 /*!< Identifier */
} qu_font;

/**
 * Sound handle.
 */
typedef struct qu_sound
{
    int32_t id;                 /*!< Identifier */
} qu_sound;

/**
 * Music handle.
 */
typedef struct qu_music
{
    int32_t id;                 /*!< Identifier */
} qu_music;

/**
 * Voice handle.
 */
typedef struct qu_voice
{
    int32_t id;                 /*!< Identifier */
} qu_voice;

/**
 * Initialization parameters.
 *
 * This structure holds parameters that are define how the library
 * is initialized.
 */
typedef struct qu_params
{
    char const *title;          /*!< Window title */
    
    int antialiasing_level;     /*!< MSAA level */

    int display_width;          /*!< Window width */
    int display_height;         /*!< Window height */

    bool enable_canvas;         /*!< True to enable canvas */
    bool canvas_smooth;         /*!< True to make canvas smooth */
    int canvas_antialiasing_level; /*!< Canvas MSAA level */
    int canvas_width;           /*!< Canvas width */
    int canvas_height;          /*!< Canvas height */
} qu_params;

/**
 * Keyboard state.
 *
 * This structure holds state of all keyboard keys.
 */
typedef struct qu_keyboard_state
{
    qu_key_state keys[QU_TOTAL_KEYS]; /*!< Array of key states */
} qu_keyboard_state;

/**
 * Blend mode.
 *
 * This structure defines the blending mode.
 */
typedef struct qu_blend_mode
{
    qu_blend_factor color_src_factor; /*!< Source pixel color factor */
    qu_blend_factor color_dst_factor; /*!< Destination pixel color factor */
    qu_blend_equation color_equation; /*!< Color equation */

    qu_blend_factor alpha_src_factor; /*!< Source pixel alpha factor */
    qu_blend_factor alpha_dst_factor; /*!< Destination pixel alpha factor */
    qu_blend_equation alpha_equation; /*!< Alpha equation */
} qu_blend_mode;

/**
 * Callback function for the main loop.
 * @return False if the loop should stop, and true otherwise.
 */
typedef bool (*qu_loop_fn)(void);

/**
 * Keyboard event callback.
 */
typedef void (*qu_key_fn)(qu_key key);

/**
 * Mouse button event callback.
 */
typedef void (*qu_mouse_button_fn)(qu_mouse_button button);

/**
 * Mouse wheel event callback.
 */
typedef void (*qu_mouse_wheel_fn)(int x_delta, int y_delta);

/**
 * Mouse cursor event callback.
 */
typedef void (*qu_mouse_cursor_fn)(int x_delta, int y_delta);

/**@}*/

//------------------------------------------------------------------------------

#if defined(__cplusplus)
extern "C" {
#endif

//------------------------------------------------------------------------------
// Base

/**
 * @defgroup base Base
 * @brief Basic functions related to initialization, cleanup, etc.
 * @{
 */

/**
 * Initialize the library with the specified parameters.
 *
 * This function initializes the library with the provided parameters.
 * If the `params` argument is set to NULL, default parameters will be used.
 *
 * @param params A pointer to a structure containing initialization parameters.
 *               Use NULL to use default parameters.
 */
QU_API void QU_CALL qu_initialize(qu_params const *params);

/**
 * Terminate the library and clean up resources.
 *
 * This function terminates the library and performs necessary clean-up
 * operations to release allocated resources.
 * After calling this function, the library is no longer usable until it is
 * initialized again.
 */
QU_API void QU_CALL qu_terminate(void);

/**
 * Process user input.
 * 
 * This function processes user input and updates internal state accordingly.
 * It should be called every frame to handle user input.
 * 
 * @return Returns false if the user wants to close the application/game, and
 *         true otherwise.
 */
QU_API bool QU_CALL qu_process(void);

/**
 * Starts the automatic game loop.
 * 
 * The automatic loop is the only way to handle game loop on Web and mobile
 * platforms.
 * This function does not return.
 * 
 * @param loop_fn Callback function that will be called every frame.
 */
QU_API QU_NO_RET void QU_CALL qu_execute(qu_loop_fn loop_fn);

/**
 * Swap buffers.
 * 
 * This function should be called when the rendering is finished in order to
 * put on the screen everything that was drawn so far.
 */
QU_API void QU_CALL qu_present(void);

/**@}*/

//------------------------------------------------------------------------------
// Core

/**
 * @defgroup core Core
 * @brief Functions for window and user input management.
 * @{
 */

//----------------------------------------------------------
// Window

/**
 * @name Window
 * @brief Functions related to window size, etc.
 * @{
 */

/**
 * Get window title.
 * 
 * @return Current title of window. No need to free.
 */
QU_API char const * QU_CALL qu_get_window_title(void);

/**
 * Update window title.
 * 
 * Note: has no effect on Android and Emscripten.
 * 
 * @param title New window title.
 */
QU_API void QU_CALL qu_set_window_title(char const *title);

/**
 * Get window size.
 * 
 * @return Current window size in pixels (excluding title bar
 *         and other OS-specific elements).
 */
QU_API qu_vec2i QU_CALL qu_get_window_size(void);

/**
 * Resize window.
 * 
 * Note: has no effect on Android and Emscripten.
 * 
 * @param width New width of window.
 * @param height New height of window.
 */
QU_API void QU_CALL qu_set_window_size(int width, int height);

/**
 * Check if window is focused.
 *
 * @return True if the libqu window is focused.
 */
QU_API bool QU_CALL qu_is_window_active(void);

/**@}*/

//----------------------------------------------------------
// Keyboard

/**
 * @name Keyboard
 * @brief These functions can be used to handle keyboard input.
 * @{
 */

/**
 * Get current keyboard state.
 * 
 * @return Pointer to a struct that holds keyboard state.
 */
QU_API qu_keyboard_state const * QU_CALL qu_get_keyboard_state(void);

/**
 * Get current state of a particular key.
 */
QU_API qu_key_state QU_CALL qu_get_key_state(qu_key key);

/**
 * Check if a key is pressed.
 * 
 * @param key Key value.
 * @return True if the specified key is pressed.
 */
QU_API bool QU_CALL qu_is_key_pressed(qu_key key);

/**
 * Set key press callback.
 * The callback will be called if a key is pressed.
 *
 * @param fn Key event callback.
 */
QU_API void QU_CALL qu_on_key_pressed(qu_key_fn fn);

/**
 * Set key repeat callback.
 * The callback will be repeatedly called if a key is hold down.
 *
 * @param fn Key event callback.
 */
QU_API void QU_CALL qu_on_key_repeated(qu_key_fn fn);

/**
 * Set key repeat callback.
 * The callback will be called if a key is released.
 *
 * @param fn Key event callback.
 */
QU_API void QU_CALL qu_on_key_released(qu_key_fn fn);

/**@}*/

//----------------------------------------------------------
// Mouse

/**
 * @name Mouse
 * @brief These functions can be used to handle mouse input, including
 *        mouse buttons and wheel.
 * @{
 */

/**
 * Get current mouse button state.
 * 
 * @return Bitmask representing button state.
 */
QU_API uint8_t QU_CALL qu_get_mouse_button_state(void);

/**
 * Check if mouse button is pressed.
 * 
 * @param button Mouse button.
 * @return True if the button is pressed, otherwise false.
 */
QU_API bool QU_CALL qu_is_mouse_button_pressed(qu_mouse_button button);

/**
 * Get mouse cursor position.
 * Cursor position is relative to the window.
 * Top-left corner of the window is (0, 0) coordinate.
 * Cursor position is not updated if it's outside of the window.
 * 
 * @return Cursor position in a 2D vector.
 */
QU_API qu_vec2i QU_CALL qu_get_mouse_cursor_position(void);

/**
 * Get mouse cursor movement delta.
 * This function returns how far cursor moved since the last frame.
 * 
 * @return Cursor movement delta in a 2D vector.
 */
QU_API qu_vec2i QU_CALL qu_get_mouse_cursor_delta(void);

/**
 * Get mouse wheel movement delta.
 * This function returns how much wheel scrolled since the last frame.
 *
 * @return Wheel movement delta in a 2D vector.
 */
QU_API qu_vec2i QU_CALL qu_get_mouse_wheel_delta(void);

/**
 * Set mouse button press callback.
 * The callback will be called if a mouse button is pressed.
 *
 * @param fn Mouse button event callback.
 */
QU_API void QU_CALL qu_on_mouse_button_pressed(qu_mouse_button_fn fn);

/**
 * Set mouse button release callback.
 * The callback will be called if a mouse button is released.
 *
 * @param fn Mouse button event callback.
 */
QU_API void QU_CALL qu_on_mouse_button_released(qu_mouse_button_fn fn);

/**
 * Set the mouse cursor movement callback.
 *
 * This callback will be triggered once per frame if the cursor position has
 * changed since the last frame.
 *
 * @param fn Mouse cursor event callback.
 */
QU_API void QU_CALL qu_on_mouse_cursor_moved(qu_mouse_cursor_fn fn);

/**
 * Set mouse cursor movement callback.
 * The callback will be called once a frame if the mouse wheel was scrolled
 * during last frame.
 *
 * @param fn Mouse wheel event callback.
 */
QU_API void QU_CALL qu_on_mouse_wheel_scrolled(qu_mouse_wheel_fn fn);

/**@}*/

//----------------------------------------------------------
// Touch Input

/**
 * @name Touch Input
 * @brief This group of functions provides access to touchscreen input.
 * @{
 */

/**
 * Check if touch pressed.
 *
 * @param index Number of touch point (starts from 0).
 * @return True if index is down.
 */
QU_API bool QU_CALL qu_is_touch_pressed(int index);

/**
 * Get position of given touch input.
 * 
 * @param index Number of touch point (starts from 0).
 * @return Position of touch point.
 */
QU_API qu_vec2i QU_CALL qu_get_touch_position(int index);

/**@}*/

//----------------------------------------------------------
// Joystick

/**
 * @name Joystick
 * @brief Abstract joystick input.
 *
 * Since joysticks often vary greatly, this API aims to be as generic as
 * possible.
 *
 * A joystick is defined as an input device that has two types of inputs:
 * buttons and axes.
 *
 * The functions provided by this API allow users to determine the number of
 * joysticks connected to the computer, the number of buttons and axes each
 * joystick has, and to retrieve the corresponding input values.
 *
 * @{
 */

/**
 * Check if joystick is connected.
 * This function can slow down a program, it's not recommended
 * to use it too frequently (e. g. every frame).
 * Joystick numbers start with 0, the joystick #0 is the first one,
 * #1 is the second etc.
 *
 * @param joystick Number of a joystick.
 * @return True if the joystick is connected.
 */
QU_API bool QU_CALL qu_is_joystick_connected(int joystick);

/**
 * Get identifier of joystick.
 * Identifier is usually a product name.
 * 
 * @param joystick Number of a joystick.
 * @return String containing joystick identifier.
 */
QU_API char const * QU_CALL qu_get_joystick_id(int joystick);

/**
 * Get number of buttons joystick has.
 * 
 * @param joystick Number of a joystick.
 * @return Number of buttons.
 */
QU_API int QU_CALL qu_get_joystick_button_count(int joystick);

/**
 * Get number of axes joystick has.
 *
 * @param joystick Number of a joystick.
 * @return Number of axes.
 */
QU_API int QU_CALL qu_get_joystick_axis_count(int joystick);

/**
 * Get the identifier of a joystick button.
 *
 * @param joystick Number of a joystick.
 * @param button Number of a button.
 * @return String containing button identifier.
 */
QU_API char const * QU_CALL qu_get_joystick_button_id(int joystick, int button);

/**
 * Get the identifier of a joystick axis.
 *
 * @param joystick Number of a joystick.
 * @param axis Number of an axis.
 * @return String containing axis identifier.
 */
QU_API char const * QU_CALL qu_get_joystick_axis_id(int joystick, int axis);

/**
 * Check if a joystick button is pressed.
 *
 * @param joystick Number of a joystick.
 * @param button Number of a button.
 * @return True if the button is pressed, otherwise false.
 */
QU_API bool QU_CALL qu_is_joystick_button_pressed(int joystick, int button);

/**
 * Get the value of a joystick axis.
 *
 * @param joystick Number of a joystick.
 * @param axis Number of an axis.
 * @return Value of the axis from -1.0 to 1.0.
 */
QU_API float QU_CALL qu_get_joystick_axis_value(int joystick, int axis);

/**@}*/

//----------------------------------------------------------
// Time

/**
 * @name Time
 * 
 * These functions can be used to determine the amount of time that has passed
 * since the library was initialized, measured in seconds.
 *
 * @{
 */

/**
 * Get medium-precision time.
 * 
 * This function returns the elapsed time in seconds since the library was
 * initialized.
 * 
 * @return The elapsed time in seconds since the initialization, with
 *         millisecond precision.
 */
QU_API float QU_CALL qu_get_time_mediump(void);

/**
 * Get high-precision time.
 * 
 * This function returns the elapsed time in seconds since the library was
 * initialized.
 *
 * @return The elapsed time in seconds since the initialization, with
 *         nanosecond precision.
 */
QU_API double QU_CALL qu_get_time_highp(void);

/**@}*/
/**@}*/

//------------------------------------------------------------------------------
// Graphics

/**
 * @defgroup graphics Graphics
 * @brief This module provides a set of functions for 2D graphics.
 * @{
 */

/**
 * Set blend mode.
 */
QU_API void QU_CALL qu_set_blend_mode(qu_blend_mode mode);

/**
 * Clear the screen with a specified color.
 *
 * This function clears the entire screen with the specified color.
 *
 * @param color The color to fill the screen with.
 */
QU_API void QU_CALL qu_clear(qu_color color);

/**
 * @name Views
 * @{
 */

/**
 * Set the view parameters for rendering.
 *
 * This function sets the view parameters for rendering, including the position,
 * width, height, and rotation.
 *
 * @param x The x-coordinate of the view position.
 * @param y The y-coordinate of the view position.
 * @param w The width of the view.
 * @param h The height of the view.
 * @param rotation The rotation angle of the view in degrees.
 */
QU_API void QU_CALL qu_set_view(float x, float y, float w, float h, float rotation);

/**
 * Reset the view parameters to their default values.
 *
 * This function resets the view parameters to their default values, restoring
 * the original view settings.
 */
QU_API void QU_CALL qu_reset_view(void);

/**@}*/

/**
 * @name Transformation matrix
 * @{
 */

/**
 * Push matrix.
 */
QU_API void QU_CALL qu_push_matrix(void);

/**
 * Pop matrix.
 */
QU_API void QU_CALL qu_pop_matrix(void);

/**
 * Translate matrix.
 */
QU_API void QU_CALL qu_translate(float x, float y);

/**
 * Scale matrix.
 */
QU_API void QU_CALL qu_scale(float x, float y);

/**
 * Rotate matrix.
 */
QU_API void QU_CALL qu_rotate(float degrees);

/**@}*/

/**
 * @name Primitives
 * @{
 */

/**
 * Draw a point on the screen.
 *
 * This function draws a point on the screen at the specified coordinates with
 * the specified color.
 *
 * @param x The x-coordinate of the point.
 * @param y The y-coordinate of the point.
 * @param color The color of the point.
 */
QU_API void QU_CALL qu_draw_point(float x, float y, qu_color color);

/**
 * Draw a line on the screen.
 *
 * This function draws a line on the screen from point A (ax, ay) to point B
 * (bx, by) with the specified color.
 *
 * @param ax The x-coordinate of point A.
 * @param ay The y-coordinate of point A.
 * @param bx The x-coordinate of point B.
 * @param by The y-coordinate of point B.
 * @param color The color of the line.
 */
QU_API void QU_CALL qu_draw_line(float ax, float ay, float bx, float by,
                                 qu_color color);

/**
 * Draw a triangle on the screen.
 *
 * This function draws a triangle on the screen using the specified coordinates
 * for its three points.
 * The triangle can have both an outline and a fill color.
 *
 * @param ax The x-coordinate of the first point.
 * @param ay The y-coordinate of the first point.
 * @param bx The x-coordinate of the second point.
 * @param by The y-coordinate of the second point.
 * @param cx The x-coordinate of the third point.
 * @param cy The y-coordinate of the third point.
 * @param outline The color of the triangle's outline.
 * @param fill The color to fill the triangle with.
 */
QU_API void QU_CALL qu_draw_triangle(float ax, float ay, float bx, float by,
                                     float cx, float cy, qu_color outline,
                                     qu_color fill);

/**
 * Draw a rectangle on the screen.
 *
 * This function draws a rectangle on the screen using the specified
 * coordinates, width, and height.
 * The rectangle can have both an outline and a fill color.
 *
 * @param x The x-coordinate of the top-left corner of the rectangle.
 * @param y The y-coordinate of the top-left corner of the rectangle.
 * @param w The width of the rectangle.
 * @param h The height of the rectangle.
 * @param outline The color of the rectangle's outline.
 * @param fill The color to fill the rectangle with.
 */
QU_API void QU_CALL qu_draw_rectangle(float x, float y, float w, float h,
                                      qu_color outline, qu_color fill);

/**
 * Draw a circle on the screen.
 *
 * This function draws a circle on the screen with the specified center
 * coordinates and radius.
 * The circle can have both an outline and a fill color.
 *
 * @param x The x-coordinate of the center of the circle.
 * @param y The y-coordinate of the center of the circle.
 * @param radius The radius of the circle.
 * @param outline The color of the circle's outline.
 * @param fill The color to fill the circle with.
 */
QU_API void QU_CALL qu_draw_circle(float x, float y, float radius,
                                   qu_color outline, qu_color fill);

/**@}*/

/**
 * @name Textures.
 * @{
 */

/**
 * Load texture from given path.
 */
QU_API qu_texture QU_CALL qu_load_texture(char const *path);

/**
 * Delete texture.
 */
QU_API void QU_CALL qu_delete_texture(qu_texture texture);

/**
 * Enable or disable texture smoothing.
 */
QU_API void QU_CALL qu_set_texture_smooth(qu_texture texture, bool smooth);

/**
 * Draw texture on the screen.
 */
QU_API void QU_CALL qu_draw_texture(qu_texture texture,
    float x, float y, float w, float h);

/**
 * Draw part of a texture on the screen.
 */
QU_API void QU_CALL qu_draw_subtexture(qu_texture texture,
    float x, float y, float w, float h,
    float rx, float ry, float rw, float rh);

/**@}*/

/**
 * @name Surfaces
 * @{
 */

/**
 * Create surface.
 */
QU_API qu_surface QU_CALL qu_create_surface(int width, int height);

/**
 * Delete surface.
 */
QU_API void QU_CALL qu_delete_surface(qu_surface surface);

/**
 * Toggle surface smoothing (used when drawing).
 */
QU_API void QU_CALL qu_set_surface_smooth(qu_surface surface, bool smooth);

/**
 * Set MSAA level for a surface.
 */
QU_API void QU_CALL qu_set_surface_antialiasing_level(qu_surface surface, int level);

/**
 * Set the surface as a main screen.
 */
QU_API void QU_CALL qu_set_surface(qu_surface surface);

/**
 * Switch back to the main screen.
 */
QU_API void QU_CALL qu_reset_surface(void);

/**
 * Draw surface on the screen.
 */
QU_API void QU_CALL qu_draw_surface(qu_surface surface, float x, float y, float w, float h);

/**@}*/

/**
 * @name Fonts.
 * @{
 */

/**
 * Load font from TTF file.
 */
QU_API qu_font QU_CALL qu_load_font(char const *path, float pt);

/**
 * Delete font.
 */
QU_API void QU_CALL qu_delete_font(qu_font font);

/**
 * Calculate dimensions of text.
 */
QU_API qu_vec2f QU_CALL qu_calculate_text_box(qu_font font, char const *str);

/**
 * Calculate dimensions of formatted text.
 */
QU_API qu_vec2f QU_CALL qu_calculate_text_box_fmt(qu_font font, char const *fmt, ...);

/**
 * Draw text.
 */
QU_API void QU_CALL qu_draw_text(qu_font font, float x, float y, qu_color color, char const *str);

/**
 * Draw formatted text.
 */
QU_API void QU_CALL qu_draw_text_fmt(qu_font font, float x, float y, qu_color color, char const *fmt, ...);

/**@}*/
/**@}*/

//------------------------------------------------------------------------------

/**
 * @defgroup audio Audio
 * @brief This module contains functions for playing sound and music.
 * @{
 */

/**
 * Set master volume.
 *
 * @param volume Volume level from 0.0 to 1.0
 */
QU_API void QU_CALL qu_set_master_volume(float volume);

/**
 * Load sound file to memory.
 */
QU_API qu_sound QU_CALL qu_load_sound(char const *path);

/**
 * Delete sound from memory.
 */
QU_API void QU_CALL qu_delete_sound(qu_sound sound);

/**
 * Play sound and get sound stream.
 */
QU_API qu_voice QU_CALL qu_play_sound(qu_sound sound);

/**
 * Loop sound and get sound stream.
 */
QU_API qu_voice QU_CALL qu_loop_sound(qu_sound sound);

/**
 * Open music.
 */
QU_API qu_music QU_CALL qu_open_music(char const *path);

/**
 * Close music.
 */
QU_API void QU_CALL qu_close_music(qu_music music);

/**
 * Play music.
 */
QU_API qu_voice QU_CALL qu_play_music(qu_music music);

/**
 * Loop music.
 */
QU_API qu_voice QU_CALL qu_loop_music(qu_music music);

/**
 * Pause voice.
 */
QU_API void QU_CALL qu_pause_voice(qu_voice voice);

/**
 * Unpause voice.
 */
QU_API void QU_CALL qu_unpause_voice(qu_voice voice);

/**
 * Stop voice.
 */
QU_API void QU_CALL qu_stop_voice(qu_voice voice);

/**@}*/

//------------------------------------------------------------------------------
// Doxygen start page

/**
 * @mainpage Index
 * 
 * @section intro_sec Introduction
 * 
 * Welcome to the `libqu` documentation!
 * `libqu` is a simple and easy-to-use 2D game library written in C99.
 * 
 * @section example_sec Quick example
 * 
 * ```c
 * #include <libqu.h>
 * 
 * int main(int argc, char *argv[])
 * {
 *     qu_initialize(NULL);
 *     atexit(qu_terminate);
 * 
 *     while (qu_process()) {
 *         qu_clear(QU_COLOR(20, 20, 20));
 *         qu_present();
 *     }
 * 
 *     return 0;
 * }
 * ```
 * 
 */

//------------------------------------------------------------------------------

#if defined(__cplusplus)
} // extern "C"
#endif

//------------------------------------------------------------------------------

#endif // LIBQU_H
