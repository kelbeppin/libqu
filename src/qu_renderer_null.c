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

#include "qu_graphics.h"
#include "qu_log.h"

//------------------------------------------------------------------------------

static bool query(void)
{
    return true;
}

static void initialize(void)
{
    QU_LOGI("Initialized.\n");
}

static void terminate(void)
{
    QU_LOGI("Terminated.\n");
}

static void upload_vertex_data(qu_vertex_format format, float const *data, size_t size)
{
}

static void apply_projection(qu_mat4 const *projection)
{
}

static void apply_transform(qu_mat4 const *transform)
{
}

static void apply_surface(qu_surface_obj const *surface)
{
}

static void apply_texture(qu_texture_obj const *texture)
{
}

static void apply_clear_color(qu_color color)
{
}

static void apply_draw_color(qu_color color)
{
}

static void apply_brush(qu_brush brush)
{
}

static void apply_vertex_format(qu_vertex_format format)
{
}

static void apply_blend_mode(qu_blend_mode mode)
{
}

static void exec_resize(int width, int height)
{
}

static void exec_clear(void)
{
}

static void exec_draw(qu_render_mode mode, unsigned int first_vertex, unsigned int total_vertices)
{
}

static void load_texture(qu_texture_obj *texture)
{
}

static void unload_texture(qu_texture_obj *texture)
{
}

static void set_texture_smooth(qu_texture_obj *texture, bool smooth)
{
}

static void create_surface(qu_surface_obj *surface)
{
}

static void destroy_surface(qu_surface_obj *surface)
{
}

static void set_surface_antialiasing_level(qu_surface_obj *surface, int level)
{
}

//------------------------------------------------------------------------------

qu_renderer_impl const qu_null_renderer_impl = {
    .query = query,
    .initialize = initialize,
    .terminate = terminate,
    .upload_vertex_data = upload_vertex_data,
    .apply_projection = apply_projection,
    .apply_transform = apply_transform,
    .apply_surface = apply_surface,
    .apply_texture = apply_texture,
    .apply_clear_color = apply_clear_color,
    .apply_draw_color = apply_draw_color,
    .apply_brush = apply_brush,
    .apply_vertex_format = apply_vertex_format,
    .apply_blend_mode = apply_blend_mode,
    .exec_resize = exec_resize,
	.exec_clear = exec_clear,
	.exec_draw = exec_draw,
    .load_texture = load_texture,
    .unload_texture = unload_texture,
    .set_texture_smooth = set_texture_smooth,
    .create_surface = create_surface,
    .destroy_surface = destroy_surface,
    .set_surface_antialiasing_level = set_surface_antialiasing_level,
};
