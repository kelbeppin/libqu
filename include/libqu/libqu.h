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

#ifndef LIBQU_H
#define LIBQU_H

//------------------------------------------------------------------------------
// libqu.h: public header file
//------------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

//------------------------------------------------------------------------------

#define QU_VERSION "0.1.1"

//------------------------------------------------------------------------------
// Exported function attribute

#ifdef QU_SHARED                                                                            

#if defined(_MSC_VER)

#ifdef QU_BUILD
#define QU_API      __declspec(dllexport)
#else
#define QU_API      __declspec(dllimport)
#endif

#elif defined(__GNUC__)

#define QU_API      __attribute__((visibility("default")))

#endif

#endif

#ifndef QU_API
#define QU_API
#endif

//------------------------------------------------------------------------------
// Calling conventions

#if defined(_WIN32)
#define QU_CALL     __cdecl
#else
#define QU_CALL
#endif

//------------------------------------------------------------------------------
// Non-returning function attribute

#if defined(_MSC_VER)
#define QU_NO_RET   __declspec(noreturn)
#elif defined(__GNUC__)
#define QU_NO_RET   __attribute__((__noreturn__))
#else
#define QU_NO_RET
#endif

//------------------------------------------------------------------------------

#if defined(__cplusplus)
extern "C" {
#endif

//------------------------------------------------------------------------------
// Math

/**
 * \defgroup math Math
 * \brief Various math-related constants, macros and functions.
 * @{
 */

/**
 * \brief Approximate value of Pi.
 */
#define QU_PI               3.14159265358979323846

/**
 * \brief Convert degrees to radians.
 * \param deg Angle in degrees.
 */
#define QU_DEG2RAD(deg)     ((deg) * (QU_PI / 180.0))

/**
 * \brief Convert radians to degrees.
 * \param rad Angle in radians.
 */
#define QU_RAD2DEG(rad)     ((rad) * (180.0 / QU_PI))

/**
 * \brief Get maximum of two values.
 * \param a First value.
 * \param b Second value.
 */
#define QU_MAX(a, b)        ((a) > (b) ? (a) : (b))

/**
 * \brief Get minimum of two values.
 * \param a First value.
 * \param b Second value.
 */
#define QU_MIN(a, b)        ((a) < (b) ? (a) : (b))

/**
 * \brief Two-dimensional floating-point vector.
 */
typedef struct qu_vec2f
{
    float x;
    float y;
} qu_vec2f;

/**
 * \brief Two-dimensional integer vector.
 */
typedef struct qu_vec2i
{
    int x;
    int y;
} qu_vec2i;

/**@}*/

//------------------------------------------------------------------------------
// Base

/**
 * \defgroup base Base
 * \brief Basic functions related to initialization, cleanup, etc.
 * @{
 */

/**
 * \brief Initialization parameters.
 */
typedef struct qu_params
{
    char const *title;
    
    int display_width;
    int display_height;

    bool enable_canvas;
    bool canvas_smooth;
    int canvas_width;
    int canvas_height;
} qu_params;

/**
 * \brief Callback function for the main loop.
 * \return False if the loop should stop, and true otherwise.
 */
typedef bool (*qu_loop_fn)(void);

/**
 * \brief Initialize the library with the specified parameters.
 *
 * This function initializes the library with the provided parameters.
 * If the `params` argument is set to NULL, default parameters will be used.
 *
 * \param params A pointer to a structure containing initialization parameters.
 *               Use NULL to use default parameters.
 */
QU_API void QU_CALL qu_initialize(qu_params const *params);

/**
 * \brief Terminate the library and clean up resources.
 *
 * This function terminates the library and performs necessary clean-up
 * operations to release allocated resources.
 * After calling this function, the library is no longer usable until it is
 * initialized again.
 */
QU_API void QU_CALL qu_terminate(void);

/**
 * \brief Process user input.
 * 
 * This function processes user input and updates internal state accordingly.
 * It should be called every frame to handle user input.
 * 
 * \return Returns false if the user wants to close the application/game, and
 *         true otherwise.
 */
QU_API bool QU_CALL qu_process(void);

/**
 * \brief Starts the automatic game loop.
 * 
 * The automatic loop is the only way to handle game loop on Web and mobile
 * platforms.
 * This function does not return.
 * 
 * \param loop_fn Callback function that will be called every frame.
 */
QU_API QU_NO_RET void QU_CALL qu_execute(qu_loop_fn loop_fn);

/**
 * \brief Swap buffers.
 * 
 * This function should be called when the rendering is finished in order to
 * put on the screen everything that was drawn so far.
 */
QU_API void QU_CALL qu_present(void);

/**@}*/

//------------------------------------------------------------------------------
// Core

/**
 * \defgroup core Core
 * \brief Functions related to window and user input handling.
 * @{
 */

//----------------------------------------------------------
// Keyboard

/**
 * \name Keyboard
 * @{
 */

/**
 * \brief Keys of keyboard.
 */
typedef enum qu_key
{
    QU_KEY_0,
    QU_KEY_1,
    QU_KEY_2,
    QU_KEY_3,
    QU_KEY_4,
    QU_KEY_5,
    QU_KEY_6,
    QU_KEY_7,
    QU_KEY_8,
    QU_KEY_9,
    QU_KEY_A,
    QU_KEY_B,
    QU_KEY_C,
    QU_KEY_D,
    QU_KEY_E,
    QU_KEY_F,
    QU_KEY_G,
    QU_KEY_H,
    QU_KEY_I,
    QU_KEY_J,
    QU_KEY_K,
    QU_KEY_L,
    QU_KEY_M,
    QU_KEY_N,
    QU_KEY_O,
    QU_KEY_P,
    QU_KEY_Q,
    QU_KEY_R,
    QU_KEY_S,
    QU_KEY_T,
    QU_KEY_U,
    QU_KEY_V,
    QU_KEY_W,
    QU_KEY_X,
    QU_KEY_Y,
    QU_KEY_Z,
    QU_KEY_GRAVE,
    QU_KEY_APOSTROPHE,
    QU_KEY_MINUS,
    QU_KEY_EQUAL,
    QU_KEY_LBRACKET,
    QU_KEY_RBRACKET,
    QU_KEY_COMMA,
    QU_KEY_PERIOD,
    QU_KEY_SEMICOLON,
    QU_KEY_SLASH,
    QU_KEY_BACKSLASH,
    QU_KEY_SPACE,
    QU_KEY_ESCAPE,
    QU_KEY_BACKSPACE,
    QU_KEY_TAB,
    QU_KEY_ENTER,
    QU_KEY_F1,
    QU_KEY_F2,
    QU_KEY_F3,
    QU_KEY_F4,
    QU_KEY_F5,
    QU_KEY_F6,
    QU_KEY_F7,
    QU_KEY_F8,
    QU_KEY_F9,
    QU_KEY_F10,
    QU_KEY_F11,
    QU_KEY_F12,
    QU_KEY_UP,
    QU_KEY_DOWN,
    QU_KEY_LEFT,
    QU_KEY_RIGHT,
    QU_KEY_LSHIFT,
    QU_KEY_RSHIFT,
    QU_KEY_LCTRL,
    QU_KEY_RCTRL,
    QU_KEY_LALT,
    QU_KEY_RALT,
    QU_KEY_LSUPER,
    QU_KEY_RSUPER,
    QU_KEY_MENU,
    QU_KEY_PGUP,
    QU_KEY_PGDN,
    QU_KEY_HOME,
    QU_KEY_END,
    QU_KEY_INSERT,
    QU_KEY_DELETE,
    QU_KEY_PRINTSCREEN,
    QU_KEY_PAUSE,
    QU_KEY_CAPSLOCK,
    QU_KEY_SCROLLLOCK,
    QU_KEY_NUMLOCK,
    QU_KEY_KP_0,
    QU_KEY_KP_1,
    QU_KEY_KP_2,
    QU_KEY_KP_3,
    QU_KEY_KP_4,
    QU_KEY_KP_5,
    QU_KEY_KP_6,
    QU_KEY_KP_7,
    QU_KEY_KP_8,
    QU_KEY_KP_9,
    QU_KEY_KP_MUL,
    QU_KEY_KP_ADD,
    QU_KEY_KP_SUB,
    QU_KEY_KP_POINT,
    QU_KEY_KP_DIV,
    QU_KEY_KP_ENTER,
    QU_KEY_INVALID,
    QU_TOTAL_KEYS,
} qu_key;

/**
 * \brief Keyboard event callback.
 */
typedef void (*qu_key_fn)(qu_key key);

/**
 * \brief Get current keyboard state.
 * 
 * \return Array of QU_TOTAL_KEYS boolean values.
           An element corresponding to a key is true it's pressed.
 */
QU_API bool const * QU_CALL qu_get_keyboard_state(void);

/**
 * \brief Check if a key is pressed.
 * 
 * \param key Key value.
 * \return True if the specified key is pressed.
 */
QU_API bool QU_CALL qu_is_key_pressed(qu_key key);

/**
 * \brief Set key press callback.
 * The callback will be called if a key is pressed.
 *
 * \param fn Key event callback.
 */
QU_API void QU_CALL qu_on_key_pressed(qu_key_fn fn);

/**
 * \brief Set key repeat callback.
 * The callback will be repeatedly called if a key is hold down.
 *
 * \param fn Key event callback.
 */
QU_API void QU_CALL qu_on_key_repeated(qu_key_fn fn);

/**
 * \brief Set key repeat callback.
 * The callback will be called if a key is released.
 *
 * \param fn Key event callback.
 */
QU_API void QU_CALL qu_on_key_released(qu_key_fn fn);

/**@}*/

//----------------------------------------------------------
// Mouse

/**
 * \name Mouse
 * @{
 */

/**
 * \brief Mouse buttons.
 */
typedef enum qu_mouse_button
{
    QU_MOUSE_BUTTON_LEFT,
    QU_MOUSE_BUTTON_RIGHT,
    QU_MOUSE_BUTTON_MIDDLE,
    QU_MOUSE_BUTTON_INVALID,
    QU_TOTAL_MOUSE_BUTTONS,
} qu_mouse_button;

/**
 * \brief Bitmasks of mouse buttons.
 */
typedef enum qu_mouse_button_bits
{
    QU_MOUSE_BUTTON_LEFT_BIT = (1 << QU_MOUSE_BUTTON_LEFT),
    QU_MOUSE_BUTTON_RIGHT_BIT = (1 << QU_MOUSE_BUTTON_RIGHT),
    QU_MOUSE_BUTTON_MIDDLE_BIT = (1 << QU_MOUSE_BUTTON_MIDDLE),
} qu_mouse_button_bits;

/**
 * \brief Mouse button event callback.
 */
typedef void (*qu_mouse_button_fn)(qu_mouse_button button);

/**
 * \brief Mouse wheel event callback.
 */
typedef void (*qu_mouse_wheel_fn)(int x_delta, int y_delta);

/**
 * \brief Mouse cursor event callback.
 */
typedef void (*qu_mouse_cursor_fn)(int x_delta, int y_delta);

/**
 * \brief Get current mouse button state.
 * 
 * \return Bitmask representing button state.
 */
QU_API uint8_t QU_CALL qu_get_mouse_button_state(void);

/**
 * \brief Check if mouse button is pressed.
 * 
 * \param button Mouse button.
 * \return True if the button is pressed, otherwise false.
 */
QU_API bool QU_CALL qu_is_mouse_button_pressed(qu_mouse_button button);

/**
 * \brief Get mouse cursor position.
 * Cursor position is relative to the window.
 * Top-left corner of the window is (0, 0) coordinate.
 * Cursor position is not updated if it's outside of the window.
 * 
 * \return Cursor position in a 2D vector.
 */
QU_API qu_vec2i QU_CALL qu_get_mouse_cursor_position(void);

/**
 * \brief Get mouse cursor movement delta.
 * This function returns how far cursor moved since the last frame.
 * 
 * \return Cursor movement delta in a 2D vector.
 */
QU_API qu_vec2i QU_CALL qu_get_mouse_cursor_delta(void);

/**
 * \brief Get mouse wheel movement delta.
 * This function returns how much wheel scrolled since the last frame.
 *
 * \return Wheel movement delta in a 2D vector.
 */
QU_API qu_vec2i QU_CALL qu_get_mouse_wheel_delta(void);

/**
 * \brief Set mouse button press callback.
 * The callback will be called if a mouse button is pressed.
 *
 * \param fn Mouse button event callback.
 */
QU_API void QU_CALL qu_on_mouse_button_pressed(qu_mouse_button_fn fn);

/**
 * \brief Set mouse button release callback.
 * The callback will be called if a mouse button is released.
 *
 * \param fn Mouse button event callback.
 */
QU_API void QU_CALL qu_on_mouse_button_released(qu_mouse_button_fn fn);

/**
 * \brief Set the mouse cursor movement callback.
 *
 * This callback will be triggered once per frame if the cursor position has
 * changed since the last frame.
 *
 * \param fn Mouse cursor event callback.
 */
QU_API void QU_CALL qu_on_mouse_cursor_moved(qu_mouse_cursor_fn fn);

/**
 * \brief Set mouse cursor movement callback.
 * The callback will be called once a frame if the mouse wheel was scrolled
 * during last frame.
 *
 * \param fn Mouse wheel event callback.
 */
QU_API void QU_CALL qu_on_mouse_wheel_scrolled(qu_mouse_wheel_fn fn);

/**@}*/

//----------------------------------------------------------
// Joystick

/**
 * \name Joystick
 * @{
 */

/**
 * \brief Check if joystick is connected.
 * This function can slow down a program, it's not recommended
 * to use it too frequently (e. g. every frame).
 * Joystick numbers start with 0, the joystick #0 is the first one,
 * #1 is the second etc.
 *
 * \param joystick Number of a joystick.
 * \return True if the joystick is connected.
 */
QU_API bool QU_CALL qu_is_joystick_connected(int joystick);

/**
 * \brief Get identifier of joystick.
 * Identifier is usually a product name.
 * 
 * \param joystick Number of a joystick.
 * \return String containing joystick identifier.
 */
QU_API char const * QU_CALL qu_get_joystick_id(int joystick);

/**
 * \brief Get number of buttons joystick has.
 * 
 * \param joystick Number of a joystick.
 * \return Number of buttons.
 */
QU_API int QU_CALL qu_get_joystick_button_count(int joystick);

/**
 * \brief Get number of axes joystick has.
 *
 * \param joystick Number of a joystick.
 * \return Number of axes.
 */
QU_API int QU_CALL qu_get_joystick_axis_count(int joystick);

/**
 * \brief Get the identifier of a joystick button.
 *
 * \param joystick Number of a joystick.
 * \param button Number of a button.
 * \return String containing button identifier.
 */
QU_API char const * QU_CALL qu_get_joystick_button_id(int joystick, int button);

/**
 * \brief Get the identifier of a joystick axis.
 *
 * \param joystick Number of a joystick.
 * \param axis Number of an axis.
 * \return String containing axis identifier.
 */
QU_API char const * QU_CALL qu_get_joystick_axis_id(int joystick, int axis);

/**
 * \brief Check if a joystick button is pressed.
 *
 * \param joystick Number of a joystick.
 * \param button Number of a button.
 * \return True if the button is pressed, otherwise false.
 */
QU_API bool QU_CALL qu_is_joystick_button_pressed(int joystick, int button);

/**
 * \brief Get the value of a joystick axis.
 *
 * \param joystick Number of a joystick.
 * \param axis Number of an axis.
 * \return Value of the axis from -1.0 to 1.0.
 */
QU_API float QU_CALL qu_get_joystick_axis_value(int joystick, int axis);

/**@}*/

//----------------------------------------------------------
// Time

/**
 * \name Time
 * @{
 */

/**
 * \brief Get medium-precision time.
 * 
 * This function returns the elapsed time in seconds since the library was
 * initialized.
 * 
 * \return The elapsed time in seconds since the initialization, with
 *         millisecond precision.
 */
QU_API float QU_CALL qu_get_time_mediump(void);

/**
 * \brief Get high-precision time.
 * 
 * This function returns the elapsed time in seconds since the library was
 * initialized.
 *
 * \return The elapsed time in seconds since the initialization, with
 *         nanosecond precision.
 */
QU_API double QU_CALL qu_get_time_highp(void);

/**@}*/
/**@}*/

//------------------------------------------------------------------------------
// Graphics

/**
 * \defgroup graphics Graphics
 * \brief TODO.
 * @{
 */

/**
 * \brief Get color value from individual RGB components.
 * \param red Red component (in range 0-255).
 * \param green Green component (in range 0-255).
 * \param blue Blue component (in range 0-255).
 */
#define QU_COLOR(red, green, blue) \
    (255 << 24 | (red) << 16 | (green) << 8 | (blue))

/**
 * \brief Get color value from individual RGBA components.
 * \param red Red component (in range 0-255).
 * \param green Green component (in range 0-255).
 * \param blue Blue component (in range 0-255).
 * \param alpha Alpha component (in range 0-255).
 */
#define QU_RGBA(red, green, blue, alpha) \
    ((alpha) << 24 | (red) << 16 | (green) << 8 | (blue))

/**
 * \brief Color type.
 *
 * Assumed to hold components in ARGB order.
 */
typedef uint64_t qu_color;

/**
 * \brief Texture handle.
 */
typedef struct qu_texture
{
    int32_t id;
} qu_texture;

/**
 * \brief Surface handle.
 */
typedef struct qu_surface
{
    int32_t id;
} qu_surface;

/**
 * \brief Font handle.
 */
typedef struct qu_font
{
    int32_t id;
} qu_font;

/**
 * \brief Set the view parameters for rendering.
 *
 * This function sets the view parameters for rendering, including the position,
 * width, height, and rotation.
 *
 * \param x The x-coordinate of the view position.
 * \param y The y-coordinate of the view position.
 * \param w The width of the view.
 * \param h The height of the view.
 * \param rotation The rotation angle of the view in degrees.
 */
QU_API void QU_CALL qu_set_view(float x, float y, float w, float h, float rotation);

/**
 * \brief Reset the view parameters to their default values.
 *
 * This function resets the view parameters to their default values, restoring
 * the original view settings.
 */
QU_API void QU_CALL qu_reset_view(void);

QU_API void QU_CALL qu_push_matrix(void);
QU_API void QU_CALL qu_pop_matrix(void);
QU_API void QU_CALL qu_translate(float x, float y);
QU_API void QU_CALL qu_scale(float x, float y);
QU_API void QU_CALL qu_rotate(float degrees);

/**
 * \brief Clear the screen with a specified color.
 *
 * This function clears the entire screen with the specified color.
 *
 * \param color The color to fill the screen with.
 */
QU_API void QU_CALL qu_clear(qu_color color);

/**
 * \brief Draw a point on the screen.
 *
 * This function draws a point on the screen at the specified coordinates with
 * the specified color.
 *
 * \param x The x-coordinate of the point.
 * \param y The y-coordinate of the point.
 * \param color The color of the point.
 */
QU_API void QU_CALL qu_draw_point(float x, float y, qu_color color);

/**
 * \brief Draw a line on the screen.
 *
 * This function draws a line on the screen from point A (ax, ay) to point B
 * (bx, by) with the specified color.
 *
 * \param ax The x-coordinate of point A.
 * \param ay The y-coordinate of point A.
 * \param bx The x-coordinate of point B.
 * \param by The y-coordinate of point B.
 * \param color The color of the line.
 */
QU_API void QU_CALL qu_draw_line(float ax, float ay, float bx, float by,
                                 qu_color color);

/**
 * \brief Draw a triangle on the screen.
 *
 * This function draws a triangle on the screen using the specified coordinates
 * for its three points.
 * The triangle can have both an outline and a fill color.
 *
 * \param ax The x-coordinate of the first point.
 * \param ay The y-coordinate of the first point.
 * \param bx The x-coordinate of the second point.
 * \param by The y-coordinate of the second point.
 * \param cx The x-coordinate of the third point.
 * \param cy The y-coordinate of the third point.
 * \param outline The color of the triangle's outline.
 * \param fill The color to fill the triangle with.
 */
QU_API void QU_CALL qu_draw_triangle(float ax, float ay, float bx, float by,
                                     float cx, float cy, qu_color outline,
                                     qu_color fill);

/**
 * \brief Draw a rectangle on the screen.
 *
 * This function draws a rectangle on the screen using the specified
 * coordinates, width, and height.
 * The rectangle can have both an outline and a fill color.
 *
 * \param x The x-coordinate of the top-left corner of the rectangle.
 * \param y The y-coordinate of the top-left corner of the rectangle.
 * \param w The width of the rectangle.
 * \param h The height of the rectangle.
 * \param outline The color of the rectangle's outline.
 * \param fill The color to fill the rectangle with.
 */
QU_API void QU_CALL qu_draw_rectangle(float x, float y, float w, float h,
                                      qu_color outline, qu_color fill);

/**
 * \brief Draw a circle on the screen.
 *
 * This function draws a circle on the screen with the specified center
 * coordinates and radius.
 * The circle can have both an outline and a fill color.
 *
 * \param x The x-coordinate of the center of the circle.
 * \param y The y-coordinate of the center of the circle.
 * \param radius The radius of the circle.
 * \param outline The color of the circle's outline.
 * \param fill The color to fill the circle with.
 */
QU_API void QU_CALL qu_draw_circle(float x, float y, float radius,
                                   qu_color outline, qu_color fill);

/**
 * \brief Load texture from given path.
 * 
 * \param path File path.
 */
QU_API qu_texture QU_CALL qu_load_texture(char const *path);
QU_API void QU_CALL qu_delete_texture(qu_texture texture);
QU_API void QU_CALL qu_set_texture_smooth(qu_texture texture, bool smooth);
QU_API void QU_CALL qu_draw_texture(qu_texture texture, float x, float y, float w, float h);
QU_API void QU_CALL qu_draw_subtexture(qu_texture texture, float x, float y, float w, float h, float rx, float ry, float rw, float rh);

QU_API qu_font QU_CALL qu_load_font(char const *path, float pt);
QU_API void QU_CALL qu_delete_font(qu_font font);
QU_API void QU_CALL qu_draw_text(qu_font font, float x, float y, qu_color color, char const *str);
QU_API void QU_CALL qu_draw_text_fmt(qu_font font, float x, float y, qu_color color, char const *fmt, ...);

QU_API qu_surface QU_CALL qu_create_surface(int width, int height);
QU_API void QU_CALL qu_delete_surface(qu_surface surface);
QU_API void QU_CALL qu_set_surface(qu_surface surface);
QU_API void QU_CALL qu_reset_surface(void);
QU_API void QU_CALL qu_draw_surface(qu_surface surface, float x, float y, float w, float h);

/**@}*/

//------------------------------------------------------------------------------
// Audio

/**
 * \defgroup audio Audio
 * \brief TODO.
 * @{
 */

/**
 * \brief Sound handle.
 */
typedef struct qu_sound
{
    int32_t id;
} qu_sound;

/**
 * \brief Music handle.
 */
typedef struct qu_music
{
    int32_t id;
} qu_music;

/**
 * \brief Audio stream handle.
 */
typedef struct qu_stream
{
    int32_t id;
} qu_stream;

/**
 * Set master volume.
 *
 * \param volume Volume level from 0.0 to 1.0
 */
QU_API void QU_CALL qu_set_master_volume(float volume);

// Load sound file to memory.
QU_API qu_sound QU_CALL qu_load_sound(char const *path);

// Delete sound from memory.
QU_API void QU_CALL qu_delete_sound(qu_sound sound);

// Play sound and get sound stream.
QU_API qu_stream QU_CALL qu_play_sound(qu_sound sound);

// Loop sound and get sound stream.
QU_API qu_stream QU_CALL qu_loop_sound(qu_sound sound);

QU_API qu_music QU_CALL qu_open_music(char const *path);
QU_API void QU_CALL qu_close_music(qu_music music);
QU_API qu_stream QU_CALL qu_play_music(qu_music music);
QU_API qu_stream QU_CALL qu_loop_music(qu_music music);

// Pause sound stream.
QU_API void QU_CALL qu_pause_stream(qu_stream stream);

// Unpause sound stream.
QU_API void QU_CALL qu_unpause_stream(qu_stream stream);

// Stop sound stream. The stream is lost after that.
QU_API void QU_CALL qu_stop_stream(qu_stream stream);

/**@}*/

//------------------------------------------------------------------------------
// Doxygen start page

/**
 * \mainpage Index
 * 
 * \section intro_sec Introduction
 * 
 * Welcome to the `libqu` documentation!
 * `libqu` is a simple and easy-to-use 2D game library written in C99.
 * 
 * \section example_sec Quick example
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
