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
// qu_graphics.h: 2D graphics
//------------------------------------------------------------------------------

#ifndef QU_GRAPHICS_H_INC
#define QU_GRAPHICS_H_INC

//------------------------------------------------------------------------------

#include "qu_math.h"

//------------------------------------------------------------------------------

typedef enum qu_render_mode
{
    QU_RENDER_MODE_POINTS,
    QU_RENDER_MODE_LINES,
    QU_RENDER_MODE_LINE_LOOP,
    QU_RENDER_MODE_LINE_STRIP,
    QU_RENDER_MODE_TRIANGLES,
    QU_RENDER_MODE_TRIANGLE_STRIP,
    QU_RENDER_MODE_TRIANGLE_FAN,
    QU_TOTAL_RENDER_MODES,
} qu_render_mode;

typedef enum qu_vertex_attribute
{
    QU_VERTEX_ATTRIBUTE_POSITION,
    QU_VERTEX_ATTRIBUTE_COLOR,
    QU_VERTEX_ATTRIBUTE_TEXCOORD,
    QU_TOTAL_VERTEX_ATTRIBUTES,
} qu_vertex_attribute;

typedef enum qu_vertex_attribute_bits
{
    QU_VERTEX_ATTRIBUTE_BIT_POSITION = (1 << QU_VERTEX_ATTRIBUTE_POSITION),
    QU_VERTEX_ATTRIBUTE_BIT_COLOR = (1 << QU_VERTEX_ATTRIBUTE_COLOR),
    QU_VERTEX_ATTRIBUTE_BIT_TEXCOORD = (1 << QU_VERTEX_ATTRIBUTE_TEXCOORD),
} qu_vertex_attribute_bits;

typedef enum qu_vertex_format
{
    QU_VERTEX_FORMAT_2XY,
    QU_VERTEX_FORMAT_4XYST,
    QU_TOTAL_VERTEX_FORMATS,
} qu_vertex_format;

typedef enum qu_brush
{
    QU_BRUSH_SOLID, // single color
    QU_BRUSH_TEXTURED, // textured
    QU_BRUSH_FONT,
    QU_TOTAL_BRUSHES,
} qu_brush;

typedef struct qu_texture_obj
{
    int width;
    int height;
    int channels;
    unsigned char *pixels;
    uintptr_t priv[4];
    bool smooth;
} qu_texture_obj;

typedef struct qu_surface_obj
{
    qu_texture_obj texture;

    qu_mat4 projection;
    qu_mat4 modelview[32];
    int modelview_index;

    int sample_count;

    uintptr_t priv[4];
} qu_surface_obj;

typedef struct qu_renderer_impl
{
    bool (*query)(void);
    void (*initialize)(void);
    void (*terminate)(void);

    void (*upload_vertex_data)(qu_vertex_format vertex_format, float const *data, size_t size);

    void (*apply_projection)(qu_mat4 const *projection);
    void (*apply_transform)(qu_mat4 const *transform);
    void (*apply_surface)(qu_surface_obj const *surface);
    void (*apply_texture)(qu_texture_obj const *texture);
    void (*apply_clear_color)(qu_color clear_color);
    void (*apply_draw_color)(qu_color draw_color);
    void (*apply_brush)(qu_brush brush);
    void (*apply_vertex_format)(qu_vertex_format vertex_format);
    void (*apply_blend_mode)(qu_blend_mode mode);

    void (*exec_resize)(int width, int height);
    void (*exec_clear)(void);
    void (*exec_draw)(qu_render_mode render_mode, unsigned int first_vertex, unsigned int total_vertices);

    void (*load_texture)(qu_texture_obj *texture);
    void (*unload_texture)(qu_texture_obj *texture);
    void (*set_texture_smooth)(qu_texture_obj *texture, bool smooth);

    void (*create_surface)(qu_surface_obj *surface);
    void (*destroy_surface)(qu_surface_obj *surface);
    void (*set_surface_antialiasing_level)(qu_surface_obj *surface, int level);
} qu_renderer_impl;

//------------------------------------------------------------------------------

extern qu_renderer_impl const qu_es2_renderer_impl;
extern qu_renderer_impl const qu_gl1_renderer_impl;
extern qu_renderer_impl const qu_gl3_renderer_impl;
extern qu_renderer_impl const qu_null_renderer_impl;

//------------------------------------------------------------------------------

void qu_initialize_graphics(void);
void qu_terminate_graphics(void);
void qu_flush_graphics(void);
void qu_event_context_lost(void);
void qu_event_context_restored(void);
void qu_event_window_resize(int width, int height);
qu_vec2i qu_convert_window_pos_to_canvas_pos(qu_vec2i position);
qu_vec2i qu_convert_window_delta_to_canvas_delta(qu_vec2i position);
qu_texture qu_create_texture(int w, int h, int channels);
void qu_update_texture(qu_texture texture, int x, int y, int w, int h, uint8_t const *pixels);
void qu_resize_texture(qu_texture texture, int width, int height);
void qu_draw_font(qu_texture texture, qu_color color, float const *data, int count);

//------------------------------------------------------------------------------

#endif // QU_GRAPHICS_H_INC

