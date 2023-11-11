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
// qu_resource_loader.c: resource loader
//------------------------------------------------------------------------------

#define QU_MODULE "rl"

#define STB_IMAGE_IMPLEMENTATION

//------------------------------------------------------------------------------

#include <stb_image.h>
#include <vorbis/vorbisfile.h>

#include "qu.h"

//------------------------------------------------------------------------------
// Image pseudo-format: stb_image

static int stbi_loader_io_read(void *user, char *data, int size)
{
    return (int) qu_file_read(data, size, (qu_file *) user);
}

static void stbi_loader_io_skip(void *user, int n)
{
    qu_file_seek((qu_file *) user, n, SEEK_CUR);
}

static int stbi_loader_io_eof(void *user)
{
    size_t pos = qu_file_tell((qu_file *) user);
    size_t size = ((qu_file *) user)->size;

    return pos == size;
}

static stbi_io_callbacks const stbi_loader_io_callbacks = {
    .read = stbi_loader_io_read,
    .skip = stbi_loader_io_skip,
    .eof = stbi_loader_io_eof,
};

static qu_result stbi_loader_open(qu_image_loader *loader)
{
    qu_file_seek(loader->file, 0, SEEK_SET);

    int status = stbi_info_from_callbacks(&stbi_loader_io_callbacks, loader->file,
        &loader->width, &loader->height, &loader->channels);

    if (status == 0) {
        return QU_FAILURE;
    }

    return QU_SUCCESS;
}

static qu_result stbi_loader_load(qu_image_loader *loader, unsigned char *pixels)
{
    qu_file_seek(loader->file, 0, SEEK_SET);

    unsigned char *stbi = stbi_load_from_callbacks(&stbi_loader_io_callbacks, loader->file,
        &loader->width, &loader->height, &loader->channels, 0);
    
    if (!stbi) {
        return QU_FAILURE;
    }

    memcpy(pixels, stbi, loader->width * loader->height * loader->channels);
    stbi_image_free(stbi);

    return QU_SUCCESS;
}

static void stbi_loader_close(qu_image_loader *loader)
{
}

//------------------------------------------------------------------------------
// Image loader

struct image_loader_callbacks
{
    qu_result (*open)(qu_image_loader *loader);
    void (*close)(qu_image_loader *loader);
    qu_result (*load)(qu_image_loader *loader, unsigned char *pixels);
};

static struct image_loader_callbacks const image_loader_callbacks[] = {
    [QU_IMAGE_LOADER_STBI] = {
        .open = stbi_loader_open,
        .load = stbi_loader_load,
        .close = stbi_loader_close,
    },
};

qu_image_loader *qu_open_image_loader(qu_file *file)
{
    qu_image_loader *loader = pl_calloc(1, sizeof(*loader));

    loader->file = file;

    for (int i = 0; i < QU_TOTAL_IMAGE_LOADERS; i++) {
        if (image_loader_callbacks[i].open(loader) == QU_SUCCESS) {
            loader->format = i;
            return loader;
        }
    }

    pl_free(loader);

    return NULL;
}

void qu_close_image_loader(struct qu_image_loader *loader)
{
    image_loader_callbacks[loader->format].close(loader);
    pl_free(loader);
}

int qu_image_loader_load(struct qu_image_loader *loader, unsigned char *pixels)
{
    return image_loader_callbacks[loader->format].load(loader, pixels);
}

//------------------------------------------------------------------------------
// Audio format: Wave

struct riff
{
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;

    int64_t data_start;
    int64_t data_end;
};

static qu_result wave_loader_open(qu_audio_loader *loader)
{
    char chunk_id[4];
    uint32_t chunk_size;

    if ((qu_file_read(chunk_id, 4, loader->file) < 4) || memcmp("RIFF", chunk_id, 4)) {
        return QU_FAILURE;
    }

    if (qu_file_read(&chunk_size, 4, loader->file) < 4) {
        return QU_FAILURE;
    }

    char format[4];

    if ((qu_file_read(format, 4, loader->file) < 4) || memcmp("WAVE", format, 4)) {
        return QU_FAILURE;
    }

    struct riff *riff = pl_calloc(1, sizeof(*riff));
    
    while (true) {
        char subchunk_id[4];
        uint32_t subchunk_size;

        if (qu_file_read(subchunk_id, 4, loader->file) < 4) {
            return false;
        }

        if (qu_file_read(&subchunk_size, 4, loader->file) < 4) {
            return false;
        }

        int64_t subchunk_start = qu_file_tell(loader->file);

        if (memcmp("fmt ", subchunk_id, 4) == 0) {
            if (qu_file_read(riff, 16, loader->file) < 16) {
                return -1;
            }

            loader->num_channels = (int16_t) riff->num_channels;
            loader->sample_rate = (int64_t) riff->sample_rate;
        } else if (memcmp("data", subchunk_id, 4) == 0) {
            loader->num_samples = subchunk_size / (riff->bits_per_sample / 8);
            riff->data_start = qu_file_tell(loader->file);
            riff->data_end = riff->data_start + subchunk_size;
            break;
        }

        if (qu_file_seek(loader->file, subchunk_start + subchunk_size, SEEK_SET) == -1) {
            return false;
        }
    }

    loader->context = riff;

    qu_file_seek(loader->file, riff->data_start, SEEK_SET);

    return QU_SUCCESS;
}

static int64_t wave_loader_read(qu_audio_loader *loader, int16_t *samples, int64_t max_samples)
{
    struct riff *riff = loader->context;
    int16_t bytes_per_sample = riff->bits_per_sample / 8;
    int64_t samples_read = 0;

    while (samples_read < max_samples) {
        int64_t position = qu_file_tell(loader->file);

        if (position >= riff->data_end) {
            break;
        }

        unsigned char bytes[4];

        if (qu_file_read(bytes, bytes_per_sample, loader->file) != bytes_per_sample) {
            break;
        }

        switch (bytes_per_sample) {
        case 1:
            /* Unsigned 8-bit PCM */
            *samples++ = (((short) bytes[0]) - 128) << 8;
            break;
        case 2:
            /* Signed 16-bit PCM */
            *samples++ = (bytes[1] << 8) | bytes[0];
            break;
        case 3:
            /* Signed 24-bit PCM */
            *samples++ = (bytes[2] << 8) | bytes[1];
            break;
        case 4:
            /* Signed 32-bit PCM */
            *samples++ = (bytes[3] << 8) | bytes[2];
            break;
        }

        samples_read++;
    }

    return samples_read;
}

static int64_t wave_loader_seek(qu_audio_loader *loader, int64_t sample_offset)
{
    struct riff *riff = loader->context;
    int64_t offset = riff->data_start + (sample_offset * (riff->bits_per_sample / 8));
    return qu_file_seek(loader->file, offset, SEEK_SET);
}

static void wave_loader_close(qu_audio_loader *loader)
{
    struct riff *riff = loader->context;
    pl_free(riff);
}

//------------------------------------------------------------------------------
// Audio format: Ogg Vorbis

static char const *vorbis_loader_err_str(int status)
{
    if (status >= 0) {
        return "(no error)";
    }

    switch (status) {
    case OV_HOLE:           return "OV_HOLE";
    case OV_EREAD:          return "OV_EREAD";
    case OV_EFAULT:         return "OV_EFAULT";
    case OV_EINVAL:         return "OV_EINVAL";
    case OV_ENOTVORBIS:     return "OV_ENOTVORBIS";
    case OV_EBADHEADER:     return "OV_EBADHEADER";
    case OV_EVERSION:       return "OV_EVERSION";
    case OV_EBADLINK:       return "OV_EBADLINK";
    }

    return "(unknown error)";
}

static size_t vorbis_loader_io_read(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    return qu_file_read(ptr, size * nmemb, (qu_file *) datasource);
}

static int vorbis_loader_io_seek(void *datasource, ogg_int64_t offset, int whence)
{
    return (int) qu_file_seek((qu_file *) datasource, offset, whence);
}

static long vorbis_loader_io_tell(void *datasource)
{
    return (long) qu_file_tell((qu_file *) datasource);
}

static qu_result vorbis_loader_open(qu_audio_loader *loader)
{
    OggVorbis_File *vf = pl_calloc(1, sizeof(*vf));

    if (!vf) {
        return QU_FAILURE;
    }

    ov_callbacks callbacks = {
        .read_func = vorbis_loader_io_read,
        .seek_func = vorbis_loader_io_seek,
        .close_func = NULL,
        .tell_func = vorbis_loader_io_tell,
    };

    int status = ov_open_callbacks(loader->file, vf, NULL, 0, callbacks);

    if (status < 0) {
        QU_LOGE("Failed to open Ogg Vorbis media: %s\n", vorbis_loader_err_str(status));
        pl_free(vf);
        return QU_FAILURE;
    }

    vorbis_info *info = ov_info(vf, -1);
    ogg_int64_t samples_per_channel = ov_pcm_total(vf, -1);

    loader->num_channels = info->channels;
    loader->num_samples = samples_per_channel * info->channels;
    loader->sample_rate = info->rate;

    loader->context = vf;

    return QU_SUCCESS;
}

static int64_t vorbis_loader_read(qu_audio_loader *loader, int16_t *samples, int64_t max_samples)
{
    long samples_read = 0;

    while (samples_read < max_samples) {
        int bytes_left = (max_samples - samples_read) / sizeof(int16_t);
        long bytes_read = ov_read((OggVorbis_File *) loader->context,
            (char *) samples, bytes_left, 0, 2, 1, NULL);

        // End of file.
        if (bytes_read == 0) {
            break;
        }
        
        // Some error occured.
        // ...but it still works as intended, right?
        // Couldn't care less if it still reports some errors.
        // I'll just ignore them.
        if (bytes_read < 0) {
#if 0
            QU_LOGE("Failed to read Ogg Vorbis from file %s. Reason: %s\n",
                loader->file->name, vorbis_err_str(bytes_read));
#endif
            break;
        }

        samples_read += bytes_read / sizeof(int16_t);
        samples += bytes_read / sizeof(int16_t);
    }

    return samples_read;
}

static int64_t vorbis_loader_seek(qu_audio_loader *loader, int64_t sample_offset)
{
    return ov_pcm_seek((OggVorbis_File *) loader->context,
        sample_offset / loader->num_channels);
}

static void vorbis_loader_close(qu_audio_loader *loader)
{
    ov_clear((OggVorbis_File *) loader->context);
    pl_free(loader->context);
}

//------------------------------------------------------------------------------
// Audio loader

struct audio_loader_callbacks
{
    qu_result (*open)(qu_audio_loader *loader);
    void (*close)(qu_audio_loader *loader);
    int64_t (*read)(qu_audio_loader *loader, int16_t *dst, int64_t max_samples);
    int64_t (*seek)(qu_audio_loader *loader, int64_t sample_offset);
};

static struct audio_loader_callbacks const audio_loader_callbacks[] = {
    [QU_AUDIO_LOADER_WAVE] = {
        .open = wave_loader_open,
        .close = wave_loader_close,
        .read = wave_loader_read,
        .seek = wave_loader_seek,
    },
    [QU_AUDIO_LOADER_VORBIS] = {
        .open = vorbis_loader_open,
        .close = vorbis_loader_close,
        .read = vorbis_loader_read,
        .seek = vorbis_loader_seek,
    },
};

static char const *audio_loader_names[] = {
    [QU_AUDIO_LOADER_WAVE] = "RIFF WAVE",
    [QU_AUDIO_LOADER_VORBIS] = "Ogg Vorbis",
};

qu_audio_loader *qu_open_audio_loader(qu_file *file)
{
    qu_audio_loader *loader = pl_calloc(1, sizeof(*loader));

    if (!loader) {
        return NULL;
    }

    loader->file = file;

    for (int i = 0; i < QU_TOTAL_AUDIO_LOADERS; i++) {
        qu_file_seek(file, 0, SEEK_SET);

        if (audio_loader_callbacks[i].open(loader) == QU_SUCCESS) {
            QU_LOGI("File \"%s\" is recognized as %s.\n", file->name, audio_loader_names[i]);

            loader->format = i;
            return loader;
        }
    }

    QU_LOGE("Can't open \"%s\", format not recognized.\n", file->name);

    pl_free(loader);

    return NULL;
}

void qu_close_audio_loader(qu_audio_loader *loader)
{
    if (!loader) {
        return;
    }

    audio_loader_callbacks[loader->format].close(loader);
    pl_free(loader);
}

int64_t qu_audio_loader_read(qu_audio_loader *loader, int16_t *samples, int64_t max_samples)
{
    if (!loader) {
        return -1;
    }

    return audio_loader_callbacks[loader->format].read(loader, samples, max_samples);
}

int64_t qu_audio_loader_seek(qu_audio_loader *loader, int64_t sample_offset)
{
    if (!loader) {
        return -1;
    }

    return audio_loader_callbacks[loader->format].seek(loader, sample_offset);
}
