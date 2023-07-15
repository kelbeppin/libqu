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

#define QU_MODULE "fs"
#include "qu.h"

//------------------------------------------------------------------------------
// qu_fs.c: filesystem abstraction
//------------------------------------------------------------------------------

typedef enum file_source
{
    SOURCE_FS,
    SOURCE_MEMORY,
} file_source;

struct qu_file
{
    file_source source;

    union {
        struct {
            FILE *stream;
        } fs;

        struct {
            uint8_t const *buffer;
            size_t offset;
        } memory;
    };

    size_t size;
    char repr[256];
};

qu_file *qu_fopen(char const *path)
{
    qu_file *file = calloc(1, sizeof(qu_file));

    if (!file) {
        return NULL;
    }

    file->source = SOURCE_FS;
    file->fs.stream = fopen(path, "rb");

    if (!file->fs.stream) {
        free(file);
        return NULL;
    }

    fseek(file->fs.stream, 0, SEEK_END);
    file->size = (size_t) ftell(file->fs.stream);
    fseek(file->fs.stream, 0, SEEK_SET);

    strncpy(file->repr, path, sizeof(file->repr) - 1);

    return file;
}

qu_file *qu_mopen(void const *buffer, size_t size)
{
    qu_file *file = calloc(1, sizeof(qu_file));

    if (!file) {
        return NULL;
    }

    file->source = SOURCE_MEMORY;
    file->memory.buffer = (uint8_t const *) buffer;
    file->memory.offset = 0;
    file->size = size;

    snprintf(file->repr, sizeof(file->repr) - 1, "0x%p", buffer);

    return file;
}

void qu_fclose(qu_file *file)
{
    if (file->source == SOURCE_FS) {
        fclose(file->fs.stream);
    }

    free(file);
}

int64_t qu_fread(void *buffer, size_t size, qu_file *file)
{
    int64_t bytes = 0;

    switch (file->source) {
    case SOURCE_FS:
        bytes = fread(buffer, 1, size, file->fs.stream);
        break;
    case SOURCE_MEMORY:
        if ((file->memory.offset + size) <= file->size) {
            bytes = size;
        } else {
            bytes = file->size - file->memory.offset;
        }
        memcpy(buffer, file->memory.buffer + file->memory.offset, bytes);
        break;
    default:
        break;
    }

    return bytes;
}

int64_t qu_ftell(qu_file *file)
{
    switch (file->source) {
    case SOURCE_FS:
        return ftell(file->fs.stream);
    case SOURCE_MEMORY:
        return file->memory.offset;
    default:
        return 0;
    }
}

int64_t qu_fseek(qu_file *file, int64_t offset, int origin)
{
    int64_t abs_offset = offset;

    switch (file->source) {
    case SOURCE_FS:
        return fseek(file->fs.stream, offset, origin);
    case SOURCE_MEMORY:
        if (origin == SEEK_CUR) {
            abs_offset = file->memory.offset + offset;
        } else if (origin == SEEK_END) {
            abs_offset = file->size + offset;
        }

        if (abs_offset < 0 || abs_offset > (int64_t) file->size) {
            return -1;
        }

        file->memory.offset = abs_offset;
        return 0;
    default:
        return -1;
    }
}

size_t qu_file_size(qu_file *file)
{
    return file->size;
}

char const *qu_file_repr(qu_file *file)
{
    return file->repr;
}
