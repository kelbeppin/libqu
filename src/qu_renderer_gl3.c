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

#define QU_MODULE "renderer-gl3"

#include "qu.h"

#include <GL/gl.h>
#include <GL/glext.h>

//------------------------------------------------------------------------------
// qu_renderer_gl3.c: OpenGL 3.3 renderer
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

enum gl3__shader
{
    GL3__SHADER_VERTEX,
    GL3__SHADER_SOLID,
    GL3__SHADER_TEXTURED,
    GL3__SHADER_CANVAS,
    GL3__TOTAL_SHADERS,
};

enum gl3__program
{
    GL3__PROGRAM_SHAPE,
    GL3__PROGRAM_TEXTURE,
    GL3__PROGRAM_CANVAS,
    GL3__TOTAL_PROGRAMS,
};

enum gl3__uniform
{
    GL3__UNIFORM_PROJECTION,
    GL3__UNIFORM_MODELVIEW,
    GL3__UNIFORM_COLOR,
    GL3__TOTAL_UNIFORMS,
};

struct gl3__shader_desc
{
    GLenum type;
    char const *name;
    char const *src[1];
};

struct gl3__program_desc
{
    char const *name;
    enum gl3__shader vert;
    enum gl3__shader frag;
};

struct gl3__vertex_attribute_desc
{
    unsigned int size;
};

struct gl3__vertex_format_desc
{
    unsigned int attributes;
    unsigned int stride;
    enum gl3__program program;
};

static char const *s_attributes[QU__TOTAL_VERTEX_ATTRIBUTES] = {
    [QU__VERTEX_ATTRIBUTE_POSITION] = "a_position",
    [QU__VERTEX_ATTRIBUTE_COLOR] = "a_color",
    [QU__VERTEX_ATTRIBUTE_TEXCOORD] = "a_texCoord",
};

static struct gl3__shader_desc const s_shaders[GL3__TOTAL_SHADERS] = {
    [GL3__SHADER_VERTEX] = {
        .type = GL_VERTEX_SHADER,
        .name = "SHADER_VERTEX",
        .src = {
            "#version 330 core\n"
            "in vec2 a_position;\n"
            "in vec4 a_color;\n"
            "in vec2 a_texCoord;\n"
            "out vec4 v_color;\n"
            "out vec2 v_texCoord;\n"
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
    [GL3__SHADER_SOLID] = {
        .type = GL_FRAGMENT_SHADER,
        .name = "SHADER_SOLID",
        .src = {
            "#version 330 core\n"
            "uniform vec4 u_color;\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = u_color;\n"
            "}\n"
        },
    },
    [GL3__SHADER_TEXTURED] = {
        .type = GL_FRAGMENT_SHADER,
        .name = "SHADER_TEXTURED",
        .src = {
            "#version 330 core\n"
            "in vec2 v_texCoord;\n"
            "uniform sampler2D u_texture;\n"
            "uniform vec4 u_color;\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = texture2D(u_texture, v_texCoord) * u_color;\n"
            "}\n"
        },
    },
    [GL3__SHADER_CANVAS] = {
        .type = GL_VERTEX_SHADER,
        .name = "SHADER_CANVAS",
        .src = {
            "#version 330 core\n"
            "in vec2 a_position;\n"
            "in vec2 a_texCoord;\n"
            "out vec2 v_texCoord;\n"
            "uniform mat4 u_projection;\n"
            "void main()\n"
            "{\n"
            "    v_texCoord = a_texCoord;\n"
            "    vec4 position = vec4(a_position, 0.0, 1.0);\n"
            "    gl_Position = u_projection * position;\n"
            "}\n"
        },
    },
};

static struct gl3__program_desc const s_programs[GL3__TOTAL_PROGRAMS] = {
    [GL3__PROGRAM_SHAPE] = {
        .name = "PROGRAM_SHAPE",
        .vert = GL3__SHADER_VERTEX,
        .frag = GL3__SHADER_SOLID,
    },
    [GL3__PROGRAM_TEXTURE] = {
        .name = "PROGRAM_TEXTURE",
        .vert = GL3__SHADER_VERTEX,
        .frag = GL3__SHADER_TEXTURED,
    },
    [GL3__PROGRAM_CANVAS] = {
        .name = "PROGRAM_CANVAS",
        .vert = GL3__SHADER_CANVAS,
        .frag = GL3__SHADER_TEXTURED,
    },
};

static char const *s_uniforms[GL3__TOTAL_UNIFORMS] = {
    [GL3__UNIFORM_PROJECTION] = "u_projection",
    [GL3__UNIFORM_MODELVIEW] = "u_modelView",
    [GL3__UNIFORM_COLOR] = "u_color",
};

static struct gl3__vertex_attribute_desc const s_vertex_attributes[QU__TOTAL_VERTEX_ATTRIBUTES] = {
    [QU__VERTEX_ATTRIBUTE_POSITION] = { .size = 2 },
    [QU__VERTEX_ATTRIBUTE_COLOR] = { .size = 4 },
    [QU__VERTEX_ATTRIBUTE_TEXCOORD] = { .size = 2 },
};

static struct gl3__vertex_format_desc const s_vertex_formats[QU__TOTAL_VERTEX_FORMATS] = {
    [QU__VERTEX_FORMAT_SOLID] = {
        .attributes = (1 << QU__VERTEX_ATTRIBUTE_POSITION),
        .stride = 2,
        .program = GL3__PROGRAM_SHAPE,
    },
    [QU__VERTEX_FORMAT_TEXTURED] = {
        .attributes = (1 << QU__VERTEX_ATTRIBUTE_POSITION) | (1 << QU__VERTEX_ATTRIBUTE_TEXCOORD),
        .stride = 4,
        .program = GL3__PROGRAM_TEXTURE,
    },
};

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

struct gl3__program_info
{
    GLuint id;
    GLint uniforms[GL3__TOTAL_UNIFORMS];
    unsigned int dirty_uniforms;
};

struct gl3__vertex_format_info
{
    GLuint array;
    GLuint buffer;
    GLuint buffer_size;
};

struct qu__gl3_renderer_priv
{
    struct qu__texture_data const *bound_texture;
    struct qu__texture_data const *bound_surface;
    enum gl3__program used_program;

    struct gl3__program_info programs[GL3__TOTAL_PROGRAMS];
    struct gl3__vertex_format_info vertex_formats[QU__TOTAL_VERTEX_FORMATS];

    qu_mat4 projection;
    qu_mat4 modelview;
    GLfloat color[4];

    int w_display;
    int h_display;

    PFNGLATTACHSHADERPROC glAttachShader;
    PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
    PFNGLCOMPILESHADERPROC glCompileShader;
    PFNGLCREATEPROGRAMPROC glCreateProgram;
    PFNGLCREATESHADERPROC glCreateShader;
    PFNGLDELETEPROGRAMPROC glDeleteProgram;
    PFNGLDELETESHADERPROC glDeleteShader;
    PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
    PFNGLGETPROGRAMIVPROC glGetProgramiv;
    PFNGLGETSHADERIVPROC glGetShaderiv;
    PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
    PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
    PFNGLLINKPROGRAMPROC glLinkProgram;
    PFNGLSHADERSOURCEPROC glShaderSource;
    PFNGLUNIFORM4FVPROC glUniform4fv;
    PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
    PFNGLUSEPROGRAMPROC glUseProgram;

    PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
    PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
    PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;

    PFNGLBINDBUFFERPROC glBindBuffer;
    PFNGLBUFFERDATAPROC glBufferData;
    PFNGLBUFFERSUBDATAPROC glBufferSubData;
    PFNGLDELETEBUFFERSPROC glDeleteBuffers;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
    PFNGLGENBUFFERSPROC glGenBuffers;
    PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;

    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
    PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
    PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
    PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
    PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
    PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
    PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
    PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
};

static struct qu__gl3_renderer_priv priv;

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
    priv.glAttachShader = qu__core_get_gl_proc_address("glAttachShader");
    priv.glBindAttribLocation = qu__core_get_gl_proc_address("glBindAttribLocation");
    priv.glCompileShader = qu__core_get_gl_proc_address("glCompileShader");
    priv.glCreateProgram = qu__core_get_gl_proc_address("glCreateProgram");
    priv.glCreateShader = qu__core_get_gl_proc_address("glCreateShader");
    priv.glDeleteProgram = qu__core_get_gl_proc_address("glDeleteProgram");
    priv.glDeleteShader = qu__core_get_gl_proc_address("glDeleteShader");
    priv.glGetProgramInfoLog = qu__core_get_gl_proc_address("glGetProgramInfoLog");
    priv.glGetProgramiv = qu__core_get_gl_proc_address("glGetProgramiv");
    priv.glGetShaderiv = qu__core_get_gl_proc_address("glGetShaderiv");
    priv.glGetUniformLocation = qu__core_get_gl_proc_address("glGetUniformLocation");
    priv.glGetShaderInfoLog = qu__core_get_gl_proc_address("glGetShaderInfoLog");
    priv.glLinkProgram = qu__core_get_gl_proc_address("glLinkProgram");
    priv.glShaderSource = qu__core_get_gl_proc_address("glShaderSource");
    priv.glUniform4fv = qu__core_get_gl_proc_address("glUniform4fv");
    priv.glUniformMatrix4fv = qu__core_get_gl_proc_address("glUniformMatrix4fv");
    priv.glUseProgram = qu__core_get_gl_proc_address("glUseProgram");

    priv.glBindBuffer = qu__core_get_gl_proc_address("glBindBuffer");
    priv.glBufferData = qu__core_get_gl_proc_address("glBufferData");
    priv.glBufferSubData = qu__core_get_gl_proc_address("glBufferSubData");
    priv.glDeleteBuffers = qu__core_get_gl_proc_address("glDeleteBuffers");
    priv.glDisableVertexAttribArray = qu__core_get_gl_proc_address("glDisableVertexAttribArray");
    priv.glEnableVertexAttribArray = qu__core_get_gl_proc_address("glEnableVertexAttribArray");
    priv.glGenBuffers = qu__core_get_gl_proc_address("glGenBuffers");
    priv.glVertexAttribPointer = qu__core_get_gl_proc_address("glVertexAttribPointer");

    priv.glBindVertexArray = qu__core_get_gl_proc_address("glBindVertexArray");
    priv.glDeleteVertexArrays = qu__core_get_gl_proc_address("glDeleteVertexArrays");
    priv.glGenVertexArrays = qu__core_get_gl_proc_address("glGenVertexArrays");

    priv.glBindFramebuffer = qu__core_get_gl_proc_address("glBindFramebuffer");
    priv.glBindRenderbuffer = qu__core_get_gl_proc_address("glBindRenderbuffer");
    priv.glCheckFramebufferStatus = qu__core_get_gl_proc_address("glCheckFramebufferStatus");
    priv.glDeleteFramebuffers = qu__core_get_gl_proc_address("glDeleteFramebuffers");
    priv.glDeleteRenderbuffers = qu__core_get_gl_proc_address("glDeleteRenderbuffers");
    priv.glGenFramebuffers = qu__core_get_gl_proc_address("glGenFramebuffers");
    priv.glGenRenderbuffers = qu__core_get_gl_proc_address("glGenRenderbuffers");
    priv.glFramebufferRenderbuffer = qu__core_get_gl_proc_address("glFramebufferRenderbuffer");
    priv.glFramebufferTexture2D = qu__core_get_gl_proc_address("glFramebufferTexture2D");
    priv.glRenderbufferStorage = qu__core_get_gl_proc_address("glRenderbufferStorage");
}

static GLuint load_shader(struct gl3__shader_desc const *desc)
{
    GLuint shader = priv.glCreateShader(desc->type);
    priv.glShaderSource(shader, 1, desc->src, NULL);
    priv.glCompileShader(shader);

    GLint success;
    priv.glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char buffer[256];

        priv.glGetShaderInfoLog(shader, 256, NULL, buffer);
        priv.glDeleteShader(shader);

        QU_ERROR("Failed to compile GLSL shader %s. Reason:\n%s\n", desc->name, buffer);
        return 0;
    }

    QU_INFO("Shader %s is compiled successfully.\n", desc->name);
    return shader;
}

static GLuint build_program(char const *name, GLuint vs, GLuint fs)
{
    GLuint program = priv.glCreateProgram();

    priv.glAttachShader(program, vs);
    priv.glAttachShader(program, fs);

    for (int j = 0; j < QU__TOTAL_VERTEX_ATTRIBUTES; j++) {
        priv.glBindAttribLocation(program, j, s_attributes[j]);
    }

    priv.glLinkProgram(program);

    GLint success;
    priv.glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        char buffer[256];
        
        priv.glGetProgramInfoLog(program, 256, NULL, buffer);
        priv.glDeleteProgram(program);

        QU_ERROR("Failed to link GLSL program %s: %s\n", name, buffer);
        return 0;
    }

    QU_INFO("Shader program %s is built successfully.\n", name);
    return program;
}

static void update_uniforms(void)
{
    if (priv.used_program == -1) {
        return;
    }

    struct gl3__program_info *info = &priv.programs[priv.used_program];

    if (info->dirty_uniforms & (1 << GL3__UNIFORM_PROJECTION)) {
        _GL_CHECK(priv.glUniformMatrix4fv(
            info->uniforms[GL3__UNIFORM_PROJECTION],
            1, GL_FALSE, priv.projection.m
        ));
    }

    if (info->dirty_uniforms & (1 << GL3__UNIFORM_MODELVIEW)) {
        _GL_CHECK(priv.glUniformMatrix4fv(
            info->uniforms[GL3__UNIFORM_MODELVIEW],
            1, GL_FALSE, priv.modelview.m
        ));
    }

    if (info->dirty_uniforms & (1 << GL3__UNIFORM_COLOR)) {
        _GL_CHECK(priv.glUniform4fv(info->uniforms[GL3__UNIFORM_COLOR], 1, priv.color));
    }

    info->dirty_uniforms = 0;
}

//------------------------------------------------------------------------------

static bool gl3__query(qu_params const *params)
{
    if (qu__core_get_renderer() != QU__RENDERER_GL_CORE) {
        return false;
    }

    return true;
}

static void gl3__initialize(qu_params const *params)
{
    memset(&priv, 0, sizeof(priv));

    load_gl_functions();

    _GL_CHECK(glEnable(GL_BLEND));

    _GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    _GL_CHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

    GLuint shaders[GL3__TOTAL_SHADERS];
    
    for (int i = 0; i < GL3__TOTAL_SHADERS; i++) {
        shaders[i] = load_shader(&s_shaders[i]);
    }

    for (int i = 0; i < GL3__TOTAL_PROGRAMS; i++) {
        GLuint vs = shaders[s_programs[i].vert];
        GLuint fs = shaders[s_programs[i].frag];

        priv.programs[i].id = build_program(s_programs[i].name, vs, fs);

        for (int j = 0; j < GL3__TOTAL_UNIFORMS; j++) {
            priv.programs[i].uniforms[j] = priv.glGetUniformLocation(priv.programs[i].id, s_uniforms[j]);
        }
    }

    for (int i = 0; i < GL3__TOTAL_SHADERS; i++) {
        _GL_CHECK(priv.glDeleteShader(shaders[i]));
    }

    priv.used_program = -1;

    for (int i = 0; i < QU__TOTAL_VERTEX_FORMATS; i++) {
        struct gl3__vertex_format_info *format = &priv.vertex_formats[i];

        _GL_CHECK(priv.glGenVertexArrays(1, &format->array));
        _GL_CHECK(priv.glGenBuffers(1, &format->buffer));

        _GL_CHECK(priv.glBindVertexArray(format->array));

        for (int j = 0; j < QU__TOTAL_VERTEX_ATTRIBUTES; j++) {
            if (s_vertex_formats[i].attributes & (1 << j)) {
                _GL_CHECK(priv.glEnableVertexAttribArray(j));
            }
        }
    }

    QU_INFO("Initialized.\n");
}

static void gl3__terminate(void)
{
    for (int i = 0; i < QU__TOTAL_VERTEX_FORMATS; i++) {
        priv.glDeleteVertexArrays(1, &priv.vertex_formats[i].array);
        priv.glDeleteBuffers(1, &priv.vertex_formats[i].buffer);
    }

    for (int i = 0; i < GL3__TOTAL_PROGRAMS; i++) {
        priv.glDeleteProgram(priv.programs[i].id);
    }

    QU_INFO("Terminated.\n");
}

static void gl3__upload_vertex_data(enum qu__vertex_format vertex_format, float const *data, size_t size)
{
    struct gl3__vertex_format_info *info = &priv.vertex_formats[vertex_format];

    _GL_CHECK(priv.glBindBuffer(GL_ARRAY_BUFFER, info->buffer));

    if (size < info->buffer_size) {
        _GL_CHECK(priv.glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * size, data));
        return;
    }

    _GL_CHECK(priv.glBufferData(GL_ARRAY_BUFFER, sizeof(float) * size, data, GL_STREAM_DRAW));

    _GL_CHECK(priv.glBindVertexArray(info->array));

    unsigned int offset = 0;

    for (int i = 0; i < QU__TOTAL_VERTEX_ATTRIBUTES; i++) {
        if (s_vertex_formats[vertex_format].attributes & (1 << i)) {
            GLsizei size = s_vertex_attributes[i].size;
            GLsizei stride = sizeof(float) * s_vertex_formats[vertex_format].stride;

            _GL_CHECK(priv.glVertexAttribPointer(i, size, GL_FLOAT, GL_FALSE, stride, (void *) offset));
            offset += sizeof(float) * size;
        }
    }

    info->buffer_size = size;
}

static void gl3__apply_projection(qu_mat4 const *projection)
{
    qu_mat4_copy(&priv.projection, projection);

    for (int i = 0; i < GL3__TOTAL_PROGRAMS; i++) {
        priv.programs[i].dirty_uniforms |= (1 << GL3__UNIFORM_PROJECTION);
    }

    update_uniforms();
}

static void gl3__apply_transform(qu_mat4 const *transform)
{
    qu_mat4_copy(&priv.modelview, transform);
    
    for (int i = 0; i < GL3__TOTAL_PROGRAMS; i++) {
        priv.programs[i].dirty_uniforms |= (1 << GL3__UNIFORM_MODELVIEW);
    }

    update_uniforms();
}

static void gl3__apply_surface(struct qu__texture_data const *data)
{
    if (priv.bound_surface == data) {
        return;
    }

    _GL_CHECK(priv.glBindFramebuffer(GL_FRAMEBUFFER, data ? data->priv[0] : 0));

    if (data) {
        _GL_CHECK(glViewport(0, 0, data->width, data->height));
    } else {
        _GL_CHECK(glViewport(0, 0, priv.w_display, priv.h_display));
    }

    priv.bound_surface = data;
}

static void gl3__apply_texture(struct qu__texture_data const *data)
{
    if (priv.bound_texture == data) {
        return;
    }

    _GL_CHECK(glBindTexture(GL_TEXTURE_2D, data ? data->u : 0));
    priv.bound_texture = data;
}

static void gl3__apply_clear_color(qu_color color)
{
    GLfloat v[4];

    color_conv(v, color);
    _GL_CHECK(glClearColor(v[0], v[1], v[2], v[3]));
}

static void gl3__apply_draw_color(qu_color color)
{
    color_conv(priv.color, color);
    
    for (int i = 0; i < GL3__TOTAL_PROGRAMS; i++) {
        priv.programs[i].dirty_uniforms |= (1 << GL3__UNIFORM_COLOR);
    }

    update_uniforms();
}

static void gl3__apply_vertex_format(enum qu__vertex_format vertex_format)
{
    struct gl3__vertex_format_info *info = &priv.vertex_formats[vertex_format];

    _GL_CHECK(priv.glBindVertexArray(info->array));

    GLuint program = s_vertex_formats[vertex_format].program;

    if (priv.used_program != program) {
        priv.used_program = program;

        _GL_CHECK(priv.glUseProgram(priv.programs[program].id));
        update_uniforms();
    }
}

static void gl3__exec_resize(int width, int height)
{
    if (!priv.bound_surface) {
        _GL_CHECK(glViewport(0, 0, width, height));
    }

    priv.w_display = width;
    priv.h_display = height;
}

static void gl3__exec_clear(void)
{
    _GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
}

static void gl3__exec_draw(enum qu__render_mode render_mode, unsigned int first_vertex, unsigned int total_vertices)
{
    _GL_CHECK(glDrawArrays(mode_map[render_mode], (GLint) first_vertex, (GLsizei) total_vertices));
}

static void gl3__load_texture(struct qu__texture_data *texture)
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

    _GL_CHECK(glBindTexture(GL_TEXTURE_2D, priv.bound_texture ? priv.bound_texture->u : 0));
}

static void gl3__unload_texture(struct qu__texture_data *texture)
{
    GLuint id = (GLuint) texture->u;

    _GL_CHECK(glDeleteTextures(1, &id));
}

static void gl3__set_texture_smooth(struct qu__texture_data *data, bool smooth)
{
    GLuint id = (GLuint) data->u;

    _GL_CHECK(glBindTexture(GL_TEXTURE_2D, id));

    if (smooth) {
        _GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    } else {
        _GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    }

    if (priv.bound_texture) {
        _GL_CHECK(glBindTexture(GL_TEXTURE_2D, priv.bound_texture->u));
    } else {
        _GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
    }
}

static void gl3__create_surface(struct qu__texture_data *data)
{
    GLuint fbo;
    GLuint depth;
    GLuint color;

    _GL_CHECK(priv.glGenFramebuffers(1, &fbo));
    _GL_CHECK(priv.glBindFramebuffer(GL_FRAMEBUFFER, fbo));

    _GL_CHECK(priv.glGenRenderbuffers(1, &depth));
    _GL_CHECK(priv.glBindRenderbuffer(GL_RENDERBUFFER, depth));
    _GL_CHECK(priv.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, data->width, data->height));

    _GL_CHECK(glGenTextures(1, &color));
    _GL_CHECK(glBindTexture(GL_TEXTURE_2D, color));
    _GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, data->width, data->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));

    _GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    _GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

    _GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    _GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    _GL_CHECK(priv.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth));
    _GL_CHECK(priv.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0));

    GLenum status = priv.glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        QU_HALT("[TODO] FBO error handling.");
    }

    data->priv[0] = fbo;
    data->priv[1] = depth;
    data->u = color;

    _GL_CHECK(priv.glBindFramebuffer(GL_FRAMEBUFFER, priv.bound_surface ? priv.bound_surface->priv[0] : 0));
    _GL_CHECK(glBindTexture(GL_TEXTURE_2D, priv.bound_texture ? priv.bound_texture->u : 0));
}

static void gl3__destroy_surface(struct qu__texture_data *data)
{
    GLuint fbo = data->priv[0];
    GLuint depth = data->priv[1];
    GLuint color = data->u;

    _GL_CHECK(priv.glDeleteFramebuffers(1, &fbo));
    _GL_CHECK(priv.glDeleteRenderbuffers(1, &depth));
    _GL_CHECK(glDeleteTextures(1, &color));
}

//------------------------------------------------------------------------------

struct qu__renderer_impl const qu__renderer_gl3 = {
    .query = gl3__query,
    .initialize = gl3__initialize,
    .terminate = gl3__terminate,
    .upload_vertex_data = gl3__upload_vertex_data,
    .apply_projection = gl3__apply_projection,
    .apply_transform = gl3__apply_transform,
    .apply_surface = gl3__apply_surface,
    .apply_texture = gl3__apply_texture,
    .apply_clear_color = gl3__apply_clear_color,
    .apply_draw_color = gl3__apply_draw_color,
    .apply_vertex_format = gl3__apply_vertex_format,
    .exec_resize = gl3__exec_resize,
	.exec_clear = gl3__exec_clear,
	.exec_draw = gl3__exec_draw,
    .load_texture = gl3__load_texture,
    .unload_texture = gl3__unload_texture,
    .set_texture_smooth = gl3__set_texture_smooth,
    .create_surface = gl3__create_surface,
    .destroy_surface = gl3__destroy_surface,
};
