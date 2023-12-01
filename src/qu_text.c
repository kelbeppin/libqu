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
// qu_text.c: Text rendering module
//------------------------------------------------------------------------------

#include <hb-ft.h>
#include <stb_ds.h>

#include "qu_fs.h"
#include "qu_graphics.h"
#include "qu_log.h"
#include "qu_platform.h"
#include "qu_text.h"

//------------------------------------------------------------------------------

#define INITIAL_VERTEX_BUFFER_SIZE  256
#define INITIAL_INDEX_BUFFER_SIZE   256

//------------------------------------------------------------------------------

struct text_calculate_state
{
    float width;
    float height;
};

struct text_draw_state
{
    float x_current;
    float y_current;
    float *vertices;
    int count;
    qu_color color;
};

struct atlas
{
    qu_texture texture;             // texture identifier
    int width;                      // texture width
    int height;                     // texture height
    int cursor_x;                   // x position where next glyph will be added
    int cursor_y;                   // y position where next glyph will be added
    int line_height;                // max height of current line in the atlas
    int x_padding;                  // min x padding between glyphs
    int y_padding;                  // min y padding between glyphs
};

struct glyph
{
    unsigned long key;              // glyph codepoint (unicode?)
    int s0, t0;                     // top-left position in atlas
    int s1, t1;                     // bottom-right position in atlas
    float x_advance;                // how much x to add after printing the glyph
    float y_advance;                // how much y to add (usually 0)
    int x_bearing;                  // additional x offset
    int y_bearing;                  // additional y offset (?)
};

struct font
{
    int32_t key;                    // identifier
    hb_font_t *font;                // harfbuzz font handle
    FT_Face face;                   // FreeType font handle
    FT_StreamRec stream;            // FreeType font handle
    struct atlas atlas;             // texture atlas (TODO: multiple atlases)
    struct glyph *glyphs;           // dynamic array of glyphs
    float height;                   // general height of font (glyphs may be taller)
};

static struct
{
    bool initialized;
    FT_Library freetype;            // FreeType object
    struct font *fonts;             // dynamic array of font objects
    int font_count;                 // font id counter
    float *vertex_buffer;           // dynamic array of vertex data
    int vertex_buffer_size;         // size of vertex data
    qu_color text_color;            // basic color
    qu_color outline_color;         // outline color (not implemented yet)
} impl;

//------------------------------------------------------------------------------

/**
 * io() callback for FreeType stream.
 */
static unsigned long ft_stream_io(FT_Stream stream, unsigned long offset,
                                  unsigned char *buffer, unsigned long count)
{
    if (qu_file_seek(stream->descriptor.pointer, offset, SEEK_SET) == -1) {
        if (count == 0) {
            return 1;
        }

        return 0;
    }

    return qu_file_read(buffer, count, stream->descriptor.pointer);
}

/**
 * close() callback for FreeType stream.
 */
static void ft_stream_close(FT_Stream stream)
{
    // qu_close_file(stream->descriptor.pointer);
}

static FT_Face open_ft_face(FT_Stream stream, float pt)
{
    FT_Face face;

    FT_Error error = FT_Open_Face(impl.freetype, &(FT_Open_Args) {
        .flags = FT_OPEN_STREAM,
        .stream = stream,
    }, 0, &face);

    if (error) {
        return NULL;
    }

    FT_Set_Char_Size(face, 0, (int) (pt * 64.0f), 0, 0);

    return face;
}

static qu_result create_atlas(struct atlas *atlas, float pt)
{
    int width = 4096;
    int height = 16;

    while (height < (pt * 4)) {
        height *= 2;
    }

    qu_texture texture = qu_create_texture(width, height, 2);

    if (!texture.id) {
        return QU_FAILURE;
    }

    qu_set_texture_smooth(texture, true);

    atlas->texture = texture;
    atlas->width = width;
    atlas->height = height;
    atlas->x_padding = 4;
    atlas->y_padding = 4;
    atlas->cursor_x = atlas->x_padding;
    atlas->cursor_y = atlas->y_padding;
    atlas->line_height = 0;

    return QU_SUCCESS;
}

static int32_t open_font(qu_file *file, float pt)
{
    // FreeType requires that FT_Stream is always available while
    // the font is open.
    // Thus, we first need to put font object (which holds FT_Stream)
    // to the hashmap so that it always stored there.

    int32_t id = ++impl.font_count;

    hmputs(impl.fonts, ((struct font) {
        .key = id,
        .stream = {
            .base = NULL,
            .size = file->size,
            .pos = qu_file_tell(file),
            .descriptor.pointer = file,
            .pathname.pointer = (void *) file->name,
            .read = ft_stream_io,
            .close = ft_stream_close,
        },
    }));

    struct font *font = hmgetp(impl.fonts, id);

    font->face = open_ft_face(&font->stream, pt);

    if (!font->face) {
        QU_LOGE("Failed to open font %s.\n", file->name);
        hmdel(impl.fonts, id);
        return 0;
    }

    font->font = hb_ft_font_create_referenced(font->face);

    if (!font->font) {
        FT_Done_Face(font->face);
        hmdel(impl.fonts, id);
        return 0;
    }

    FT_F26Dot6 ascender = font->face->size->metrics.ascender;
    FT_F26Dot6 descender = font->face->size->metrics.descender;

    font->height = (ascender - descender) / 64.0f;

    if (create_atlas(&font->atlas, pt) != QU_SUCCESS) {
        hb_font_destroy(font->font);
        hmdel(impl.fonts, id);
        return 0;
    }

    return id;
}

static void close_font(struct font *font)
{
    if (!font) {
        return;
    }

    hmfree(font->glyphs);
    qu_delete_texture(font->atlas.texture);

    hb_font_destroy(font->font);
    qu_close_file(font->stream.descriptor.pointer);
}

/**
 * Attempts to make texture atlas larger.
 */
static bool grow_atlas(struct atlas *atlas)
{
    atlas->height *= 2;

    qu_resize_texture(atlas->texture, atlas->width, atlas->height);

    return true;
}

/**
 * Convert 8-bit B/W bitmap to 16-bit bitmap with alpha channel.
 */
static unsigned char *conv_8bit_to_16bit(unsigned char *bitmap8, int width, int height)
{
    unsigned char *bitmap16 = pl_calloc(sizeof(*bitmap16), width * height * 2);

    if (!bitmap16) {
        return NULL;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            bitmap16[y * width * 2 + x * 2 + 0] = 255;
            bitmap16[y * width * 2 + x * 2 + 1] = bitmap8[y * width + x];
        }
    }

    return bitmap16;
}

/**
 * Render the glyph to the texture atlas.
 */
static struct glyph *cache_glyph(struct font *font, unsigned long codepoint, float x_advance, float y_advance)
{
    if (FT_Load_Glyph(font->face, codepoint, FT_LOAD_RENDER)) {
        return NULL;
    }

    if (FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_NORMAL)) {
        return NULL;
    }

    unsigned char *bitmap8 = font->face->glyph->bitmap.buffer;
    int bitmap_w = font->face->glyph->bitmap.width;
    int bitmap_h = font->face->glyph->bitmap.rows;

    struct atlas *atlas = &font->atlas;

    int edge_x = atlas->width - atlas->x_padding - bitmap_w;
    int edge_y = atlas->height - atlas->y_padding - bitmap_h;

    if (atlas->cursor_x > edge_x) {
        atlas->cursor_x = atlas->x_padding;
        atlas->cursor_y += atlas->line_height + atlas->y_padding;

        if (atlas->cursor_y > edge_y) {
            if (!grow_atlas(atlas)) {
                return NULL;
            }
        }

        atlas->line_height = 0;
    }

    unsigned char *bitmap16 = conv_8bit_to_16bit(bitmap8, bitmap_w, bitmap_h);

    // Update on-VRAM texture portion.
    qu_update_texture(atlas->texture, atlas->cursor_x, atlas->cursor_y,
        bitmap_w, bitmap_h, bitmap16);

    pl_free(bitmap16);

    hmputs(font->glyphs, ((struct glyph) {
        .key = codepoint,
        .s0 = atlas->cursor_x,
        .t0 = atlas->cursor_y,
        .s1 = atlas->cursor_x + bitmap_w,
        .t1 = atlas->cursor_y + bitmap_h,
        .x_advance = x_advance,
        .y_advance = y_advance,
        .x_bearing = font->face->glyph->bitmap_left,
        .y_bearing = font->face->glyph->bitmap_top,
    }));

    atlas->cursor_x += bitmap_w + atlas->x_padding;

    if (atlas->line_height < bitmap_h) {
        atlas->line_height = bitmap_h;
    }

    return hmgetp(font->glyphs, codepoint);
}

/**
 * Cache ASCII glyphs.
 */
static void prerender_ascii(struct font *font)
{
    for (int i = 0x20; i <= 0xFF; i++) {
        hb_codepoint_t codepoint;

        if (!hb_font_get_glyph(font->font, i, 0, &codepoint)) {
            continue;
        }

        hb_position_t x_advance, y_advance;
        hb_font_get_glyph_advance_for_direction(font->font, codepoint,
            HB_DIRECTION_LTR, &x_advance, &y_advance);

        cache_glyph(font, codepoint, x_advance / 64.0f, y_advance / 64.0f);
    }
}

/**
 * Get vertex buffer pointer, grow it if necessary.
 */
static float *maintain_vertex_buffer(int required_size)
{
    if (impl.vertex_buffer_size > required_size) {
        return impl.vertex_buffer;
    }

    int next_size = QU_MAX(impl.vertex_buffer_size * 2, INITIAL_VERTEX_BUFFER_SIZE);

    while (next_size < required_size) {
        next_size *= 2;
    }

    float *next_buffer = pl_realloc(impl.vertex_buffer, sizeof(float) * next_size);
    
    if (!next_buffer) {
        return NULL;
    }

    QU_LOGD("text: grow vertex buffer (%d -> %d)\n", impl.vertex_buffer_size, next_size);

    impl.vertex_buffer = next_buffer;
    impl.vertex_buffer_size = next_size;

    return impl.vertex_buffer;
}

static void calculate_glyph_callback(struct font *font, struct glyph *glyph, void *data)
{
    struct text_calculate_state *state = (struct text_calculate_state *) data;

    state->width += glyph->x_advance;
    state->height += glyph->y_advance;
}

static void calculate_text_callback(struct font *font, void *data)
{
    struct text_calculate_state *state = (struct text_calculate_state *) data;

    state->height += font->height;
}

static void draw_glyph_callback(struct font *font, struct glyph *glyph, void *data)
{
    struct text_draw_state *state = (struct text_draw_state *) data;

    float x0 = state->x_current + glyph->x_bearing;
    float y0 = state->y_current - glyph->y_bearing + font->height;
    float x1 = x0 + glyph->s1 - glyph->s0;
    float y1 = y0 + glyph->t1 - glyph->t0;

    float s0 = glyph->s0 / (float) font->atlas.width;
    float t0 = glyph->t0 / (float) font->atlas.height;
    float s1 = glyph->s1 / (float) font->atlas.width;
    float t1 = glyph->t1 / (float) font->atlas.height;

    maintain_vertex_buffer(24 * (state->count + 100));

    float *v;

    if (state->vertices) {
        v = state->vertices;
    } else {
        v = impl.vertex_buffer;
    }

    *v++ = x0;  *v++ = y0;  *v++ = s0;  *v++ = t0;
    *v++ = x1;  *v++ = y0;  *v++ = s1;  *v++ = t0;
    *v++ = x1;  *v++ = y1;  *v++ = s1;  *v++ = t1;
    *v++ = x1;  *v++ = y1;  *v++ = s1;  *v++ = t1;
    *v++ = x0;  *v++ = y1;  *v++ = s0;  *v++ = t1;
    *v++ = x0;  *v++ = y0;  *v++ = s0;  *v++ = t0;

    state->x_current += glyph->x_advance;
    state->y_current += glyph->y_advance;
    state->vertices = v;
    state->count++;
}

static void draw_text_callback(struct font *font, void *data)
{
    struct text_draw_state *state = (struct text_draw_state *) data;

    qu_draw_font(font->atlas.texture, state->color, impl.vertex_buffer, 6 * state->count);
}

static qu_result process_text(int32_t font_id, char const *text, void *data,
                              void (*glyph_callback)(struct font *, struct glyph *, void *),
                              void (*text_callback)(struct font *, void *))
{
    struct font *font = hmgetp_null(impl.fonts, font_id);

    if (!font) {
        return QU_FAILURE;
    }

    hb_buffer_t *buffer = hb_buffer_create();

    hb_buffer_add_utf8(buffer, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buffer);

    hb_shape(font->font, buffer, NULL, 0);

    hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buffer, NULL);
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buffer, NULL);

    unsigned int length = hb_buffer_get_length(buffer);

    for (unsigned int i = 0; i < length; i++) {
        struct glyph *glyph = hmgetp_null(font->glyphs, info[i].codepoint);

        if (!glyph) {
            float x_adv = pos[i].x_advance / 64.0f;
            float y_adv = pos[i].y_advance / 64.0f;

            glyph = cache_glyph(font, info[i].codepoint, x_adv, y_adv);

            if (!glyph) {
                continue;
            }
        }

        if (glyph_callback) {
            glyph_callback(font, glyph, data);
        }
    }

    hb_buffer_destroy(buffer);

    if (text_callback) {
        text_callback(font, data);
    }

    return QU_SUCCESS;
}

//------------------------------------------------------------------------------

/**
 * Initialize text module.
 */
void qu_initialize_text(void)
{
    FT_Error error = FT_Init_FreeType(&impl.freetype);

    if (error) {
        QU_HALT("Failed to initialize FreeType.");
    }

    qu_atexit(qu_terminate_text);

    QU_LOGI("Text module initialized.\n");
    impl.initialized = true;
}

/**
 * Terminate text module.
 */
void qu_terminate_text(void)
{
    if (!impl.initialized) {
        return;
    }

    for (int i = 0; i < hmlen(impl.fonts); i++) {
        close_font(&impl.fonts[i]);
    }

    pl_free(impl.vertex_buffer);
    hmfree(impl.fonts);

    FT_Done_FreeType(impl.freetype);

    QU_LOGI("Text module terminated.\n");

    memset(&impl, 0, sizeof(impl));
}

/**
 * Loads font.
 * TODO: Weight is not implemented yet.
 */
qu_font qu_load_font(char const *path, float pt)
{
    if (!impl.initialized) {
        qu_initialize_text();
    }

    qu_file *file = qu_open_file_from_path(path);

    if (!file) {
        return (qu_font) { 0 };
    }

    int32_t id = open_font(file, pt);

    if (id) {
        prerender_ascii(hmgetp(impl.fonts, id));
    } else {
        qu_close_file(file);
    }

    return (qu_font) { id };
}

/**
 * Delete a font.
 */
void qu_delete_font(qu_font font)
{
    if (!impl.initialized) {
        return;
    }

    close_font(hmgetp_null(impl.fonts, font.id));
    hmdel(impl.fonts, font.id);
}

qu_vec2f qu_calculate_text_box(qu_font font, char const *str)
{
    if (!impl.initialized) {
        return (qu_vec2f) { -1.f, -1.f };
    }

    struct text_calculate_state state = {
        .width = 0.f,
        .height = 0.f,
    };

    process_text(font.id, str, &state, calculate_glyph_callback, calculate_text_callback);

    return (qu_vec2f) { state.width, state.height };
}

qu_vec2f qu_calculate_text_box_fmt(qu_font font, char const *fmt, ...)
{
    if (!impl.initialized) {
        return (qu_vec2f) { -1.f, -1.f };
    }

    va_list ap;
    char buffer[256];
    char *heap = NULL;

    va_start(ap, fmt);
    int required = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    if ((size_t) required >= sizeof(buffer)) {
        heap = pl_malloc(required + 1);

        if (heap) {
            va_start(ap, fmt);
            vsnprintf(heap, required + 1, fmt, ap);
            va_end(ap);
        }
    }

    qu_vec2f box = qu_calculate_text_box(font, heap ? heap : buffer);

    pl_free(heap);

    return box;
}

/**
 * Draw the text using a specified font.
 */
void qu_draw_text(qu_font font, float x, float y, qu_color color, char const *str)
{
    if (!impl.initialized) {
        return;
    }

    struct text_draw_state state = {
        .x_current = x,
        .y_current = y,
        .vertices = NULL,
        .count = 0,
        .color = color,
    };

    process_text(font.id, str, &state, draw_glyph_callback, draw_text_callback);
}

void qu_draw_text_fmt(qu_font font, float x, float y, qu_color color, char const *fmt, ...)
{
    if (!impl.initialized) {
        return;
    }

    va_list ap;
    char buffer[256];
    char *heap = NULL;

    va_start(ap, fmt);
    int required = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    if ((size_t) required >= sizeof(buffer)) {
        heap = pl_malloc(required + 1);

        if (heap) {
            va_start(ap, fmt);
            vsnprintf(heap, required + 1, fmt, ap);
            va_end(ap);
        }
    }

    qu_draw_text(font, x, y, color, heap ? heap : buffer);
    pl_free(heap);
}
