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
#include "qu_core.h"

#include <GL/gl.h>
#include <GL/glext.h>

//------------------------------------------------------------------------------
// qu_renderer_gl3.c: OpenGL 3.3 renderer
//------------------------------------------------------------------------------

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
    [SHADER_SOLID] = {
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
    [SHADER_TEXTURED] = {
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
    [SHADER_FONT] = {
        .type = GL_FRAGMENT_SHADER,
        .name = "SHADER_FONT",
        .src = {
            "#version 330 core\n"
            "in vec2 v_texCoord;\n"
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

static GLenum const texture_format_map[4][2] = {
    { GL_R8, GL_RED },
    { GL_RG8, GL_RG },
    { GL_RGB8, GL_RGB },
    { GL_RGBA8, GL_RGBA },
};

static GLenum const texture_swizzle_map[4][4] = {
    { GL_RED, GL_RED, GL_RED, GL_ONE },
    { GL_RED, GL_RED, GL_RED, GL_GREEN },
    { GL_RED, GL_GREEN, GL_BLUE, GL_ONE },
    { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
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

struct ext
{
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
    PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
    PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
    PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
    PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
    PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
    PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
    PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample;

    PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
    PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;
};

struct priv
{
    qu_texture_obj const *bound_texture;
    qu_surface_obj const *bound_surface;
    int used_program;

    struct program_info programs[QU_TOTAL_BRUSHES];
    struct vertex_format_info vertex_formats[QU_TOTAL_VERTEX_FORMATS];

    qu_mat4 projection;
    qu_mat4 modelview;
    GLfloat color[4];
};

//------------------------------------------------------------------------------

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
    ext.glAttachShader = qu_gl_get_proc_address("glAttachShader");
    ext.glBindAttribLocation = qu_gl_get_proc_address("glBindAttribLocation");
    ext.glCompileShader = qu_gl_get_proc_address("glCompileShader");
    ext.glCreateProgram = qu_gl_get_proc_address("glCreateProgram");
    ext.glCreateShader = qu_gl_get_proc_address("glCreateShader");
    ext.glDeleteProgram = qu_gl_get_proc_address("glDeleteProgram");
    ext.glDeleteShader = qu_gl_get_proc_address("glDeleteShader");
    ext.glGetProgramInfoLog = qu_gl_get_proc_address("glGetProgramInfoLog");
    ext.glGetProgramiv = qu_gl_get_proc_address("glGetProgramiv");
    ext.glGetShaderiv = qu_gl_get_proc_address("glGetShaderiv");
    ext.glGetUniformLocation = qu_gl_get_proc_address("glGetUniformLocation");
    ext.glGetShaderInfoLog = qu_gl_get_proc_address("glGetShaderInfoLog");
    ext.glLinkProgram = qu_gl_get_proc_address("glLinkProgram");
    ext.glShaderSource = qu_gl_get_proc_address("glShaderSource");
    ext.glUniform4fv = qu_gl_get_proc_address("glUniform4fv");
    ext.glUniformMatrix4fv = qu_gl_get_proc_address("glUniformMatrix4fv");
    ext.glUseProgram = qu_gl_get_proc_address("glUseProgram");

    ext.glBindBuffer = qu_gl_get_proc_address("glBindBuffer");
    ext.glBufferData = qu_gl_get_proc_address("glBufferData");
    ext.glBufferSubData = qu_gl_get_proc_address("glBufferSubData");
    ext.glDeleteBuffers = qu_gl_get_proc_address("glDeleteBuffers");
    ext.glDisableVertexAttribArray = qu_gl_get_proc_address("glDisableVertexAttribArray");
    ext.glEnableVertexAttribArray = qu_gl_get_proc_address("glEnableVertexAttribArray");
    ext.glGenBuffers = qu_gl_get_proc_address("glGenBuffers");
    ext.glVertexAttribPointer = qu_gl_get_proc_address("glVertexAttribPointer");

    ext.glBindVertexArray = qu_gl_get_proc_address("glBindVertexArray");
    ext.glDeleteVertexArrays = qu_gl_get_proc_address("glDeleteVertexArrays");
    ext.glGenVertexArrays = qu_gl_get_proc_address("glGenVertexArrays");

    ext.glBindFramebuffer = qu_gl_get_proc_address("glBindFramebuffer");
    ext.glBindRenderbuffer = qu_gl_get_proc_address("glBindRenderbuffer");
    ext.glBlitFramebuffer = qu_gl_get_proc_address("glBlitFramebuffer");
    ext.glCheckFramebufferStatus = qu_gl_get_proc_address("glCheckFramebufferStatus");
    ext.glDeleteFramebuffers = qu_gl_get_proc_address("glDeleteFramebuffers");
    ext.glDeleteRenderbuffers = qu_gl_get_proc_address("glDeleteRenderbuffers");
    ext.glGenFramebuffers = qu_gl_get_proc_address("glGenFramebuffers");
    ext.glGenRenderbuffers = qu_gl_get_proc_address("glGenRenderbuffers");
    ext.glFramebufferRenderbuffer = qu_gl_get_proc_address("glFramebufferRenderbuffer");
    ext.glFramebufferTexture2D = qu_gl_get_proc_address("glFramebufferTexture2D");
    ext.glRenderbufferStorage = qu_gl_get_proc_address("glRenderbufferStorage");
    ext.glRenderbufferStorageMultisample = qu_gl_get_proc_address("glRenderbufferStorageMultisample");

    ext.glBlendFuncSeparate = qu_gl_get_proc_address("glBlendFuncSeparate");
    ext.glBlendEquationSeparate = qu_gl_get_proc_address("glBlendEquationSeparate");
}

static GLuint load_shader(struct shader_desc const *desc)
{
    GLuint shader = ext.glCreateShader(desc->type);
    ext.glShaderSource(shader, 1, desc->src, NULL);
    ext.glCompileShader(shader);

    GLint success;
    ext.glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char buffer[256];

        ext.glGetShaderInfoLog(shader, 256, NULL, buffer);
        ext.glDeleteShader(shader);

        QU_LOGE("Failed to compile GLSL shader %s. Reason:\n%s\n", desc->name, buffer);
        return 0;
    }

    QU_LOGI("Shader %s is compiled successfully.\n", desc->name);
    return shader;
}

static GLuint build_program(char const *name, GLuint vs, GLuint fs)
{
    GLuint program = ext.glCreateProgram();

    ext.glAttachShader(program, vs);
    ext.glAttachShader(program, fs);

    for (int j = 0; j < QU_TOTAL_VERTEX_ATTRIBUTES; j++) {
        ext.glBindAttribLocation(program, j, vertex_attribute_desc[j].name);
    }

    ext.glLinkProgram(program);

    GLint success;
    ext.glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        char buffer[256];
        
        ext.glGetProgramInfoLog(program, 256, NULL, buffer);
        ext.glDeleteProgram(program);

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
        CHECK_GL(ext.glUniformMatrix4fv(
            info->uniforms[UNIFORM_PROJECTION],
            1, GL_FALSE, priv.projection.m
        ));
    }

    if (info->dirty_uniforms & (1 << UNIFORM_MODELVIEW)) {
        CHECK_GL(ext.glUniformMatrix4fv(
            info->uniforms[UNIFORM_MODELVIEW],
            1, GL_FALSE, priv.modelview.m
        ));
    }

    if (info->dirty_uniforms & (1 << UNIFORM_COLOR)) {
        CHECK_GL(ext.glUniform4fv(info->uniforms[UNIFORM_COLOR], 1, priv.color));
    }

    info->dirty_uniforms = 0;
}

static void surface_add_multisample_buffer(qu_surface_obj *surface)
{
    GLsizei width = surface->texture.width;
    GLsizei height = surface->texture.height;

    GLuint ms_fbo;
    GLuint ms_color;

    CHECK_GL(ext.glGenFramebuffers(1, &ms_fbo));
    CHECK_GL(ext.glBindFramebuffer(GL_FRAMEBUFFER, ms_fbo));

    CHECK_GL(ext.glGenRenderbuffers(1, &ms_color));
    CHECK_GL(ext.glBindRenderbuffer(GL_RENDERBUFFER, ms_color));

    CHECK_GL(ext.glRenderbufferStorageMultisample(
        GL_RENDERBUFFER, surface->sample_count, GL_RGBA8, width, height
    ));

    CHECK_GL(ext.glFramebufferRenderbuffer(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, ms_color
    ));

    GLenum status = ext.glCheckFramebufferStatus(GL_FRAMEBUFFER);

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

    CHECK_GL(ext.glDeleteFramebuffers(1, &ms_fbo));
    CHECK_GL(ext.glDeleteRenderbuffers(1, &ms_color));
}

//------------------------------------------------------------------------------

static bool gl3_query(void)
{
    if (strcmp(qu_get_graphics_context_name(), "OpenGL")) {
        return false;
    }

    memset(&ext, 0, sizeof(ext));

    load_gl_functions();

    return true;
}

static void gl3_initialize(void)
{
    memset(&priv, 0, sizeof(priv));

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
            priv.programs[i].uniforms[j] = ext.glGetUniformLocation(priv.programs[i].id, uniform_names[j]);
        }
    }

    for (int i = 0; i < TOTAL_SHADERS; i++) {
        CHECK_GL(ext.glDeleteShader(shaders[i]));
    }

    priv.used_program = -1;

    for (int i = 0; i < QU_TOTAL_VERTEX_FORMATS; i++) {
        struct vertex_format_info *format = &priv.vertex_formats[i];

        CHECK_GL(ext.glGenVertexArrays(1, &format->array));
        CHECK_GL(ext.glGenBuffers(1, &format->buffer));

        CHECK_GL(ext.glBindVertexArray(format->array));

        for (int j = 0; j < QU_TOTAL_VERTEX_ATTRIBUTES; j++) {
            if (vertex_format_desc[i].attributes & (1 << j)) {
                CHECK_GL(ext.glEnableVertexAttribArray(j));
            }
        }
    }

    QU_LOGI("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
    QU_LOGI("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
    QU_LOGI("GL_VERSION: %s\n", glGetString(GL_VERSION));

    QU_LOGI("Initialized.\n");
}

static void gl3_terminate(void)
{
    for (int i = 0; i < QU_TOTAL_VERTEX_FORMATS; i++) {
        ext.glDeleteVertexArrays(1, &priv.vertex_formats[i].array);
        ext.glDeleteBuffers(1, &priv.vertex_formats[i].buffer);
    }

    for (int i = 0; i < QU_TOTAL_BRUSHES; i++) {
        ext.glDeleteProgram(priv.programs[i].id);
    }

    QU_LOGI("Terminated.\n");
}

static void gl3_upload_vertex_data(qu_vertex_format vertex_format, float const *data, size_t size)
{
    struct vertex_format_info *info = &priv.vertex_formats[vertex_format];

    CHECK_GL(ext.glBindBuffer(GL_ARRAY_BUFFER, info->buffer));

    if (size < info->buffer_size) {
        CHECK_GL(ext.glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * size, data));
        return;
    }

    CHECK_GL(ext.glBufferData(GL_ARRAY_BUFFER, sizeof(float) * size, data, GL_STREAM_DRAW));

    CHECK_GL(ext.glBindVertexArray(info->array));

    unsigned int offset = 0;

    for (int i = 0; i < QU_TOTAL_VERTEX_ATTRIBUTES; i++) {
        if (vertex_format_desc[vertex_format].attributes & (1 << i)) {
            GLsizei size = vertex_attribute_desc[i].size;
            GLsizei stride = sizeof(float) * vertex_format_desc[vertex_format].stride;

            CHECK_GL(ext.glVertexAttribPointer(i, size, GL_FLOAT, GL_FALSE, stride, (void *) offset));
            offset += sizeof(float) * size;
        }
    }

    info->buffer_size = size;
}

static void gl3_apply_projection(qu_mat4 const *projection)
{
    qu_mat4_copy(&priv.projection, projection);

    for (int i = 0; i < QU_TOTAL_BRUSHES; i++) {
        priv.programs[i].dirty_uniforms |= (1 << UNIFORM_PROJECTION);
    }

    update_uniforms();
}

static void gl3_apply_transform(qu_mat4 const *transform)
{
    qu_mat4_copy(&priv.modelview, transform);
    
    for (int i = 0; i < QU_TOTAL_BRUSHES; i++) {
        priv.programs[i].dirty_uniforms |= (1 << UNIFORM_MODELVIEW);
    }

    update_uniforms();
}

static void gl3_apply_surface(qu_surface_obj const *surface)
{
    if (priv.bound_surface && priv.bound_surface->sample_count > 1) {
        GLuint fbo = priv.bound_surface->priv[0];
        GLuint ms_fbo = priv.bound_surface->priv[2];

        CHECK_GL(ext.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo));
        CHECK_GL(ext.glBindFramebuffer(GL_READ_FRAMEBUFFER, ms_fbo));

        GLsizei width = priv.bound_surface->texture.width;
        GLsizei height = priv.bound_surface->texture.height;

        CHECK_GL(ext.glBlitFramebuffer(
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
        CHECK_GL(ext.glBindFramebuffer(GL_FRAMEBUFFER, surface->priv[2]));
    } else {
        CHECK_GL(ext.glBindFramebuffer(GL_FRAMEBUFFER, surface->priv[0]));
    }

    CHECK_GL(glViewport(0, 0, width, height));

    priv.bound_surface = surface;
}

static void gl3_apply_texture(qu_texture_obj const *texture)
{
    if (priv.bound_texture == texture) {
        return;
    }

    CHECK_GL(glBindTexture(GL_TEXTURE_2D, texture ? texture->priv[0] : 0));
    priv.bound_texture = texture;
}

static void gl3_apply_clear_color(qu_color color)
{
    GLfloat v[4];

    color_conv(v, color);
    CHECK_GL(glClearColor(v[0], v[1], v[2], v[3]));
}

static void gl3_apply_draw_color(qu_color color)
{
    color_conv(priv.color, color);
    
    for (int i = 0; i < QU_TOTAL_BRUSHES; i++) {
        priv.programs[i].dirty_uniforms |= (1 << UNIFORM_COLOR);
    }

    update_uniforms();
}

static void gl3_apply_brush(qu_brush brush)
{
    if (priv.used_program == brush) {
        return;
    }

    CHECK_GL(ext.glUseProgram(priv.programs[brush].id));
    priv.used_program = brush;

    update_uniforms();
}

static void gl3_apply_vertex_format(qu_vertex_format vertex_format)
{
    struct vertex_format_info *info = &priv.vertex_formats[vertex_format];

    CHECK_GL(ext.glBindVertexArray(info->array));
}

static void gl3_apply_blend_mode(qu_blend_mode mode)
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

static void gl3_exec_resize(int width, int height)
{
    CHECK_GL(glViewport(0, 0, width, height));
}

static void gl3_exec_clear(void)
{
    CHECK_GL(glClear(GL_COLOR_BUFFER_BIT));
}

static void gl3_exec_draw(qu_render_mode render_mode, unsigned int first_vertex, unsigned int total_vertices)
{
    CHECK_GL(glDrawArrays(mode_map[render_mode], (GLint) first_vertex, (GLsizei) total_vertices));
}

static void gl3_load_texture(qu_texture_obj *texture)
{
    GLuint id = texture->priv[0];
    
    if (id == 0) {
        CHECK_GL(glGenTextures(1, &id));
        texture->priv[0] = (uintptr_t) id;
    }

    CHECK_GL(glBindTexture(GL_TEXTURE_2D, id));

    GLenum internal_format = texture_format_map[texture->channels - 1][0];
    GLenum format = texture_format_map[texture->channels - 1][1];

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

    if (texture->smooth) {
        CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    } else {
        CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    }

    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

    GLenum const *swizzle = texture_swizzle_map[texture->channels - 1];

    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, swizzle[0]));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, swizzle[1]));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, swizzle[2]));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, swizzle[3]));

    if (priv.bound_texture) {
        CHECK_GL(glBindTexture(GL_TEXTURE_2D, priv.bound_texture->priv[0]));
    } else {
        CHECK_GL(glBindTexture(GL_TEXTURE_2D, 0));
    }
}

static void gl3_unload_texture(qu_texture_obj *texture)
{
    GLuint id = (GLuint) texture->priv[0];

    CHECK_GL(glDeleteTextures(1, &id));
}

static void gl3_set_texture_smooth(qu_texture_obj *data, bool smooth)
{
    GLuint id = (GLuint) data->priv[0];

    CHECK_GL(glBindTexture(GL_TEXTURE_2D, id));

    if (smooth) {
        CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    } else {
        CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    }

    data->smooth = smooth;

    if (priv.bound_texture) {
        CHECK_GL(glBindTexture(GL_TEXTURE_2D, priv.bound_texture->priv[0]));
    } else {
        CHECK_GL(glBindTexture(GL_TEXTURE_2D, 0));
    }
}

static void gl3_create_surface(qu_surface_obj *surface)
{
    GLsizei width = surface->texture.width;
    GLsizei height = surface->texture.height;

    GLuint fbo;
    GLuint depth;
    GLuint color;

    CHECK_GL(ext.glGenFramebuffers(1, &fbo));
    CHECK_GL(ext.glBindFramebuffer(GL_FRAMEBUFFER, fbo));

    CHECK_GL(ext.glGenRenderbuffers(1, &depth));
    CHECK_GL(ext.glBindRenderbuffer(GL_RENDERBUFFER, depth));
    CHECK_GL(ext.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                                         width, height));

    CHECK_GL(glGenTextures(1, &color));
    CHECK_GL(glBindTexture(GL_TEXTURE_2D, color));
    CHECK_GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height,
                           0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));

    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    CHECK_GL(ext.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth));
    CHECK_GL(ext.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0));

    GLenum status = ext.glCheckFramebufferStatus(GL_FRAMEBUFFER);

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
        CHECK_GL(ext.glBindFramebuffer(GL_FRAMEBUFFER, priv.bound_surface->priv[0]));
    } else {
        CHECK_GL(ext.glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }

    if (priv.bound_texture) {
        CHECK_GL(glBindTexture(GL_TEXTURE_2D, priv.bound_texture->priv[0]));
    } else {
        CHECK_GL(glBindTexture(GL_TEXTURE_2D, 0));
    }
}

static void gl3_destroy_surface(qu_surface_obj *surface)
{
    if (surface->sample_count > 1) {
        surface_remove_multisample_buffer(surface);
    }

    GLuint fbo = surface->priv[0];
    GLuint depth = surface->priv[1];
    GLuint color = surface->texture.priv[0];

    CHECK_GL(ext.glDeleteFramebuffers(1, &fbo));
    CHECK_GL(ext.glDeleteRenderbuffers(1, &depth));
    CHECK_GL(glDeleteTextures(1, &color));
}

static void gl3_set_surface_antialiasing_level(qu_surface_obj *surface, int level)
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

qu_renderer_impl const qu_gl3_renderer_impl = {
    .query = gl3_query,
    .initialize = gl3_initialize,
    .terminate = gl3_terminate,
    .upload_vertex_data = gl3_upload_vertex_data,
    .apply_projection = gl3_apply_projection,
    .apply_transform = gl3_apply_transform,
    .apply_surface = gl3_apply_surface,
    .apply_texture = gl3_apply_texture,
    .apply_clear_color = gl3_apply_clear_color,
    .apply_draw_color = gl3_apply_draw_color,
    .apply_brush = gl3_apply_brush,
    .apply_vertex_format = gl3_apply_vertex_format,
    .apply_blend_mode = gl3_apply_blend_mode,
    .exec_resize = gl3_exec_resize,
    .exec_clear = gl3_exec_clear,
    .exec_draw = gl3_exec_draw,
    .load_texture = gl3_load_texture,
    .unload_texture = gl3_unload_texture,
    .set_texture_smooth = gl3_set_texture_smooth,
    .create_surface = gl3_create_surface,
    .destroy_surface = gl3_destroy_surface,
    .set_surface_antialiasing_level = gl3_set_surface_antialiasing_level,
};
