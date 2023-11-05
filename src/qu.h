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
// qu.h: private header file
//------------------------------------------------------------------------------

#ifndef QU_PRIVATE_H_INC
#define QU_PRIVATE_H_INC

//------------------------------------------------------------------------------

#if !defined(QU_MODULE)
#define QU_MODULE               "?"__FILE__
#endif

//------------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(_UNICODE)
#define _UNICODE
#endif

#if !defined(UNICODE)
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN

#endif // defined(_WIN32)

//------------------------------------------------------------------------------

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <libqu.h>

#if defined(_WIN32)
#include <windows.h>
#endif

//------------------------------------------------------------------------------

#define QU_FILE_NAME_LENGTH     (256)

#if defined(NDEBUG)
#define QU_LOGD(...)
#else
#define QU_LOGD(...)    qu_log_printf(QU_LOG_LEVEL_DEBUG, QU_MODULE, __VA_ARGS__)
#endif

#define QU_LOGI(...)    qu_log_printf(QU_LOG_LEVEL_INFO, QU_MODULE, __VA_ARGS__)
#define QU_LOGW(...)    qu_log_printf(QU_LOG_LEVEL_WARNING, QU_MODULE, __VA_ARGS__)
#define QU_LOGE(...)    qu_log_printf(QU_LOG_LEVEL_ERROR, QU_MODULE, __VA_ARGS__)

#define QU_HALT(...) \
    do { \
        qu_log_printf(QU_LOG_LEVEL_ERROR, QU_MODULE, __VA_ARGS__); \
        abort(); \
    } while (0);

#define QU_HALT_IF(condition) \
    do { \
        if (condition) { \
            qu_log_printf(QU_LOG_LEVEL_ERROR, QU_MODULE, "QU_HALT_IF: %s\n", #condition); \
            abort(); \
        } \
    } while (0);

#define QU_ARRAY_SIZE(array) \
    sizeof(array) / sizeof(array[0])

#define QU_ALLOC_ARRAY(ptr, size) \
    ptr = pl_malloc(sizeof(*(ptr)) * (size))

//------------------------------------------------------------------------------

typedef enum qu_result
{
    QU_FAILURE = -1,
    QU_SUCCESS = 0,
} qu_result;

typedef enum qu_log_level
{
    QU_LOG_LEVEL_DEBUG,
    QU_LOG_LEVEL_INFO,
    QU_LOG_LEVEL_WARNING,
    QU_LOG_LEVEL_ERROR,
} qu_log_level;

typedef enum qu_file_source
{
    QU_FILE_SOURCE_STANDARD,
    QU_FILE_SOURCE_ANDROID_ASSET,
    QU_FILE_SOURCE_MEMORY_BUFFER,
    QU_TOTAL_FILE_SOURCES,
} qu_file_source;

typedef enum qu_image_loader_format
{
    QU_IMAGE_LOADER_STBI,
    QU_TOTAL_IMAGE_LOADERS,
} qu_image_loader_format;

typedef enum qu_audio_loader_format
{
    QU_AUDIO_LOADER_WAVE,
    QU_AUDIO_LOADER_VORBIS,
    QU_TOTAL_AUDIO_LOADERS,
} qu_audio_loader_format;

typedef enum qu_event_type
{
    QU_EVENT_TYPE_INVALID = -1,
    QU_EVENT_TYPE_KEY_PRESSED,
    QU_EVENT_TYPE_KEY_RELEASED,
    QU_EVENT_TYPE_MOUSE_BUTTON_PRESSED,
    QU_EVENT_TYPE_MOUSE_BUTTON_RELEASED,
    QU_EVENT_TYPE_MOUSE_CURSOR_MOVED,
    QU_EVENT_TYPE_MOUSE_WHEEL_SCROLLED,
    QU_EVENT_TYPE_ACTIVATE,
    QU_EVENT_TYPE_DEACTIVATE,
    QU_EVENT_TYPE_TOUCH_STARTED,
    QU_EVENT_TYPE_TOUCH_ENDED,
    QU_EVENT_TYPE_TOUCH_MOVED,
} qu_event_type;

typedef enum qu_render_mode
{
    QU_RENDER_MODE_POINTS,
    QU_RENDER_MODE_LINES,
    QU_RENDER_MODE_LINE_LOOP,
    QU_RENDER_MODE_LINE_STRIP,
    QU_RENDER_MODE_TRIANGLES,
    QU_RENDER_MODE_TRIANGLE_STRIP,
    QU_RENDER_MODE_TRIANGLE_FAN,
    QU_TOTAL_RENDER_MODES,
} qu_render_mode;

typedef enum qu_vertex_attribute
{
    QU_VERTEX_ATTRIBUTE_POSITION,
    QU_VERTEX_ATTRIBUTE_COLOR,
    QU_VERTEX_ATTRIBUTE_TEXCOORD,
    QU_TOTAL_VERTEX_ATTRIBUTES,
} qu_vertex_attribute;

typedef enum qu_vertex_attribute_bits
{
    QU_VERTEX_ATTRIBUTE_BIT_POSITION = (1 << QU_VERTEX_ATTRIBUTE_POSITION),
    QU_VERTEX_ATTRIBUTE_BIT_COLOR = (1 << QU_VERTEX_ATTRIBUTE_COLOR),
    QU_VERTEX_ATTRIBUTE_BIT_TEXCOORD = (1 << QU_VERTEX_ATTRIBUTE_TEXCOORD),
} qu_vertex_attribute_bits;

typedef enum qu_vertex_format
{
    QU_VERTEX_FORMAT_2XY,
    QU_VERTEX_FORMAT_4XYST,
    QU_TOTAL_VERTEX_FORMATS,
} qu_vertex_format;

typedef enum qu_brush
{
    QU_BRUSH_SOLID, // single color
    QU_BRUSH_TEXTURED, // textured
    QU_BRUSH_FONT,
    QU_TOTAL_BRUSHES,
} qu_brush;

typedef struct pl_thread pl_thread;
typedef struct pl_mutex pl_mutex;
typedef struct qu_handle_list qu_handle_list;

typedef struct qu_mat4
{
    float m[16];
} qu_mat4;

typedef struct qu_file {
    qu_file_source source;

    char name[QU_FILE_NAME_LENGTH];
    size_t size;

    void *context;
} qu_file;

typedef struct qu_image_loader
{
    qu_image_loader_format format;

    int width;
    int height;
    int channels;

    qu_file *file;
    void *context;
} qu_image_loader;

typedef struct qu_audio_loader
{
    qu_audio_loader_format format;

    int16_t num_channels;
    int64_t num_samples;
    int64_t sample_rate;

    qu_file *file;
    void *context;
} qu_audio_loader;

typedef struct qu_keyboard_event
{
    qu_key key;
} qu_keyboard_event;

typedef struct qu_mouse_event
{
    qu_mouse_button button;
    int x_cursor;
    int y_cursor;
    int dx_wheel;
    int dy_wheel;
} qu_mouse_event;

typedef struct qu_touch_event
{
    int index;
    int x;
    int y;
} qu_touch_event;

typedef struct qu_event
{
    qu_event_type type;

    union {
        qu_keyboard_event keyboard;
        qu_mouse_event mouse;
        qu_touch_event touch;
    } data;
} qu_event;

typedef struct qu_texture_obj
{
    int width;
    int height;
    int channels;
    unsigned char *pixels;
    uintptr_t priv[4];
    bool smooth;
} qu_texture_obj;

typedef struct qu_surface_obj
{
    qu_texture_obj texture;

    qu_mat4 projection;
    qu_mat4 modelview[32];
    int modelview_index;

    int sample_count;

    uintptr_t priv[4];
} qu_surface_obj;

typedef struct qu_audio_buffer
{
    int16_t *data;      // sample data
    size_t samples;     // number of samples
    intptr_t priv[4];   // implementation-specific data
} qu_audio_buffer;

typedef struct qu_audio_source
{
    int channels;       // number of channels (1 or 2)
    int sample_rate;    // sample rate (usually 44100)
    int loop;           // should this source loop (-1 if yes)
    intptr_t priv[4];   // implementation-specific data
} qu_audio_source;

//------------------------------------------------------------------------------

typedef struct qu_core_impl
{
    void (*initialize)(qu_params const *params);
    void (*terminate)(void);
    bool (*process_input)(void);
    void (*swap_buffers)(void);

    char const *(*get_graphics_context_name)(void);

    void *(*gl_proc_address)(char const *name);
    int (*get_gl_multisample_samples)(void);

    bool (*set_window_title)(char const *title);
    bool (*set_window_size)(int width, int height);
} qu_core_impl;

typedef struct qu_joystick_impl
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
} qu_joystick_impl;

typedef struct qu_renderer_impl
{
    bool (*query)(qu_params const *params);
    void (*initialize)(qu_params const *params);
    void (*terminate)(void);

    void (*upload_vertex_data)(qu_vertex_format vertex_format, float const *data, size_t size);

    void (*apply_projection)(qu_mat4 const *projection);
    void (*apply_transform)(qu_mat4 const *transform);
    void (*apply_surface)(qu_surface_obj const *surface);
    void (*apply_texture)(qu_texture_obj const *texture);
    void (*apply_clear_color)(qu_color clear_color);
    void (*apply_draw_color)(qu_color draw_color);
    void (*apply_brush)(qu_brush brush);
    void (*apply_vertex_format)(qu_vertex_format vertex_format);
    void (*apply_blend_mode)(qu_blend_mode mode);

    void (*exec_resize)(int width, int height);
    void (*exec_clear)(void);
    void (*exec_draw)(qu_render_mode render_mode, unsigned int first_vertex, unsigned int total_vertices);

    void (*load_texture)(qu_texture_obj *texture);
    void (*unload_texture)(qu_texture_obj *texture);
    void (*set_texture_smooth)(qu_texture_obj *texture, bool smooth);

    void (*create_surface)(qu_surface_obj *surface);
    void (*destroy_surface)(qu_surface_obj *surface);
    void (*set_surface_antialiasing_level)(qu_surface_obj *surface, int level);
} qu_renderer_impl;

typedef struct qu_audio_impl
{
    int (*check)(qu_params const *params);
    int (*initialize)(qu_params const *params);
    void (*terminate)(void);

    void (*set_master_volume)(float volume);

    int (*create_source)(qu_audio_source *source);
    void (*destroy_source)(qu_audio_source *source);
    bool (*is_source_used)(qu_audio_source *source);
    int (*queue_buffer)(qu_audio_source *source, qu_audio_buffer *buffer);
    int (*get_queued_buffers)(qu_audio_source *source);
    int (*start_source)(qu_audio_source *source);
    int (*stop_source)(qu_audio_source *source);
} qu_audio_impl;

//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------

extern qu_core_impl const qu_android_core_impl;
extern qu_core_impl const qu_emscripten_core_impl;
extern qu_core_impl const qu_win32_core_impl;
extern qu_core_impl const qu_x11_core_impl;

extern qu_joystick_impl const qu_null_joystick_impl;
extern qu_joystick_impl const qu_win32_joystick_impl;
extern qu_joystick_impl const qu_linux_joystick_impl;

extern qu_renderer_impl const qu_es2_renderer_impl;
extern qu_renderer_impl const qu_gl1_renderer_impl;
extern qu_renderer_impl const qu_gl3_renderer_impl;
extern qu_renderer_impl const qu_null_renderer_impl;

extern qu_audio_impl const qu_null_audio_impl;
extern qu_audio_impl const qu_openal_audio_impl;
extern qu_audio_impl const qu_xaudio2_audio_impl;
extern qu_audio_impl const qu_sles_audio_impl;

//------------------------------------------------------------------------------

void pl_initialize(void);
void pl_terminate(void);
void *pl_malloc(size_t size);
void *pl_calloc(size_t count, size_t size);
void *pl_realloc(void *data, size_t size);
void pl_free(void *data);
pl_thread *pl_create_thread(char const *name, intptr_t (*func)(void *), void *arg);
void pl_detach_thread(pl_thread *thread);
intptr_t pl_wait_thread(pl_thread *thread);
pl_mutex *pl_create_mutex(void);
void pl_destroy_mutex(pl_mutex *mutex);
void pl_lock_mutex(pl_mutex *mutex);
void pl_unlock_mutex(pl_mutex *mutex);
void pl_sleep(double seconds);
void *pl_open_dll(char const *path);
void pl_close_dll(void *dll);
void *pl_get_dll_proc(void *dll, char const *name);

void qu_mat4_identity(qu_mat4 *mat);
void qu_mat4_copy(qu_mat4 *dst, qu_mat4 const *src);
void qu_mat4_multiply(qu_mat4 *a, qu_mat4 const *b);
void qu_mat4_ortho(qu_mat4 *mat, float l, float r, float b, float t);
void qu_mat4_translate(qu_mat4 *mat, float x, float y, float z);
void qu_mat4_scale(qu_mat4 *mat, float x, float y, float z);
void qu_mat4_rotate(qu_mat4 *mat, float rad, float x, float y, float z);
void qu_mat4_inverse(qu_mat4 *dst, qu_mat4 const *src);
qu_vec2f qu_mat4_transform_point(qu_mat4 const *mat, qu_vec2f p);

void qu_log_puts(qu_log_level level, char const *tag, char const *str);
void qu_log_printf(qu_log_level level, char const *tag, char const *fmt, ...);

char *qu_strdup(char const *str);

qu_handle_list *qu_create_handle_list(size_t element_size, void (*dtor)(void *));
void qu_destroy_handle_list(qu_handle_list *list);
int32_t qu_handle_list_add(qu_handle_list *list, void *data);
void qu_handle_list_remove(qu_handle_list *list, int32_t id);
void *qu_handle_list_get(qu_handle_list *list, int32_t id);
void *qu_handle_list_get_first(qu_handle_list *list);
void *qu_handle_list_get_next(qu_handle_list *list);

qu_file *qu_open_file_from_path(char const *path);
qu_file *qu_open_file_from_buffer(void const *data, size_t size);
void qu_close_file(qu_file *file);
int64_t qu_file_read(void *buffer, size_t size, qu_file *file);
int64_t qu_file_tell(qu_file *file);
int64_t qu_file_seek(qu_file *file, int64_t offset, int origin);

qu_image_loader *qu_open_image_loader(qu_file *file);
void qu_close_image_loader(qu_image_loader *loader);
qu_result qu_image_loader_load(qu_image_loader *loader, unsigned char *pixels);

qu_audio_loader *qu_open_audio_loader(qu_file *file);
void qu_close_audio_loader(qu_audio_loader *loader);
int64_t qu_audio_loader_read(qu_audio_loader *loader, int16_t *samples, int64_t max_samples);
int64_t qu_audio_loader_seek(qu_audio_loader *loader, int64_t sample_offset);

void qu_initialize_core(qu_params const *params);
void qu_terminate_core(void);
bool qu_handle_events(void);
void qu_swap_buffers(void);
char const *qu_get_graphics_context_name(void);
void *qu_gl_get_proc_address(char const *name);
int qu_gl_get_samples(void);
void qu_enqueue_event(qu_event const *event);

void qu_initialize_graphics(qu_params const *params);
void qu_terminate_graphics(void);
void qu_flush_graphics(void);
void qu_event_context_lost(void);
void qu_event_context_restored(void);
void qu_event_window_resize(int width, int height);
qu_vec2i qu_convert_window_pos_to_canvas_pos(qu_vec2i position);
qu_vec2i qu_convert_window_delta_to_canvas_delta(qu_vec2i position);
qu_texture qu_create_texture(int w, int h, int channels, unsigned char *fill);
void qu_update_texture(qu_texture texture, int x, int y, int w, int h, uint8_t const *pixels);
void qu_draw_font(qu_texture texture, qu_color color, float const *data, int count);

void qu_initialize_text(void);
void qu_terminate_text(void);

void qu_initialize_audio(qu_params const *params);
void qu_terminate_audio(void);

#if defined(ANDROID)

void np_android_write_log(int level, char const *tag, char const *message);
void *np_android_asset_open(char const *path);
void np_android_asset_close(void *asset);
int64_t np_android_asset_read(void *dst, size_t size, void *asset);
int64_t np_android_asset_tell(void *asset);
int64_t np_android_asset_seek(void *asset, int64_t offset, int whence);

#endif // defined(ANDROID)

//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif // QU_PRIVATE_H_INC
