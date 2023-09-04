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

static struct qu__renderer_impl const *supported_renderer_impl_list[] = {

#ifdef QU_USE_GL
    &qu__renderer_gl3,
    &qu__renderer_gl1,
#endif

#ifdef QU_USE_ES2
    &qu__renderer_es2,
#endif

    &qu__renderer_null,
};

static unsigned int vertex_size_map[QU__TOTAL_VERTEX_FORMATS] = {
    [QU__VERTEX_FORMAT_SOLID] = 2,
    [QU__VERTEX_FORMAT_TEXTURED] = 4,
};

//------------------------------------------------------------------------------

#define QU__MATRIX_STACK_SIZE                           32
#define QU__RENDER_COMMAND_BUFFER_INITIAL_CAPACITY      256
#define QU__VERTEX_BUFFER_INITIAL_CAPACITY              1024
#define QU__CIRCLE_VERTEX_COUNT                         64

enum qu__render_command
{
    QU__RENDER_COMMAND_NO_OP,
    QU__RENDER_COMMAND_RESIZE,
    QU__RENDER_COMMAND_VIEW_OP,
    QU__RENDER_COMMAND_STACK_OP,
    QU__RENDER_COMMAND_TRANSFORM,
    QU__RENDER_COMMAND_CLEAR,
    QU__RENDER_COMMAND_DRAW,
    QU__RENDER_COMMAND_SURFACE,
};

enum qu__stack_op
{
    QU__STACK_PUSH,
    QU__STACK_POP,
};

enum qu__transform
{
    QU__TRANSFORM_TRANSLATE,
    QU__TRANSFORM_SCALE,
    QU__TRANSFORM_ROTATE,
};

struct qu__resize_render_command_args
{
    int width;
    int height;
};

struct qu__view_op_render_command_args
{
    int reset;
    float x;
    float y;
    float w;
    float h;
    float r;
};

struct qu__stack_op_render_command_args
{
    enum qu__stack_op type;
};

struct qu__transform_render_command_args
{
    enum qu__transform type;
    float v[2];
};

struct qu__clear_render_command_args
{
    qu_color color;
};

struct qu__draw_render_command_args
{
    struct qu__texture *texture;
    qu_color color;
    enum qu__vertex_format vertex_format;
    enum qu__render_mode render_mode;
    unsigned int first_vertex;
    unsigned int total_vertices;
};

struct qu__surface_render_command_args
{
    struct qu__surface *surface;
};

union qu__render_command_args
{
    struct qu__resize_render_command_args resize;
    struct qu__view_op_render_command_args view_op;
    struct qu__stack_op_render_command_args stack_op;
    struct qu__transform_render_command_args transform;
    struct qu__clear_render_command_args clear;
    struct qu__draw_render_command_args draw;
    struct qu__surface_render_command_args surface;
};

struct qu__render_command_info
{
    enum qu__render_command command;
    union qu__render_command_args args;
};

struct qu__render_command_buffer
{
    struct qu__render_command_info *data;
    size_t size;
    size_t capacity;
};

struct qu__vertex_buffer
{
    float *data;
    size_t size;
    size_t capacity;
};

struct qu__graphics_priv
{
    struct qu__renderer_impl const *renderer;

    struct qu__render_command_buffer command_buffer;
    struct qu__vertex_buffer vertex_buffers[QU__TOTAL_VERTEX_FORMATS];
    float *circle_vertices;

    qu_array *textures; // struct qu__texture
    qu_array *surfaces; // struct qu__surface

    qu_color clear_color;
    qu_color draw_color;
    enum qu__vertex_format vertex_format;

    struct qu__surface display;
    struct qu__surface canvas;

    struct qu__texture *current_texture;
    struct qu__surface *current_surface;

    bool canvas_enabled;

    float canvas_ax;
    float canvas_ay;
    float canvas_bx;
    float canvas_by;
};

static struct qu__graphics_priv priv;

//------------------------------------------------------------------------------
// Render commands

static void graphics__update_canvas_coords(int w_display, int h_display);

static void graphics__exec_resize(struct qu__resize_render_command_args const *args)
{
    priv.display.texture.image.width = args->width;
    priv.display.texture.image.height = args->height;

    qu_mat4_ortho(&priv.display.projection, 0.f, args->width, args->height, 0.f);

    if (priv.canvas_enabled) {
        graphics__update_canvas_coords(args->width, args->height);
    }

    if (priv.current_surface == &priv.display) {
        priv.renderer->apply_projection(&priv.display.projection);
        priv.renderer->exec_resize(args->width, args->height);
    }
}

static void graphics__exec_view_op(struct qu__view_op_render_command_args const *args)
{
    float x;
    float y;
    float w;
    float h;
    float rot;

    if (args->reset) {
        x = priv.current_surface->texture.image.width / 2.f;
        y = priv.current_surface->texture.image.height / 2.f;
        w = priv.current_surface->texture.image.width;
        h = priv.current_surface->texture.image.height;
        rot = 0.f;
    } else {
        x = args->x;
        y = args->y;
        w = args->w;
        h = args->h;
        rot = args->r;
    }

    qu_mat4_ortho(&priv.current_surface->projection,
                  x - (w / 2.f), x + (w / 2.f),
                  y + (h / 2.f), y - (h / 2.f));

    if (rot != 0.f) {
        qu_mat4_translate(&priv.current_surface->projection, x, y, 0.f);
        qu_mat4_rotate(&priv.current_surface->projection, QU_DEG2RAD(rot), 0.f, 0.f, 1.f);
        qu_mat4_translate(&priv.current_surface->projection, -x, -y, 0.f);
    }

    priv.renderer->apply_projection(&priv.current_surface->projection);
}

static void graphics__exec_stack_op(struct qu__stack_op_render_command_args const *args)
{
    int *index = &priv.current_surface->modelview_index;

    switch (args->type) {
    case QU__STACK_PUSH:
        if ((*index) < (QU__MATRIX_STACK_SIZE - 1)) {
            qu_mat4 *next = &priv.current_surface->modelview[*index + 1];
            qu_mat4 *current = &priv.current_surface->modelview[*index];

            qu_mat4_copy(next, current);
            (*index)++;
        }
        break;
    case QU__STACK_POP:
        if ((*index) > 0) {
            (*index)--;

            qu_mat4 *current = &priv.current_surface->modelview[*index];
            priv.renderer->apply_transform(current);
        }
        break;
    }
}

static void graphics__exec_transform(struct qu__transform_render_command_args const *args)
{
    int index = priv.current_surface->modelview_index;
    qu_mat4 *matrix = &priv.current_surface->modelview[index];

    switch (args->type) {
    case QU__TRANSFORM_TRANSLATE:
        qu_mat4_translate(matrix, args->v[0], args->v[1], 0.f);
        break;
    case QU__TRANSFORM_SCALE:
        qu_mat4_scale(matrix, args->v[0], args->v[1], 1.f);
        break;
    case QU__TRANSFORM_ROTATE:
        qu_mat4_rotate(matrix, QU_DEG2RAD(args->v[0]), 0.f, 0.f, 1.f);
        break;
    }

    priv.renderer->apply_transform(matrix);
}

static void graphics__exec_clear(struct qu__clear_render_command_args const *args)
{
    if (priv.clear_color != args->color) {
        priv.clear_color = args->color;
        priv.renderer->apply_clear_color(priv.clear_color);
    }

    priv.renderer->exec_clear();
}

static void graphics__exec_draw(struct qu__draw_render_command_args const *args)
{
    if (priv.current_texture != args->texture) {
        priv.renderer->apply_texture(args->texture);
        priv.current_texture = args->texture;
    }

    if (priv.draw_color != args->color) {
        priv.draw_color = args->color;
        priv.renderer->apply_draw_color(priv.draw_color);
    }

    if (priv.vertex_format != args->vertex_format) {
        priv.vertex_format = args->vertex_format;
        priv.renderer->apply_vertex_format(priv.vertex_format);
    }

    priv.renderer->exec_draw(args->render_mode, args->first_vertex, args->total_vertices);
}

static void graphics__exec_surface(struct qu__surface_render_command_args const *args)
{
    if (priv.current_surface == args->surface) {
        return;
    }

    priv.renderer->apply_projection(&args->surface->projection);
    priv.renderer->apply_surface(args->surface);

    priv.current_surface = args->surface;
}

//------------------------------------------------------------------------------
// Command buffer

static void graphics__grow_render_command_buffer(void)
{
    size_t next_capacity = priv.command_buffer.capacity * 2;
    size_t data_bytes = sizeof(struct qu__render_command_info) * next_capacity;
    struct qu__render_command_info *next_data = realloc(priv.command_buffer.data, data_bytes);

    QU_HALT_IF(!next_data);

    priv.command_buffer.data = next_data;
    priv.command_buffer.capacity = next_capacity;
}

static void graphics__append_render_command(struct qu__render_command_info const *info)
{
    size_t last_index = priv.command_buffer.size;
    struct qu__render_command_info *last = &priv.command_buffer.data[last_index];

    if (last->command == info->command) {
        switch (last->command) {
        case QU__RENDER_COMMAND_RESIZE:
            last->args.resize.width = info->args.resize.width;
            last->args.resize.height = info->args.resize.height;
            return;
        default:
            break;
        }
    }

    if (priv.command_buffer.size >= priv.command_buffer.capacity) {
        graphics__grow_render_command_buffer();
    }

    size_t index = priv.command_buffer.size++;
    memcpy(&priv.command_buffer.data[index], info, sizeof(struct qu__render_command_info));
}

static void graphics__execute_command(struct qu__render_command_info const *info)
{
    switch (info->command) {
    case QU__RENDER_COMMAND_RESIZE:
        graphics__exec_resize(&info->args.resize);
        break;
    case QU__RENDER_COMMAND_VIEW_OP:
        graphics__exec_view_op(&info->args.view_op);
        break;
    case QU__RENDER_COMMAND_STACK_OP:
        graphics__exec_stack_op(&info->args.stack_op);
        break;
    case QU__RENDER_COMMAND_TRANSFORM:
        graphics__exec_transform(&info->args.transform);
        break;
    case QU__RENDER_COMMAND_CLEAR:
        graphics__exec_clear(&info->args.clear);
        break;
    case QU__RENDER_COMMAND_DRAW:
        graphics__exec_draw(&info->args.draw);
        break;
    case QU__RENDER_COMMAND_SURFACE:
        graphics__exec_surface(&info->args.surface);
        break;
    default:
        break;
    }
}

static void graphics__execute_command_buffer(void)
{
    for (size_t i = 0; i < priv.command_buffer.size; i++) {
        graphics__execute_command(&priv.command_buffer.data[i]);
    }

    priv.command_buffer.size = 0;
}

//------------------------------------------------------------------------------
// Vertex buffer

static void graphics__grow_vertex_buffer(struct qu__vertex_buffer *buffer, size_t required_capacity)
{
    size_t next_capacity = buffer->capacity * 2;

    while (next_capacity < required_capacity) {
        next_capacity *= 2;
    }

    size_t data_bytes = sizeof(float) * next_capacity;
    float *next_data = realloc(buffer->data, data_bytes);

    QU_HALT_IF(!next_data);

    buffer->data = next_data;
    buffer->capacity = next_capacity;
}

static unsigned int graphics__append_vertex_data(enum qu__vertex_format format, float const *data, size_t size)
{
    struct qu__vertex_buffer *buffer = &priv.vertex_buffers[format];

    size_t required_capacity = buffer->size + size;

    if (buffer->capacity < required_capacity) {
        graphics__grow_vertex_buffer(buffer, required_capacity);
    }

    float *dst = &buffer->data[buffer->size];

    memcpy(dst, data, sizeof(float) * size);

    size_t offset = buffer->size;
    buffer->size += size;

    return (unsigned int) (offset / vertex_size_map[format]);
}

static void graphics__upload_vertex_data(enum qu__vertex_format format)
{
    struct qu__vertex_buffer *buffer = &priv.vertex_buffers[format];

    if (buffer->size == 0) {
        return;
    }

    priv.renderer->upload_vertex_data(format, buffer->data, buffer->size);
    buffer->size = 0;
}

//------------------------------------------------------------------------------

static void graphics__update_canvas_coords(int w_display, int h_display)
{
    float ard = w_display / (float) h_display;
    float arc = priv.canvas.texture.image.width / (float) priv.canvas.texture.image.height;

    if (ard > arc) {
        priv.canvas_ax = (w_display / 2.f) - ((arc / ard) * w_display / 2.f);
        priv.canvas_ay = 0.f;
        priv.canvas_bx = (w_display / 2.f) + ((arc / ard) * w_display / 2.f);
        priv.canvas_by = h_display;
    } else {
        priv.canvas_ax = 0.f;
        priv.canvas_ay = (h_display / 2.f) - ((ard / arc) * h_display / 2.f);
        priv.canvas_bx = w_display;
        priv.canvas_by = (h_display / 2.f) + ((ard / arc) * h_display / 2.f);
    }
}

static void graphics__flush_canvas(void)
{
    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_SURFACE,
        .args.surface.surface = &priv.display,
    });

    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_CLEAR,
        .args.clear.color = QU_COLOR(0, 0, 0),
    });

    float vertices[] = {
        priv.canvas_ax, priv.canvas_ay, 0.f, 1.f,
        priv.canvas_bx, priv.canvas_ay, 1.f, 1.f,
        priv.canvas_bx, priv.canvas_by, 1.f, 0.f,
        priv.canvas_ax, priv.canvas_by, 0.f, 0.f,
    };

    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_DRAW,
        .args.draw = {
            .texture = &priv.canvas.texture,
            .color = QU_COLOR(255, 255, 255),
            .render_mode = QU__RENDER_MODE_TRIANGLE_FAN,
            .vertex_format = QU__VERTEX_FORMAT_TEXTURED,
            .first_vertex = graphics__append_vertex_data(QU__VERTEX_FORMAT_TEXTURED, vertices, 16),
            .total_vertices = 4,
        },
    });
}

static void texture_dtor(void *ptr)
{
    // This won't be called on surface textures.

    struct qu__texture *texture = ptr;

    priv.renderer->unload_texture(texture);
    qu__image_delete(&texture->image);
}

static void surface_dtor(void *ptr)
{
    priv.renderer->destroy_surface((struct qu__surface *) ptr);
}

//------------------------------------------------------------------------------

void qu__graphics_initialize(qu_params const *params)
{
    memset(&priv, 0, sizeof(priv));

    int renderer_impl_count = QU__ARRAY_SIZE(supported_renderer_impl_list);

    if (renderer_impl_count == 0) {
        QU_HALT("renderer_impl_count == 0");
    }

    for (int i = 0; i < renderer_impl_count; i++) {
        priv.renderer = supported_renderer_impl_list[i];

        QU_HALT_IF(!priv.renderer->query);

        if (priv.renderer->query(params)) {
            QU_DEBUG("Selected graphics implementation #%d.\n", i);
            break;
        }
    }

    QU_HALT_IF(!priv.renderer->initialize);
    QU_HALT_IF(!priv.renderer->terminate);
    QU_HALT_IF(!priv.renderer->upload_vertex_data);

    QU_HALT_IF(!priv.renderer->apply_projection);
    QU_HALT_IF(!priv.renderer->apply_transform);
    QU_HALT_IF(!priv.renderer->apply_surface);
    QU_HALT_IF(!priv.renderer->apply_texture);
    QU_HALT_IF(!priv.renderer->apply_clear_color);
    QU_HALT_IF(!priv.renderer->apply_draw_color);
    QU_HALT_IF(!priv.renderer->apply_vertex_format);

    QU_HALT_IF(!priv.renderer->exec_resize);
    QU_HALT_IF(!priv.renderer->exec_clear);
    QU_HALT_IF(!priv.renderer->exec_draw);

    QU_HALT_IF(!priv.renderer->load_texture);
    QU_HALT_IF(!priv.renderer->unload_texture);
    QU_HALT_IF(!priv.renderer->set_texture_smooth);

    QU_HALT_IF(!priv.renderer->create_surface);
    QU_HALT_IF(!priv.renderer->destroy_surface);

    priv.renderer->initialize(params);

    QU__ALLOC_ARRAY(priv.command_buffer.data, QU__RENDER_COMMAND_BUFFER_INITIAL_CAPACITY);
    priv.command_buffer.size = 0;
    priv.command_buffer.capacity = QU__RENDER_COMMAND_BUFFER_INITIAL_CAPACITY;

    for (int i = 0; i < QU__TOTAL_VERTEX_FORMATS; i++) {
        QU__ALLOC_ARRAY(priv.vertex_buffers[i].data, QU__VERTEX_BUFFER_INITIAL_CAPACITY);
        priv.vertex_buffers[i].size = 0;
        priv.vertex_buffers[i].capacity = QU__VERTEX_BUFFER_INITIAL_CAPACITY;
    }
    
    priv.textures = qu_create_array(sizeof(struct qu__texture), texture_dtor);
    priv.surfaces = qu_create_array(sizeof(struct qu__surface), surface_dtor);

    QU__ALLOC_ARRAY(priv.circle_vertices, 2 * QU__CIRCLE_VERTEX_COUNT);

    priv.clear_color = QU_COLOR(0, 0, 0);
    priv.draw_color = QU_COLOR(255, 255, 255);

    priv.renderer->apply_clear_color(priv.clear_color);
    priv.renderer->apply_draw_color(priv.draw_color);

    // Trigger resize.
    priv.renderer->exec_resize(params->display_width, params->display_height);

    // Create pseudo-texture for display.
    priv.display = (struct qu__surface) {
        .texture = {
            .image.width = params->display_width,
            .image.height = params->display_height,
        },
    };

    qu_mat4_ortho(&priv.display.projection, 0.f, params->display_width, params->display_height, 0.f);
    qu_mat4_identity(&priv.display.modelview[0]);

    priv.current_texture = NULL;
    priv.current_surface = &priv.display;

    priv.renderer->apply_projection(&priv.current_surface->projection);
    priv.renderer->apply_transform(&priv.current_surface->modelview[0]);

    // Create texture for canvas if needed.
    if (params->enable_canvas) {
        priv.canvas_enabled = true;

        priv.canvas = (struct qu__surface) {
            .texture = {
                .image.width = params->display_width,
                .image.height = params->display_height,
            },
        };

        qu_mat4_ortho(&priv.canvas.projection, 0.f, params->canvas_width, params->canvas_height, 0.f);
        qu_mat4_identity(&priv.canvas.modelview[0]);

        priv.renderer->create_surface(&priv.canvas);

        graphics__append_render_command(&(struct qu__render_command_info) {
            .command = QU__RENDER_COMMAND_SURFACE,
            .args.surface.surface = &priv.canvas,
        });

        graphics__update_canvas_coords(params->display_width, params->display_height);
    }
}

void qu__graphics_terminate(void)
{
    free(priv.command_buffer.data);

    for (int i = 0; i < QU__TOTAL_VERTEX_FORMATS; i++) {
        free(priv.vertex_buffers[i].data);
    }

    qu_destroy_array(priv.textures);
    qu_destroy_array(priv.surfaces);

    if (priv.canvas_enabled) {
        priv.renderer->destroy_surface(&priv.canvas);
    }

    free(priv.circle_vertices);

    priv.renderer->terminate();
}

void qu__graphics_refresh(void)
{
    // [TODO] Bring back.
}

void qu__graphics_swap(void)
{
    if (priv.canvas_enabled) {
        graphics__flush_canvas();
    }

    for (int i = 0; i < QU__TOTAL_VERTEX_FORMATS; i++) {
        graphics__upload_vertex_data(i);
    }

    graphics__execute_command_buffer();

    priv.vertex_format = -1;

    if (priv.canvas_enabled) {
        graphics__append_render_command(&(struct qu__render_command_info) {
            .command = QU__RENDER_COMMAND_SURFACE,
            .args.surface.surface = &priv.canvas,
        });
    }
}

void qu__graphics_on_display_resize(int width, int height)
{
    // This may be called too early; temporary crash fix.
    if (!priv.renderer) {
        return;
    }

    struct qu__render_command_info info = { QU__RENDER_COMMAND_RESIZE };

    info.args.resize.width = width;
    info.args.resize.height = height;

    graphics__append_render_command(&info);
}

qu_vec2i qu__graphics_conv_cursor(qu_vec2i position)
{
    if (!priv.canvas_enabled) {
        return position;
    }

    float dw = priv.display.texture.image.width;
    float dh = priv.display.texture.image.height;
    float cw = priv.canvas.texture.image.width;
    float ch = priv.canvas.texture.image.height;

    float dar = dw / dh;
    float car = cw / ch;

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

qu_vec2i qu__graphics_conv_cursor_delta(qu_vec2i delta)
{
    float dw = priv.display.texture.image.width;
    float dh = priv.display.texture.image.height;
    float cw = priv.canvas.texture.image.width;
    float ch = priv.canvas.texture.image.height;

    float dar = dw / dh;
    float car = cw / ch;

    if (dar > car) {
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

int32_t qu__graphics_create_texture(int width, int height, int channels)
{
    struct qu__texture texture = {
        .image = {
            .width = width,
            .height = height,
            .channels = channels,
        }
    };

    qu__image_create(&texture.image);

    if (!texture.image.pixels) {
        return 0;
    }

    priv.renderer->load_texture(&texture);
    return qu_array_add(priv.textures, &texture);
}

void qu__graphics_update_texture(int32_t texture_id, int x, int y, int w, int h, uint8_t const *pixels)
{
    struct qu__texture *texture = qu_array_get(priv.textures, texture_id);

    if (!texture || !texture->image.pixels) {
        return;
    }

    if (w == -1) {
        w = texture->image.width;
    }

    if (h == -1) {
        h = texture->image.height;
    }

    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            size_t nc = texture->image.channels;
            size_t di = (y + i) * texture->image.width * nc + (x + j) * nc;
            size_t si = i * w * nc + j * nc;

            for (int k = 0; k < nc; k++) {
                texture->image.pixels[di + k] = pixels[si + k];
            }
        }
    }

    priv.renderer->load_texture(texture);
}

void qu__graphics_draw_text(int32_t texture_id, qu_color color, float const *data, int count)
{
    struct qu__texture *texture = qu_array_get(priv.textures, texture_id);

    if (!texture) {
        return;
    }

    struct qu__render_command_info info = { QU__RENDER_COMMAND_DRAW };

    info.args.draw.texture = texture;
    info.args.draw.color = QU_COLOR(255, 255, 255);
    info.args.draw.render_mode = QU__RENDER_MODE_TRIANGLES;
    info.args.draw.vertex_format = QU__VERTEX_FORMAT_TEXTURED;
    info.args.draw.first_vertex = graphics__append_vertex_data(info.args.draw.vertex_format, data, count * 4);
    info.args.draw.total_vertices = count;

    graphics__append_render_command(&info);
}

//------------------------------------------------------------------------------
// API entries

void qu_set_view(float x, float y, float w, float h, float rotation)
{
    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_VIEW_OP,
        .args.view_op = {
            .reset = 0,
            .x = x,
            .y = y,
            .w = w,
            .h = h,
            .r = rotation,
        },
    });
}

void qu_reset_view(void)
{
    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_VIEW_OP,
        .args.view_op = {
            .reset = 1,
        },
    });
}

void qu_push_matrix(void)
{
    struct qu__render_command_info info = { QU__RENDER_COMMAND_STACK_OP };

    info.args.stack_op.type = QU__STACK_PUSH;

    graphics__append_render_command(&info);
}

void qu_pop_matrix(void)
{
    struct qu__render_command_info info = { QU__RENDER_COMMAND_STACK_OP };

    info.args.stack_op.type = QU__STACK_POP;

    graphics__append_render_command(&info);
}

void qu_translate(float x, float y)
{
    struct qu__render_command_info info = { QU__RENDER_COMMAND_TRANSFORM };

    info.args.transform.type = QU__TRANSFORM_TRANSLATE;
    info.args.transform.v[0] = x;
    info.args.transform.v[1] = y;

    graphics__append_render_command(&info);
}

void qu_scale(float x, float y)
{
    struct qu__render_command_info info = { QU__RENDER_COMMAND_TRANSFORM };

    info.args.transform.type = QU__TRANSFORM_SCALE;
    info.args.transform.v[0] = x;
    info.args.transform.v[1] = y;

    graphics__append_render_command(&info);
}

void qu_rotate(float degrees)
{
    struct qu__render_command_info info = { QU__RENDER_COMMAND_TRANSFORM };

    info.args.transform.type = QU__TRANSFORM_ROTATE;
    info.args.transform.v[0] = degrees;

    graphics__append_render_command(&info);
}

void qu_clear(qu_color color)
{
    struct qu__render_command_info info = { 0 };

    info.command = QU__RENDER_COMMAND_CLEAR;
    info.args.clear.color = color;

    graphics__append_render_command(&info);
}

void qu_draw_point(float x, float y, qu_color color)
{
    float const vertex[] = { x, y };

    struct qu__render_command_info info = { QU__RENDER_COMMAND_DRAW };

    info.args.draw.color = color;
    info.args.draw.render_mode = QU__RENDER_MODE_POINTS;
    info.args.draw.vertex_format = QU__VERTEX_FORMAT_SOLID;
    info.args.draw.first_vertex = graphics__append_vertex_data(info.args.draw.vertex_format, vertex, 2);
    info.args.draw.total_vertices = 1;

    graphics__append_render_command(&info);
}

void qu_draw_line(float ax, float ay, float bx, float by, qu_color color)
{
    float const vertices[] = {
        ax, ay,
        bx, by,
    };

    struct qu__render_command_info info = { QU__RENDER_COMMAND_DRAW };

    info.args.draw.color = color;
    info.args.draw.render_mode = QU__RENDER_MODE_LINES;
    info.args.draw.vertex_format = QU__VERTEX_FORMAT_SOLID;
    info.args.draw.first_vertex = graphics__append_vertex_data(info.args.draw.vertex_format, vertices, 4);
    info.args.draw.total_vertices = 2;

    graphics__append_render_command(&info);
}

void qu_draw_triangle(float ax, float ay, float bx, float by, float cx, float cy, qu_color outline, qu_color fill)
{
    int outline_alpha = (outline >> 24) & 255;
    int fill_alpha = (fill >> 24) & 255;
    
    float const vertices[] = {
        ax, ay,
        bx, by,
        cx, cy,
    };

    struct qu__render_command_info info = { QU__RENDER_COMMAND_DRAW };

    info.args.draw.vertex_format = QU__VERTEX_FORMAT_SOLID;
    info.args.draw.first_vertex = graphics__append_vertex_data(info.args.draw.vertex_format, vertices, 6);
    info.args.draw.total_vertices = 3;

    if (fill_alpha > 0) {
        info.args.draw.color = fill;
        info.args.draw.render_mode = QU__RENDER_MODE_TRIANGLES;
        graphics__append_render_command(&info);
    }

    if (outline_alpha > 0) {
        info.args.draw.color = outline;
        info.args.draw.render_mode = QU__RENDER_MODE_LINE_LOOP;
        graphics__append_render_command(&info);
    }
}

void qu_draw_rectangle(float x, float y, float w, float h, qu_color outline, qu_color fill)
{
    int outline_alpha = (outline >> 24) & 255;
    int fill_alpha = (fill >> 24) & 255;
    
    float const vertices[] = {
        x,          y,
        x + w,      y,
        x + w,      y + h,
        x,          y + h,
    };

    struct qu__render_command_info info = { QU__RENDER_COMMAND_DRAW };

    info.args.draw.vertex_format = QU__VERTEX_FORMAT_SOLID;
    info.args.draw.first_vertex = graphics__append_vertex_data(info.args.draw.vertex_format, vertices, 8);
    info.args.draw.total_vertices = 4;

    if (fill_alpha > 0) {
        info.args.draw.color = fill;
        info.args.draw.render_mode = QU__RENDER_MODE_TRIANGLE_FAN;
        graphics__append_render_command(&info);
    }

    if (outline_alpha > 0) {
        info.args.draw.color = outline;
        info.args.draw.render_mode = QU__RENDER_MODE_LINE_LOOP;
        graphics__append_render_command(&info);
    }
}

void qu_draw_circle(float x, float y, float radius, qu_color outline, qu_color fill)
{
    int outline_alpha = (outline >> 24) & 255;
    int fill_alpha = (fill >> 24) & 255;

    int total_vertices = QU__CIRCLE_VERTEX_COUNT;
    float *vertices = priv.circle_vertices;

    float angle = QU_DEG2RAD(360.f / total_vertices);
    
    for (int i = 0; i < total_vertices; i++) {
        vertices[2 * i + 0] = x + (radius * cosf(i * angle));
        vertices[2 * i + 1] = y + (radius * sinf(i * angle));
    }

    struct qu__render_command_info info = { QU__RENDER_COMMAND_DRAW };

    info.args.draw.vertex_format = QU__VERTEX_FORMAT_SOLID;
    info.args.draw.first_vertex = graphics__append_vertex_data(info.args.draw.vertex_format, vertices, 2 * total_vertices);
    info.args.draw.total_vertices = total_vertices;

    if (fill_alpha > 0) {
        info.args.draw.color = fill;
        info.args.draw.render_mode = QU__RENDER_MODE_TRIANGLE_FAN;
        graphics__append_render_command(&info);
    }

    if (outline_alpha > 0) {
        info.args.draw.color = outline;
        info.args.draw.render_mode = QU__RENDER_MODE_LINE_LOOP;
        graphics__append_render_command(&info);
    }
}

qu_texture qu_load_texture(char const *path)
{
    qu_texture result = { 0 };
    qu_file *file = qu_fopen(path);

    if (file) {
        struct qu__texture texture = { 0 };
        qu__image_load(&texture.image, file);

        if (texture.image.pixels) {
            priv.renderer->load_texture(&texture);
            result.id = qu_array_add(priv.textures, &texture);
        }

        qu_fclose(file);
    }

    return result;
}

void qu_delete_texture(qu_texture texture)
{
    qu_array_remove(priv.textures, texture.id);
}

void qu_set_texture_smooth(qu_texture texture, bool smooth)
{
    struct qu__texture *texture_p = qu_array_get(priv.textures, texture.id);

    if (texture_p) {
        priv.renderer->set_texture_smooth(texture_p, smooth);
    }
}

void qu_draw_texture(qu_texture texture, float x, float y, float w, float h)
{
    float const vertices[] = {
        x,      y,      0.f,    0.f,
        x + w,  y,      1.f,    0.f,
        x + w,  y + h,  1.f,    1.f,
        x,      y + h,  0.f,    1.f,
    };

    struct qu__render_command_info info = { QU__RENDER_COMMAND_DRAW };

    info.args.draw.texture = qu_array_get(priv.textures, texture.id);
    info.args.draw.color = QU_COLOR(255, 255, 255);
    info.args.draw.render_mode = QU__RENDER_MODE_TRIANGLE_FAN;
    info.args.draw.vertex_format = QU__VERTEX_FORMAT_TEXTURED;
    info.args.draw.first_vertex = graphics__append_vertex_data(info.args.draw.vertex_format, vertices, 16);
    info.args.draw.total_vertices = 4;

    graphics__append_render_command(&info);
}

void qu_draw_subtexture(qu_texture texture, float x, float y, float w, float h, float rx, float ry, float rw, float rh)
{
    struct qu__texture *texture_p = qu_array_get(priv.textures, texture.id);

    if (!texture_p) {
        return;
    }

    float s = rx / texture_p->image.width;
    float t = ry / texture_p->image.height;
    float u = rw / texture_p->image.width;
    float v = rh / texture_p->image.height;

    float const vertices[] = {
        x,      y,      s,      t,
        x + w,  y,      s + u,  t,
        x + w,  y + h,  s + u,  t + v,
        x,      y + h,  s,      t + v,
    };

    struct qu__render_command_info info = { QU__RENDER_COMMAND_DRAW };

    info.args.draw.texture = texture_p;
    info.args.draw.color = QU_COLOR(255, 255, 255);
    info.args.draw.render_mode = QU__RENDER_MODE_TRIANGLE_FAN;
    info.args.draw.vertex_format = QU__VERTEX_FORMAT_TEXTURED;
    info.args.draw.first_vertex = graphics__append_vertex_data(info.args.draw.vertex_format, vertices, 16);
    info.args.draw.total_vertices = 4;

    graphics__append_render_command(&info);
}

qu_surface qu_create_surface(int width, int height)
{
    struct qu__surface surface = {
        .texture = {
            .image = {
                .width = width,
                .height = height,
            },
        },
    };

    qu_mat4_ortho(&surface.projection, 0.f, width, height, 0.f);
    qu_mat4_identity(&surface.modelview[0]);

    priv.renderer->create_surface(&surface);

    return (qu_surface) { qu_array_add(priv.surfaces, &surface) };
}

void qu_delete_surface(qu_surface surface)
{
    qu_array_remove(priv.surfaces, surface.id);
}

void qu_set_surface(qu_surface surface)
{
    struct qu__surface *surface_p = qu_array_get(priv.surfaces, surface.id);

    if (!surface_p) {
        return;
    }

    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_SURFACE,
        .args.surface.surface = surface_p,
    });
}

void qu_reset_surface(void)
{
    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_SURFACE,
        .args.surface.surface = priv.canvas_enabled ? &priv.canvas : &priv.display,
    });
}

void qu_draw_surface(qu_surface surface, float x, float y, float w, float h)
{
    struct qu__surface *surface_p = qu_array_get(priv.surfaces, surface.id);

    if (!surface_p) {
        return;
    }

    float const vertices[] = {
        x,      y,      0.f,    0.f,
        x + w,  y,      1.f,    0.f,
        x + w,  y + h,  1.f,    1.f,
        x,      y + h,  0.f,    1.f,
    };

    struct qu__render_command_info info = { QU__RENDER_COMMAND_DRAW };

    info.args.draw.texture = &surface_p->texture;
    info.args.draw.color = QU_COLOR(255, 255, 255);
    info.args.draw.render_mode = QU__RENDER_MODE_TRIANGLE_FAN;
    info.args.draw.vertex_format = QU__VERTEX_FORMAT_TEXTURED;
    info.args.draw.first_vertex = graphics__append_vertex_data(info.args.draw.vertex_format, vertices, 16);
    info.args.draw.total_vertices = 4;

    graphics__append_render_command(&info);
}
