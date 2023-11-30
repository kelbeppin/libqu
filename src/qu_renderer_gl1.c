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
// qu_renderer_gl1.c: Legacy OpenGL renderer
//
// This renderer targets OpenGL 1.5, which was released back in 2005.
// 
// It also uses the following extension(s):
// * EXT_framebuffer_object [required]
// * EXT_framebuffer_blit
// * EXT_framebuffer_multisample
//------------------------------------------------------------------------------

#include <GL/gl.h>
#include <GL/glext.h>

#include "qu_core.h"
#include "qu_graphics.h"
#include "qu_log.h"
#include "qu_util.h"

//------------------------------------------------------------------------------
// Debug messages and error checks

#ifdef NDEBUG

#define CHECK_GL(_call) \
    _call

#else

static void _check_gl(char const *call, char const *file, int line)
{
    GLenum error = glGetError();

    if (error == GL_NO_ERROR) {
        return;
    }
    
    QU_LOGW("OpenGL error(s) occured in %s:\n", file);
    QU_LOGW("%4d: %s\n", line, call);

    do {
        switch (error) {
        case GL_INVALID_ENUM:
            QU_LOGW("-- GL_INVALID_ENUM\n");
            break;
        case GL_INVALID_VALUE:
            QU_LOGW("-- GL_INVALID_VALUE\n");
            break;
        case GL_INVALID_OPERATION:
            QU_LOGW("-- GL_INVALID_OPERATION\n");
            break;
        case GL_STACK_OVERFLOW:
            QU_LOGW("-- GL_STACK_OVERFLOW\n");
            break;
        case GL_STACK_UNDERFLOW:
            QU_LOGW("-- GL_STACK_UNDERFLOW\n");
            break;
        case GL_OUT_OF_MEMORY:
            QU_LOGW("-- GL_OUT_OF_MEMORY\n");
            break;
        default:
            QU_LOGW("-- 0x%04x\n", error);
            break;
        }
    } while ((error = glGetError()) != GL_NO_ERROR);
}

#define CHECK_GL(_call) \
    do { \
        _call; \
        _check_gl(#_call, __FILE__, __LINE__); \
    } while (0);

#endif

//------------------------------------------------------------------------------

static GLenum const mode_map[QU_TOTAL_RENDER_MODES] = {
    GL_POINTS,
    GL_LINES,
    GL_LINE_LOOP,
    GL_LINE_STRIP,
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

static GLenum const blend_factor_map[10] = {
    GL_ZERO,
    GL_ONE,
    GL_SRC_COLOR,
    GL_ONE_MINUS_SRC_COLOR,
    GL_DST_COLOR,
    GL_ONE_MINUS_DST_COLOR,
    GL_SRC_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_DST_ALPHA,
    GL_ONE_MINUS_DST_ALPHA,
};

static GLenum const blend_equation_map[3] = {
    GL_FUNC_ADD,
    GL_FUNC_SUBTRACT,
    GL_FUNC_REVERSE_SUBTRACT,
};

//------------------------------------------------------------------------------

struct ext
{
    PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
    PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;

    PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT;
    PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT;
    PFNGLBLITFRAMEBUFFEREXTPROC glBlitFramebufferEXT;
    PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glCheckFramebufferStatusEXT;
    PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT;
    PFNGLDELETERENDERBUFFERSEXTPROC glDeleteRenderbuffersEXT;
    PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT;
    PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT;
    PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT;
    PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT;
    PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT;
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT;
};

struct priv
{
    GLuint bound_texture;
    qu_surface_obj const *bound_surface;
    float const *vertex_data[QU_TOTAL_VERTEX_FORMATS];
};

static struct ext ext;
static struct priv priv;

//------------------------------------------------------------------------------

static void color_conv(GLfloat *dst, qu_color color)
{
    dst[0] = ((color >> 0x10) & 0xFF) / 255.f;
    dst[1] = ((color >> 0x08) & 0xFF) / 255.f;
    dst[2] = ((color >> 0x00) & 0xFF) / 255.f;
    dst[3] = ((color >> 0x18) & 0xFF) / 255.f;
}

static void load_gl_functions(void)
{
    ext.glBlendFuncSeparate = qu_gl_get_proc_address("glBlendFuncSeparate");
    ext.glBlendEquationSeparate = qu_gl_get_proc_address("glBlendEquationSeparate");

    char *extensions = qu_strdup((char const *) glGetString(GL_EXTENSIONS));
    char *token = strtok(extensions, " ");

    while (token) {
        if (strcmp(token, "GL_EXT_framebuffer_object") == 0) {
            ext.glBindFramebufferEXT = qu_gl_get_proc_address("glBindFramebufferEXT");
            ext.glBindRenderbufferEXT = qu_gl_get_proc_address("glBindRenderbufferEXT");
            ext.glCheckFramebufferStatusEXT = qu_gl_get_proc_address("glCheckFramebufferStatusEXT");
            ext.glDeleteFramebuffersEXT = qu_gl_get_proc_address("glDeleteFramebuffersEXT");
            ext.glDeleteRenderbuffersEXT = qu_gl_get_proc_address("glDeleteRenderbuffersEXT");
            ext.glGenFramebuffersEXT = qu_gl_get_proc_address("glGenFramebuffersEXT");
            ext.glGenRenderbuffersEXT = qu_gl_get_proc_address("glGenRenderbuffersEXT");
            ext.glFramebufferRenderbufferEXT = qu_gl_get_proc_address("glFramebufferRenderbufferEXT");
            ext.glFramebufferTexture2DEXT = qu_gl_get_proc_address("glFramebufferTexture2DEXT");
            ext.glRenderbufferStorageEXT = qu_gl_get_proc_address("glRenderbufferStorageEXT");
        } else if (strcmp(token, "GL_EXT_framebuffer_blit") == 0) {
            ext.glBlitFramebufferEXT = qu_gl_get_proc_address("glBlitFramebufferEXT");
        } else if (strcmp(token, "GL_EXT_framebuffer_multisample") == 0) {
            ext.glRenderbufferStorageMultisampleEXT = qu_gl_get_proc_address("glRenderbufferStorageMultisampleEXT");
        }

        token = strtok(NULL, " ");
    }

    pl_free(extensions);
}

static void surface_add_multisample_buffer(qu_surface_obj *surface)
{
    GLsizei width = surface->texture.width;
    GLsizei height = surface->texture.height;

    GLuint ms_fbo;
    GLuint ms_color;

    CHECK_GL(ext.glGenFramebuffersEXT(1, &ms_fbo));
    CHECK_GL(ext.glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, ms_fbo));

    CHECK_GL(ext.glGenRenderbuffersEXT(1, &ms_color));
    CHECK_GL(ext.glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, ms_color));

    CHECK_GL(ext.glRenderbufferStorageMultisampleEXT(
        GL_RENDERBUFFER_EXT, surface->sample_count, GL_RGBA8, width, height
    ));

    CHECK_GL(ext.glFramebufferRenderbufferEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, ms_color
    ));

    GLenum status = ext.glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);

    if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
        QU_HALT("[TODO] FBO error handling.");
    }

    surface->priv[2] = ms_fbo;
    surface->priv[3] = ms_color;
}

static void surface_remove_multisample_buffer(qu_surface_obj *surface)
{
    GLuint ms_fbo = surface->priv[2];
    GLuint ms_color = surface->priv[3];

    CHECK_GL(ext.glDeleteFramebuffersEXT(1, &ms_fbo));
    CHECK_GL(ext.glDeleteRenderbuffersEXT(1, &ms_color));
}

//------------------------------------------------------------------------------

static bool gl1_query(void)
{
    if (strcmp(qu_get_graphics_context_name(), "OpenGL (Compatibility Profile)")) {
        return false;
    }

    memset(&ext, 0, sizeof(ext));

    load_gl_functions();

    if (!ext.glGenFramebuffersEXT) {
        QU_LOGE("Required OpenGL extension EXT_framebuffer_object is not supported.\n");
        return false;
    }

    if (!ext.glBlitFramebufferEXT) {
        QU_LOGW("OpenGL extension EXT_framebuffer_blit is not supported.\n");
    }

    if (!ext.glRenderbufferStorageMultisampleEXT) {
        QU_LOGW("OpenGL extension EXT_framebuffer_multisample is not supported.\n");
    }

    return true;
}

static void gl1_initialize(void)
{
    memset(&priv, 0, sizeof(priv));

    CHECK_GL(glEnable(GL_TEXTURE_2D));
    CHECK_GL(glEnable(GL_BLEND));

    CHECK_GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

    QU_LOGI("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
    QU_LOGI("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
    QU_LOGI("GL_VERSION: %s\n", glGetString(GL_VERSION));

    QU_LOGI("Initialized.\n");
}

static void gl1_terminate(void)
{
    QU_LOGI("Terminated.\n");
}

static void gl1_upload_vertex_data(qu_vertex_format vertex_format, float const *data, size_t size)
{
    // Don't actually upload anything.
    priv.vertex_data[vertex_format] = data;
}

static void gl1_apply_projection(qu_mat4 const *projection)
{
    CHECK_GL(glMatrixMode(GL_PROJECTION));
    CHECK_GL(glLoadMatrixf(projection->m));

    CHECK_GL(glMatrixMode(GL_MODELVIEW));
}

static void gl1_apply_transform(qu_mat4 const *transform)
{
    glLoadMatrixf(transform->m);
}

static void gl1_apply_surface(qu_surface_obj const *surface)
{
    if (priv.bound_surface && priv.bound_surface->sample_count > 1) {
        GLuint fbo = priv.bound_surface->priv[0];
        GLuint ms_fbo = priv.bound_surface->priv[2];

        CHECK_GL(ext.glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, fbo));
        CHECK_GL(ext.glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, ms_fbo));

        GLsizei width = priv.bound_surface->texture.width;
        GLsizei height = priv.bound_surface->texture.height;

        CHECK_GL(ext.glBlitFramebufferEXT(
            0, 0, width, height,
            0, 0, width, height,
            GL_COLOR_BUFFER_BIT,
            GL_NEAREST
        ));
    }

    if (priv.bound_surface == surface) {
        return;
    }

    GLsizei width = surface->texture.width;
    GLsizei height = surface->texture.height;

    if (surface->sample_count > 1) {
        CHECK_GL(ext.glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, surface->priv[2]));
    } else {
        CHECK_GL(ext.glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, surface->priv[0]));
    }

    CHECK_GL(glViewport(0, 0, width, height));

    priv.bound_surface = surface;
}

static void gl1_apply_texture(qu_texture_obj const *texture)
{
    GLuint id = texture ? texture->priv[0] : 0;

    if (priv.bound_texture == id) {
        return;
    }

    CHECK_GL(glBindTexture(GL_TEXTURE_2D, id));
    priv.bound_texture = id;
}

static void gl1_apply_clear_color(qu_color color)
{
    GLfloat v[4];

    color_conv(v, color);
    CHECK_GL(glClearColor(v[0], v[1], v[2], v[3]));
}

static void gl1_apply_draw_color(qu_color color)
{
    GLfloat v[4];

    color_conv(v, color);
    CHECK_GL(glColor4fv(v));
}

static void gl1_apply_brush(qu_brush brush)
{
}

static void gl1_apply_vertex_format(qu_vertex_format vertex_format)
{
    switch (vertex_format) {
    case QU_VERTEX_FORMAT_2XY:
        CHECK_GL(glEnableClientState(GL_VERTEX_ARRAY));
        CHECK_GL(glDisableClientState(GL_COLOR_ARRAY));
        CHECK_GL(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
        
        CHECK_GL(glVertexPointer(2, GL_FLOAT, 0, priv.vertex_data[vertex_format]));
        break;
    case QU_VERTEX_FORMAT_4XYST:
        CHECK_GL(glEnableClientState(GL_VERTEX_ARRAY));
        CHECK_GL(glDisableClientState(GL_COLOR_ARRAY));
        CHECK_GL(glEnableClientState(GL_TEXTURE_COORD_ARRAY));

        CHECK_GL(glVertexPointer(2, GL_FLOAT, sizeof(float) * 4, priv.vertex_data[vertex_format] + 0));
        CHECK_GL(glTexCoordPointer(2, GL_FLOAT, sizeof(float) * 4, priv.vertex_data[vertex_format] + 2));
        break;
    default:
        break;
    }
}

static void gl1_apply_blend_mode(qu_blend_mode mode)
{
    GLenum csf = blend_factor_map[mode.color_src_factor];
    GLenum cdf = blend_factor_map[mode.color_dst_factor];
    GLenum asf = blend_factor_map[mode.alpha_src_factor];
    GLenum adf = blend_factor_map[mode.alpha_dst_factor];

    GLenum ceq = blend_equation_map[mode.color_equation];
    GLenum aeq = blend_equation_map[mode.alpha_equation];

    CHECK_GL(ext.glBlendFuncSeparate(csf, cdf, asf, adf));
    CHECK_GL(ext.glBlendEquationSeparate(ceq, aeq));
}

static void gl1_exec_resize(int width, int height)
{
    CHECK_GL(glViewport(0, 0, width, height));
}

static void gl1_exec_clear(void)
{
    CHECK_GL(glClear(GL_COLOR_BUFFER_BIT));
}

static void gl1_exec_draw(qu_render_mode render_mode, unsigned int first_vertex, unsigned int total_vertices)
{
    CHECK_GL(glDrawArrays(mode_map[render_mode], (GLint) first_vertex, (GLsizei) total_vertices));
}

static void gl1_load_texture(qu_texture_obj *texture)
{
    GLuint id = texture->priv[0];
    
    if (id == 0) {
        CHECK_GL(glGenTextures(1, &id));
        texture->priv[0] = (uintptr_t) id;
    }

    CHECK_GL(glBindTexture(GL_TEXTURE_2D, id));

    GLenum format = texture_format_map[texture->channels - 1];

    CHECK_GL(glTexImage2D(
        GL_TEXTURE_2D,
        0,
        format,
        texture->width,
        texture->height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        texture->pixels
    ));

    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

    CHECK_GL(glBindTexture(GL_TEXTURE_2D, priv.bound_texture));
}

static void gl1_unload_texture(qu_texture_obj *texture)
{
    GLuint id = (GLuint) texture->priv[0];

    CHECK_GL(glDeleteTextures(1, &id));
}

static void gl1_set_texture_smooth(qu_texture_obj *texture, bool smooth)
{
    GLuint id = (GLuint) texture->priv[0];

    if (!id) {
        return;
    }

    CHECK_GL(glBindTexture(GL_TEXTURE_2D, id));

    if (smooth) {
        CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    } else {
        CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    }

    CHECK_GL(glBindTexture(GL_TEXTURE_2D, priv.bound_texture));
}

static void gl1_create_surface(qu_surface_obj *surface)
{
    GLsizei width = surface->texture.width;
    GLsizei height = surface->texture.height;

    GLuint fbo;
    GLuint depth;
    GLuint color;

    CHECK_GL(ext.glGenFramebuffersEXT(1, &fbo));
    CHECK_GL(ext.glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo));

    CHECK_GL(ext.glGenRenderbuffersEXT(1, &depth));
    CHECK_GL(ext.glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth));
    CHECK_GL(ext.glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT16,
                                            width, height));

    CHECK_GL(glGenTextures(1, &color));
    CHECK_GL(glBindTexture(GL_TEXTURE_2D, color));
    CHECK_GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height,
                           0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));

    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    CHECK_GL(ext.glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                                                GL_RENDERBUFFER_EXT, depth));
    CHECK_GL(ext.glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                             GL_TEXTURE_2D, color, 0));

    GLenum status = ext.glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);

    if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
        QU_HALT("[TODO] FBO error handling.");
    }

    surface->priv[0] = fbo;
    surface->priv[1] = depth;
    surface->texture.priv[0] = color;

    int max_samples = 1;

    if (ext.glBlitFramebufferEXT && ext.glRenderbufferStorageMultisampleEXT) {
        max_samples = qu_gl_get_samples();
    }

    surface->sample_count = QU_MIN(max_samples, surface->sample_count);

    if (surface->sample_count > 1) {
        surface_add_multisample_buffer(surface);
    }

    if (priv.bound_surface) {
        CHECK_GL(ext.glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, priv.bound_surface->priv[0]));
    } else {
        CHECK_GL(ext.glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0));
    }

    CHECK_GL(glBindTexture(GL_TEXTURE_2D, priv.bound_texture));
}

static void gl1_destroy_surface(qu_surface_obj *surface)
{
    if (surface->sample_count > 1) {
        surface_remove_multisample_buffer(surface);
    }

    GLuint fbo = surface->priv[0];
    GLuint depth = surface->priv[1];
    GLuint color = surface->texture.priv[0];

    CHECK_GL(ext.glDeleteFramebuffersEXT(1, &fbo));
    CHECK_GL(ext.glDeleteRenderbuffersEXT(1, &depth));
    CHECK_GL(glDeleteTextures(1, &color));
}

static void gl1_set_surface_antialiasing_level(qu_surface_obj *surface, int level)
{
    if (surface->sample_count > 1) {
        surface_remove_multisample_buffer(surface);
    }

    surface->sample_count = level;

    if (surface->sample_count > 1) {
        surface_add_multisample_buffer(surface);
    }
}

//------------------------------------------------------------------------------

qu_renderer_impl const qu_gl1_renderer_impl = {
    .query = gl1_query,
    .initialize = gl1_initialize,
    .terminate = gl1_terminate,
    .upload_vertex_data = gl1_upload_vertex_data,
    .apply_projection = gl1_apply_projection,
    .apply_transform = gl1_apply_transform,
    .apply_surface = gl1_apply_surface,
    .apply_texture = gl1_apply_texture,
    .apply_clear_color = gl1_apply_clear_color,
    .apply_draw_color = gl1_apply_draw_color,
    .apply_brush = gl1_apply_brush,
    .apply_vertex_format = gl1_apply_vertex_format,
    .apply_blend_mode = gl1_apply_blend_mode,
    .exec_resize = gl1_exec_resize,
	.exec_clear = gl1_exec_clear,
	.exec_draw = gl1_exec_draw,
    .load_texture = gl1_load_texture,
    .unload_texture = gl1_unload_texture,
    .set_texture_smooth = gl1_set_texture_smooth,
    .create_surface = gl1_create_surface,
    .destroy_surface = gl1_destroy_surface,
    .set_surface_antialiasing_level = gl1_set_surface_antialiasing_level,
};
