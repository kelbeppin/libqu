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

#define QU_MODULE "sound-reader"
#include "qu.h"

#include <vorbis/vorbisfile.h>

//------------------------------------------------------------------------------
// qu_sound_reader.c: sound reader
//------------------------------------------------------------------------------

struct sound_data
{
    int64_t (*read)(qu_sound_reader *reader, int16_t *dst, int64_t max_samples);
    void (*seek)(qu_sound_reader *reader, int64_t sample_offset);
    void (*close)(qu_sound_reader *reader);

    union {
        struct {
            int16_t bytes_per_sample;
            int64_t data_start;
            int64_t data_end;
        } wav;

        OggVorbis_File vorbis;
    };
};

//------------------------------------------------------------------------------
// WAV reader

struct wav_chunk
{
    char id[4];
    uint32_t size;
    char format[4];
};

struct wav_subchunk
{
    char id[4];
    uint32_t size;
};

struct wav_fmt
{
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

static int64_t open_wav(qu_sound_reader *reader)
{
    qu_fseek(reader->file, 0, SEEK_SET);

    struct wav_chunk chunk;

    if (qu_fread(&chunk, sizeof(chunk), reader->file) < (int64_t) sizeof(chunk)) {
        return -1;
    }

    if (strncmp("RIFF", chunk.id, 4) || strncmp("WAVE", chunk.format, 4)) {
        return -1;
    }

    bool data_found = false;
    struct sound_data *data = reader->data;

    while (!data_found) {
        struct wav_subchunk subchunk;

        if (qu_fread(&subchunk, sizeof(subchunk), reader->file) < (int64_t) sizeof(subchunk)) {
            return -1;
        }

        int64_t subchunk_start = qu_ftell(reader->file);

        if (strncmp("fmt ", subchunk.id, 4) == 0) {
            struct wav_fmt fmt;

            if (qu_fread(&fmt, sizeof(fmt), reader->file) < (int64_t) sizeof(fmt)) {
                return -1;
            }

            data->wav.bytes_per_sample = fmt.bits_per_sample / 8;
            reader->num_channels = fmt.num_channels;
            reader->sample_rate = fmt.sample_rate;
        } else if (strncmp("data", subchunk.id, 4) == 0) {
            reader->num_samples = subchunk.size / data->wav.bytes_per_sample;
            data->wav.data_start = qu_ftell(reader->file);
            data->wav.data_end = data->wav.data_start + subchunk.size;

            data_found = true;
        }

        if (qu_fseek(reader->file, subchunk_start + subchunk.size, SEEK_SET) == -1) {
            return -1;
        }
    }

    qu_fseek(reader->file, data->wav.data_start, SEEK_SET);

    return 0;
}

static int64_t read_wav(qu_sound_reader *reader, int16_t *samples, int64_t max_samples)
{
    struct sound_data *data = reader->data;
    int16_t bytes_per_sample = data->wav.bytes_per_sample;
    int64_t samples_read = 0;

    while (samples_read < max_samples) {
        int64_t position = qu_ftell(reader->file);

        if (position >= data->wav.data_end) {
            break;
        }

        unsigned char bytes[4];

        if (qu_fread(bytes, bytes_per_sample, reader->file) != bytes_per_sample) {
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

static void seek_wav(qu_sound_reader *reader, int64_t sample_offset)
{
    struct sound_data *data = reader->data;
    int64_t offset = data->wav.data_start + (sample_offset * data->wav.bytes_per_sample);
    qu_fseek(reader->file, offset, SEEK_SET);
}

static void close_wav(qu_sound_reader *reader)
{
}

//------------------------------------------------------------------------------
// Ogg Vorbis reader

static char const *ogg_err(int status)
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

static size_t vorbis_read_func(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    qu_file *file = (qu_file *) datasource;
    return qu_fread(ptr, size * nmemb, file);
}

static int vorbis_seek_func(void *datasource, ogg_int64_t offset, int whence)
{
    qu_file *file = (qu_file *) datasource;
    return (int) qu_fseek(file, offset, whence);
}

static long vorbis_tell_func(void *datasource)
{
    qu_file *file = (qu_file *) datasource;
    return (long) qu_ftell(file);
}

static int64_t open_ogg(qu_sound_reader *reader)
{
    struct sound_data *data = reader->data;

    qu_fseek(reader->file, 0, SEEK_SET);

    int test = ov_test_callbacks(
        reader->file,
        &data->vorbis,
        NULL, 0,
        (ov_callbacks) {
            .read_func = vorbis_read_func,
            .seek_func = vorbis_seek_func,
            .close_func = NULL,
            .tell_func = vorbis_tell_func,
        }
    );

    if (test < 0) {
        return -1;
    }

    int status = ov_test_open(&data->vorbis);

    if (status < 0) {
        QU_ERROR("Failed to open Ogg Vorbis media: %s\n", ogg_err(status));
        return -1;
    }

    vorbis_info *info = ov_info(&data->vorbis, -1);
    ogg_int64_t samples_per_channel = ov_pcm_total(&data->vorbis, -1);

    reader->num_channels = info->channels;
    reader->num_samples = samples_per_channel * info->channels;
    reader->sample_rate = info->rate;

    return 0;
}

static int64_t read_ogg(qu_sound_reader *reader, int16_t *samples, int64_t max_samples)
{
    struct sound_data *data = reader->data;
    long samples_read = 0;

    while (samples_read < max_samples) {
        int bytes_left = (max_samples - samples_read) / sizeof(int16_t);
        long bytes_read = ov_read(&data->vorbis, (char *) samples, bytes_left, 0, 2, 1, NULL);

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
                     qu_file_repr(sound->file), ogg_err(bytes_read));
#endif
            break;
        }

        samples_read += bytes_read / sizeof(int16_t);
        samples += bytes_read / sizeof(int16_t);
    }

    return samples_read;
}

static void seek_ogg(qu_sound_reader *reader, int64_t sample_offset)
{
    struct sound_data *data = reader->data;
    ov_pcm_seek(&data->vorbis, sample_offset / reader->num_channels);
}

static void close_ogg(qu_sound_reader *reader)
{
    struct sound_data *data = reader->data;
    ov_clear(&data->vorbis);
}

//------------------------------------------------------------------------------

qu_sound_reader *qu_open_sound_reader(qu_file *file)
{
    qu_sound_reader *reader = calloc(1, sizeof(qu_sound_reader));
    struct sound_data *data = calloc(1, sizeof(struct sound_data));

    if (reader && data) {
        reader->file = file;
        reader->data = data;

        if (open_wav(reader) == 0) {
            data->close = close_wav;
            data->read = read_wav;
            data->seek = seek_wav;

            return reader;
        }

        if (open_ogg(reader) == 0) {
            data->close = close_ogg;
            data->read = read_ogg;
            data->seek = seek_ogg;

            return reader;
        }
    }

    free(reader);
    free(data);

    return NULL;
}

void qu_close_sound_reader(qu_sound_reader *reader)
{
    if (!reader) {
        return;
    }

    ((struct sound_data *) reader->data)->close(reader);

    free(reader->data);
    free(reader);
}

int64_t qu_sound_read(qu_sound_reader *reader, int16_t *samples, int64_t max_samples)
{
    return ((struct sound_data *) reader->data)->read(reader, samples, max_samples);
}

void qu_sound_seek(qu_sound_reader *reader, int64_t sample_offset)
{
    ((struct sound_data *) reader->data)->seek(reader, sample_offset);
}
