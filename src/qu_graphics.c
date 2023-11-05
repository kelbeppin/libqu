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

static qu_renderer_impl const *supported_renderer_impl_list[] = {

#ifdef QU_USE_GL
    &qu_gl3_renderer_impl,
    &qu_gl1_renderer_impl,
#endif

#ifdef QU_USE_ES2
    &qu_es2_renderer_impl,
#endif

    &qu_null_renderer_impl,
};

static unsigned int vertex_size_map[QU_TOTAL_VERTEX_FORMATS] = {
    [QU_VERTEX_FORMAT_2XY] = 2,
    [QU_VERTEX_FORMAT_4XYST] = 4,
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
    QU__RENDER_COMMAND_SET_SURFACE,
    QU__RENDER_COMMAND_SET_VIEW,
    QU__RENDER_COMMAND_RESET_VIEW,
    QU__RENDER_COMMAND_PUSH_MATRIX,
    QU__RENDER_COMMAND_POP_MATRIX,
    QU__RENDER_COMMAND_TRANSLATE,
    QU__RENDER_COMMAND_SCALE,
    QU__RENDER_COMMAND_ROTATE,
    QU__RENDER_COMMAND_SET_BLEND_MODE,
    QU__RENDER_COMMAND_CLEAR,
    QU__RENDER_COMMAND_DRAW,
};

struct qu__resize_render_command_args
{
    int width;
    int height;
};

struct qu__surface_render_command_args
{
    qu_surface_obj *surface;
};

struct qu__set_view_render_command_args
{
    float x;
    float y;
    float w;
    float h;
    float rot;
};

struct qu__transform_render_command_args
{
    float a;
    float b;
};

struct qu__blend_render_command_args
{
    qu_blend_mode mode;
};

struct qu__clear_render_command_args
{
    qu_color color;
};

struct qu__draw_render_command_args
{
    qu_texture_obj *texture;
    qu_color color;
    qu_brush brush;
    qu_vertex_format vertex_format;
    qu_render_mode render_mode;
    unsigned int first_vertex;
    unsigned int total_vertices;
};

union qu__render_command_args
{
    struct qu__resize_render_command_args resize;
    struct qu__surface_render_command_args surface;
    struct qu__set_view_render_command_args view;
    struct qu__transform_render_command_args transform;
    struct qu__blend_render_command_args blend;
    struct qu__clear_render_command_args clear;
    struct qu__draw_render_command_args draw;
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
    qu_renderer_impl const *renderer;

    struct qu__render_command_buffer command_buffer;
    struct qu__vertex_buffer vertex_buffers[QU_TOTAL_VERTEX_FORMATS];
    float *circle_vertices;

    qu_handle_list *textures; // qu_texture_obj
    qu_handle_list *surfaces; // qu_surface_obj

    qu_color clear_color;
    qu_color draw_color;
    qu_brush brush;
    qu_vertex_format vertex_format;

    qu_surface_obj display;
    qu_surface_obj canvas;

    qu_texture_obj *current_texture;
    qu_surface_obj *current_surface;

    bool canvas_enabled;

    float canvas_ax;
    float canvas_ay;
    float canvas_bx;
    float canvas_by;

    qu_params tmp_params;
};

static struct qu__graphics_priv priv;

//------------------------------------------------------------------------------
// Render commands

static void graphics__update_canvas_coords(int w_display, int h_display);

static void graphics__exec_resize(struct qu__resize_render_command_args const *args)
{
    priv.display.texture.width = args->width;
    priv.display.texture.height = args->height;

    qu_mat4_ortho(&priv.display.projection, 0.f, args->width, args->height, 0.f);

    if (priv.canvas_enabled) {
        graphics__update_canvas_coords(args->width, args->height);
    }

    if (priv.current_surface == &priv.display) {
        priv.renderer->apply_projection(&priv.display.projection);
        priv.renderer->exec_resize(args->width, args->height);
    }
}

static void graphics__exec_set_surface(struct qu__surface_render_command_args const *args)
{
    if (priv.current_surface == args->surface) {
        return;
    }

    priv.renderer->apply_projection(&args->surface->projection);
    priv.renderer->apply_surface(args->surface);

    priv.current_surface = args->surface;
}

static void graphics__exec_set_view(struct qu__set_view_render_command_args const *args)
{
    qu_mat4_ortho(&priv.current_surface->projection,
                  args->x - (args->w / 2.f), args->x + (args->w / 2.f),
                  args->y + (args->h / 2.f), args->y - (args->h / 2.f));

    if (args->rot != 0.f) {
        qu_mat4_translate(&priv.current_surface->projection, args->x, args->y, 0.f);
        qu_mat4_rotate(&priv.current_surface->projection, QU_DEG2RAD(args->rot), 0.f, 0.f, 1.f);
        qu_mat4_translate(&priv.current_surface->projection, -args->x, -args->y, 0.f);
    }

    priv.renderer->apply_projection(&priv.current_surface->projection);
}

static void graphics__exec_reset_view(void)
{
    float w = priv.current_surface->texture.width;
    float h = priv.current_surface->texture.height;

    qu_mat4_ortho(&priv.current_surface->projection, 0.f, w, h, 0.f);
    priv.renderer->apply_projection(&priv.current_surface->projection);
}

static void graphics__exec_push_matrix(void)
{
    int index = priv.current_surface->modelview_index;

    if (index < (QU__MATRIX_STACK_SIZE - 1)) {
        qu_mat4 *next = &priv.current_surface->modelview[index + 1];
        qu_mat4 *current = &priv.current_surface->modelview[index];

        qu_mat4_copy(next, current);
        priv.current_surface->modelview_index++;
    }
}

static void graphics__exec_pop_matrix(void)
{
    int index = priv.current_surface->modelview_index;

    if (index > 0) {
        priv.renderer->apply_transform(&priv.current_surface->modelview[index - 1]);
        priv.current_surface->modelview_index--;
    }
}

static void graphics__exec_translate(struct qu__transform_render_command_args const *args)
{
    int index = priv.current_surface->modelview_index;
    qu_mat4 *matrix = &priv.current_surface->modelview[index];

    qu_mat4_translate(matrix, args->a, args->b, 0.f);
    priv.renderer->apply_transform(matrix);
}

static void graphics__exec_scale(struct qu__transform_render_command_args const *args)
{
    int index = priv.current_surface->modelview_index;
    qu_mat4 *matrix = &priv.current_surface->modelview[index];

    qu_mat4_scale(matrix, args->a, args->b, 1.f);
    priv.renderer->apply_transform(matrix);
}

static void graphics__exec_rotate(struct qu__transform_render_command_args const *args)
{
    int index = priv.current_surface->modelview_index;
    qu_mat4 *matrix = &priv.current_surface->modelview[index];

    qu_mat4_rotate(matrix, QU_DEG2RAD(args->a), 0.f, 0.f, 1.f);
    priv.renderer->apply_transform(matrix);
}

static void graphics__exec_set_blend_mode(struct qu__blend_render_command_args const *args)
{
    priv.renderer->apply_blend_mode(args->mode);
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

    if (priv.brush != args->brush) {
        priv.brush = args->brush;
        priv.renderer->apply_brush(priv.brush);
    }

    if (priv.vertex_format != args->vertex_format) {
        priv.vertex_format = args->vertex_format;
        priv.renderer->apply_vertex_format(priv.vertex_format);
    }

    priv.renderer->exec_draw(args->render_mode, args->first_vertex, args->total_vertices);
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
    if (priv.command_buffer.size > 0) {
        size_t last_index = priv.command_buffer.size - 1;
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
    case QU__RENDER_COMMAND_SET_SURFACE:
        graphics__exec_set_surface(&info->args.surface);
        break;
    case QU__RENDER_COMMAND_SET_VIEW:
        graphics__exec_set_view(&info->args.view);
        break;
    case QU__RENDER_COMMAND_RESET_VIEW:
        graphics__exec_reset_view();
        break;
    case QU__RENDER_COMMAND_PUSH_MATRIX:
        graphics__exec_push_matrix();
        break;
    case QU__RENDER_COMMAND_POP_MATRIX:
        graphics__exec_pop_matrix();
        break;
    case QU__RENDER_COMMAND_TRANSLATE:
        graphics__exec_translate(&info->args.transform);
        break;
    case QU__RENDER_COMMAND_SCALE:
        graphics__exec_scale(&info->args.transform);
        break;
    case QU__RENDER_COMMAND_ROTATE:
        graphics__exec_rotate(&info->args.transform);
        break;
    case QU__RENDER_COMMAND_SET_BLEND_MODE:
        graphics__exec_set_blend_mode(&info->args.blend);
        break;
    case QU__RENDER_COMMAND_CLEAR:
        graphics__exec_clear(&info->args.clear);
        break;
    case QU__RENDER_COMMAND_DRAW:
        graphics__exec_draw(&info->args.draw);
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

static unsigned int graphics__append_vertex_data(qu_vertex_format format, float const *data, size_t size)
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

static void graphics__upload_vertex_data(qu_vertex_format format)
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
    float arc = priv.canvas.texture.width / (float) priv.canvas.texture.height;

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
        .command = QU__RENDER_COMMAND_SET_SURFACE,
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
            .brush = QU_BRUSH_TEXTURED,
            .render_mode = QU_RENDER_MODE_TRIANGLE_FAN,
            .vertex_format = QU_VERTEX_FORMAT_4XYST,
            .first_vertex = graphics__append_vertex_data(QU_VERTEX_FORMAT_4XYST, vertices, 16),
            .total_vertices = 4,
        },
    });
}

static void texture_dtor(void *ptr)
{
    qu_texture_obj *texture = ptr;

    free(texture->pixels);

    if (!priv.renderer) {
        return;
    }

    // This won't be called on surface textures.
    priv.renderer->unload_texture(texture);
}

static void surface_dtor(void *ptr)
{
    if (!priv.renderer) {
        return;
    }

    priv.renderer->destroy_surface((qu_surface_obj *) ptr);
}

//------------------------------------------------------------------------------

static void initialize_renderer(qu_params const *params)
{
    QU_LOGD("Initializing renderer...\n");

    int renderer_impl_count = QU_ARRAY_SIZE(supported_renderer_impl_list);

    if (renderer_impl_count == 0) {
        QU_HALT("renderer_impl_count == 0");
    }

    for (int i = 0; i < renderer_impl_count; i++) {
        priv.renderer = supported_renderer_impl_list[i];

        QU_HALT_IF(!priv.renderer->query);

        if (priv.renderer->query(params)) {
            QU_LOGD("Selected graphics implementation #%d.\n", i);
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
    QU_HALT_IF(!priv.renderer->apply_brush);
    QU_HALT_IF(!priv.renderer->apply_vertex_format);
    QU_HALT_IF(!priv.renderer->apply_blend_mode);

    QU_HALT_IF(!priv.renderer->exec_resize);
    QU_HALT_IF(!priv.renderer->exec_clear);
    QU_HALT_IF(!priv.renderer->exec_draw);

    QU_HALT_IF(!priv.renderer->load_texture);
    QU_HALT_IF(!priv.renderer->unload_texture);
    QU_HALT_IF(!priv.renderer->set_texture_smooth);

    QU_HALT_IF(!priv.renderer->create_surface);
    QU_HALT_IF(!priv.renderer->destroy_surface);
    QU_HALT_IF(!priv.renderer->set_surface_antialiasing_level);

    priv.renderer->initialize(params);

    qu_texture_obj *texture = qu_handle_list_get_first(priv.textures);

    while (texture) {
        priv.renderer->load_texture(texture);
        texture = qu_handle_list_get_next(priv.textures);
    }

    qu_surface_obj *surface = qu_handle_list_get_first(priv.surfaces);

    while (surface) {
        priv.renderer->create_surface(surface);
        surface = qu_handle_list_get_next(priv.surfaces);
    }

    priv.renderer->apply_clear_color(priv.clear_color);
    priv.renderer->apply_draw_color(priv.draw_color);
    priv.renderer->apply_brush(priv.brush);
    priv.renderer->apply_vertex_format(priv.vertex_format);
    priv.renderer->apply_projection(&priv.current_surface->projection);
    priv.renderer->apply_transform(&priv.current_surface->modelview[0]);
    priv.renderer->apply_surface(priv.current_surface);
    priv.renderer->apply_texture(priv.current_texture);

    // Set default viewport.
    priv.renderer->exec_resize(params->display_width, params->display_height);

    // Enable alpha blend by default.
    priv.renderer->apply_blend_mode(QU_BLEND_MODE_ALPHA);

    if (params->enable_canvas) {
        priv.renderer->create_surface(&priv.canvas);
        priv.renderer->set_texture_smooth(&priv.canvas.texture, params->canvas_smooth);
    }

    QU_LOGD("Renderer is initialized.\n");
}

static void terminate_renderer(void)
{
    qu_texture_obj *texture = qu_handle_list_get_first(priv.textures);

    while (texture) {
        priv.renderer->unload_texture(texture);
        texture = qu_handle_list_get_next(priv.textures);
    }

    qu_surface_obj *surface = qu_handle_list_get_first(priv.surfaces);

    while (surface) {
        priv.renderer->destroy_surface(surface);
        surface = qu_handle_list_get_next(priv.surfaces);
    }

    if (priv.canvas_enabled) {
        priv.renderer->destroy_surface(&priv.canvas);
    }

    priv.renderer->terminate();

    priv.renderer = NULL;
}

//------------------------------------------------------------------------------

void qu_initialize_graphics(qu_params const *params)
{
    memset(&priv, 0, sizeof(priv));

    priv.tmp_params = *params;

    QU_ALLOC_ARRAY(priv.command_buffer.data, QU__RENDER_COMMAND_BUFFER_INITIAL_CAPACITY);
    priv.command_buffer.size = 0;
    priv.command_buffer.capacity = QU__RENDER_COMMAND_BUFFER_INITIAL_CAPACITY;

    for (int i = 0; i < QU_TOTAL_VERTEX_FORMATS; i++) {
        QU_ALLOC_ARRAY(priv.vertex_buffers[i].data, QU__VERTEX_BUFFER_INITIAL_CAPACITY);
        priv.vertex_buffers[i].size = 0;
        priv.vertex_buffers[i].capacity = QU__VERTEX_BUFFER_INITIAL_CAPACITY;
    }
    
    priv.textures = qu_create_handle_list(sizeof(qu_texture_obj), texture_dtor);
    priv.surfaces = qu_create_handle_list(sizeof(qu_surface_obj), surface_dtor);

    QU_ALLOC_ARRAY(priv.circle_vertices, 2 * QU__CIRCLE_VERTEX_COUNT);

    priv.clear_color = QU_COLOR(0, 0, 0);
    priv.draw_color = QU_COLOR(255, 255, 255);

    priv.brush = QU_BRUSH_SOLID;
    priv.vertex_format = QU_VERTEX_FORMAT_2XY;

    // Create pseudo-texture for display.
    priv.display = (qu_surface_obj) {
        .texture = {
            .width = params->display_width,
            .height = params->display_height,
        },
    };

    qu_mat4_ortho(&priv.display.projection, 0.f, params->display_width, params->display_height, 0.f);
    qu_mat4_identity(&priv.display.modelview[0]);

    priv.current_texture = NULL;
    priv.current_surface = &priv.display;

    // Create texture for canvas if needed.
    if (params->enable_canvas) {
        priv.canvas_enabled = true;

        priv.canvas = (qu_surface_obj) {
            .texture = {
                .width = params->canvas_width,
                .height = params->canvas_height,
            },
            .sample_count = params->canvas_antialiasing_level,
        };

        qu_mat4_ortho(&priv.canvas.projection, 0.f, params->canvas_width, params->canvas_height, 0.f);
        qu_mat4_identity(&priv.canvas.modelview[0]);

        graphics__append_render_command(&(struct qu__render_command_info) {
            .command = QU__RENDER_COMMAND_SET_SURFACE,
            .args.surface.surface = &priv.canvas,
        });

        graphics__update_canvas_coords(params->display_width, params->display_height);
    }

    initialize_renderer(params);
}

void qu_terminate_graphics(void)
{
    terminate_renderer();

    free(priv.command_buffer.data);

    for (int i = 0; i < QU_TOTAL_VERTEX_FORMATS; i++) {
        free(priv.vertex_buffers[i].data);
    }

    qu_destroy_handle_list(priv.textures);
    qu_destroy_handle_list(priv.surfaces);

    free(priv.circle_vertices);
}

void qu_flush_graphics(void)
{
    if (priv.canvas_enabled) {
        graphics__flush_canvas();
    }

    for (int i = 0; i < QU_TOTAL_VERTEX_FORMATS; i++) {
        graphics__upload_vertex_data(i);
    }

    graphics__execute_command_buffer();

    if (priv.canvas_enabled) {
        graphics__append_render_command(&(struct qu__render_command_info) {
            .command = QU__RENDER_COMMAND_SET_SURFACE,
            .args.surface.surface = &priv.canvas,
        });
    }
}

void qu_event_context_lost(void)
{
    terminate_renderer();
    
    priv.renderer = &qu_null_renderer_impl;
    priv.renderer->initialize(NULL);
}

void qu_event_context_restored(void)
{
    priv.renderer->terminate();

    initialize_renderer(&priv.tmp_params);
}

void qu_event_window_resize(int width, int height)
{
    // This may be called too early; temporary crash fix.
    if (!priv.renderer) {
        return;
    }

    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_RESIZE,
        .args.resize = {
            .width = width,
            .height = height,
        },
    });

    priv.tmp_params.display_width = width;
    priv.tmp_params.display_height = height;
}

qu_vec2i qu_convert_window_pos_to_canvas_pos(qu_vec2i position)
{
    if (!priv.canvas_enabled) {
        return position;
    }

    float dw = priv.display.texture.width;
    float dh = priv.display.texture.height;
    float cw = priv.canvas.texture.width;
    float ch = priv.canvas.texture.height;

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

qu_vec2i qu_convert_window_delta_to_canvas_delta(qu_vec2i delta)
{
    float dw = priv.display.texture.width;
    float dh = priv.display.texture.height;
    float cw = priv.canvas.texture.width;
    float ch = priv.canvas.texture.height;

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

//------------------------------------------------------------------------------
// Public API

void qu_set_view(float x, float y, float w, float h, float rotation)
{
    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_SET_VIEW,
        .args.view = {
            .x = x,
            .y = y,
            .w = w,
            .h = h,
            .rot = rotation,
        },
    });
}

void qu_reset_view(void)
{
    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_RESET_VIEW,
    });
}

void qu_push_matrix(void)
{
    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_PUSH_MATRIX,
    });
}

void qu_pop_matrix(void)
{
    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_POP_MATRIX,
    });
}

void qu_translate(float x, float y)
{
    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_TRANSLATE,
        .args.transform = {
            .a = x,
            .b = y,
        },
    });
}

void qu_scale(float x, float y)
{
    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_SCALE,
        .args.transform = {
            .a = x,
            .b = y,
        },
    });
}

void qu_rotate(float degrees)
{
    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_ROTATE,
        .args.transform = {
            .a = degrees,
        },
    });
}

void qu_set_blend_mode(qu_blend_mode mode)
{
    if (mode.color_src_factor < 0 || mode.color_src_factor >= 10) {
        return;
    }

    if (mode.color_dst_factor < 0 || mode.color_dst_factor >= 10) {
        return;
    }

    if (mode.alpha_src_factor < 0 || mode.alpha_src_factor >= 10) {
        return;
    }

    if (mode.alpha_dst_factor < 0 || mode.alpha_dst_factor >= 10) {
        return;
    }

    if (mode.color_equation < 0 || mode.color_equation >= 3) {
        return;
    }

    if (mode.alpha_equation < 0 || mode.alpha_equation >= 3) {
        return;
    }

    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_SET_BLEND_MODE,
        .args.blend = {
            .mode = mode,
        },
    });
}

void qu_clear(qu_color color)
{
    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_CLEAR,
        .args.clear = {
            .color = color,
        },
    });
}

void qu_draw_point(float x, float y, qu_color color)
{
    float const vertex[] = { x, y };

    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_DRAW,
        .args.draw = {
            .color = color,
            .brush = QU_BRUSH_SOLID,
            .vertex_format = QU_VERTEX_FORMAT_2XY,
            .render_mode = QU_RENDER_MODE_POINTS,
            .first_vertex = graphics__append_vertex_data(QU_VERTEX_FORMAT_2XY, vertex, 2),
            .total_vertices = 1,
        },
    });
}

void qu_draw_line(float ax, float ay, float bx, float by, qu_color color)
{
    float const vertices[] = {
        ax, ay,
        bx, by,
    };

    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_DRAW,
        .args.draw = {
            .color = color,
            .brush = QU_BRUSH_SOLID,
            .vertex_format = QU_VERTEX_FORMAT_2XY,
            .render_mode = QU_RENDER_MODE_LINES,
            .first_vertex = graphics__append_vertex_data(QU_VERTEX_FORMAT_2XY, vertices, 4),
            .total_vertices = 2,
        },
    });
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

    unsigned int first_vertex = graphics__append_vertex_data(QU_VERTEX_FORMAT_2XY, vertices, 6);
    
    if (fill_alpha > 0) {
        graphics__append_render_command(&(struct qu__render_command_info) {
            .command = QU__RENDER_COMMAND_DRAW,
            .args.draw = {
                .color = fill,
                .brush = QU_BRUSH_SOLID,
                .vertex_format = QU_VERTEX_FORMAT_2XY,
                .render_mode = QU_RENDER_MODE_TRIANGLES,
                .first_vertex = first_vertex,
                .total_vertices = 3,
            },
        });
    }

    if (outline_alpha > 0) {
        graphics__append_render_command(&(struct qu__render_command_info) {
            .command = QU__RENDER_COMMAND_DRAW,
            .args.draw = {
                .color = outline,
                .brush = QU_BRUSH_SOLID,
                .vertex_format = QU_VERTEX_FORMAT_2XY,
                .render_mode = QU_RENDER_MODE_LINE_LOOP,
                .first_vertex = first_vertex,
                .total_vertices = 3,
            },
        });
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

    unsigned int first_vertex = graphics__append_vertex_data(QU_VERTEX_FORMAT_2XY, vertices, 8);
    
    if (fill_alpha > 0) {
        graphics__append_render_command(&(struct qu__render_command_info) {
            .command = QU__RENDER_COMMAND_DRAW,
            .args.draw = {
                .color = fill,
                .brush = QU_BRUSH_SOLID,
                .vertex_format = QU_VERTEX_FORMAT_2XY,
                .render_mode = QU_RENDER_MODE_TRIANGLE_FAN,
                .first_vertex = first_vertex,
                .total_vertices = 4,
            },
        });
    }

    if (outline_alpha > 0) {
        graphics__append_render_command(&(struct qu__render_command_info) {
            .command = QU__RENDER_COMMAND_DRAW,
            .args.draw = {
                .color = outline,
                .brush = QU_BRUSH_SOLID,
                .vertex_format = QU_VERTEX_FORMAT_2XY,
                .render_mode = QU_RENDER_MODE_LINE_LOOP,
                .first_vertex = first_vertex,
                .total_vertices = 4,
            },
        });
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

    unsigned int first_vertex = graphics__append_vertex_data(QU_VERTEX_FORMAT_2XY,
                                                             vertices, 2 * total_vertices);
    
    if (fill_alpha > 0) {
        graphics__append_render_command(&(struct qu__render_command_info) {
            .command = QU__RENDER_COMMAND_DRAW,
            .args.draw = {
                .color = fill,
                .brush = QU_BRUSH_SOLID,
                .vertex_format = QU_VERTEX_FORMAT_2XY,
                .render_mode = QU_RENDER_MODE_TRIANGLE_FAN,
                .first_vertex = first_vertex,
                .total_vertices = total_vertices,
            },
        });
    }

    if (outline_alpha > 0) {
        graphics__append_render_command(&(struct qu__render_command_info) {
            .command = QU__RENDER_COMMAND_DRAW,
            .args.draw = {
                .color = outline,
                .brush = QU_BRUSH_SOLID,
                .vertex_format = QU_VERTEX_FORMAT_2XY,
                .render_mode = QU_RENDER_MODE_LINE_LOOP,
                .first_vertex = first_vertex,
                .total_vertices = total_vertices,
            },
        });
    }
}

qu_texture qu_create_texture(int width, int height, int channels, unsigned char *fill)
{
    qu_texture_obj texture = {
        .width = width,
        .height = height,
        .channels = channels,
        .pixels = malloc(width * height * channels),
    };

    if (!texture.pixels) {
        return (qu_texture) { 0 };
    }

    if (fill) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                for (int c = 0; c < channels; c++) {
                    texture.pixels[y * width * channels + x * channels + c] = fill[c];
                }
            }
        }
    }

    priv.renderer->load_texture(&texture);

    return (qu_texture) {
        .id = qu_handle_list_add(priv.textures, &texture),
    };
}

qu_texture qu_load_texture(char const *path)
{
    qu_file *file = qu_open_file_from_path(path);

    if (!file) {
        return (qu_texture) { .id = 0 };
    }

    qu_image_loader *loader = qu_open_image_loader(file);

    if (!loader) {
        qu_close_file(file);
        return (qu_texture) { .id = 0 };
    }

    unsigned char *pixels = malloc(loader->width * loader->height * loader->channels);

    if (!pixels) {
        qu_close_image_loader(loader);
        qu_close_file(file);
        return (qu_texture) { .id = 0 };
    }

    qu_result status = qu_image_loader_load(loader, pixels);

    qu_close_image_loader(loader);
    qu_close_file(file);

    if (status != QU_SUCCESS) {
        free(pixels);
        return (qu_texture) { .id = 0 };
    }

    qu_texture_obj texture = {
        .width = loader->width,
        .height = loader->height,
        .channels = loader->channels,
        .pixels = pixels,
    };

    priv.renderer->load_texture(&texture);

    return (qu_texture) {
        .id = qu_handle_list_add(priv.textures, &texture),
    };
}

void qu_delete_texture(qu_texture texture)
{
    qu_handle_list_remove(priv.textures, texture.id);
}

void qu_set_texture_smooth(qu_texture texture, bool smooth)
{
    qu_texture_obj *texture_p = qu_handle_list_get(priv.textures, texture.id);

    if (texture_p) {
        return;
    }

    texture_p->smooth = smooth;
    priv.renderer->set_texture_smooth(texture_p, smooth);
}

void qu_update_texture(qu_texture texture, int x, int y, int w, int h, uint8_t const *pixels)
{
    qu_texture_obj *texture_p = qu_handle_list_get(priv.textures, texture.id);

    if (!texture_p || !texture_p->pixels) {
        return;
    }

    if (w == -1) {
        w = texture_p->width;
    }

    if (h == -1) {
        h = texture_p->height;
    }

    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            size_t nc = texture_p->channels;
            size_t di = (y + i) * texture_p->width * nc + (x + j) * nc;
            size_t si = i * w * nc + j * nc;

            for (size_t k = 0; k < nc; k++) {
                texture_p->pixels[di + k] = pixels[si + k];
            }
        }
    }

    priv.renderer->load_texture(texture_p);
}

void qu_draw_texture(qu_texture texture, float x, float y, float w, float h)
{
    qu_texture_obj *texture_p = qu_handle_list_get(priv.textures, texture.id);

    if (!texture_p) {
        return;
    }

    float const vertices[] = {
        x,      y,      0.f,    0.f,
        x + w,  y,      1.f,    0.f,
        x + w,  y + h,  1.f,    1.f,
        x,      y + h,  0.f,    1.f,
    };

    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_DRAW,
        .args.draw = {
            .texture = texture_p,
            .color = QU_COLOR(255, 255, 255),
            .brush = QU_BRUSH_TEXTURED,
            .vertex_format = QU_VERTEX_FORMAT_4XYST,
            .render_mode = QU_RENDER_MODE_TRIANGLE_FAN,
            .first_vertex = graphics__append_vertex_data(QU_VERTEX_FORMAT_4XYST, vertices, 16),
            .total_vertices = 4,
        },
    });
}

void qu_draw_subtexture(qu_texture texture, float x, float y, float w, float h, float rx, float ry, float rw, float rh)
{
    qu_texture_obj *texture_p = qu_handle_list_get(priv.textures, texture.id);

    if (!texture_p) {
        return;
    }

    float s = rx / texture_p->width;
    float t = ry / texture_p->height;
    float u = rw / texture_p->width;
    float v = rh / texture_p->height;

    float const vertices[] = {
        x,      y,      s,      t,
        x + w,  y,      s + u,  t,
        x + w,  y + h,  s + u,  t + v,
        x,      y + h,  s,      t + v,
    };

    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_DRAW,
        .args.draw = {
            .texture = texture_p,
            .color = QU_COLOR(255, 255, 255),
            .brush = QU_BRUSH_TEXTURED,
            .vertex_format = QU_VERTEX_FORMAT_4XYST,
            .render_mode = QU_RENDER_MODE_TRIANGLE_FAN,
            .first_vertex = graphics__append_vertex_data(QU_VERTEX_FORMAT_4XYST, vertices, 16),
            .total_vertices = 4,
        },
    });
}

void qu_draw_font(qu_texture texture, qu_color color, float const *data, int count)
{
    qu_texture_obj *texture_p = qu_handle_list_get(priv.textures, texture.id);

    if (!texture_p) {
        return;
    }

    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_DRAW,
        .args.draw = {
            .texture = texture_p,
            .color = color,
            .brush = QU_BRUSH_FONT,
            .vertex_format = QU_VERTEX_FORMAT_4XYST,
            .render_mode = QU_RENDER_MODE_TRIANGLES,
            .first_vertex = graphics__append_vertex_data(QU_VERTEX_FORMAT_4XYST, data, count * 4),
            .total_vertices = count,
        },
    });
}

qu_surface qu_create_surface(int width, int height)
{
    qu_surface_obj surface = {
        .texture = {
            .width = width,
            .height = height,
        },
    };

    qu_mat4_ortho(&surface.projection, 0.f, width, height, 0.f);
    qu_mat4_identity(&surface.modelview[0]);

    priv.renderer->create_surface(&surface);

    return (qu_surface) {
        .id = qu_handle_list_add(priv.surfaces, &surface),
    };
}

void qu_delete_surface(qu_surface surface)
{
    qu_handle_list_remove(priv.surfaces, surface.id);
}

void qu_set_surface_smooth(qu_surface surface, bool smooth)
{
    qu_surface_obj *surface_p = qu_handle_list_get(priv.surfaces, surface.id);

    if (surface_p) {
        priv.renderer->set_texture_smooth(&surface_p->texture, smooth);
    }
}

void qu_set_surface_antialiasing_level(qu_surface surface, int level)
{
    qu_surface_obj *surface_p = qu_handle_list_get(priv.surfaces, surface.id);

    if (surface_p) {
        priv.renderer->set_surface_antialiasing_level(surface_p, level);
    }
}

void qu_set_surface(qu_surface surface)
{
    qu_surface_obj *surface_p = qu_handle_list_get(priv.surfaces, surface.id);

    if (!surface_p) {
        return;
    }

    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_SET_SURFACE,
        .args.surface.surface = surface_p,
    });
}

void qu_reset_surface(void)
{
    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_SET_SURFACE,
        .args.surface.surface = priv.canvas_enabled ? &priv.canvas : &priv.display,
    });
}

void qu_draw_surface(qu_surface surface, float x, float y, float w, float h)
{
    qu_surface_obj *surface_p = qu_handle_list_get(priv.surfaces, surface.id);

    if (!surface_p) {
        return;
    }

    float const vertices[] = {
        x,      y,      0.f,    0.f,
        x + w,  y,      1.f,    0.f,
        x + w,  y + h,  1.f,    1.f,
        x,      y + h,  0.f,    1.f,
    };

    graphics__append_render_command(&(struct qu__render_command_info) {
        .command = QU__RENDER_COMMAND_DRAW,
        .args.draw = {
            .texture = &surface_p->texture,
            .color = QU_COLOR(255, 255, 255),
            .brush = QU_BRUSH_TEXTURED,
            .vertex_format = QU_VERTEX_FORMAT_4XYST,
            .render_mode = QU_RENDER_MODE_TRIANGLE_FAN,
            .first_vertex = graphics__append_vertex_data(QU_VERTEX_FORMAT_4XYST, vertices, 16),
            .total_vertices = 4,
        },
    });
}
