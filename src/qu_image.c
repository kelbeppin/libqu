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

#define QU_MODULE "image-loader"
#include "qu.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

//------------------------------------------------------------------------------
// qu_image_loader.c: image loader
//------------------------------------------------------------------------------

static int read(void *user, char *data, int size)
{
    qx_file *file = (qx_file *) user;
    return (int) qx_fread(data, size, file);
}

static void skip(void *user, int n)
{
    qx_file *file = (qx_file *) user;
    qx_fseek(file, n, SEEK_CUR);
}

static int eof(void *user)
{
    qx_file *file = (qx_file *) user;
    int64_t position = qx_ftell(file);
    int64_t size = qx_file_get_size(file);

    return (position == size);
}

//------------------------------------------------------------------------------

void qu__image_create(struct qu__image *image, unsigned char *fill)
{
    if (image->width <= 0 || image->height <= 0) {
        QU_ERROR("create(): invalid dimensions (%dx%d)\n", image->width, image->height);
        return;
    }

    if (image->channels < 1 || image->channels > 4) {
        QU_ERROR("create(): wrong number of channels (%d)\n", image->channels);
        return;
    }

    if (image->pixels) {
        QU_ERROR("create(): non-NULL .pixels pointer\n", image->channels);
        return;
    }

    size_t size = image->width * image->height * image->channels;
    image->pixels = malloc(sizeof(unsigned char) * size);

    if (!image->pixels) {
        QU_ERROR("create(): malloc() failed.\n");
        return;
    }

    unsigned char value[4] = { 0x80, 0x80, 0x80, 0xFF };

    if (fill) {
        for (int k = 0; k < image->channels; k++) {
            value[k] = fill[k];
        }
    } else {
        value[0] = 0x80;
        value[1] = 0xFF;
        value[2] = 0x80;
        value[3] = 0xFF;
    }

    for (int i = 0; i < image->height; i++) {
        size_t r = i * image->width * image->channels;

        for (int j = 0; j < image->width; j++) {
            size_t c = j * image->channels;

            for (int k = 0; k < image->channels; k++) {
                image->pixels[r + c + k] = value[k];
            }
        }
    }
}

void qu__image_load(struct qu__image *image, qx_file *file)
{
    image->pixels = stbi_load_from_callbacks(
        &(stbi_io_callbacks) {
            .read = read,
            .skip = skip,
            .eof = eof,
        }, file,
        &image->width,
        &image->height,
        &image->channels, 0
    );

    if (!image->pixels) {
        QU_ERROR("Failed to load image from %s. stbi error: %s\n",
            qx_file_get_name(file), stbi_failure_reason());

        image->width = -1;
        image->height = -1;
        image->channels = 0;

        return;
    }

    QU_INFO("Loaded %dx%d (%d bits) image from %s.\n",
        image->width, image->height,
        image->channels * 8, qx_file_get_name(file));
}

void qu__image_delete(struct qu__image *image)
{
    free(image->pixels);

    image->width = -1;
    image->height = -1;
    image->channels = 0;
    image->pixels = NULL;
}
