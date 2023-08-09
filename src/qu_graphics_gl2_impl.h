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

#ifndef QU_GRAPHICS_GL2_IMPL_H
#define QU_GRAPHICS_GL2_IMPL_H

//------------------------------------------------------------------------------

#include "qu.h"

//------------------------------------------------------------------------------
// qu_graphics_gl2_impl.h: GL2/ES2 shared implementation
//------------------------------------------------------------------------------

#define GL2__MAX_MATRICES               (32)

//------------------------------------------------------------------------------

enum
{
    GL2__ATTR_POSITION,
    GL2__ATTR_COLOR,
    GL2__ATTR_TEXCOORD,
    GL2__ATTR_TOTAL,
};

enum
{
    GL2__VF_SOLID,
    GL2__VF_TEXTURED,
    GL2__VF_TOTAL,
};

enum
{
    GL2__SHADER_VERTEX,
    GL2__SHADER_SOLID,
    GL2__SHADER_TEXTURED,
    GL2__SHADER_CANVAS,
    GL2__SHADER_TOTAL,
};

enum
{
    GL2__PROG_SHAPE,
    GL2__PROG_TEXTURE,
    GL2__PROG_CANVAS,
    GL2__PROG_TOTAL,
};

enum
{
    GL2__UNI_PROJ,
    GL2__UNI_MV,
    GL2__UNI_COLOR,
    GL2__UNI_TOTAL,
};

enum
{
    GL2__CMD_NONE,
    GL2__CMD_CLEAR,
    GL2__CMD_DRAW,
    GL2__CMD_SET_SURFACE,
    GL2__CMD_RESET_SURFACE,
    GL2__CMD_SET_VIEW,
    GL2__CMD_RESET_VIEW,
    GL2__CMD_PUSH_MATRIX,
    GL2__CMD_POP_MATRIX,
    GL2__CMD_TRANSLATE,
    GL2__CMD_SCALE,
    GL2__CMD_ROTATE,
    GL2__CMD_RESIZE,
};

typedef struct
{
    GLenum type;
    GLchar *ident;
    GLchar *source;
} gl2__shader_desc;

typedef struct
{
    GLchar *ident;
    int vs;
    int fs;
} gl2__prog_desc;

typedef struct
{
    GLuint handle;
    GLuint width;
    GLuint height;

    GLint channels;
    GLenum format;
} gl2__texture;

typedef struct
{
    GLuint handle;
    GLuint depth;
    int32_t color_id;
    int width;
    int height;
} gl2__surface;

typedef struct
{
    GLuint handle;
    GLint uni_locations[GL2__UNI_TOTAL];
    uint32_t dirty;
} gl2__prog;

typedef struct
{
    int type;

    union
    {
        struct
        {
            qu_color color;
        } clear;

        struct
        {
            qu_color color;
            int32_t texture_id;
            int program;
            int format;
            int mode;
            int first;
            int count;
        } draw;

        struct
        {
            int32_t id;
        } surface;

        struct
        {
            float x;
            float y;
            float w;
            float h;
            float r;
        } view;

        struct
        {
            int w;
            int h;
        } size;
    };
} gl2__cmd;

typedef struct
{
    gl2__cmd *array;
    unsigned int size;
    unsigned int capacity;
} gl2__cmd_buf;

typedef struct
{
    float *array;
    unsigned int size;
    unsigned int capacity;

    GLuint vbo;
    GLuint vbo_size;
} gl2__vertex_buf;

typedef struct
{
    bool use_canvas;

    int display_width;          // current display width
    int display_height;         // current display height
    float display_aspect;       // current display aspect ratio

    int canvas_id;              // canvas surface id
    int canvas_width;           // current canvas width
    int canvas_height;          // current canvas height
    float canvas_aspect;        // current canvas aspect ratio

    float canvas_ax;            // calculated canvas left-most point
    float canvas_ay;            // calculated canvas top-most point
    float canvas_bx;            // calculated canvas right-most point
    float canvas_by;            // calculated canvas bottom-most point

    int texture_id;             // currently bound texture
    int surface_id;             // currently active framebuffer
    int program;                // currently used program
    int vertex_format;          // current vertex format
    qu_color clear_color;       // current clear color
    qu_color draw_color;        // current draw color
    float draw_color_f[4];

    qu_mat4 projection;
    qu_mat4 matrix[GL2__MAX_MATRICES];
    int current_matrix;
} gl2__state;

//------------------------------------------------------------------------------

static char const *s_attr_names[GL2__ATTR_TOTAL] = {
    "a_position", "a_color", "a_texCoord",
};

static int s_attr_sizes[GL2__ATTR_TOTAL] = { 2, 4, 2 };

static int s_vf_masks[GL2__VF_TOTAL] = { 0x01, 0x05 };

static gl2__shader_desc s_shaders[GL2__SHADER_TOTAL] = {
    { GL_VERTEX_SHADER, "SHADER_VERTEX", GL2_SHADER_VERTEX_SRC },
    { GL_FRAGMENT_SHADER, "SHADER_SOLID", GL2_SHADER_SOLID_SRC },
    { GL_FRAGMENT_SHADER, "SHADER_TEXTURED", GL2_SHADER_TEXTURED_SRC },
    { GL_VERTEX_SHADER, "SHADER_CANVAS", GL2_SHADER_CANVAS_SRC },
};

static gl2__prog_desc s_progs[GL2__PROG_TOTAL] = {
    { "PROGRAM_SHAPE", GL2__SHADER_VERTEX, GL2__SHADER_SOLID },
    { "PROGRAM_TEXTURE", GL2__SHADER_VERTEX, GL2__SHADER_TEXTURED },
    { "PROGRAM_CANVAS", GL2__SHADER_CANVAS, GL2__SHADER_TEXTURED },
};

static char const *s_uniform_names[GL2__UNI_TOTAL] = {
    "u_projection", "u_modelView", "u_color",
};

//------------------------------------------------------------------------------

static gl2__state           g_state;
static gl2__cmd_buf         g_cmd_buf;
static gl2__vertex_buf      g_vertex_bufs[GL2__VF_TOTAL];
static qu_array             *g_textures;
static qu_array             *g_surfaces;
static gl2__prog            g_progs[GL2__PROG_TOTAL];

//------------------------------------------------------------------------------

static void gl2__unpack_color(qu_color c, float *a)
{
    a[0] = ((c >> 16) & 255) / 255.f;
    a[1] = ((c >> 8) & 255) / 255.f;
    a[2] = ((c >> 0) & 255) / 255.f;
    a[3] = ((c >> 24) & 255) / 255.f;
}

static GLenum gl2__get_texture_format(int channels)
{
    switch (channels) {
    case 1:
        return GL_LUMINANCE;
    case 2:
        return GL_LUMINANCE_ALPHA;
    case 3:
        return GL_RGB;
    case 4:
        return GL_RGBA;
    default:
        return GL_INVALID_ENUM;
    }
}

static void gl2__texture_dtor(void *data)
{
    gl2__texture *texture = data;
    glDeleteTextures(1, &texture->handle);
}

static void gl2__surface_dtor(void *data)
{
    gl2__surface *surface = data;

    qu_array_remove(g_textures, surface->color_id);

    glDeleteFramebuffers(1, &surface->handle);
    glDeleteRenderbuffers(1, &surface->depth);
}

static GLuint gl2__load_shader(gl2__shader_desc *desc)
{
    GLuint shader = glCreateShader(desc->type);
    glShaderSource(shader, 1, (GLchar const *const *) &desc->source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char buffer[256];
        glGetShaderInfoLog(shader, 256, NULL, buffer);
        glDeleteShader(shader);

        QU_ERROR("Failed to compile GLSL shader %s. Reason:\n%s\n",
                    desc->ident, buffer);

        return 0;
    }

    QU_INFO("Shader %s is compiled successfully.\n", desc->ident);

    return shader;
}

static GLuint gl2__build_program(char const *ident, GLuint vs, GLuint fs)
{
    GLuint program = glCreateProgram();

    glAttachShader(program, vs);
    glAttachShader(program, fs);

    for (int j = 0; j < GL2__ATTR_TOTAL; j++) {
        glBindAttribLocation(program, j, s_attr_names[j]);
    }

    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        char buffer[256];
        glGetProgramInfoLog(program, 256, NULL, buffer);
        glDeleteProgram(program);

        QU_ERROR("Failed to link GLSL program %s: %s\n", ident, buffer);

        return 0;
    }

    QU_INFO("Shader program %s is compiled successfully.\n", ident);

    return program;
}

//------------------------------------------------------------------------------

static void gl2__upd_canvas_coords(int w_display, int h_display)
{
    struct surface *canvas = qu_array_get(g_surfaces, g_state.canvas_id);

    float ard = g_state.display_aspect;
    float arc = g_state.canvas_aspect;

    if (ard > arc) {
        g_state.canvas_ax = (w_display / 2.f) - ((arc / ard) * w_display / 2.f);
        g_state.canvas_ay = 0.f;
        g_state.canvas_bx = (w_display / 2.f) + ((arc / ard) * w_display / 2.f);
        g_state.canvas_by = h_display;
    } else {
        g_state.canvas_ax = 0.f;
        g_state.canvas_ay = (h_display / 2.f) - ((ard / arc) * h_display / 2.f);
        g_state.canvas_bx = w_display;
        g_state.canvas_by = (h_display / 2.f) + ((ard / arc) * h_display / 2.f);
    }
}

//------------------------------------------------------------------------------

static void gl2__upload_uniform(int uniform, GLuint location)
{
    switch (uniform) {
    case GL2__UNI_PROJ:
        glUniformMatrix4fv(location, 1, GL_FALSE, g_state.projection.m);
        break;
    case GL2__UNI_MV:
        glUniformMatrix4fv(location, 1, GL_FALSE, g_state.matrix[g_state.current_matrix].m);
        break;
    case GL2__UNI_COLOR:
        glUniform4fv(location, 1, g_state.draw_color_f);
        break;
    default:
        break;
    }
}

static void gl2__upd_projection(float x, float y, float w, float h, float rot)
{
    float l = x - (w / 2.f);
    float r = x + (w / 2.f);
    float b = y + (h / 2.f);
    float t = y - (h / 2.f);

    qu_mat4_ortho(&g_state.projection, l, r, b, t);

    if (rot != 0.f) {
        qu_mat4_translate(&g_state.projection, x, y, 0.f);
        qu_mat4_rotate(&g_state.projection, QU_DEG2RAD(rot), 0.f, 0.f, 1.f);
        qu_mat4_translate(&g_state.projection, -x, -y, 0.f);
    }

    for (int i = 0; i < GL2__PROG_TOTAL; i++) {
        g_progs[i].dirty |= (1 << GL2__UNI_PROJ);
    }
}

static void gl2__upd_model_view(void)
{
    for (int i = 0; i < GL2__PROG_TOTAL; i++) {
        g_progs[i].dirty |= (1 << GL2__UNI_MV);
    }
}

static void gl2__upd_draw_color(qu_color color)
{
    if (g_state.draw_color == color) {
        return;
    }

    gl2__unpack_color(color, g_state.draw_color_f);

    for (int i = 0; i < GL2__PROG_TOTAL; i++) {
        g_progs[i].dirty |= (1 << GL2__UNI_COLOR);
    }

    g_state.draw_color = color;
}

//------------------------------------------------------------------------------

static void gl2__upd_clear_color(qu_color color)
{
    if (g_state.clear_color == color) {
        return;
    }

    float c[4];

    gl2__unpack_color(color, c);
    glClearColor(c[0], c[1], c[2], c[3]);

    g_state.clear_color = color;
}

static void gl2__upd_program(int program)
{
    if (g_state.program == program && !g_progs[program].dirty) {
        return;
    }

    glUseProgram(g_progs[program].handle);

    if (g_progs[program].dirty) {
        for (int i = 0; i < GL2__UNI_TOTAL; i++) {
            if (g_progs[program].dirty & (1 << i)) {
                gl2__upload_uniform(i, g_progs[program].uni_locations[i]);
            }
        }
    }

    g_progs[program].dirty = 0;
    g_state.program = program;
}

static void gl2__upd_vertex_format(int format)
{
    if (g_state.vertex_format == format) {
        return;
    }

    int mask = s_vf_masks[format];

    glBindBuffer(GL_ARRAY_BUFFER, g_vertex_bufs[format].vbo);

    GLsizei stride = 0;

    for (int i = 0; i < GL2__ATTR_TOTAL; i++) {
        if (mask & (1 << i)) {
            glEnableVertexAttribArray(i);
            stride += sizeof(GLfloat) * s_attr_sizes[i];
        } else {
            glDisableVertexAttribArray(i);
        }
    }

    GLsizei offset = 0;

    for (int i = 0; i < GL2__ATTR_TOTAL; i++) {
        if (mask & (1 << i)) {
            glVertexAttribPointer(i, s_attr_sizes[i], GL_FLOAT,
                                  GL_FALSE, stride, (void *) offset);
            offset += sizeof(GLfloat) * s_attr_sizes[i];
        }
    }

    g_state.vertex_format = format;
}

static void gl2__upd_texture(int32_t id)
{
    if (g_state.texture_id == id) {
        return;
    }

    GLuint handle;

    if (id == 0) {
        handle = 0;
    } else {
        gl2__texture *texture = qu_array_get(g_textures, id);

        if (!texture) {
            return;
        }

        handle = texture->handle;
    }

    glBindTexture(GL_TEXTURE_2D, handle);
    g_state.texture_id = id;
}

static void gl2__upd_surface(int32_t id)
{
    if (g_state.surface_id == id) {
        return;
    }

    GLuint handle;
    GLsizei width, height;

    if (id == 0) {
        handle = 0;
        width = g_state.display_width;
        height = g_state.display_height;
    } else {
        gl2__surface *surface = qu_array_get(g_surfaces, id);

        if (!surface) {
            return;
        }

        handle = surface->handle;
        width = surface->width;
        height = surface->height;
    }

    // Restore view
    gl2__upd_projection(width / 2.f, height / 2.f, width, height, 0.f);

    // Restore transformation stack
    g_state.current_matrix = 0;
    qu_mat4_identity(&g_state.matrix[0]);
    gl2__upd_model_view();

    // Bind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, handle);
    glViewport(0, 0, width, height);

    g_state.surface_id = id;
}

//------------------------------------------------------------------------------
// Commands

static void gl2__exec_clear(qu_color color)
{
    gl2__upd_clear_color(color);

    glClear(GL_COLOR_BUFFER_BIT);
}

static void gl2__exec_draw(qu_color color, int32_t texture, int program, int format,
                      GLenum mode, GLint first, GLsizei count)
{
    gl2__upd_draw_color(color);
    gl2__upd_texture(texture);
    gl2__upd_program(program);
    gl2__upd_vertex_format(format);

    glDrawArrays(mode, first, count);
}

static void gl2__exec_set_surface(int32_t id)
{
    gl2__upd_surface(id);
}

static void gl2__exec_reset_surface(void)
{
    if (g_state.use_canvas) {
        gl2__upd_surface(g_state.canvas_id);
    } else {
        gl2__upd_surface(0);
    }
}

static void gl2__exec_set_view(float x, float y, float w, float h, float rotation)
{
    gl2__upd_projection(x, y, w, h, rotation);
}

static void gl2__exec_reset_view(void)
{
    GLsizei width, height;

    if (g_state.surface_id == 0) {
        width = g_state.display_width;
        height = g_state.display_height;
    } else {
        gl2__surface *surface = qu_array_get(g_surfaces, g_state.surface_id);

        if (!surface) {
            return;
        }

        width = surface->width;
        height = surface->height;
    }

    gl2__upd_projection(width / 2.f, height / 2.f, width, height, 0.f);
}

static void gl2__exec_push_matrix(void)
{
    if (g_state.current_matrix == (GL2__MAX_MATRICES - 1)) {
        QU_WARNING("Can't qu_push_matrix(): limit of %d matrices reached.\n", GL2__MAX_MATRICES);
        return;
    }

    qu_mat4_copy(&g_state.matrix[g_state.current_matrix + 1],
                 &g_state.matrix[g_state.current_matrix]);

    g_state.current_matrix++;
}

static void gl2__exec_pop_matrix(void)
{
    if (g_state.current_matrix == 0) {
        QU_WARNING("Can't qu_pop_matrix(): already at the first matrix.\n");
        return;
    }

    g_state.current_matrix--;
}

static void gl2__exec_translate(float x, float y)
{
    qu_mat4_translate(&g_state.matrix[g_state.current_matrix], x, y, 0.f);
    gl2__upd_model_view();
}

static void gl2__exec_scale(float x, float y)
{
    qu_mat4_scale(&g_state.matrix[g_state.current_matrix], x, y, 1.f);
    gl2__upd_model_view();
}

static void gl2__exec_rotate(float degrees)
{
    qu_mat4_rotate(&g_state.matrix[g_state.current_matrix],
                   QU_DEG2RAD(degrees), 0.f, 0.f, 1.f);
    gl2__upd_model_view();
}

static void gl2__exec_resize(int width, int height)
{
    g_state.display_width = width;
    g_state.display_height = height;
    g_state.display_aspect = width / (float) height;

    if (g_state.use_canvas) {
        gl2__upd_canvas_coords(width, height);
    }

    if (g_state.surface_id == 0) {
        glViewport(0, 0, width, height);
        gl2__upd_projection(width / 2.f, height / 2.f, width, height, 0.f);
    }
}

//------------------------------------------------------------------------------
// Command buffer

static void gl2__append_command(gl2__cmd const *command)
{
    gl2__cmd_buf *buffer = &g_cmd_buf;

    if (buffer->size == buffer->capacity) {
        unsigned int next_capacity = buffer->capacity * 2;

        if (!next_capacity) {
            next_capacity = 256;
        }

        gl2__cmd *next_array =
            realloc(buffer->array, sizeof(gl2__cmd) * next_capacity);

        if (!next_array) {
            return;
        }

        buffer->array = next_array;
        buffer->capacity = next_capacity;
    }

    memcpy(&buffer->array[buffer->size++], command, sizeof(gl2__cmd));
}

static void gl2__execute_command(gl2__cmd *command)
{
    switch (command->type) {
    case GL2__CMD_CLEAR:
        gl2__exec_clear(command->clear.color);
        break;
    case GL2__CMD_DRAW:
        gl2__exec_draw(command->draw.color, command->draw.texture_id,
                  command->draw.program, command->draw.format,
                  command->draw.mode, command->draw.first, command->draw.count);
        break;
    case GL2__CMD_SET_SURFACE:
        gl2__exec_set_surface(command->surface.id);
        break;
    case GL2__CMD_RESET_SURFACE:
        gl2__exec_reset_surface();
        break;
    case GL2__CMD_SET_VIEW:
        gl2__exec_set_view(command->view.x, command->view.y,
                      command->view.w, command->view.h,
                      command->view.r);
        break;
    case GL2__CMD_RESET_VIEW:
        gl2__exec_reset_view();
        break;
    case GL2__CMD_PUSH_MATRIX:
        gl2__exec_push_matrix();
        break;
    case GL2__CMD_POP_MATRIX:
        gl2__exec_pop_matrix();
        break;
    case GL2__CMD_TRANSLATE:
        gl2__exec_translate(command->view.x, command->view.y);
        break;
    case GL2__CMD_SCALE:
        gl2__exec_scale(command->view.x, command->view.y);
        break;
    case GL2__CMD_ROTATE:
        gl2__exec_rotate(command->view.r);
        break;
    case GL2__CMD_RESIZE:
        gl2__exec_resize(command->size.w, command->size.h);
        break;
    default:
        break;
    }
}

//------------------------------------------------------------------------------
// Vertex buffer

static int gl2__append_vertex_data(int format, float const *data, int size)
{
    gl2__vertex_buf *buffer = &g_vertex_bufs[format];

    unsigned int required = buffer->size + size;

    if (required > buffer->capacity) {
        unsigned int next_capacity = buffer->capacity;

        if (next_capacity == 0) {
            next_capacity = 256;
        }

        while (next_capacity < required) {
            next_capacity *= 2;
        }

        float *next_array = realloc(buffer->array, sizeof(float) * next_capacity);

        if (!next_array) {
            return 0;
        }

        buffer->array = next_array;
        buffer->capacity = next_capacity;
    }

    memcpy(buffer->array + buffer->size, data, sizeof(float) * size);

    int offset = buffer->size;
    buffer->size += size;

    return offset;
}

//------------------------------------------------------------------------------
// Views

static void gl2_set_view(float x, float y, float w, float h, float rotation)
{
    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_SET_VIEW,
        .view = {
            .x = x,
            .y = y,
            .w = w,
            .h = h,
            .r = rotation,
        },
    });
}

static void gl2_reset_view(void)
{
    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_RESET_VIEW,
    });
}

//------------------------------------------------------------------------------
// Transformation

static void gl2_push_matrix(void)
{
    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_PUSH_MATRIX,
    });
}

static void gl2_pop_matrix(void)
{
    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_POP_MATRIX,
    });
}

static void gl2_translate(float x, float y)
{
    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_TRANSLATE,
        .view.x = x,
        .view.y = y,
    });
}

static void gl2_scale(float x, float y)
{
    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_SCALE,
        .view.x = x,
        .view.y = y,
    });
}

static void gl2_rotate(float degrees)
{
    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_ROTATE,
        .view.r = degrees,
    });
}

//------------------------------------------------------------------------------
// Primitives

static void gl2_clear(qu_color color)
{
    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_CLEAR,
        .clear = {
            .color = color,
        },
    });
}

static void gl2_draw_point(float x, float y, qu_color color)
{
    float vertices[] = {
        x, y,
    };

    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_DRAW,
        .draw = {
            .color = color,
            .program = GL2__PROG_SHAPE,
            .format = GL2__VF_SOLID,
            .mode = GL_POINTS,
            .first = gl2__append_vertex_data(GL2__VF_SOLID, vertices, 2) / 2,
            .count = 2,
        },
    });
}

static void gl2_draw_line(float ax, float ay, float bx, float by, qu_color color)
{
    float vertices[] = {
        ax, ay,
        bx, by,
    };

    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_DRAW,
        .draw = {
            .color = color,
            .program = GL2__PROG_SHAPE,
            .format = GL2__VF_SOLID,
            .mode = GL_LINES,
            .first = gl2__append_vertex_data(GL2__VF_SOLID, vertices, 4) / 2,
            .count = 2,
        },
    });
}

static void gl2_draw_triangle(float ax, float ay, float bx, float by,
                              float cx, float cy, qu_color outline, qu_color fill)
{
    int fill_alpha = (fill >> 24) & 255;
    int outline_alpha = (outline >> 24) & 255;

    float vertices[] = {
        ax, ay,
        bx, by,
        cx, cy,
    };

    int first = gl2__append_vertex_data(GL2__VF_SOLID, vertices, 6) / 2;

    if (fill_alpha > 0) {
        gl2__append_command(&(gl2__cmd) {
            .type = GL2__CMD_DRAW,
            .draw = {
                .color = fill,
                .program = GL2__PROG_SHAPE,
                .format = GL2__VF_SOLID,
                .mode = GL_TRIANGLE_FAN,
                .first = first,
                .count = 3,
            },
        });
    }

    if (outline_alpha > 0) {
        gl2__append_command(&(gl2__cmd) {
            .type = GL2__CMD_DRAW,
            .draw = {
                .color = outline,
                .program = GL2__PROG_SHAPE,
                .format = GL2__VF_SOLID,
                .mode = GL_LINE_LOOP,
                .first = first,
                .count = 3,
            },
        });
    }
}

static void gl2_draw_rectangle(float x, float y, float w, float h,
                               qu_color outline, qu_color fill)
{
    int fill_alpha = (fill >> 24) & 255;
    int outline_alpha = (outline >> 24) & 255;

    float vertices[] = {
        x, y,
        x + w, y,
        x + w, y + h,
        x, y + h,
    };

    int first = gl2__append_vertex_data(GL2__VF_SOLID, vertices, 8) / 2;

    if (fill_alpha > 0) {
        gl2__append_command(&(gl2__cmd) {
            .type = GL2__CMD_DRAW,
            .draw = {
                .color = fill,
                .program = GL2__PROG_SHAPE,
                .format = GL2__VF_SOLID,
                .mode = GL_TRIANGLE_FAN,
                .first = first,
                .count = 4,
            },
        });
    }

    if (outline_alpha > 0) {
        gl2__append_command(&(gl2__cmd) {
            .type = GL2__CMD_DRAW,
            .draw = {
                .color = outline,
                .program = GL2__PROG_SHAPE,
                .format = GL2__VF_SOLID,
                .mode = GL_LINE_LOOP,
                .first = first,
                .count = 4,
            },
        });
    }
}

static void gl2_draw_circle(float x, float y, float radius, qu_color outline, qu_color fill)
{
    int fill_alpha = (fill >> 24) & 255;
    int outline_alpha = (outline >> 24) & 255;

    float vertices[64];
    qu_make_circle(x, y, radius, vertices, 32);

    int first = gl2__append_vertex_data(GL2__VF_SOLID, vertices, 64) / 2;

    if (fill_alpha > 0) {
        gl2__append_command(&(gl2__cmd) {
            .type = GL2__CMD_DRAW,
            .draw = {
                .color = fill,
                .program = GL2__PROG_SHAPE,
                .format = GL2__VF_SOLID,
                .mode = GL_TRIANGLE_FAN,
                .first = first,
                .count = 32,
            },
        });
    }

    if (outline_alpha > 0) {
        gl2__append_command(&(gl2__cmd) {
            .type = GL2__CMD_DRAW,
            .draw = {
                .color = outline,
                .program = GL2__PROG_SHAPE,
                .format = GL2__VF_SOLID,
                .mode = GL_LINE_LOOP,
                .first = first,
                .count = 32,
            },
        });
    }
}

//------------------------------------------------------------------------------
// Textures

static int32_t gl2_create_texture(int width, int height, int channels)
{
    if ((width < 0) || (height < 0)) {
        return 0;
    }

    if ((channels < 1) || (channels > 4)) {
        return 0;
    }

    GLenum format = gl2__get_texture_format(channels);

    if (format == GL_INVALID_ENUM) {
        return 0;
    }

    gl2__texture texture = {0};

    glGenTextures(1, &texture.handle);

    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height,
                 0, format, GL_UNSIGNED_BYTE, NULL);

    texture.width = width;
    texture.height = height;
    texture.channels = channels;
    texture.format = format;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    g_state.texture_id = qu_array_add(g_textures, &texture);

    if (g_state.texture_id > 0) {
        QU_INFO("Created texture 0x%08x.\n", g_state.texture_id);
    }

    return g_state.texture_id;
}

static void gl2_update_texture(int32_t texture_id, int x, int y, int w, int h,
                               uint8_t const *pixels)
{
    gl2__texture *texture = qu_array_get(g_textures, texture_id);

    if (!texture) {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, texture->handle);

    if (x == 0 && y == 0 && w == -1 && h == -1) {
        glTexImage2D(GL_TEXTURE_2D, 0, texture->format,
                     texture->width, texture->height, 0,
                     texture->format, GL_UNSIGNED_BYTE, pixels);
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h,
                        texture->format, GL_UNSIGNED_BYTE, pixels);
    }

    g_state.texture_id = texture_id;
}

static int32_t gl2_load_texture(qu_file *file)
{
    qu_image *image = qu_load_image(file);
    qu_fclose(file);

    if (!image) {
        return 0;
    }

    GLenum format = gl2__get_texture_format(image->channels);

    if (format == GL_INVALID_ENUM) {
        qu_delete_image(image);
        return 0;
    }

    gl2__texture texture = {0};

    glGenTextures(1, &texture.handle);

    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glTexImage2D(GL_TEXTURE_2D, 0, format, image->width, image->height,
                 0, format, GL_UNSIGNED_BYTE, image->pixels);

    texture.width = image->width;
    texture.height = image->height;
    texture.channels = image->channels;
    texture.format = format;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

#ifdef __EMSCRIPTEN__
    // I don't know what's going on, but without these parameters
    // textures are rendered as black in WebGL

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif

    g_state.texture_id = qu_array_add(g_textures, &texture);

    if (g_state.texture_id > 0) {
        QU_INFO("Loaded texture 0x%08x.\n", g_state.texture_id);
    }

    qu_delete_image(image);

    return g_state.texture_id;
}

static void gl2_delete_texture(int32_t texture_id)
{
    gl2__texture *texture = qu_array_get(g_textures, texture_id);

    if (!texture) {
        return;
    }

    qu_array_remove(g_textures, texture_id);
}

static void gl2_set_texture_smooth(int32_t texture_id, bool smooth)
{
    gl2__texture *texture = qu_array_get(g_textures, texture_id);

    if (!texture) {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, texture->handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);

    g_state.texture_id = texture_id;
}

static void gl2_draw_texture(int32_t texture_id, float x, float y, float w, float h)
{
    float vertices[] = {
        x,      y,      0.f,    0.f,
        x + w,  y,      1.f,    0.f,
        x + w,  y + h,  1.f,    1.f,
        x,      y + h,  0.f,    1.f,
    };

    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_DRAW,
        .draw = {
            .color = 0xffffffff,
            .texture_id = texture_id,
            .program = GL2__PROG_TEXTURE,
            .format = GL2__VF_TEXTURED,
            .mode = GL_TRIANGLE_FAN,
            .first = gl2__append_vertex_data(GL2__VF_TEXTURED, vertices, 16) / 4,
            .count = 4,
        },
    });
}

static void gl2_draw_subtexture(int32_t texture_id, float x, float y, float w,
                                float h, float rx, float ry, float rw, float rh)
{
    gl2__texture *texture = qu_array_get(g_textures, texture_id);

    if (!texture) {
        return;
    }

    float s = rx / texture->width;
    float t = ry / texture->height;
    float u = rw / texture->width;
    float v = rh / texture->height;

    float vertices[] = {
        x,      y,      s,      t,
        x + w,  y,      s + u,  t,
        x + w,  y + h,  s + u,  t + v,
        x,      y + h,  s,      t + v,
    };

    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_DRAW,
        .draw = {
            .color = 0xffffffff,
            .texture_id = texture_id,
            .program = GL2__PROG_TEXTURE,
            .format = GL2__VF_TEXTURED,
            .mode = GL_TRIANGLE_FAN,
            .first = gl2__append_vertex_data(GL2__VF_TEXTURED, vertices, 16) / 4,
            .count = 4,
        },
    });
}

//------------------------------------------------------------------------------
// Fonts

static void gl2_draw_text(int32_t texture_id, qu_color color, float const *data, int count)
{
    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_DRAW,
        .draw = {
            .color = color,
            .texture_id = texture_id,
            .program = GL2__PROG_TEXTURE,
            .format = GL2__VF_TEXTURED,
            .mode = GL_TRIANGLES,
            .first = gl2__append_vertex_data(GL2__VF_TEXTURED, data, count * 4) / 4,
            .count = count,
        },
    });
}

//------------------------------------------------------------------------------
// Surfaces

static int32_t gl2_create_surface(int width, int height)
{
    if (width < 0 || height < 0) {
        return 0;
    }

    gl2__surface surface = {0};

    glGenFramebuffers(1, &surface.handle);
    glBindFramebuffer(GL_FRAMEBUFFER, surface.handle);

    glGenRenderbuffers(1, &surface.depth);
    glBindRenderbuffer(GL_RENDERBUFFER, surface.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, surface.depth);

    surface.color_id = gl2_create_texture(width, height, 4);

    if (surface.color_id == 0) {
        glDeleteFramebuffers(1, &surface.handle);
        return 0;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl2__texture *texture = qu_array_get(g_textures, surface.color_id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texture->handle, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        QU_ERROR("Failed to create OpenGL framebuffer.\n");
        return 0;
    }

    surface.width = width;
    surface.height = height;

    g_state.surface_id = qu_array_add(g_surfaces, &surface);

    if (g_state.surface_id == 0) {
        QU_ERROR("Failed to create surface: insufficient memory.\n");
        return 0;
    }

    return g_state.surface_id;
}

static void gl2_delete_surface(int32_t id)
{
    qu_array_remove(g_surfaces, id);
}

static void gl2_set_surface(int32_t id)
{
    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_SET_SURFACE,
        .surface.id = id,
    });
}

static void gl2_reset_surface(void)
{
    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_RESET_SURFACE,
    });
}

static void gl2_draw_surface(int32_t id, float x, float y, float w, float h)
{
    gl2__surface *surface = qu_array_get(g_surfaces, id);

    if (!surface) {
        return;
    }

    float vertices[] = {
        x,      y,      0.f,    1.f,
        x + w,  y,      1.f,    1.f,
        x + w,  y + h,  1.f,    0.f,
        x,      y + h,  0.f,    0.f,
    };

    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_DRAW,
        .draw = {
            .color = 0xffffffff,
            .texture_id = surface->color_id,
            .program = GL2__PROG_TEXTURE,
            .format = GL2__VF_TEXTURED,
            .mode = GL_TRIANGLE_FAN,
            .first = gl2__append_vertex_data(GL2__VF_TEXTURED, vertices, 16) / 4,
            .count = 4,
        },
    });
}

//------------------------------------------------------------------------------

static void gl2_initialize(qu_params const *params)
{
    g_textures = qu_create_array(sizeof(gl2__texture), gl2__texture_dtor);
    g_surfaces = qu_create_array(sizeof(gl2__surface), gl2__surface_dtor);

    if (!g_textures || !g_surfaces) {
        QU_HALT("Failed to initialize OpenGL");
    }

    GLuint shaders[GL2__SHADER_TOTAL];

    for (int i = 0; i < GL2__SHADER_TOTAL; i++) {
        shaders[i] = gl2__load_shader(&s_shaders[i]);

        if (!shaders[i]) {
            QU_HALT("Failed to initialize OpenGL");
        }
    }

    for (int i = 0; i < GL2__PROG_TOTAL; i++) {
        g_progs[i].handle =
            gl2__build_program(s_progs[i].ident, shaders[s_progs[i].vs],
                          shaders[s_progs[i].fs]);

        for (int j = 0; j < GL2__UNI_TOTAL; j++) {
            g_progs[i].uni_locations[j] =
                glGetUniformLocation(g_progs[i].handle, s_uniform_names[j]);
        }

        g_progs[i].dirty = (uint32_t) -1;
    }

    for (int i = 0; i < GL2__SHADER_TOTAL; i++) {
        glDeleteShader(shaders[i]);
    }

    for (int i = 0; i < GL2__VF_TOTAL; i++) {
        glGenBuffers(1, &g_vertex_bufs[i].vbo);
    }

    g_state.use_canvas = params->enable_canvas;

    g_state.display_width = params->display_width;
    g_state.display_height = params->display_height;
    g_state.display_aspect = params->display_width / (float) params->display_height;

    if (g_state.use_canvas) {
        g_state.canvas_width = params->canvas_width;
        g_state.canvas_height = params->canvas_height;
        g_state.canvas_aspect = params->canvas_width / (float) params->canvas_height;

        g_state.canvas_id = gl2_create_surface(g_state.canvas_width, g_state.canvas_height);

        if (!g_state.canvas_id) {
            QU_HALT("Failed to initialize default framebuffer.\n");
        }

        gl2__upd_canvas_coords(g_state.display_width, g_state.display_height);
    }

    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_RESET_SURFACE,
    });

    g_state.texture_id = -1;
    g_state.surface_id = -1;
    g_state.program = -1;
    g_state.vertex_format = -1;
    g_state.clear_color = 0;
    g_state.draw_color = 0;

    qu_mat4_ortho(&g_state.projection,
                  0.f, params->display_width,
                  params->display_height, 0.f);

    g_state.current_matrix = 0;
    qu_mat4_identity(&g_state.matrix[0]);

    glClearColor(0.f, 0.f, 0.f, 0.f);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    QU_INFO("OpenGL graphics module initialized.\n");
    QU_INFO("OpenGL vendor: %s\n", glGetString(GL_VENDOR));
    QU_INFO("OpenGL version: %s\n", glGetString(GL_VERSION));
    QU_INFO("OpenGL renderer: %s\n", glGetString(GL_RENDERER));
}

static void gl2_terminate(void)
{
    qu_destroy_array(g_surfaces);
    qu_destroy_array(g_textures);
    free(g_cmd_buf.array);

    for (int i = 0; i < GL2__PROG_TOTAL; i++) {
        glDeleteProgram(g_progs[i].handle);
    }

    QU_INFO("OpenGL graphics module terminated.\n");
}

static void gl2_refresh(void)
{
    // TODO: restore context?

    QU_HALT("OpenGL context lost.\n");
}

static void gl2_swap(void)
{
    // If using canvas, then draw it in the default framebuffer
    if (g_state.use_canvas) {
        gl2__append_command(&(gl2__cmd) {
            .type = GL2__CMD_SET_SURFACE,
            .surface.id = 0,
        });

        gl2__append_command(&(gl2__cmd) {
            .type = GL2__CMD_CLEAR,
            .clear.color = 0xff000000,
        });

        gl2__surface *canvas = qu_array_get(g_surfaces, g_state.canvas_id);

        float vertices[] = {
            g_state.canvas_ax, g_state.canvas_ay, 0.f, 1.f,
            g_state.canvas_bx, g_state.canvas_ay, 1.f, 1.f,
            g_state.canvas_bx, g_state.canvas_by, 1.f, 0.f,
            g_state.canvas_ax, g_state.canvas_by, 0.f, 0.f,
        };

        gl2__append_command(&(gl2__cmd) {
            .type = GL2__CMD_DRAW,
            .draw = {
                .color = 0xffffffff,
                .texture_id = canvas->color_id,
                .program = GL2__PROG_TEXTURE,
                .format = GL2__VF_TEXTURED,
                .mode = GL_TRIANGLE_FAN,
                .first = gl2__append_vertex_data(GL2__VF_TEXTURED, vertices, 16) / 4,
                .count = 4,
            },
        });
    }

    // Upload vertex data to the GPU...
    for (int i = 0; i < GL2__VF_TOTAL; i++) {
        gl2__vertex_buf *buffer = &g_vertex_bufs[i];

        if (buffer->size == 0) {
            continue;
        }

        glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);

        if (buffer->size > buffer->vbo_size) {
            glBufferData(GL_ARRAY_BUFFER, buffer->size * sizeof(float),
                         buffer->array, GL_STREAM_DRAW);
        } else {
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                            buffer->size * sizeof(float),
                            buffer->array);
        }

        buffer->size = 0;
    }

    // Force VBO pointer update
    g_state.vertex_format = -1;

    // Just in case
    glFlush();

    // Execute all pending rendering commands...
    for (unsigned int i = 0; i < g_cmd_buf.size; i++) {
        gl2__execute_command(&g_cmd_buf.array[i]);
    }

    // Reset size of the command buffer to 0
    g_cmd_buf.size = 0;

    // Restore transformation stack
    g_state.current_matrix = 0;
    qu_mat4_identity(&g_state.matrix[0]);
    gl2__upd_model_view();

    // Restore surface
    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_RESET_SURFACE,
        .surface.id = g_state.canvas_id,
    });
}

static void gl2_on_display_resize(int width, int height)
{
    gl2__append_command(&(gl2__cmd) {
        .type = GL2__CMD_RESIZE,
        .size = { width, height },
    });
}

static qu_vec2i gl2_conv_cursor(qu_vec2i position)
{
    if (!g_state.use_canvas) {
        return position;
    }

    float dar = g_state.display_aspect;
    float car = g_state.canvas_aspect;
    float dw = g_state.display_width;
    float dh = g_state.display_height;
    float cw = g_state.canvas_width;
    float ch = g_state.canvas_height;

    if (dar > car) {
        float x_scale = dh / ch;
        float x_offset = (dw - (cw * x_scale)) / (x_scale * 2.0f);

        return (qu_vec2i) {
            .x = (position.x * ch) / dh - x_offset,
            .y = (position.y / dh) * ch,
        };
    } else {
        float y_scale = dw / cw;
        float y_offset = (dh - (ch * y_scale)) / (y_scale * 2.0f);

        return (qu_vec2i) {
            .x = (position.x / dw) * cw,
            .y = (position.y * cw) / dw - y_offset,
        };
    }
}

static qu_vec2i gl2_conv_cursor_delta(qu_vec2i delta)
{
    if (!g_state.use_canvas) {
        return delta;
    }

    float dar = g_state.display_aspect;
    float car = g_state.canvas_aspect;
    float dw = g_state.display_width;
    float dh = g_state.display_height;
    float cw = g_state.canvas_width;
    float ch = g_state.canvas_height;

    if (g_state.display_aspect > g_state.canvas_aspect) {
        return (qu_vec2i) {
            .x = (delta.x * ch) / dh,
            .y = (delta.y / dh) * ch,
        };
    } else {
        return (qu_vec2i) {
            .x = (delta.x / dw) * cw,
            .y = (delta.y * cw) / dw,
        };
    }
}

//------------------------------------------------------------------------------

#endif // QU_GRAPHICS_GL2_IMPL_H
