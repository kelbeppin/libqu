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

#include <GLES2/gl2.h>

//------------------------------------------------------------------------------
// qu_graphics_es2.c: OpenGL ES 2.0 graphics
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Adapter macros

#define GL2_SHADER_VERTEX_SRC \
    "attribute vec2 a_position;\n" \
    "attribute vec4 a_color;\n" \
    "attribute vec2 a_texCoord;\n" \
    "varying vec4 v_color;\n" \
    "varying vec2 v_texCoord;\n" \
    "uniform mat4 u_projection;\n" \
    "uniform mat4 u_modelView;\n" \
    "void main()\n" \
    "{\n" \
    "    v_texCoord = a_texCoord;\n" \
    "    v_color = a_color;\n" \
    "    vec4 position = vec4(a_position, 0.0, 1.0);\n" \
    "    gl_Position = u_projection * u_modelView * position;\n" \
    "}\n"

#define GL2_SHADER_SOLID_SRC \
    "precision mediump float;\n" \
    "uniform vec4 u_color;\n" \
    "void main()\n" \
    "{\n" \
    "    gl_FragColor = u_color;\n" \
    "}\n"

#define GL2_SHADER_TEXTURED_SRC \
    "precision mediump float;\n" \
    "varying vec2 v_texCoord;\n" \
    "uniform sampler2D u_texture;\n" \
    "uniform vec4 u_color;\n" \
    "void main()\n" \
    "{\n" \
    "    gl_FragColor = texture2D(u_texture, v_texCoord) * u_color;\n" \
    "}\n"

#define GL2_SHADER_CANVAS_SRC \
    "attribute vec2 a_position;\n" \
    "attribute vec2 a_texCoord;\n" \
    "varying vec2 v_texCoord;\n" \
    "uniform mat4 u_projection;\n" \
    "void main()\n" \
    "{\n" \
    "    v_texCoord = a_texCoord;\n" \
    "    vec4 position = vec4(a_position, 0.0, 1.0);\n" \
    "    gl_Position = u_projection * position;\n" \
    "}\n"

//------------------------------------------------------------------------------
// Shared implementation

#define QU_MODULE "es2"
#include "qu_graphics_gl2_impl.h"

//------------------------------------------------------------------------------

static bool query(qu_params const *params)
{
    return true;
}

//------------------------------------------------------------------------------
// Constructor

struct qu__graphics const qu__graphics_es2 = {
    .query = query,
    .initialize = gl2_initialize,
    .terminate = gl2_terminate,
    .refresh = gl2_refresh,
    .swap = gl2_swap,
    .notify_display_resize = gl2_notify_display_resize,
    .conv_cursor = gl2_conv_cursor,
    .conv_cursor_delta = gl2_conv_cursor_delta,
    .set_view = gl2_set_view,
    .reset_view = gl2_reset_view,
    .push_matrix = gl2_push_matrix,
    .pop_matrix = gl2_pop_matrix,
    .translate = gl2_translate,
    .scale = gl2_scale,
    .rotate = gl2_rotate,
    .clear = gl2_clear,
    .draw_point = gl2_draw_point,
    .draw_line = gl2_draw_line,
    .draw_triangle = gl2_draw_triangle,
    .draw_rectangle = gl2_draw_rectangle,
    .draw_circle = gl2_draw_circle,
    .create_texture = gl2_create_texture,
    .update_texture = gl2_update_texture,
    .load_texture = gl2_load_texture,
    .delete_texture = gl2_delete_texture,
    .set_texture_smooth = gl2_set_texture_smooth,
    .draw_texture = gl2_draw_texture,
    .draw_subtexture = gl2_draw_subtexture,
    .draw_text = gl2_draw_text,
    .create_surface = gl2_create_surface,
    .delete_surface = gl2_delete_surface,
    .set_surface = gl2_set_surface,
    .reset_surface = gl2_reset_surface,
    .draw_surface = gl2_draw_surface,
};
