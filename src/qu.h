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

#ifndef QU_PRIVATE_H_INC
#define QU_PRIVATE_H_INC

//------------------------------------------------------------------------------
// qu.h: private header file
//------------------------------------------------------------------------------

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <libqu.h>

#ifdef _WIN32

#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#endif // _WIN32

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Math

typedef struct
{
    float m[16];
} qu_mat4;

void qu_mat4_identity(qu_mat4 *mat);
void qu_mat4_copy(qu_mat4 *dst, qu_mat4 const *src);
void qu_mat4_multiply(qu_mat4 *a, qu_mat4 const *b);
void qu_mat4_ortho(qu_mat4 *mat, float l, float r, float b, float t);
void qu_mat4_translate(qu_mat4 *mat, float x, float y, float z);
void qu_mat4_scale(qu_mat4 *mat, float x, float y, float z);
void qu_mat4_rotate(qu_mat4 *mat, float rad, float x, float y, float z);
void qu_mat4_inverse(qu_mat4 *dst, qu_mat4 const *src);
qu_vec2f qu_mat4_transform_point(qu_mat4 const *mat, qu_vec2f p);

//------------------------------------------------------------------------------
// Util

#define QU__ARRAY_SIZE(_array) \
    sizeof(_array) / sizeof(_array[0])

#define QU__ALLOC_ARRAY(_ptr, _size) \
    _ptr = malloc(sizeof(*_ptr) * _size)

char *qu_strdup(char const *str);
void qu_make_circle(float x, float y, float radius, float *data, int num_verts);

//------------------------------------------------------------------------------
// Log

#ifndef QU_MODULE
#   define QU_MODULE    "?"__FILE__
#endif

typedef enum
{
    QU_LOG_DEBUG,
    QU_LOG_INFO,
    QU_LOG_WARNING,
    QU_LOG_ERROR,
} qu_log;

void qu_log_printf(qu_log level, char const *module, char const *fmt, ...);

#ifdef NDEBUG
#define QU_DEBUG(...)
#else
#define QU_DEBUG(...) \
    qu_log_printf(QU_LOG_DEBUG, QU_MODULE, __VA_ARGS__)
#endif

#define QU_INFO(...) \
    qu_log_printf(QU_LOG_INFO, QU_MODULE, __VA_ARGS__)

#define QU_WARNING(...) \
    qu_log_printf(QU_LOG_WARNING, QU_MODULE, __VA_ARGS__)

#define QU_ERROR(...) \
    qu_log_printf(QU_LOG_ERROR, QU_MODULE, __VA_ARGS__)

//------------------------------------------------------------------------------
// Halt

#define QU_HALT(...) \
    do { \
        qu_log_printf(QU_LOG_ERROR, QU_MODULE, __VA_ARGS__); \
        abort(); \
    } while (0);

#define QU_HALT_IF(_condition) \
    do { \
        if (_condition) { \
            qu_log_printf(QU_LOG_ERROR, QU_MODULE, "QU_HALT_IF: %s\n", #_condition); \
            abort(); \
        } \
    } while (0);

//------------------------------------------------------------------------------
// Array

typedef struct qu_array qu_array;

qu_array *qu_create_array(size_t element_size, void (*dtor)(void *));
void qu_destroy_array(qu_array *array);

int32_t qu_array_add(qu_array *array, void *data);
void qu_array_remove(qu_array *array, int32_t id);
void *qu_array_get(qu_array *array, int32_t id);

//------------------------------------------------------------------------------
// FS

typedef struct qu_file qu_file;

qu_file *qu_fopen(char const *path);
qu_file *qu_mopen(void const *buffer, size_t size);
void qu_fclose(qu_file *file);
int64_t qu_fread(void *buffer, size_t size, qu_file *file);
int64_t qu_ftell(qu_file *file);
int64_t qu_fseek(qu_file *file, int64_t offset, int origin);
size_t qu_file_size(qu_file *file);
char const *qu_file_repr(qu_file *file);

//------------------------------------------------------------------------------
// Image loader

struct qu__image
{
    int width;
    int height;
    int channels;
    unsigned char *pixels;
};

void qu__image_create(struct qu__image *image);
void qu__image_load(struct qu__image *image, qu_file *file);
void qu__image_delete(struct qu__image *image);

typedef struct qu__image qu_image;
qu_image *qu_load_image(qu_file *file);
void qu_delete_image(qu_image *image);

//------------------------------------------------------------------------------
// Sound reader

typedef struct
{
    int16_t num_channels;
    int64_t num_samples;
    int64_t sample_rate;
    qu_file *file;
    void *data;
} qu_sound_reader;

qu_sound_reader *qu_open_sound_reader(qu_file *file);
void qu_close_sound_reader(qu_sound_reader *reader);
int64_t qu_sound_read(qu_sound_reader *reader, int16_t *samples, int64_t max_samples);
void qu_sound_seek(qu_sound_reader *reader, int64_t sample_offset);

//------------------------------------------------------------------------------
// Platform

typedef struct qu_thread qu_thread;
typedef struct qu_mutex qu_mutex;
typedef intptr_t (*qu_thread_func)(void *);
typedef void *qu__library;
typedef void *qu__procedure;

void qu_platform_initialize(void);
void qu_platform_terminate(void);

qu_thread *qu_create_thread(char const *name, qu_thread_func func, void *arg);
void qu_detach_thread(qu_thread *thread);
intptr_t qu_wait_thread(qu_thread *thread);

qu_mutex *qu_create_mutex(void);
void qu_destroy_mutex(qu_mutex *mutex);
void qu_lock_mutex(qu_mutex *mutex);
void qu_unlock_mutex(qu_mutex *mutex);

void qu_sleep(double seconds);

qu__library qu__platform_open_library(char const *path);
void qu__platform_close_library(qu__library library);
qu__procedure qu__platform_get_procedure(qu__library library, char const *name);

//------------------------------------------------------------------------------
// Core

enum qu__renderer
{
    QU__RENDERER_NULL,
    QU__RENDERER_GL_COMPAT,
    QU__RENDERER_GL_CORE,
    QU__RENDERER_ES2,
};

struct qu__core
{
    void (*initialize)(qu_params const *params);
    void (*terminate)(void);
    bool (*process)(void);
    void (*present)(void);

    enum qu__renderer (*get_renderer)(void);

    bool (*gl_check_extension)(char const *name);
    void *(*gl_proc_address)(char const *name);

    bool (*is_joystick_connected)(int joystick);
    char const *(*get_joystick_id)(int joystick);
    int (*get_joystick_button_count)(int joystick);
    int (*get_joystick_axis_count)(int joystick);
    char const *(*get_joystick_button_id)(int joystick, int button);
    char const *(*get_joystick_axis_id)(int joystick, int axis);
    bool (*is_joystick_button_pressed)(int joystick, int button);
    float (*get_joystick_axis_value)(int joystick, int axis);
};

extern struct qu__core const qu__core_emscripten;
extern struct qu__core const qu__core_win32;
extern struct qu__core const qu__core_x11;

struct qu__joystick
{
    void (*initialize)(qu_params const *params);
    void (*terminate)(void);
    void (*process)(void);
    bool (*is_connected)(int id);
    char const *(*get_name)(int id);
    int (*get_button_count)(int id);
    int (*get_axis_count)(int id);
    char const *(*get_button_name)(int id, int button);
    char const *(*get_axis_name)(int id, int axis);
    bool (*is_button_pressed)(int id, int button);
    float (*get_axis_value)(int id, int axis);
};

extern struct qu__joystick const qu__joystick_null;
extern struct qu__joystick const qu__joystick_win32;
extern struct qu__joystick const qu__joystick_linux;

void qu__core_initialize(qu_params const *params);
void qu__core_terminate(void);
bool qu__core_process(void);
void qu__core_present(void);
enum qu__renderer qu__core_get_renderer(void);
void *qu__core_get_gl_proc_address(char const *name);
void qu__core_on_key_pressed(qu_key key);
void qu__core_on_key_released(qu_key key);
void qu__core_on_mouse_button_pressed(qu_mouse_button button);
void qu__core_on_mouse_button_released(qu_mouse_button button);
void qu__core_on_mouse_cursor_moved(int x, int y);
void qu__core_on_mouse_wheel_scrolled(int dx, int dy);

//------------------------------------------------------------------------------
// Graphics

enum qu__render_mode
{
    QU__RENDER_MODE_POINTS,
    QU__RENDER_MODE_LINES,
    QU__RENDER_MODE_LINE_STRIP,
    QU__RENDER_MODE_LINE_LOOP,
    QU__RENDER_MODE_TRIANGLES,
    QU__RENDER_MODE_TRIANGLE_STRIP,
    QU__RENDER_MODE_TRIANGLE_FAN,
    QU__TOTAL_RENDER_MODES,
};

enum qu__vertex_attribute
{
    QU__VERTEX_ATTRIBUTE_POSITION,
    QU__VERTEX_ATTRIBUTE_COLOR,
    QU__VERTEX_ATTRIBUTE_TEXCOORD,
    QU__TOTAL_VERTEX_ATTRIBUTES,
};

enum qu__vertex_format
{
    QU__VERTEX_FORMAT_SOLID,
    QU__VERTEX_FORMAT_TEXTURED,
    QU__TOTAL_VERTEX_FORMATS,
};

struct qu__texture
{
    struct qu__image image;
    uintptr_t priv[4];
};

struct qu__surface
{
    struct qu__texture texture;

    qu_mat4 projection;
    qu_mat4 modelview[32];
    int modelview_index;

    uintptr_t priv[4];
};

struct qu__renderer_impl
{
    bool (*query)(qu_params const *params);
    void (*initialize)(qu_params const *params);
    void (*terminate)(void);

    void (*upload_vertex_data)(enum qu__vertex_format vertex_format, float const *data, size_t size);

    void (*apply_projection)(qu_mat4 const *projection);
    void (*apply_transform)(qu_mat4 const *transform);
    void (*apply_surface)(struct qu__surface const *surface);
    void (*apply_texture)(struct qu__texture const *texture);
    void (*apply_clear_color)(qu_color clear_color);
    void (*apply_draw_color)(qu_color draw_color);
    void (*apply_vertex_format)(enum qu__vertex_format vertex_format);

    void (*exec_resize)(int width, int height);
    void (*exec_clear)(void);
    void (*exec_draw)(enum qu__render_mode render_mode, unsigned int first_vertex, unsigned int total_vertices);

    void (*load_texture)(struct qu__texture *texture);
    void (*unload_texture)(struct qu__texture *texture);
    void (*set_texture_smooth)(struct qu__texture *texture, bool smooth);

    void (*create_surface)(struct qu__surface *surface);
    void (*destroy_surface)(struct qu__surface *surface);
};

extern struct qu__renderer_impl const qu__renderer_es2;
extern struct qu__renderer_impl const qu__renderer_gl1;
extern struct qu__renderer_impl const qu__renderer_gl3;
extern struct qu__renderer_impl const qu__renderer_null;

void qu__graphics_initialize(qu_params const *params);
void qu__graphics_terminate(void);
void qu__graphics_refresh(void);
void qu__graphics_swap(void);
void qu__graphics_on_display_resize(int width, int height);
qu_vec2i qu__graphics_conv_cursor(qu_vec2i position);
qu_vec2i qu__graphics_conv_cursor_delta(qu_vec2i position);
int32_t qu__graphics_create_texture(int w, int h, int channels);
void qu__graphics_update_texture(int32_t texture_id, int x, int y, int w, int h, uint8_t const *pixels);
void qu__graphics_draw_text(int32_t texture_id, qu_color color, float const *data, int count);
void qu__graphics_set_view(float x, float y, float w, float h, float rotation);
void qu__graphics_reset_view(void);
void qu__graphics_push_matrix(void);
void qu__graphics_pop_matrix(void);
void qu__graphics_translate(float x, float y);
void qu__graphics_scale(float x, float y);
void qu__graphics_rotate(float degrees);
void qu__graphics_clear(qu_color color);
void qu__graphics_draw_point(float x, float y, qu_color color);
void qu__graphics_draw_line(float ax, float ay, float bx, float by, qu_color color);
void qu__graphics_draw_triangle(float ax, float ay, float bx, float by, float cx, float cy, qu_color outline, qu_color fill);
void qu__graphics_draw_rectangle(float x, float y, float w, float h, qu_color outline, qu_color fill);
void qu__graphics_draw_circle(float x, float y, float radius, qu_color outline, qu_color fill);
int32_t qu__graphics_load_texture(qu_file *file);
void qu__graphics_delete_texture(int32_t id);
void qu__graphics_set_texture_smooth(int32_t id, bool smooth);
void qu__graphics_draw_texture(int32_t id, float x, float y, float w, float h);
void qu__graphics_draw_subtexture(int32_t id, float x, float y, float w, float h, float rx, float ry, float rw, float rh);

//------------------------------------------------------------------------------
// Text

void qu_initialize_text(void);
void qu_terminate_text(void);

//------------------------------------------------------------------------------
// Audio

struct qu__audio
{
    bool (*query)(qu_params const *params);
    void (*initialize)(qu_params const *params);
    void (*terminate)(void);

    void (*set_master_volume)(float volume);

    int32_t (*load_sound)(qu_file *file);
    void (*delete_sound)(int32_t sound_id);
    int32_t (*play_sound)(int32_t sound_id);
    int32_t (*loop_sound)(int32_t sound_id);

    int32_t (*open_music)(qu_file *file);
    void (*close_music)(int32_t music_id);
    int32_t (*play_music)(int32_t music_id);
    int32_t (*loop_music)(int32_t music_id);

    void (*pause_stream)(int32_t stream_id);
    void (*unpause_stream)(int32_t stream_id);
    void (*stop_stream)(int32_t stream_id);
};

extern struct qu__audio const qu__audio_null;
extern struct qu__audio const qu__audio_openal;
extern struct qu__audio const qu__audio_xaudio2;

void qu__audio_initialize(qu_params const *params);
void qu__audio_terminate(void);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif // QU_PRIVATE_H_INC
