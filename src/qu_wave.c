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
// qu_wave.c: sound reader
//------------------------------------------------------------------------------

#define QU_MODULE "wave"
#include "qu.h"

#include <vorbis/vorbisfile.h>

//------------------------------------------------------------------------------

#define WAVE_FORMAT_RIFF                0
#define WAVE_FORMAT_OGG                 1
#define TOTAL_WAVE_FORMATS              2

struct wave_format
{
    char const *name;
    void *(*test)(struct qx_file *file);
    bool (*open)(struct qx_wave *wave);
    int64_t (*read)(struct qx_wave *wave, int16_t *dst, int64_t max_samples);
    int64_t (*seek)(struct qx_wave *wave, int64_t sample_offset);
    void (*close)(struct qx_wave *wave);
};

//------------------------------------------------------------------------------
// WAV reader

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

static void *riff_test(struct qx_file *file)
{
    char chunk_id[4];
    uint32_t chunk_size;

    if ((qx_fread(chunk_id, 4, file) < 4) || memcmp("RIFF", chunk_id, 4)) {
        return NULL;
    }

    if (qx_fread(&chunk_size, 4, file) < 4) {
        return NULL;
    }

    char format[4];

    if ((qx_fread(format, 4, file) < 4) || memcmp("WAVE", format, 4)) {
        return NULL;
    }

    return calloc(1, sizeof(struct riff));
}

static bool riff_open(struct qx_wave *wave)
{
    struct riff *riff = wave->priv;
    
    while (true) {
        char subchunk_id[4];
        uint32_t subchunk_size;

        if (qx_fread(subchunk_id, 4, wave->file) < 4) {
            return false;
        }

        if (qx_fread(&subchunk_size, 4, wave->file) < 4) {
            return false;
        }

        int64_t subchunk_start = qx_ftell(wave->file);

        if (memcmp("fmt ", subchunk_id, 4) == 0) {
            if (qx_fread(riff, 16, wave->file) < 16) {
                return -1;
            }

            wave->num_channels = (int16_t) riff->num_channels;
            wave->sample_rate = (int64_t) riff->sample_rate;
        } else if (memcmp("data", subchunk_id, 4) == 0) {
            wave->num_samples = subchunk_size / (riff->bits_per_sample / 8);
            riff->data_start = qx_ftell(wave->file);
            riff->data_end = riff->data_start + subchunk_size;
            break;
        }

        if (qx_fseek(wave->file, subchunk_start + subchunk_size, SEEK_SET) == -1) {
            return false;
        }
    }

    qx_fseek(wave->file, riff->data_start, SEEK_SET);

    return true;
}

static int64_t riff_read(struct qx_wave *wave, int16_t *samples, int64_t max_samples)
{
    struct riff *riff = wave->priv;
    int16_t bytes_per_sample = riff->bits_per_sample / 8;
    int64_t samples_read = 0;

    while (samples_read < max_samples) {
        int64_t position = qx_ftell(wave->file);

        if (position >= riff->data_end) {
            break;
        }

        unsigned char bytes[4];

        if (qx_fread(bytes, bytes_per_sample, wave->file) != bytes_per_sample) {
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

static int64_t riff_seek(struct qx_wave *wave, int64_t sample_offset)
{
    struct riff *riff = wave->priv;
    int64_t offset = riff->data_start + (sample_offset * (riff->bits_per_sample / 8));
    return qx_fseek(wave->file, offset, SEEK_SET);
}

static void riff_close(struct qx_wave *wave)
{
    struct riff *riff = wave->priv;
    free(riff);
}

//------------------------------------------------------------------------------
// Ogg Vorbis

static char const *ogg_get_err_str(int status)
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

static size_t vorbis_read_callback(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    qx_file *file = (qx_file *) datasource;
    return qx_fread(ptr, size * nmemb, file);
}

static int vorbis_seek_callback(void *datasource, ogg_int64_t offset, int whence)
{
    qx_file *file = (qx_file *) datasource;
    return (int) qx_fseek(file, offset, whence);
}

static long vorbis_tell_callback(void *datasource)
{
    qx_file *file = (qx_file *) datasource;
    return (long) qx_ftell(file);
}

static void *ogg_test(struct qx_file *file)
{
    OggVorbis_File *vf = calloc(1, sizeof(OggVorbis_File));

    if (!vf) {
        return NULL;
    }

    ov_callbacks callbacks = {
        .read_func = vorbis_read_callback,
        .seek_func = vorbis_seek_callback,
        .close_func = NULL,
        .tell_func = vorbis_tell_callback,
    };

    if (ov_test_callbacks(file, vf, NULL, 0, callbacks) < 0) {
        free(vf);
        return NULL;
    }

    return vf;
}

static bool ogg_open(struct qx_wave *wave)
{
    OggVorbis_File *vf = wave->priv;
    int status = ov_test_open(vf);

    if (status < 0) {
        QU_ERROR("Failed to open Ogg Vorbis media: %s\n", ogg_get_err_str(status));
        return false;
    }

    vorbis_info *info = ov_info(vf, -1);
    ogg_int64_t samples_per_channel = ov_pcm_total(vf, -1);

    wave->num_channels = info->channels;
    wave->num_samples = samples_per_channel * info->channels;
    wave->sample_rate = info->rate;

    return true;
}

static int64_t ogg_read(struct qx_wave *wave, int16_t *samples, int64_t max_samples)
{
    OggVorbis_File *vf = wave->priv;
    long samples_read = 0;

    while (samples_read < max_samples) {
        int bytes_left = (max_samples - samples_read) / sizeof(int16_t);
        long bytes_read = ov_read(vf, (char *) samples, bytes_left, 0, 2, 1, NULL);

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
            QU_ERROR("Failed to read Ogg Vorbis from file %s. Reason: %s\n",
                     qx_file_get_name(wave->file), ogg_get_err_str(bytes_read));
#endif
            break;
        }

        samples_read += bytes_read / sizeof(int16_t);
        samples += bytes_read / sizeof(int16_t);
    }

    return samples_read;
}

static int64_t ogg_seek(struct qx_wave *wave, int64_t sample_offset)
{
    OggVorbis_File *vf = wave->priv;
    return ov_pcm_seek(vf, sample_offset / wave->num_channels);
}

static void ogg_close(struct qx_wave *wave)
{
    OggVorbis_File *vf = wave->priv;
    ov_clear(vf);
    free(vf);
}

//------------------------------------------------------------------------------

static struct wave_format const wave_formats[TOTAL_WAVE_FORMATS] = {
    [WAVE_FORMAT_RIFF] = {
        .name = "WAVE",
        .test = riff_test,
        .open = riff_open,
        .read = riff_read,
        .seek = riff_seek,
        .close = riff_close,
    },
    [WAVE_FORMAT_OGG] = {
        .name = "Ogg Vorbis",
        .test = ogg_test,
        .open = ogg_open,
        .read = ogg_read,
        .seek = ogg_seek,
        .close = ogg_close,
    },
};

//------------------------------------------------------------------------------

bool qx_open_wave(struct qx_wave *wave, struct qx_file *file)
{
    char const *file_name = qx_file_get_name(file);

    wave->file = file;

    for (int i = 0; i < TOTAL_WAVE_FORMATS; i++) {
        qx_fseek(file, 0, SEEK_SET);

        void *priv = wave_formats[i].test(file);

        if (!priv) {
            continue;
        }

        wave->priv = priv;

        if (wave_formats[i].open(wave)) {
            QU_INFO("File \"%s\" is recognized as %s.\n", file_name, wave_formats[i].name);
            wave->format = i;
            return true;
        }
    }

    QU_ERROR("Can't open \"%s\", format not recognized.\n", file_name);

    memset(wave, 0, sizeof(struct qx_wave));
    return false;
}

void qx_close_wave(struct qx_wave *wave)
{
    if (!wave || wave->format < 0 || wave->format >= TOTAL_WAVE_FORMATS) {
        return;
    }

    wave_formats[wave->format].close(wave);
}

int64_t qx_read_wave(struct qx_wave *wave, int16_t *samples, int64_t max_samples)
{
    if (!wave || wave->format < 0 || wave->format >= TOTAL_WAVE_FORMATS) {
        return -1;
    }

    return wave_formats[wave->format].read(wave, samples, max_samples);
}

int64_t qx_seek_wave(struct qx_wave *wave, int64_t sample_offset)
{
    if (!wave || wave->format < 0 || wave->format >= TOTAL_WAVE_FORMATS) {
        return -1;
    }

    return wave_formats[wave->format].seek(wave, sample_offset);
}
