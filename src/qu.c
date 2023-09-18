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

    if (!qu.params.canvas_antialiasing_level) {
        qu.params.canvas_antialiasing_level = qu.params.antialiasing_level;
    }

    if (!qu.params.canvas_width || !qu.params.canvas_height) {
        qu.params.canvas_width = qu.params.display_width;
        qu.params.canvas_height = qu.params.display_height;
    }

    qu_platform_initialize();
    qu__core_initialize(&qu.params);
    qu__graphics_initialize(&qu.params);
    qu__audio_initialize(&qu.params);
    qu_initialize_text();

    qu.status = QU_STATUS_INITIALIZED;
}

void qu_terminate(void)
{
    if (qu.status == QU_STATUS_INITIALIZED) {
        qu_terminate_text();
        qu__audio_terminate();
        qu__graphics_terminate();
        qu__core_terminate();
        qu_platform_terminate();

        qu.status = QU_STATUS_TERMINATED;
    }
}

bool qu_process(void)
{
    return qu__core_process();
}

#if defined(__EMSCRIPTEN__)

#include <emscripten.h>

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
    qu__graphics_swap();
    qu__core_present();
}

//------------------------------------------------------------------------------
// Core

char const *qu_get_window_title(void)
{
    return qx_core_get_window_title();
}

void qu_set_window_title(char const *title)
{
    qx_core_set_window_title(title);
}

qu_vec2i qu_get_window_size(void)
{
    int width, height;

    qx_core_get_window_size(&width, &height);

    return (qu_vec2i) { width, height };
}

void qu_set_window_size(int width, int height)
{
    qx_core_set_window_size(width, height);
}

bool qu_is_window_active(void)
{
    return qx_core_is_window_active();
}

//------------------------------------------------------------------------------
// Graphics

void qu_set_view(float x, float y, float w, float h, float rotation)
{
    qu__graphics_set_view(x, y, w, h, rotation);
}

void qu_reset_view(void)
{
    qu__graphics_reset_view();
}

void qu_push_matrix(void)
{
    qu__graphics_push_matrix();
}

void qu_pop_matrix(void)
{
    qu__graphics_pop_matrix();
}

void qu_translate(float x, float y)
{
    qu__graphics_translate(x, y);
}

void qu_scale(float x, float y)
{
    qu__graphics_scale(x, y);
}

void qu_rotate(float degrees)
{
    qu__graphics_rotate(degrees);
}

void qu_set_blend_mode(qu_blend_mode mode)
{
    qu__graphics_set_blend_mode(mode);
}

void qu_clear(qu_color color)
{
    qu__graphics_clear(color);
}

void qu_draw_point(float x, float y, qu_color color)
{
    qu__graphics_draw_point(x, y, color);
}

void qu_draw_line(float ax, float ay, float bx, float by, qu_color color)
{
    qu__graphics_draw_line(ax, ay, bx, by, color);
}

void qu_draw_triangle(float ax, float ay, float bx, float by, float cx, float cy, qu_color outline, qu_color fill)
{
    qu__graphics_draw_triangle(ax, ay, bx, by, cx, cy, outline, fill);
}

void qu_draw_rectangle(float x, float y, float w, float h, qu_color outline, qu_color fill)
{
    qu__graphics_draw_rectangle(x, y, w, h, outline, fill);
}

void qu_draw_circle(float x, float y, float radius, qu_color outline, qu_color fill)
{
    qu__graphics_draw_circle(x, y, radius, outline, fill);
}

qu_texture qu_load_texture(char const *path)
{
    qx_file *file = qx_fopen(path);

    if (!file) {
        return (qu_texture) { .id = 0 };
    }

    int32_t id = qu__graphics_load_texture(file);

    qx_fclose(file);

    return (qu_texture) { .id = id };
}

void qu_delete_texture(qu_texture texture)
{
    qu__graphics_delete_texture(texture.id);
}

void qu_set_texture_smooth(qu_texture texture, bool smooth)
{
    qu__graphics_set_texture_smooth(texture.id, smooth);
}

void qu_draw_texture(qu_texture texture, float x, float y, float w, float h)
{
    qu__graphics_draw_texture(texture.id, x, y, w, h);
}

void qu_draw_subtexture(qu_texture texture, float x, float y, float w, float h, float rx, float ry, float rw, float rh)
{
    qu__graphics_draw_subtexture(texture.id, x, y, w, h, rx, ry, rw, rh);
}

qu_surface qu_create_surface(int width, int height)
{
    return (qu_surface) { .id = qu__graphics_create_surface(width, height) };
}

void qu_delete_surface(qu_surface surface)
{
    qu__graphics_delete_surface(surface.id);
}

void qu_set_surface_smooth(qu_surface surface, bool smooth)
{
    qu__graphics_set_surface_smooth(surface.id, smooth);
}

void qu_set_surface_antialiasing_level(qu_surface surface, int level)
{
    qu__graphics_set_surface_antialiasing_level(surface.id, level);
}

void qu_set_surface(qu_surface surface)
{
    qu__graphics_set_surface(surface.id);
}

void qu_reset_surface(void)
{
    qu__graphics_reset_surface();
}

void qu_draw_surface(qu_surface surface, float x, float y, float w, float h)
{
    qu__graphics_draw_surface(surface.id, x, y, w, h);
}

qu_font qu_load_font(char const *path, float pt)
{
    qx_file *file = qx_fopen(path);

    if (!file) {
        return (qu_font) { .id = 0 };
    }

    int32_t id = qu__text_load_font(file, pt);

    if (id == 0) {
        qx_fclose(file);
    }

    return (qu_font) { .id = id };
}

void qu_delete_font(qu_font font)
{
    qu__text_delete_font(font.id);
}

void qu_draw_text(qu_font font, float x, float y, qu_color color, char const *str)
{
    qu__text_draw(font.id, x, y, color, str);
}

void qu_draw_text_fmt(qu_font font, float x, float y, qu_color color, char const *fmt, ...)
{
    va_list ap;
    char buffer[256];
    char *heap = NULL;

    va_start(ap, fmt);
    int required = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    if ((size_t) required >= sizeof(buffer)) {
        heap = malloc(required + 1);

        if (heap) {
            va_start(ap, fmt);
            vsnprintf(heap, required + 1, fmt, ap);
            va_end(ap);
        }
    }

    qu__text_draw(font.id, x, y, color, heap ? heap : buffer);
    free(heap);
}

//------------------------------------------------------------------------------
// Audio API

void qu_set_master_volume(float volume)
{
    qu__audio_set_master_volume(volume);
}

qu_sound qu_load_sound(char const *path)
{
    qx_file *file = qx_fopen(path);

    if (!file) {
        return (qu_sound) { 0 };
    }

    int32_t id = qu__audio_load_sound(file);

    qx_fclose(file);

    return (qu_sound) { id };
}

void qu_delete_sound(qu_sound sound)
{
    qu__audio_delete_sound(sound.id);
}

qu_voice qu_play_sound(qu_sound sound)
{
    return (qu_voice) { qu__audio_play_sound(sound.id) };
}

qu_voice qu_loop_sound(qu_sound sound)
{
    return (qu_voice) { qu__audio_loop_sound(sound.id) };
}

qu_music qu_open_music(char const *path)
{
    qx_file *file = qx_fopen(path);

    if (!file) {
        return (qu_music) { 0 };
    }

    int32_t id = qu__audio_open_music(file);
        
    if (!id) {
        qx_fclose(file);
    }

    return (qu_music) { id };
}

void qu_close_music(qu_music music)
{
    qu__audio_close_music(music.id);
}

qu_voice qu_play_music(qu_music music)
{
    return (qu_voice) { qu__audio_play_music(music.id) };
}

qu_voice qu_loop_music(qu_music music)
{
    return (qu_voice) { qu__audio_loop_music(music.id) };
}

void qu_pause_voice(qu_voice voice)
{
    qu__audio_pause_voice(voice.id);
}

void qu_unpause_voice(qu_voice voice)
{
    qu__audio_unpause_voice(voice.id);
}

void qu_stop_voice(qu_voice voice)
{
    qu__audio_stop_voice(voice.id);
}
