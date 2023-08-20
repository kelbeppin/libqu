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

#define QU_MODULE "renderer-gl1"

#include "qu.h"

#include <GL/gl.h>
#include <GL/glext.h>

//------------------------------------------------------------------------------
// qu_renderer_gl1.c: Legacy OpenGL renderer
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Debug messages and error checks

#ifdef NDEBUG

#define _GL_CHECK(_call) \
    _call

#else

static void _gl_check(char const *call, char const *file, int line)
{
    GLenum error = glGetError();

    if (error == GL_NO_ERROR) {
        return;
    }
    
    QU_WARNING("OpenGL error(s) occured in %s:\n", file);
    QU_WARNING("%4d: %s\n", line, call);

    do {
        switch (error) {
        case GL_INVALID_ENUM:
            QU_WARNING("-- GL_INVALID_ENUM\n");
            break;
        case GL_INVALID_VALUE:
            QU_WARNING("-- GL_INVALID_VALUE\n");
            break;
        case GL_INVALID_OPERATION:
            QU_WARNING("-- GL_INVALID_OPERATION\n");
            break;
        case GL_STACK_OVERFLOW:
            QU_WARNING("-- GL_STACK_OVERFLOW\n");
            break;
        case GL_STACK_UNDERFLOW:
            QU_WARNING("-- GL_STACK_UNDERFLOW\n");
            break;
        case GL_OUT_OF_MEMORY:
            QU_WARNING("-- GL_OUT_OF_MEMORY\n");
            break;
        default:
            QU_WARNING("-- 0x%04x\n", error);
            break;
        }
    } while ((error = glGetError()) != GL_NO_ERROR);
}

#define _GL_CHECK(_call) \
    do { \
        _call; \
        _gl_check(#_call, __FILE__, __LINE__); \
    } while (0);

#endif

//------------------------------------------------------------------------------

static GLenum const mode_map[QU__TOTAL_RENDER_MODES] = {
    GL_POINTS,
    GL_LINES,
    GL_LINE_STRIP,
    GL_LINE_LOOP,
    GL_TRIANGLES,
    GL_TRIANGLE_STRIP,
    GL_TRIANGLE_FAN,
};

static GLenum const texture_format_map[4] = {
    GL_LUMINANCE,
    GL_LUMINANCE_ALPHA,
    GL_RGB,
    GL_RGBA,
};

//------------------------------------------------------------------------------

struct qu__gl1_renderer_priv
{
    GLuint bound_texture;
    float const *vertex_data[QU__TOTAL_VERTEX_FORMATS];
};

static struct qu__gl1_renderer_priv priv;

//------------------------------------------------------------------------------

static void color_conv(GLfloat *dst, qu_color color)
{
    dst[0] = ((color >> 0x10) & 0xFF) / 255.f;
    dst[1] = ((color >> 0x08) & 0xFF) / 255.f;
    dst[2] = ((color >> 0x00) & 0xFF) / 255.f;
    dst[3] = ((color >> 0x18) & 0xFF) / 255.f;
}

//------------------------------------------------------------------------------

static bool gl1__query(qu_params const *params)
{
    if (qu__core_get_renderer() != QU__RENDERER_GL_COMPAT) {
        return false;
    }

    return true;
}

static void gl1__initialize(qu_params const *params)
{
    memset(&priv, 0, sizeof(priv));

    // Temporary.

    glViewport(0, 0, 512, 512);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    QU_INFO("Initialized.\n");
}

static void gl1__terminate(void)
{
    QU_INFO("Terminated.\n");
}

static void gl1__upload_vertex_data(enum qu__vertex_format vertex_format, float const *data, size_t size)
{
    priv.vertex_data[vertex_format] = data;
}

static void gl1__apply_projection(qu_mat4 const *projection)
{
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projection->m);

    glMatrixMode(GL_MODELVIEW);
}

static void gl1__apply_transform(qu_mat4 const *transform)
{
    glLoadMatrixf(transform->m);
}

static void gl1__apply_texture(struct qu__texture_data const *data)
{
    priv.bound_texture = data ? data->u : 0;
    _GL_CHECK(glBindTexture(GL_TEXTURE_2D, priv.bound_texture));
}

static void gl1__apply_clear_color(qu_color color)
{
    GLfloat v[4];

    color_conv(v, color);
    _GL_CHECK(glClearColor(v[0], v[1], v[2], v[3]));
}

static void gl1__apply_draw_color(qu_color color)
{
    GLfloat v[4];

    color_conv(v, color);
    _GL_CHECK(glColor4fv(v));
}

static void gl1__apply_vertex_format(enum qu__vertex_format vertex_format)
{
    switch (vertex_format) {
    case QU__VERTEX_FORMAT_SOLID:
        glEnableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(2, GL_FLOAT, 0, priv.vertex_data[vertex_format]);
        break;
    case QU__VERTEX_FORMAT_TEXTURED:
        glEnableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(2, GL_FLOAT, sizeof(float) * 4, priv.vertex_data[vertex_format] + 0);
        glTexCoordPointer(2, GL_FLOAT, sizeof(float) * 4, priv.vertex_data[vertex_format] + 2);
        break;
    default:
        break;
    }
}

static void gl1__exec_resize(int width, int height)
{
    _GL_CHECK(glViewport(0, 0, width, height));
}

static void gl1__exec_clear(void)
{
    _GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
}

static void gl1__exec_draw(enum qu__render_mode render_mode, unsigned int first_vertex, unsigned int total_vertices)
{
    _GL_CHECK(glDrawArrays(mode_map[render_mode], (GLint) first_vertex, (GLsizei) total_vertices));
}

static void gl1__load_texture(struct qu__texture_data *texture)
{
    GLuint id;
    
    _GL_CHECK(glGenTextures(1, &id));
    _GL_CHECK(glBindTexture(GL_TEXTURE_2D, id));

    GLenum format = texture_format_map[texture->image->channels - 1];

    _GL_CHECK(glTexImage2D(
        GL_TEXTURE_2D,
        0,
        format,
        texture->image->width,
        texture->image->height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        texture->image->pixels
    ));

    _GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    _GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

    texture->u = (uintptr_t) id;

    _GL_CHECK(glBindTexture(GL_TEXTURE_2D, priv.bound_texture));
}

static void gl1__unload_texture(struct qu__texture_data *texture)
{
    GLuint id = (GLuint) texture->u;

    _GL_CHECK(glDeleteTextures(1, &id));
}

//------------------------------------------------------------------------------

struct qu__renderer_impl const qu__renderer_gl1 = {
    .query = gl1__query,
    .initialize = gl1__initialize,
    .terminate = gl1__terminate,
    .upload_vertex_data = gl1__upload_vertex_data,
    .apply_projection = gl1__apply_projection,
    .apply_transform = gl1__apply_transform,
    .apply_texture = gl1__apply_texture,
    .apply_clear_color = gl1__apply_clear_color,
    .apply_draw_color = gl1__apply_draw_color,
    .apply_vertex_format = gl1__apply_vertex_format,
    .exec_resize = gl1__exec_resize,
	.exec_clear = gl1__exec_clear,
	.exec_draw = gl1__exec_draw,
    .load_texture = gl1__load_texture,
    .unload_texture = gl1__unload_texture,
};
