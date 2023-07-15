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
    qu_file *file = (qu_file *) user;
    return (int) qu_fread(data, size, file);
}

static void skip(void *user, int n)
{
    qu_file *file = (qu_file *) user;
    qu_fseek(file, n, SEEK_CUR);
}

static int eof(void *user)
{
    qu_file *file = (qu_file *) user;
    int64_t position = qu_ftell(file);
    int64_t size = qu_file_size(file);

    return (position == size);
}

qu_image *qu_load_image(qu_file *file)
{
    qu_image *image = malloc(sizeof(qu_image));

    if (!image) {
        return NULL;
    }

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
            qu_file_repr(file), stbi_failure_reason());

        free(image);
        return NULL;
    }

    QU_INFO("Loaded %dx%d (%d bits) image from %s.\n",
        image->width, image->height,
        image->channels * 8, qu_file_repr(file));

    return image;
}

void qu_delete_image(qu_image *image)
{
    stbi_image_free(image->pixels);
    free(image);
}

//------------------------------------------------------------------------------
