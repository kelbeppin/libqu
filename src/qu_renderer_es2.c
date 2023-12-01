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
// qu_renderer_es2.c: OpenGL ES 2.0 (WebGL) renderer
//------------------------------------------------------------------------------

#ifdef __EMSCRIPTEN__
#define GL_GLEXT_PROTOTYPES
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

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

enum shader
{
    SHADER_VERTEX,
    SHADER_SOLID,
    SHADER_TEXTURED,
    SHADER_FONT,
    TOTAL_SHADERS,
};

enum uniform
{
    UNIFORM_PROJECTION,
    UNIFORM_MODELVIEW,
    UNIFORM_COLOR,
    TOTAL_UNIFORMS,
};

//------------------------------------------------------------------------------

struct shader_desc
{
    GLenum type;
    char const *name;
    char const *src[1];
};

struct program_desc
{
    char const *name;
    enum shader vert;
    enum shader frag;
};

struct vertex_attribute_desc
{
    char const *name;
    unsigned int size;
};

struct vertex_format_desc
{
    unsigned int attributes;
    unsigned int stride;
};

//------------------------------------------------------------------------------

static struct shader_desc const shader_desc[TOTAL_SHADERS] = {
    [SHADER_VERTEX] = {
        .type = GL_VERTEX_SHADER,
        .name = "SHADER_VERTEX",
        .src = {
            "#version 100\n"
            "attribute vec2 a_position;\n"
            "attribute vec4 a_color;\n"
            "attribute vec2 a_texCoord;\n"
            "varying vec4 v_color;\n"
            "varying vec2 v_texCoord;\n"
            "uniform mat4 u_projection;\n"
            "uniform mat4 u_modelView;\n"
            "void main()\n"
            "{\n"
            "    v_texCoord = a_texCoord;\n"
            "    v_color = a_color;\n"
            "    vec4 position = vec4(a_position, 0.0, 1.0);\n"
            "    gl_Position = u_projection * u_modelView * position;\n"
            "}\n"
        },
    },
    [SHADER_SOLID] = {
        .type = GL_FRAGMENT_SHADER,
        .name = "SHADER_SOLID",
        .src = {
            "#version 100\n"
            "precision mediump float;\n"
            "uniform vec4 u_color;\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = u_color;\n"
            "}\n"
        },
    },
    [SHADER_TEXTURED] = {
        .type = GL_FRAGMENT_SHADER,
        .name = "SHADER_TEXTURED",
        .src = {
            "#version 100\n"
            "precision mediump float;\n"
            "varying vec2 v_texCoord;\n"
            "uniform sampler2D u_texture;\n"
            "uniform vec4 u_color;\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = texture2D(u_texture, v_texCoord) * u_color;\n"
            "}\n"
        },
    },
    [SHADER_FONT] = {
        .type = GL_FRAGMENT_SHADER,
        .name = "SHADER_FONT",
        .src = {
            "#version 100\n"
            "precision mediump float;\n"
            "varying vec2 v_texCoord;\n"
            "uniform sampler2D u_texture;\n"
            "uniform vec4 u_color;\n"
            "void main()\n"
            "{\n"
            "    float alpha = texture2D(u_texture, v_texCoord).r;\n"
            "    gl_FragColor = vec4(u_color.rgb, alpha);\n"
            "}\n"
        },
    },
};

static struct program_desc const program_desc[QU_TOTAL_BRUSHES] = {
    [QU_BRUSH_SOLID] = {
        .name = "BRUSH_SOLID",
        .vert = SHADER_VERTEX,
        .frag = SHADER_SOLID,
    },
    [QU_BRUSH_TEXTURED] = {
        .name = "BRUSH_TEXTURED",
        .vert = SHADER_VERTEX,
        .frag = SHADER_TEXTURED,
    },
    [QU_BRUSH_FONT] = {
        .name = "BRUSH_FONT",
        .vert = SHADER_VERTEX,
        .frag = SHADER_FONT,
    },
};

static char const *uniform_names[TOTAL_UNIFORMS] = {
    [UNIFORM_PROJECTION] = "u_projection",
    [UNIFORM_MODELVIEW] = "u_modelView",
    [UNIFORM_COLOR] = "u_color",
};

static struct vertex_attribute_desc const vertex_attribute_desc[QU_TOTAL_VERTEX_ATTRIBUTES] = {
    [QU_VERTEX_ATTRIBUTE_POSITION] = { .name = "a_position", .size = 2 },
    [QU_VERTEX_ATTRIBUTE_COLOR] = { .name = "a_color", .size = 4 },
    [QU_VERTEX_ATTRIBUTE_TEXCOORD] = {.name = "a_texCoord", .size = 2 },
};

static struct vertex_format_desc const vertex_format_desc[QU_TOTAL_VERTEX_FORMATS] = {
    [QU_VERTEX_FORMAT_2XY] = {
        .attributes = QU_VERTEX_ATTRIBUTE_BIT_POSITION,
        .stride = 2,
    },
    [QU_VERTEX_FORMAT_4XYST] = {
        .attributes = QU_VERTEX_ATTRIBUTE_BIT_POSITION | QU_VERTEX_ATTRIBUTE_BIT_TEXCOORD,
        .stride = 4,
    },
};

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

struct program_info
{
    GLuint id;
    GLint uniforms[TOTAL_UNIFORMS];
    unsigned int dirty_uniforms;
};

struct vertex_format_info
{
    GLuint array;
    GLuint buffer;
    GLuint buffer_size;
};

struct priv
{
    qu_texture_obj const *bound_texture;
    qu_surface_obj const *bound_surface;
    qu_brush used_program;

    struct program_info programs[QU_TOTAL_BRUSHES];
    struct vertex_format_info vertex_formats[QU_TOTAL_VERTEX_FORMATS];

    qu_mat4 projection;
    qu_mat4 modelview;
    GLfloat color[4];

    void (*vertex_format_initialize)(qu_vertex_format);
    void (*vertex_format_terminate)(qu_vertex_format);
    void (*vertex_format_update)(qu_vertex_format);
    void (*vertex_format_apply)(qu_vertex_format);
};

//------------------------------------------------------------------------------

static struct priv priv;

//------------------------------------------------------------------------------

#ifndef GL_GLEXT_PROTOTYPES

PFNGLBINDVERTEXARRAYOESPROC glBindVertexArrayOES;
PFNGLDELETEVERTEXARRAYSOESPROC glDeleteVertexArraysOES;
PFNGLGENVERTEXARRAYSOESPROC glGenVertexArraysOES;

#endif

//------------------------------------------------------------------------------

static void color_conv(GLfloat *dst, qu_color color)
{
    dst[0] = ((color >> 0x10) & 0xFF) / 255.f;
    dst[1] = ((color >> 0x08) & 0xFF) / 255.f;
    dst[2] = ((color >> 0x00) & 0xFF) / 255.f;
    dst[3] = ((color >> 0x18) & 0xFF) / 255.f;
}

static bool check_extension(char const *extension)
{
    char *extensions = qu_strdup((char const *) glGetString(GL_EXTENSIONS));
    char *token = strtok(extensions, " ");
    bool found = false;

    while (token) {
        if (strcmp(token, extension) == 0) {
            found = true;
            break;
        }

        token = strtok(NULL, " ");
    }

    pl_free(extensions);

    return found;
}

static GLuint load_shader(struct shader_desc const *desc)
{
    GLuint shader = glCreateShader(desc->type);
    CHECK_GL(glShaderSource(shader, 1, desc->src, NULL));
    CHECK_GL(glCompileShader(shader));

    GLint success;
    CHECK_GL(glGetShaderiv(shader, GL_COMPILE_STATUS, &success));

    if (!success) {
        char buffer[256];

        CHECK_GL(glGetShaderInfoLog(shader, 256, NULL, buffer));
        CHECK_GL(glDeleteShader(shader));

        QU_LOGE("Failed to compile GLSL shader %s. Reason:\n%s\n", desc->name, buffer);
        return 0;
    }

    QU_LOGI("Shader %s is compiled successfully.\n", desc->name);
    return shader;
}

static GLuint build_program(char const *name, GLuint vs, GLuint fs)
{
    GLuint program = glCreateProgram();

    CHECK_GL(glAttachShader(program, vs));
    CHECK_GL(glAttachShader(program, fs));

    for (int j = 0; j < QU_TOTAL_VERTEX_ATTRIBUTES; j++) {
        CHECK_GL(glBindAttribLocation(program, j, vertex_attribute_desc[j].name));
    }

    CHECK_GL(glLinkProgram(program));

    GLint success;
    CHECK_GL(glGetProgramiv(program, GL_LINK_STATUS, &success));

    if (!success) {
        char buffer[256];
        
        CHECK_GL(glGetProgramInfoLog(program, 256, NULL, buffer));
        CHECK_GL(glDeleteProgram(program));

        QU_LOGE("Failed to link GLSL program %s: %s\n", name, buffer);
        return 0;
    }

    QU_LOGI("Shader program %s is built successfully.\n", name);
    return program;
}

static void update_uniforms(void)
{
    if (priv.used_program == -1) {
        return;
    }

    struct program_info *info = &priv.programs[priv.used_program];

    if (info->dirty_uniforms & (1 << UNIFORM_PROJECTION)) {
        CHECK_GL(glUniformMatrix4fv(info->uniforms[UNIFORM_PROJECTION], 1, GL_FALSE, priv.projection.m));
    }

    if (info->dirty_uniforms & (1 << UNIFORM_MODELVIEW)) {
        CHECK_GL(glUniformMatrix4fv(info->uniforms[UNIFORM_MODELVIEW], 1, GL_FALSE, priv.modelview.m));
    }

    if (info->dirty_uniforms & (1 << UNIFORM_COLOR)) {
        CHECK_GL(glUniform4fv(info->uniforms[UNIFORM_COLOR], 1, priv.color));
    }

    info->dirty_uniforms = 0;
}

static void vertex_format_initialize(qu_vertex_format format)
{
}

static void vertex_format_terminate(qu_vertex_format format)
{
}

static void vertex_format_update(qu_vertex_format format)
{
}

static void vertex_format_apply(qu_vertex_format format)
{
    struct vertex_format_info *info = &priv.vertex_formats[format];
    struct vertex_format_desc const *desc = &vertex_format_desc[format];

    unsigned int offset = 0;

    for (int i = 0; i < QU_TOTAL_VERTEX_ATTRIBUTES; i++) {
        if (desc->attributes & (1 << i)) {
            GLsizei size = vertex_attribute_desc[i].size;
            GLsizei stride = sizeof(float) * desc->stride;

            CHECK_GL(glEnableVertexAttribArray(i));
            CHECK_GL(glVertexAttribPointer(i, size, GL_FLOAT, GL_FALSE, stride, (void *) (intptr_t) offset));
            offset += sizeof(float) * size;
        } else {
            CHECK_GL(glDisableVertexAttribArray(i));
        }
    }
}

static void vao_vertex_format_initialize(qu_vertex_format format)
{
    struct vertex_format_info *info = &priv.vertex_formats[format];
    struct vertex_format_desc const *desc = &vertex_format_desc[format];

    CHECK_GL(glGenVertexArraysOES(1, &info->array));
    CHECK_GL(glBindVertexArrayOES(info->array));

    for (int i = 0; i < QU_TOTAL_VERTEX_ATTRIBUTES; i++) {
        if (desc->attributes & (1 << i)) {
            CHECK_GL(glEnableVertexAttribArray(i));
        }
    }
}

static void vao_vertex_format_terminate(qu_vertex_format format)
{
    struct vertex_format_info *info = &priv.vertex_formats[format];

    CHECK_GL(glDeleteVertexArraysOES(1, &info->array));
}

static void vao_vertex_format_update(qu_vertex_format format)
{
    struct vertex_format_info *info = &priv.vertex_formats[format];
    struct vertex_format_desc const *desc = &vertex_format_desc[format];

    CHECK_GL(glBindVertexArrayOES(info->array));

    unsigned int offset = 0;

    for (int i = 0; i < QU_TOTAL_VERTEX_ATTRIBUTES; i++) {
        if (desc->attributes & (1 << i)) {
            GLsizei size = vertex_attribute_desc[i].size;
            GLsizei stride = sizeof(float) * desc->stride;

            CHECK_GL(glVertexAttribPointer(i, size, GL_FLOAT, GL_FALSE, stride, (void *) (intptr_t) offset));
            offset += sizeof(float) * size;
        }
    }
}

static void vao_vertex_format_apply(qu_vertex_format format)
{
    struct vertex_format_info *info = &priv.vertex_formats[format];
    CHECK_GL(glBindVertexArrayOES(info->array));
}

static void surface_add_multisample_buffer(qu_surface_obj *surface)
{
    surface->sample_count = 1;

#if 0
    GLsizei width = surface->texture.width;
    GLsizei height = surface->texture.height;

    GLuint ms_fbo;
    GLuint ms_color;

    CHECK_GL(glGenFramebuffers(1, &ms_fbo));
    CHECK_GL(glBindFramebuffer(GL_FRAMEBUFFER, ms_fbo));

    CHECK_GL(glGenRenderbuffers(1, &ms_color));
    CHECK_GL(glBindRenderbuffer(GL_RENDERBUFFER, ms_color));

    CHECK_GL(glRenderbufferStorageMultisample(
        GL_RENDERBUFFER, surface->sample_count, GL_RGBA8, width, height
    ));

    CHECK_GL(glFramebufferRenderbuffer(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, ms_color
    ));

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        QU_HALT("[TODO] FBO error handling.");
    }

    surface->priv[2] = ms_fbo;
    surface->priv[3] = ms_color;
#endif
}

static void surface_remove_multisample_buffer(qu_surface_obj *surface)
{
#if 0
    GLuint ms_fbo = surface->priv[2];
    GLuint ms_color = surface->priv[3];

    CHECK_GL(glDeleteFramebuffers(1, &ms_fbo));
    CHECK_GL(glDeleteRenderbuffers(1, &ms_color));
#endif
}

static void surface_blit_multisample_buffer(qu_surface_obj const *surface)
{
#if 0
    GLuint fbo = surface->priv[0];
    GLuint ms_fbo = surface->priv[2];

    CHECK_GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo));
    CHECK_GL(glBindFramebuffer(GL_READ_FRAMEBUFFER, ms_fbo));

    GLsizei width = surface->texture.width;
    GLsizei height = surface->texture.height;

    CHECK_GL(glBlitFramebuffer(
        0, 0, width, height,
        0, 0, width, height,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST
    ));
#endif
}

//------------------------------------------------------------------------------

static bool es2_query(void)
{
    if (strcmp(qu_get_graphics_context_name(), "OpenGL ES 2.0")) {
        return false;
    }

    return true;
}

static void es2_initialize(void)
{
    memset(&priv, 0, sizeof(priv));

    if (check_extension("GL_OES_vertex_array_object")) {
#ifndef GL_GLEXT_PROTOTYPES
        glBindVertexArrayOES = qu_gl_get_proc_address("glBindVertexArrayOES");
        glDeleteVertexArraysOES = qu_gl_get_proc_address("glDeleteVertexArraysOES");
        glGenVertexArraysOES = qu_gl_get_proc_address("glGenVertexArraysOES");
#endif

        priv.vertex_format_initialize = vao_vertex_format_initialize;
        priv.vertex_format_terminate = vao_vertex_format_terminate;
        priv.vertex_format_apply = vao_vertex_format_apply;
        priv.vertex_format_update = vao_vertex_format_update;

        QU_LOGI("GL_OES_vertex_array_object is supported, will use VAOs.\n");
    } else {
        priv.vertex_format_initialize = vertex_format_initialize;
        priv.vertex_format_terminate = vertex_format_terminate;
        priv.vertex_format_apply = vertex_format_apply;
        priv.vertex_format_update = vertex_format_update;

        QU_LOGI("GL_OES_vertex_array_object is not supported, won't use VAOs.\n");
    }

    CHECK_GL(glEnable(GL_BLEND));
    CHECK_GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

    GLuint shaders[TOTAL_SHADERS];
    
    for (int i = 0; i < TOTAL_SHADERS; i++) {
        shaders[i] = load_shader(&shader_desc[i]);
    }

    for (int i = 0; i < QU_TOTAL_BRUSHES; i++) {
        GLuint vs = shaders[program_desc[i].vert];
        GLuint fs = shaders[program_desc[i].frag];

        priv.programs[i].id = build_program(program_desc[i].name, vs, fs);

        for (int j = 0; j < TOTAL_UNIFORMS; j++) {
            priv.programs[i].uniforms[j] = glGetUniformLocation(priv.programs[i].id, uniform_names[j]);
        }
    }

    for (int i = 0; i < TOTAL_SHADERS; i++) {
        CHECK_GL(glDeleteShader(shaders[i]));
    }

    priv.used_program = -1;

    for (int i = 0; i < QU_TOTAL_VERTEX_FORMATS; i++) {
        struct vertex_format_info *info = &priv.vertex_formats[i];
        CHECK_GL(glGenBuffers(1, &info->buffer));
        priv.vertex_format_initialize(i);
    }

    QU_LOGI("Initialized.\n");
}

static void es2_terminate(void)
{
    for (int i = 0; i < QU_TOTAL_VERTEX_FORMATS; i++) {
        CHECK_GL(glDeleteBuffers(1, &priv.vertex_formats[i].buffer));
        priv.vertex_format_terminate(i);
    }

    for (int i = 0; i < QU_TOTAL_BRUSHES; i++) {
        CHECK_GL(glDeleteProgram(priv.programs[i].id));
    }

    QU_LOGI("Terminated.\n");
}

static void es2_upload_vertex_data(qu_vertex_format format, float const *data, size_t size)
{
    struct vertex_format_info *info = &priv.vertex_formats[format];

    CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, info->buffer));

    if (size < info->buffer_size) {
        CHECK_GL(glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * size, data));
    } else {
        CHECK_GL(glBufferData(GL_ARRAY_BUFFER, sizeof(float) * size, data, GL_STREAM_DRAW));
    }

    priv.vertex_format_update(format);

    info->buffer_size = size;
}

static void es2_apply_projection(qu_mat4 const *projection)
{
    qu_mat4_copy(&priv.projection, projection);

    for (int i = 0; i < QU_TOTAL_BRUSHES; i++) {
        priv.programs[i].dirty_uniforms |= (1 << UNIFORM_PROJECTION);
    }

    update_uniforms();
}

static void es2_apply_transform(qu_mat4 const *transform)
{
    qu_mat4_copy(&priv.modelview, transform);
    
    for (int i = 0; i < QU_TOTAL_BRUSHES; i++) {
        priv.programs[i].dirty_uniforms |= (1 << UNIFORM_MODELVIEW);
    }

    update_uniforms();
}

static void es2_apply_surface(qu_surface_obj const *surface)
{
    if (priv.bound_surface && priv.bound_surface->sample_count > 1) {
        surface_blit_multisample_buffer(priv.bound_surface);
    }

    if (priv.bound_surface == surface) {
        return;
    }

    GLsizei width = surface->texture.width;
    GLsizei height = surface->texture.height;

    if (surface->sample_count > 1) {
        CHECK_GL(glBindFramebuffer(GL_FRAMEBUFFER, surface->priv[2]));
    } else {
        CHECK_GL(glBindFramebuffer(GL_FRAMEBUFFER, surface->priv[0]));
    }

    CHECK_GL(glViewport(0, 0, width, height));

    priv.bound_surface = surface;
}

static void es2_apply_texture(qu_texture_obj const *texture)
{
    if (priv.bound_texture == texture) {
        return;
    }

    CHECK_GL(glBindTexture(GL_TEXTURE_2D, texture ? texture->priv[0] : 0));
    priv.bound_texture = texture;
}

static void es2_apply_clear_color(qu_color color)
{
    GLfloat v[4];

    color_conv(v, color);
    CHECK_GL(glClearColor(v[0], v[1], v[2], v[3]));
}

static void es2_apply_draw_color(qu_color color)
{
    color_conv(priv.color, color);
    
    for (int i = 0; i < QU_TOTAL_BRUSHES; i++) {
        priv.programs[i].dirty_uniforms |= (1 << UNIFORM_COLOR);
    }

    update_uniforms();
}

static void es2_apply_brush(qu_brush brush)
{
    if (priv.used_program == brush) {
        return;
    }

    CHECK_GL(glUseProgram(priv.programs[brush].id));
    priv.used_program = brush;

    update_uniforms();
}

static void es2_apply_vertex_format(qu_vertex_format format)
{
    struct vertex_format_info *info = &priv.vertex_formats[format];

    CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, info->buffer));

    priv.vertex_format_apply(format);
}

static void es2_apply_blend_mode(qu_blend_mode mode)
{
    GLenum csf = blend_factor_map[mode.color_src_factor];
    GLenum cdf = blend_factor_map[mode.color_dst_factor];
    GLenum asf = blend_factor_map[mode.alpha_src_factor];
    GLenum adf = blend_factor_map[mode.alpha_dst_factor];

    GLenum ceq = blend_equation_map[mode.color_equation];
    GLenum aeq = blend_equation_map[mode.alpha_equation];

    CHECK_GL(glBlendFuncSeparate(csf, cdf, asf, adf));
    CHECK_GL(glBlendEquationSeparate(ceq, aeq));
}

static void es2_exec_resize(int width, int height)
{
    CHECK_GL(glViewport(0, 0, width, height));
}

static void es2_exec_clear(void)
{
    CHECK_GL(glClear(GL_COLOR_BUFFER_BIT));
}

static void es2_exec_draw(qu_render_mode mode, unsigned int first_vertex, unsigned int total_vertices)
{
    CHECK_GL(glDrawArrays(mode_map[mode], (GLint) first_vertex, (GLsizei) total_vertices));
}

static void es2_load_texture(qu_texture_obj *texture)
{
    GLuint id = texture->priv[0];
    
    if (id == 0) {
        CHECK_GL(glGenTextures(1, &id));
        texture->priv[0] = (uintptr_t) id;
    }

    CHECK_GL(glBindTexture(GL_TEXTURE_2D, id));

    GLenum internal_format = texture_format_map[texture->channels - 1];
    GLenum format = internal_format;

    CHECK_GL(glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internal_format,
        texture->width,
        texture->height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        texture->pixels
    ));

    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

    if (priv.bound_texture) {
        CHECK_GL(glBindTexture(GL_TEXTURE_2D, priv.bound_texture->priv[0]));
    } else {
        CHECK_GL(glBindTexture(GL_TEXTURE_2D, 0));
    }
}

static void es2_unload_texture(qu_texture_obj *texture)
{
    GLuint id = (GLuint) texture->priv[0];

    CHECK_GL(glDeleteTextures(1, &id));
}

static void es2_set_texture_smooth(qu_texture_obj *texture, bool smooth)
{
    GLuint id = (GLuint) texture->priv[0];

    CHECK_GL(glBindTexture(GL_TEXTURE_2D, id));

    if (smooth) {
        CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    } else {
        CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    }

    if (priv.bound_texture) {
        CHECK_GL(glBindTexture(GL_TEXTURE_2D, priv.bound_texture->priv[0]));
    } else {
        CHECK_GL(glBindTexture(GL_TEXTURE_2D, 0));
    }
}

static void es2_create_surface(qu_surface_obj *surface)
{
    GLsizei width = surface->texture.width;
    GLsizei height = surface->texture.height;

    GLuint fbo;
    GLuint depth;
    GLuint color;

    CHECK_GL(glGenFramebuffers(1, &fbo));
    CHECK_GL(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

    CHECK_GL(glGenRenderbuffers(1, &depth));
    CHECK_GL(glBindRenderbuffer(GL_RENDERBUFFER, depth));
    CHECK_GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height));

    CHECK_GL(glGenTextures(1, &color));
    CHECK_GL(glBindTexture(GL_TEXTURE_2D, color));
    CHECK_GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));

    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    CHECK_GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth));
    CHECK_GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0));

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        QU_HALT("[TODO] FBO error handling.");
    }

    surface->priv[0] = fbo;
    surface->priv[1] = depth;
    surface->texture.priv[0] = color;

    int max_samples = qu_gl_get_samples();
    surface->sample_count = QU_MIN(max_samples, surface->sample_count);

    if (surface->sample_count > 1) {
        surface_add_multisample_buffer(surface);
    }

    if (priv.bound_surface) {
        CHECK_GL(glBindFramebuffer(GL_FRAMEBUFFER, priv.bound_surface->priv[0]));
    } else {
        CHECK_GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }

    if (priv.bound_texture) {
        CHECK_GL(glBindTexture(GL_TEXTURE_2D, priv.bound_texture->priv[0]));
    } else {
        CHECK_GL(glBindTexture(GL_TEXTURE_2D, 0));
    }
}

static void es2_destroy_surface(qu_surface_obj *surface)
{
    if (surface->sample_count > 1) {
        surface_remove_multisample_buffer(surface);
    }

    GLuint fbo = surface->priv[0];
    GLuint depth = surface->priv[1];
    GLuint color = surface->texture.priv[0];

    CHECK_GL(glDeleteFramebuffers(1, &fbo));
    CHECK_GL(glDeleteRenderbuffers(1, &depth));
    CHECK_GL(glDeleteTextures(1, &color));
}

static void es2_set_surface_antialiasing_level(qu_surface_obj *surface, int level)
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

qu_renderer_impl const qu_es2_renderer_impl = {
    .query = es2_query,
    .initialize = es2_initialize,
    .terminate = es2_terminate,
    .upload_vertex_data = es2_upload_vertex_data,
    .apply_projection = es2_apply_projection,
    .apply_transform = es2_apply_transform,
    .apply_surface = es2_apply_surface,
    .apply_texture = es2_apply_texture,
    .apply_clear_color = es2_apply_clear_color,
    .apply_draw_color = es2_apply_draw_color,
    .apply_brush = es2_apply_brush,
    .apply_vertex_format = es2_apply_vertex_format,
    .apply_blend_mode = es2_apply_blend_mode,
    .exec_resize = es2_exec_resize,
	.exec_clear = es2_exec_clear,
	.exec_draw = es2_exec_draw,
    .load_texture = es2_load_texture,
    .unload_texture = es2_unload_texture,
    .set_texture_smooth = es2_set_texture_smooth,
    .create_surface = es2_create_surface,
    .destroy_surface = es2_destroy_surface,
    .set_surface_antialiasing_level = es2_set_surface_antialiasing_level,
};
