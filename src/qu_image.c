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

#include <stb_ds.h>

#include "qu.h"
#include "qu_platform.h"
#include "qu_resource_loader.h"

//------------------------------------------------------------------------------

struct qu_image_data
{
    int key;
    int width;
    int height;
    int channels;
    unsigned char *pixels;
};

static struct
{
    struct qu_image_data *map;
    int count;
} priv;

//------------------------------------------------------------------------------

static void image_destroy(struct qu_image_data *data);
static void image_cleanup(void);

/**
 * Get image data pointer from identifier.
 */
static struct qu_image_data *image_get(int id)
{
    return hmgetp(priv.map, id);
}

/**
 * Create new image data object.
 */
static struct qu_image_data *image_create(int width, int height, int channels)
{
    unsigned char *pixels = pl_malloc(sizeof(*pixels) * width * height * channels);

    if (!pixels) {
        return NULL;
    }

    if (!priv.map) {
        qu_atexit(image_cleanup);
    }

    struct qu_image_data data = {
        .key = ++priv.count,
        .width = width,
        .height = height,
        .channels = channels,
        .pixels = pixels,
    };

    hmputs(priv.map, data);

    return image_get(data.key);
}

/**
 * Load image from file.
 */
static struct qu_image_data *image_load(qu_file *file)
{
    qu_image_loader *loader = qu_open_image_loader(file);

    if (!loader) {
        return NULL;
    }

    int w = loader->width;
    int h = loader->height;
    int c = loader->channels;

    struct qu_image_data *data = image_create(w, h, c);

    if (!data) {
        qu_close_image_loader(loader);
        return NULL;
    }

    qu_result result = qu_image_loader_load(loader, data->pixels);
    qu_close_image_loader(loader);

    if (result != QU_SUCCESS) {
        image_destroy(data);
        return NULL;
    }

    return data;
}

/**
 * Destroy image data object, freeing all memory.
 */
static void image_destroy(struct qu_image_data *data)
{
    if (!data) {
        return;
    }

    pl_free(data->pixels);
    (void) hmdel(priv.map, data->key);
}

/**
 * Clean-up.
 */
static void image_cleanup(void)
{
    for (int i = 0; i < hmlen(priv.map); i++) {
        image_destroy(&priv.map[i]);
    }
}

//------------------------------------------------------------------------------
// Public API

qu_image qu_create_image(int width, int height, int channels)
{
    struct qu_image_data *image_data = image_create(width, height, channels);

    if (!image_data) {
        return (qu_image) { .id = 0 };
    }

    return (qu_image) { .id = image_data->key };
}

qu_image qu_load_image(char const *path, int channels)
{
    qu_file *file = qu_open_file_from_path(path);

    if (!file) {
        return (qu_image) { .id = 0 };
    }

    struct qu_image_data *data = image_load(file);
    qu_close_file(file);

    if (!data) {
        return (qu_image) { .id = 0 };
    }

    return (qu_image) { .id = data->key };
}

qu_image qu_load_image_from_memory(void *buffer, size_t size, int channels)
{
    qu_file *file = qu_open_file_from_buffer(buffer, size);

    if (!file) {
        return (qu_image) { .id = 0 };
    }

    struct qu_image_data *data = image_load(file);
    qu_close_file(file);

    if (!data) {
        return (qu_image) { .id = 0 };
    }

    return (qu_image) { .id = data->key };
}

void qu_destroy_image(qu_image image)
{
    struct qu_image_data *data = image_get(image.id);

    if (!data) {
        return;
    }

    image_destroy(data);
}

qu_vec2i qu_get_image_size(qu_image image)
{
    struct qu_image_data *data = image_get(image.id);

    if (!data) {
        return (qu_vec2i) { .x = -1, .y = -1 };
    }

    return (qu_vec2i) { .x = data->width, .y = data->height };
}

int qu_get_image_channels(qu_image image)
{
    struct qu_image_data *data = image_get(image.id);

    if (!data) {
        return -1;
    }

    return data->channels;
}

unsigned char *qu_get_image_pixels(qu_image image)
{
    struct qu_image_data *data = image_get(image.id);

    if (!data) {
        return NULL;
    }

    return data->pixels;
}

void qu_fill_image(qu_image image, uint32_t value)
{
    struct qu_image_data *data = image_get(image.id);

    if (!data) {
        return;
    }

    int w = data->width;
    int h = data->height;
    int c = data->channels;
    unsigned char *p = data->pixels;

    for (int y = 0; y < h; y++) {
        unsigned char *row = p + (y * c * w);

        for (int x = 0; x < w; x++) {
            // no love for big-endian
            memcpy(row + (x * c), &value, c);
        }
    }
}

