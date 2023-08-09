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

#define QU_MODULE "graphics"

#include "qu.h"

//------------------------------------------------------------------------------
// qu_graphics.c: Graphics module
//------------------------------------------------------------------------------

struct qu__graphics_priv
{
	struct qu__graphics const *impl;
};

static struct qu__graphics_priv priv;

//------------------------------------------------------------------------------

void qu__graphics_initialize(qu_params const *params)
{
    memset(&priv, 0, sizeof(priv));

    priv.impl = qu__core_get_graphics();

    if (!priv.impl->query(params)) {
        priv.impl = &qu__graphics_null;
    }

    priv.impl->initialize(params);
}

void qu__graphics_terminate(void)
{
    priv.impl->terminate();
}

void qu__graphics_refresh(void)
{
    priv.impl->refresh();
}

void qu__graphics_swap(void)
{
    priv.impl->swap();
}

void qu__graphics_notify_display_resize(int width, int height)
{
    priv.impl->notify_display_resize(width, height);
}

qu_vec2i qu__graphics_conv_cursor(qu_vec2i position)
{
    return priv.impl->conv_cursor(position);
}

qu_vec2i qu__graphics_conv_cursor_delta(qu_vec2i position)
{
    return priv.impl->conv_cursor_delta(position);
}

void qu__graphics_set_view(float x, float y, float w, float h, float rotation)
{
    priv.impl->set_view(x, y, w, h, rotation);
}

void qu__graphics_reset_view(void)
{
    priv.impl->reset_view();
}

void qu__graphics_push_matrix(void)
{
    priv.impl->push_matrix();
}

void qu__graphics_pop_matrix(void)
{
    priv.impl->pop_matrix();
}

void qu__graphics_translate(float x, float y)
{
    priv.impl->translate(x, y);
}

void qu__graphics_scale(float x, float y)
{
    priv.impl->scale(x, y);
}

void qu__graphics_rotate(float degrees)
{
    priv.impl->rotate(degrees);
}

void qu__graphics_clear(qu_color color)
{
    priv.impl->clear(color);
}

void qu__graphics_draw_point(float x, float y, qu_color color)
{
    priv.impl->draw_point(x, y, color);
}

void qu__graphics_draw_line(float ax, float ay, float bx, float by, qu_color color)
{
    priv.impl->draw_line(ax, ay, bx, by, color);
}

void qu__graphics_draw_triangle(float ax, float ay, float bx, float by, float cx, float cy, qu_color outline, qu_color fill)
{
    priv.impl->draw_triangle(ax, ay, bx, by, cx, cy, outline, fill);
}

void qu__graphics_draw_rectangle(float x, float y, float w, float h, qu_color outline, qu_color fill)
{
    priv.impl->draw_rectangle(x, y, w, h, outline, fill);
}

void qu__graphics_draw_circle(float x, float y, float radius, qu_color outline, qu_color fill)
{
    priv.impl->draw_circle(x, y, radius, outline, fill);
}

int32_t qu__graphics_create_texture(int w, int h, int channels)
{
    return priv.impl->create_texture(w, h, channels);
}

void qu__graphics_update_texture(int32_t texture_id, int x, int y, int w, int h, uint8_t const *pixels)
{
    priv.impl->update_texture(texture_id, x, y, w, h, pixels);
}

int32_t qu__graphics_load_texture(qu_file *file)
{
    return priv.impl->load_texture(file);
}

void qu__graphics_delete_texture(int32_t texture_id)
{
    priv.impl->delete_texture(texture_id);
}

void qu__graphics_set_texture_smooth(int32_t texture_id, bool smooth)
{
    priv.impl->set_texture_smooth(texture_id, smooth);
}

void qu__graphics_draw_texture(int32_t texture_id, float x, float y, float w, float h)
{
    priv.impl->draw_texture(texture_id, x, y, w, h);
}

void qu__graphics_draw_subtexture(int32_t texture_id, float x, float y, float w, float h, float rx, float ry, float rw, float rh)
{
    priv.impl->draw_subtexture(texture_id, x, y, w, h, rx, ry, rw, rh);
}

void qu__graphics_draw_text(int32_t texture_id, qu_color color, float const *data, int count)
{
    priv.impl->draw_text(texture_id, color, data, count);
}

int32_t qu__graphics_create_surface(int width, int height)
{
    return priv.impl->create_surface(width, height);
}

void qu__graphics_delete_surface(int32_t id)
{
    priv.impl->delete_surface(id);
}

void qu__graphics_set_surface(int32_t id)
{
    priv.impl->set_surface(id);
}

void qu__graphics_reset_surface(void)
{
    priv.impl->reset_surface();
}

void qu__graphics_draw_surface(int32_t id, float x, float y, float w, float h)
{
    priv.impl->draw_surface(id, x, y, w, h);
}
