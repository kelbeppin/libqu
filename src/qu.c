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

#include "qu.h"

//------------------------------------------------------------------------------
// qu.c: Gateway
//------------------------------------------------------------------------------

enum qu_status
{
    QU_STATUS_CLEAN,
    QU_STATUS_INITIALIZED,
    QU_STATUS_TERMINATED,
};

struct qu
{
    enum qu_status status;
    qu_params params;

    qu_core_module core;
    qu_graphics_module graphics;
    qu_audio_module audio;
};

static struct qu qu;

//------------------------------------------------------------------------------

void qu_initialize(qu_params const *user_params)
{
    if (qu.status == QU_STATUS_INITIALIZED) {
        return;
    }
    
    if (qu.status == QU_STATUS_TERMINATED) {
        memset(&qu, 0, sizeof(struct qu));
    }

    if (user_params) {
        memcpy(&qu.params, user_params, sizeof(qu_params));
    }

    if (!qu.params.title) {
        qu.params.title = "libqu application";
    }

    if (!qu.params.display_width || !qu.params.display_height) {
        qu.params.display_width = 720;
        qu.params.display_height = 480;
    }

    if (!qu.params.canvas_width || !qu.params.canvas_height) {
        qu.params.canvas_width = qu.params.display_width;
        qu.params.canvas_height = qu.params.display_height;
    }

    qu_platform_initialize();

    switch (qu_get_core_type()) {
    default:
        QU_HALT("Everything is broken.");
        break;

#ifdef _WIN32
    case QU_CORE_WIN32:
        qu_construct_win32_core(&qu.core);
        break;
#endif

#ifdef __linux__
    case QU_CORE_X11:
        qu_construct_x11_core(&qu.core);
        break;
#endif

#ifdef __EMSCRIPTEN__
    case QU_CORE_EMSCRIPTEN:
        qu_construct_emscripten_core(&qu.core);
        break;
#endif
    }

    qu.core.initialize(&qu.params);

    switch (qu.core.get_graphics_type()) {
    default:
        qu_construct_null_graphics(&qu.graphics);
        break;

#ifdef QU_USE_GL
    case QU_GRAPHICS_GL2:
        qu_construct_gl2_graphics(&qu.graphics);
        break;
#endif

#ifdef QU_USE_ES2
    case QU_GRAPHICS_ES2:
        qu_construct_es2_graphics(&qu.graphics);
        break;
#endif
    }

    switch (qu.core.get_audio_type()) {
    default:
        qu_construct_null_audio(&qu.audio);
        break;

#ifdef QU_USE_OPENAL
    case QU_AUDIO_OPENAL:
        qu_construct_openal_audio(&qu.audio);
        break;
#endif

#ifdef _WIN32
    case QU_AUDIO_XAUDIO2:
        qu_construct_xaudio2(&qu.audio);
        break;
#endif
    }

    if (!qu.graphics.query(&qu.params)) {
        QU_ERROR("Failed to initialize graphics module, falling back to dummy.\n");
        qu_construct_null_graphics(&qu.graphics);
    }

    if (!qu.audio.query(&qu.params)) {
        QU_ERROR("Failed to initialize audio module, falling back to dummy.\n");
        qu_construct_null_audio(&qu.audio);
    }

    qu.graphics.initialize(&qu.params);
    qu.audio.initialize(&qu.params);
    qu_initialize_text(&qu.graphics);

    qu.status = QU_STATUS_INITIALIZED;
}

void qu_terminate(void)
{
    if (qu.status == QU_STATUS_INITIALIZED) {
        qu_terminate_text();
        qu.audio.terminate();
        qu.graphics.terminate();
        qu.core.terminate();
        qu_platform_terminate();

        qu.status = QU_STATUS_TERMINATED;
    }
}

bool qu_process(void)
{
    return qu.core.process();
}

#if defined(__EMSCRIPTEN__)

static void main_loop(void *arg)
{
    if (qu_process()) {
        qu_loop_fn loop_fn = (qu_loop_fn) arg;
        loop_fn();
    } else {
        qu_terminate();
        emscripten_cancel_main_loop();
    }
}

void qu_execute(qu_loop_fn loop_fn)
{
    emscripten_set_main_loop_arg(main_loop, loop_fn, 0, 1);
    exit(EXIT_SUCCESS);
}

#else

void qu_execute(qu_loop_fn loop_fn)
{
    while (qu_process() && loop_fn()) {
        // Intentionally left blank
    }

    qu_terminate();
    exit(EXIT_SUCCESS);
}

#endif

void qu_present(void)
{
    qu.graphics.swap();
    qu.core.present();
}

//------------------------------------------------------------------------------

bool qu_gl_check_extension(char const *name)
{
    return qu.core.gl_check_extension(name);
}

void *qu_gl_proc_address(char const *name)
{
    return qu.core.gl_proc_address(name);
}

void qu_notify_display_resize(int width, int height)
{
    if (qu.status == QU_STATUS_INITIALIZED) {
        qu.graphics.notify_display_resize(width, height);
    }
}

//------------------------------------------------------------------------------
// Core

bool const *qu_get_keyboard_state(void)
{
    return qu.core.get_keyboard_state();
}

bool qu_is_key_pressed(qu_key key)
{
    return qu.core.is_key_pressed(key);
}

uint8_t qu_get_mouse_button_state(void)
{
    return qu.core.get_mouse_button_state();
}

bool qu_is_mouse_button_pressed(qu_mouse_button button)
{
    return qu.core.is_mouse_button_pressed(button);
}

qu_vec2i qu_get_mouse_cursor_position(void)
{
    return qu.graphics.conv_cursor(qu.core.get_mouse_cursor_position());
}

qu_vec2i qu_get_mouse_cursor_delta(void)
{
    return qu.graphics.conv_cursor_delta(qu.core.get_mouse_cursor_delta());
}

qu_vec2i qu_get_mouse_wheel_delta(void)
{
    return qu.core.get_mouse_wheel_delta();
}

bool qu_is_joystick_connected(int joystick)
{
    return qu.core.is_joystick_connected(joystick);
}

char const *qu_get_joystick_id(int joystick)
{
    return qu.core.get_joystick_id(joystick);
}

int qu_get_joystick_button_count(int joystick)
{
    return qu.core.get_joystick_button_count(joystick);
}

int qu_get_joystick_axis_count(int joystick)
{
    return qu.core.get_joystick_axis_count(joystick);
}

char const *qu_get_joystick_button_id(int joystick, int button)
{
    return qu.core.get_joystick_button_id(joystick, button);
}

char const *qu_get_joystick_axis_id(int joystick, int axis)
{
    return qu.core.get_joystick_axis_id(joystick, axis);
}

bool qu_is_joystick_button_pressed(int joystick, int button)
{
    return qu.core.is_joystick_button_pressed(joystick, button);
}

float qu_get_joystick_axis_value(int joystick, int axis)
{
    return qu.core.get_joystick_axis_value(joystick, axis);
}

void qu_on_key_pressed(qu_key_fn fn)
{
    qu.core.on_key_pressed(fn);
}

void qu_on_key_repeated(qu_key_fn fn)
{
    qu.core.on_key_repeated(fn);
}

void qu_on_key_released(qu_key_fn fn)
{
    qu.core.on_key_released(fn);
}

void qu_on_mouse_button_pressed(qu_mouse_button_fn fn)
{
    qu.core.on_mouse_button_pressed(fn);
}

void qu_on_mouse_button_released(qu_mouse_button_fn fn)
{
    qu.core.on_mouse_button_released(fn);
}

void qu_on_mouse_cursor_moved(qu_mouse_cursor_fn fn)
{
    qu.core.on_mouse_cursor_moved(fn);
}

void qu_on_mouse_wheel_scrolled(qu_mouse_wheel_fn fn)
{
    qu.core.on_mouse_wheel_scrolled(fn);
}

//------------------------------------------------------------------------------
// Graphics

void qu_set_view(float x, float y, float w, float h, float rotation)
{
    qu.graphics.set_view(x, y, w, h, rotation);
}

void qu_reset_view(void)
{
    qu.graphics.reset_view();
}

void qu_push_matrix(void)
{
    qu.graphics.push_matrix();
}

void qu_pop_matrix(void)
{
    qu.graphics.pop_matrix();
}

void qu_translate(float x, float y)
{
    qu.graphics.translate(x, y);
}

void qu_scale(float x, float y)
{
    qu.graphics.scale(x, y);
}

void qu_rotate(float degrees)
{
    qu.graphics.rotate(degrees);
}

void qu_clear(qu_color color)
{
    qu.graphics.clear(color);
}

void qu_draw_point(float x, float y, qu_color color)
{
    qu.graphics.draw_point(x, y, color);
}

void qu_draw_line(float ax, float ay, float bx, float by, qu_color color)
{
    qu.graphics.draw_line(ax, ay, bx, by, color);
}

void qu_draw_triangle(float ax, float ay, float bx, float by,
                      float cx, float cy, qu_color outline, qu_color fill)
{
    qu.graphics.draw_triangle(ax, ay, bx, by, cx, cy, outline, fill);
}

void qu_draw_rectangle(float x, float y, float w, float h, qu_color outline,
                       qu_color fill)
{
    qu.graphics.draw_rectangle(x, y, w, h, outline, fill);
}

void qu_draw_circle(float x, float y, float radius,
                    qu_color outline, qu_color fill)
{
    qu.graphics.draw_circle(x, y, radius, outline, fill);
}

qu_texture qu_load_texture(char const *path)
{
    qu_file *file = qu_fopen(path);

    if (!file) {
        return (qu_texture) { 0 };
    }

    return (qu_texture) { qu.graphics.load_texture(file) };
}

void qu_delete_texture(qu_texture texture)
{
    qu.graphics.delete_texture(texture.id);
}

void qu_set_texture_smooth(qu_texture texture, bool smooth)
{
    qu.graphics.set_texture_smooth(texture.id, smooth);
}

void qu_draw_texture(qu_texture texture, float x, float y, float w, float h)
{
    qu.graphics.draw_texture(texture.id, x, y, w, h);
}

void qu_draw_subtexture(qu_texture texture, float x, float y, float w, float h, float rx, float ry, float rw, float rh)
{
    qu.graphics.draw_subtexture(texture.id, x, y, w, h, rx, ry, rw, rh);
}

qu_surface qu_create_surface(int width, int height)
{
    return (qu_surface) { qu.graphics.create_surface(width, height) };
}

void qu_delete_surface(qu_surface surface)
{
    qu.graphics.delete_surface(surface.id);
}

void qu_set_surface(qu_surface surface)
{
    qu.graphics.set_surface(surface.id);
}

void qu_reset_surface(void)
{
    qu.graphics.reset_surface();
}

void qu_draw_surface(qu_surface surface, float x, float y, float w, float h)
{
    qu.graphics.draw_surface(surface.id, x, y, w, h);
}

//------------------------------------------------------------------------------
// Audio

void qu_set_master_volume(float volume)
{
    qu.audio.set_master_volume(volume);
}

qu_sound qu_load_sound(char const *path)
{
    qu_file *file = qu_fopen(path);

    if (file) {
        int32_t id = qu.audio.load_sound(file);
        qu_fclose(file);

        return (qu_sound) { id };
    }

    return (qu_sound) { 0 };
}

void qu_delete_sound(qu_sound sound)
{
    qu.audio.delete_sound(sound.id);
}

qu_stream qu_play_sound(qu_sound sound)
{
    return (qu_stream) { qu.audio.play_sound(sound.id) };
}

qu_stream qu_loop_sound(qu_sound sound)
{
    return (qu_stream) { qu.audio.loop_sound(sound.id) };
}

qu_music qu_open_music(char const *path)
{
    qu_file *file = qu_fopen(path);

    if (file) {
        int32_t id = qu.audio.open_music(file);
        
        if (id == 0) {
            qu_fclose(file);
        }

        return (qu_music) { id };
    }

    return (qu_music) { 0 };
}

void qu_close_music(qu_music music)
{
    qu.audio.close_music(music.id);
}

qu_stream qu_play_music(qu_music music)
{
    return (qu_stream) { qu.audio.play_music(music.id) };
}

qu_stream qu_loop_music(qu_music music)
{
    return (qu_stream) { qu.audio.loop_music(music.id) };
}

void qu_pause_stream(qu_stream stream)
{
    qu.audio.pause_stream(stream.id);
}

void qu_unpause_stream(qu_stream stream)
{
    qu.audio.unpause_stream(stream.id);
}

void qu_stop_stream(qu_stream stream)
{
    qu.audio.stop_stream(stream.id);
}
