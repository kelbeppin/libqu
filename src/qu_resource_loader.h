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
// qu_resource_loader.h: resource loader
//------------------------------------------------------------------------------

#ifndef QU_RESOURCE_LOADER_H_INC
#define QU_RESOURCE_LOADER_H_INC

//------------------------------------------------------------------------------

#include "qu_fs.h"

//------------------------------------------------------------------------------

typedef enum qu_result qu_result;

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

//------------------------------------------------------------------------------

qu_image_loader *qu_open_image_loader(qu_file *file);
void qu_close_image_loader(qu_image_loader *loader);
qu_result qu_image_loader_load(qu_image_loader *loader, unsigned char *pixels);

qu_audio_loader *qu_open_audio_loader(qu_file *file);
void qu_close_audio_loader(qu_audio_loader *loader);
int64_t qu_audio_loader_read(qu_audio_loader *loader, int16_t *samples, int64_t max_samples);
int64_t qu_audio_loader_seek(qu_audio_loader *loader, int64_t sample_offset);

//------------------------------------------------------------------------------

#endif // QU_RESOURCE_LOADER_H_INC

