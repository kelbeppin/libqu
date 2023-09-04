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

#define QU_MODULE "text"
#include "qu.h"

#include <hb-ft.h>

//------------------------------------------------------------------------------
// qu_text.c: Text rendering module
//------------------------------------------------------------------------------

#define INITIAL_FONT_COUNT          256
#define INITIAL_GLYPH_COUNT         256
#define INITIAL_VERTEX_BUFFER_SIZE  256
#define INITIAL_INDEX_BUFFER_SIZE   256

//------------------------------------------------------------------------------

struct atlas
{
    int texture_id;                 // texture identifier
    int width;                      // texture width
    int height;                     // texture height
    int cursor_x;                   // x position where next glyph will be added
    int cursor_y;                   // y position where next glyph will be added
    int line_height;                // max height of current line in the atlas
    int x_padding;                  // min x padding between glyphs
    int y_padding;                  // min y padding between glyphs
    unsigned char *bitmap;          // locally stored copy of bitmap
};

struct glyph
{
    unsigned long codepoint;        // glyph codepoint (unicode?)
    int s0, t0;                     // top-left position in atlas
    int s1, t1;                     // bottom-right position in atlas
    float x_advance;                // how much x to add after printing the glyph
    float y_advance;                // how much y to add (usually 0)
    int x_bearing;                  // additional x offset
    int y_bearing;                  // additional y offset (?)
};

struct font
{
    hb_font_t *font;                // harfbuzz font handle
    FT_Face face;                   // FreeType font handle
    FT_StreamRec stream;            // FreeType font handle
    struct atlas atlas;             // texture atlas (TODO: multiple atlases)
    struct glyph *glyphs;           // dynamic array of glyphs
    int glyph_count;                // number of items in glyph array
    float height;                   // general height of font (glyphs may be taller)
};

static struct
{
    bool initialized;
    FT_Library freetype;            // FreeType object
    struct font *fonts;             // dynamic array of font objects
    int font_count;                 // number of items in font array
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
    if (qu_fseek(stream->descriptor.pointer, offset, SEEK_SET) == -1) {
        if (count == 0) {
            return 1;
        }

        return 0;
    }

    return qu_fread(buffer, count, stream->descriptor.pointer);
}

/**
 * close() callback for FreeType stream.
 */
static void ft_stream_close(FT_Stream stream)
{
    qu_fclose(stream->descriptor.pointer);
}

/**
 * Get free font index.
 * Expands font array if it's full.
 */
static int get_font_index(void)
{
    for (int i = 0; i < impl.font_count; i++) {
        if (impl.fonts[i].face == NULL) {
            return i;
        }
    }

    int next_count = QU_MAX(impl.font_count * 2, INITIAL_FONT_COUNT);
    struct font *next_array = realloc(impl.fonts, sizeof(struct font) * next_count);

    if (!next_array) {
        return -1;
    }

    int font_index = impl.font_count;

    for (int i = font_index; i < next_count; i++) {
        memset(&next_array[i], 0, sizeof(struct font));
    }

    QU_DEBUG("text: grow font array (%d -> %d)\n", impl.font_count, next_count);

    impl.font_count = next_count;
    impl.fonts = next_array;

    return font_index;
}

/**
 * Get free glyph index for specific font.
 * Expands glyph array if it's full.
 */
static int get_glyph_index(int font_index)
{
    for (int i = 0; i < impl.fonts[font_index].glyph_count; i++) {
        if (impl.fonts[font_index].glyphs[i].codepoint == 0) {
            return i;
        }
    }

    int next_count = QU_MAX(impl.fonts[font_index].glyph_count * 2, INITIAL_GLYPH_COUNT);
    struct glyph *next_array = realloc(impl.fonts[font_index].glyphs, sizeof(struct glyph) * next_count);
    
    if (!next_array) {
        return -1;
    }

    QU_DEBUG("text: grow glyph array for %d (%d -> %d)\n", font_index,
        impl.fonts[font_index].glyph_count, next_count);

    int glyph_index = impl.fonts[font_index].glyph_count;

    for (int i = glyph_index; i < next_count; i++) {
        memset(&next_array[i], 0, sizeof(struct glyph));
    }

    impl.fonts[font_index].glyph_count = next_count;
    impl.fonts[font_index].glyphs = next_array;

    return glyph_index;
}

/**
 * Attempts to make texture atlas larger.
 */
static bool grow_atlas(struct atlas *atlas)
{
    int next_height = atlas->height * 2;
    unsigned char *next_bitmap = realloc(atlas->bitmap, atlas->width * next_height * 2);

    if (!next_bitmap) {
        return false;
    }

    QU_DEBUG("text: grow atlas (%d -> %d)\n", atlas->height, next_height);

    atlas->height = next_height;
    atlas->bitmap = next_bitmap;

    for (int y = atlas->height / 2; y < atlas->height; y++) {
        for (int x = 0; x < atlas->width; x++) {
            atlas->bitmap[y * atlas->width * 2 + x * 2 + 0] = 0;
            atlas->bitmap[y * atlas->width * 2 + x * 2 + 1] = 0;
        }
    }

    qu_delete_texture((qu_texture) { atlas->texture_id });

    unsigned char fill[2] = { 0x00, 0x00 };
    atlas->texture_id = qu__graphics_create_texture(atlas->width, atlas->height, 2, fill);
    qu_set_texture_smooth((qu_texture) { atlas->texture_id }, true);
    qu__graphics_update_texture(atlas->texture_id, 0, 0, -1, -1, atlas->bitmap);

    return true;
}

/**
 * Render the glyph (if it isn't already) and return its index.
 * TODO: consider optimizing glyph indexing.
 */
static int cache_glyph(int font_index, unsigned long codepoint, float x_advance, float y_advance)
{
    struct font *font = &impl.fonts[font_index];

    for (int i = 0; i < font->glyph_count; i++) {
        if (font->glyphs[i].codepoint == codepoint) {
            return i;
        }
    }

    int glyph_index = get_glyph_index(font_index);

    if (glyph_index == -1) {
        return -1;
    }

    if (FT_Load_Glyph(font->face, codepoint, FT_LOAD_RENDER)) {
        return -1;
    }

    if (FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_NORMAL)) {
        return -1;
    }

    unsigned char *bitmap = font->face->glyph->bitmap.buffer;
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
                return -1;
            }
        }

        atlas->line_height = 0;
    }

    unsigned char *bitmap_with_alpha = malloc(bitmap_w * bitmap_h * 2);

    if (!bitmap_with_alpha) {
        return -1;
    }

    // Convert 1-channel bitmap to 2-channel bitmap (luminance and alpha).
    for (int y = 0; y < bitmap_h; y++) {
        for (int x = 0; x < bitmap_w; x++) {
            int idx = y * bitmap_w * 2 + x * 2;

            bitmap_with_alpha[idx + 0] = 255;
            bitmap_with_alpha[idx + 1] = bitmap[y * bitmap_w + x];
        }
    }

    // Replace portion of large on-memory bitmap with new data.
    for (int y = 0; y < bitmap_h; y++) {
        int atlas_y = atlas->cursor_y + y;

        for (int x = 0; x < bitmap_w; x++) {
            int atlas_x = atlas->cursor_x + x;
            int atlas_idx = atlas_y * atlas->width * 2 + atlas_x * 2;
            int idx = y * bitmap_w * 2 + x * 2;

            atlas->bitmap[atlas_idx + 0] = bitmap_with_alpha[idx + 0];
            atlas->bitmap[atlas_idx + 1] = bitmap_with_alpha[idx + 1];
        }
    }

    // Update on-VRAM texture portion.
    qu__graphics_update_texture(atlas->texture_id,
        atlas->cursor_x, atlas->cursor_y,
        bitmap_w, bitmap_h, bitmap_with_alpha);

    free(bitmap_with_alpha);

    struct glyph *glyph = &font->glyphs[glyph_index];

    glyph->codepoint = codepoint;
    glyph->s0 = atlas->cursor_x;
    glyph->t0 = atlas->cursor_y;
    glyph->s1 = atlas->cursor_x + bitmap_w;
    glyph->t1 = atlas->cursor_y + bitmap_h;

    glyph->x_advance = x_advance;
    glyph->y_advance = y_advance;

    glyph->x_bearing = font->face->glyph->bitmap_left;
    glyph->y_bearing = font->face->glyph->bitmap_top;

    atlas->cursor_x += bitmap_w + atlas->x_padding;

    if (atlas->line_height < bitmap_h) {
        atlas->line_height = bitmap_h;
    }

    return glyph_index;
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

    float *next_buffer = realloc(impl.vertex_buffer, sizeof(float) * next_size);
    
    if (!next_buffer) {
        return NULL;
    }

    QU_DEBUG("text: grow vertex buffer (%d -> %d)\n", impl.vertex_buffer_size, next_size);

    impl.vertex_buffer = next_buffer;
    impl.vertex_buffer_size = next_size;

    return impl.vertex_buffer;
}

//------------------------------------------------------------------------------

/**
 * Initialize text module.
 */
void qu_initialize_text(void)
{
    memset(&impl, 0, sizeof(impl));

    FT_Error error = FT_Init_FreeType(&impl.freetype);

    if (error) {
        QU_ERROR("Failed to initialize FreeType.\n");
        return;
    }

    QU_INFO("Text module initialized.\n");
    impl.initialized = true;
}

/**
 * Terminate text module.
 */
void qu_terminate_text(void)
{
    for (int i = 0; i < impl.font_count; i++) {
        qu_delete_font((qu_font) { i });
    }

    free(impl.vertex_buffer);
    free(impl.fonts);

    if (impl.initialized) {
        QU_INFO("Text module terminated.\n");
        impl.initialized = false;
    }
}

/**
 * Loads font.
 * TODO: Weight is not implemented yet.
 */
int32_t qu__text_load_font(qu_file *file, float pt)
{
    int32_t index = get_font_index();

    if (index == -1) {
        return 0;
    }

    struct font *fontp = &impl.fonts[index];
    memset(fontp, 0, sizeof(struct font));

    fontp->stream.base = NULL;
    fontp->stream.size = qu_file_size(file);
    fontp->stream.pos = qu_ftell(file);
    fontp->stream.descriptor.pointer = file;
    fontp->stream.pathname.pointer = (void *) qu_file_repr(file);
    fontp->stream.read = ft_stream_io;
    fontp->stream.close = ft_stream_close;

    FT_Open_Args args = {
        .flags = FT_OPEN_STREAM,
        .stream = &fontp->stream,
    };

    FT_Error error = FT_Open_Face(impl.freetype, &args, 0, &fontp->face);

    if (error) {
        QU_ERROR("Failed to open font %s.\n", qu_file_repr(file));
        return 0;
    }

    FT_Set_Char_Size(fontp->face, 0, (int) (pt * 64.0f), 0, 0);

    fontp->font = hb_ft_font_create_referenced(fontp->face);

    if (!fontp->font) {
        FT_Done_Face(fontp->face);
        fontp->face = NULL;

        return 0;
    }

    int width = 4096;
    int height = 16;

    while (height < (pt * 4)) {
        height *= 2;
    }

    unsigned char fill[2] = { 0x00, 0x00 };
    fontp->atlas.texture_id = qu__graphics_create_texture(width, height, 2, fill);
    qu_set_texture_smooth((qu_texture) { fontp->atlas.texture_id }, true);
    fontp->atlas.bitmap = calloc(width * height * 2, sizeof(unsigned char));

    if (fontp->atlas.texture_id == -1 || !fontp->atlas.bitmap) {
        free(fontp->atlas.bitmap);
        fontp->face = NULL;

        hb_font_destroy(fontp->font);
        fontp->font = NULL;

        return 0;
    }

    fontp->atlas.width = width;
    fontp->atlas.height = height;

    fontp->atlas.x_padding = 4;
    fontp->atlas.y_padding = 4;

    fontp->atlas.cursor_x = fontp->atlas.x_padding;
    fontp->atlas.cursor_y = fontp->atlas.y_padding;

    fontp->atlas.line_height = 0;

    for (int i = 0x20; i <= 0xFF; i++) {
        hb_codepoint_t codepoint;

        if (!hb_font_get_glyph(fontp->font, i, 0, &codepoint)) {
            continue;
        }

        hb_position_t x_advance, y_advance;
        hb_font_get_glyph_advance_for_direction(fontp->font, codepoint,
            HB_DIRECTION_LTR, &x_advance, &y_advance);

        cache_glyph(index, codepoint, x_advance / 64.0f, y_advance / 64.0f);
    }

    FT_F26Dot6 ascender = fontp->face->size->metrics.ascender;
    FT_F26Dot6 descender = fontp->face->size->metrics.descender;

    fontp->height = (ascender - descender) / 64.0f;

    return index + 1;
}

/**
 * Delete a font.
 */
void qu__text_delete_font(int32_t id)
{
    int index = id - 1;

    if (index < 0 || index >= impl.font_count) {
        return;
    }

    struct font *fontp = &impl.fonts[index];

    if (!fontp->face) {
        return;
    }

    free(fontp->glyphs);
    free(fontp->atlas.bitmap);
    qu_delete_texture((qu_texture) { fontp->atlas.texture_id });

    hb_font_destroy(fontp->font);
    fontp->font = NULL;

    qu_fclose(fontp->stream.descriptor.pointer);
    fontp->face = NULL;
}

/**
 * Draw the text using a specified font.
 */
void qu__text_draw(int32_t id, float x, float y, qu_color color, char const *text)
{
    int index = id - 1;

    if (index < 0 || index >= impl.font_count) {
        return;
    }

    struct font *fontp = &impl.fonts[index];

    if (!fontp->face) {
        return;
    }

    float x_offset = 0.0f;
    unsigned int length = 0;
    int quad_count = 0;

    // TODO: add bidi

    hb_buffer_t *buffer = hb_buffer_create();

    hb_buffer_add_utf8(buffer, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buffer);

    hb_shape(fontp->font, buffer, NULL, 0);

    length = hb_buffer_get_length(buffer);
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buffer, NULL);
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buffer, NULL);

    // if (hb_buffer_get_direction(buffer) == HB_DIRECTION_RTL) {
    //     for (unsigned int i = 0; i < length; i++) {
    //         x_offset -= pos[i].x_advance / 64.0f;
    //     }
    // }

    float x_current = x + x_offset;
    float y_current = y;

    float *v = maintain_vertex_buffer(24 * length);

    for (unsigned int i = 0; i < length; i++) {
        // Special case for newline character.
        if (text[i] == '\n') {
            x_current = x + x_offset;
            y_current += fontp->height;
            continue;
        }

        float x_adv = pos[i].x_advance / 64.0f;
        float y_adv = pos[i].y_advance / 64.0f;
        int glyph_index = cache_glyph(index, info[i].codepoint, x_adv, y_adv);

        if (glyph_index == -1) {
            continue;
        }

        struct glyph *glyph = &fontp->glyphs[glyph_index];

        float x0 = x_current + glyph->x_bearing;
        float y0 = y_current - glyph->y_bearing + fontp->height;
        float x1 = x0 + glyph->s1 - glyph->s0;
        float y1 = y0 + glyph->t1 - glyph->t0;

        float s0 = glyph->s0 / (float) fontp->atlas.width;
        float t0 = glyph->t0 / (float) fontp->atlas.height;
        float s1 = glyph->s1 / (float) fontp->atlas.width;
        float t1 = glyph->t1 / (float) fontp->atlas.height;

        *v++ = x0;  *v++ = y0;  *v++ = s0;  *v++ = t0;
        *v++ = x1;  *v++ = y0;  *v++ = s1;  *v++ = t0;
        *v++ = x1;  *v++ = y1;  *v++ = s1;  *v++ = t1;
        *v++ = x1;  *v++ = y1;  *v++ = s1;  *v++ = t1;
        *v++ = x0;  *v++ = y1;  *v++ = s0;  *v++ = t1;
        *v++ = x0;  *v++ = y0;  *v++ = s0;  *v++ = t0;

        x_current += glyph->x_advance;
        y_current += glyph->y_advance;

        quad_count++;
    }

    hb_buffer_destroy(buffer);

    qu__graphics_draw_text(fontp->atlas.texture_id, color, impl.vertex_buffer, 6 * quad_count);
}
