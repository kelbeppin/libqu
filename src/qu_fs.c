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

//------------------------------------------------------------------------------

#define FILE_SOURCE_DEFAULT         0   // Standard C file stream (FILE *)
#define FILE_SOURCE_SYSTEM          1   // System-specific (e.g. Android's AAsset)
#define FILE_SOURCE_MEMORY          2   // Buffer stored in memory

//------------------------------------------------------------------------------

struct file_callbacks
{
    int64_t (*read)(void *dst, size_t size, void *handle);
    int64_t (*tell)(void *handle);
    int64_t (*seek)(void *handle, int64_t offset, int whence);
    size_t (*get_size)(void *handle);
    void (*close)(void *handle);
};

struct membuf
{
    void const *data;
    size_t size;
    int64_t offset;
};

struct qx_file
{
    int source;
    char name[256];
    void *handle;
};

//------------------------------------------------------------------------------

static int64_t fread_(void *dst, size_t size, void *handle)
{
    return fread(dst, size, 1, (FILE *) handle);
}

static int64_t ftell_(void *handle)
{
    return ftell((FILE *) handle);
}

static int64_t fseek_(void *handle, int64_t offset, int origin)
{
    return fseek((FILE *) handle, offset, origin);
}

static size_t get_file_size(void *handle)
{
    FILE *stream = (FILE *) handle;

    size_t pos, size;

    pos = ftell(stream);
    fseek(stream, 0, SEEK_END);

    size = ftell(stream);
    fseek(stream, pos, SEEK_SET);

    return size;
}

static void fclose_(void *handle)
{
    fclose((FILE *) handle);
}

//------------------------------------------------------------------------------

static int64_t membuf_read(void *dst, size_t size, void *handle)
{
    struct membuf *membuf = (struct membuf *) handle;
    int64_t bytes = 0;

    if ((membuf->offset + size) <= membuf->size) {
        bytes = size;
    } else {
        bytes = membuf->size - membuf->offset;
    }

    memcpy(dst, (uint8_t const *) membuf->data + membuf->offset, bytes);

    return bytes;
}

static int64_t membuf_tell(void *handle)
{
    struct membuf *membuf = (struct membuf *) handle;
    return membuf->offset;
}

static int64_t membuf_seek(void *handle, int64_t offset, int origin)
{
    struct membuf *membuf = (struct membuf *) handle;
    int64_t abs_offset;

    if (origin == SEEK_CUR) {
        abs_offset = membuf->offset + offset;
    } else if (origin == SEEK_END) {
        abs_offset = membuf->size + offset;
    } else {
        abs_offset = offset;
    }

    if (abs_offset < 0 || abs_offset > (int64_t) membuf->size) {
        return -1;
    }

    membuf->offset = abs_offset;

    return 0;
}

static size_t membuf_get_size(void *handle)
{
    struct membuf *membuf = (struct membuf *) handle;
    return membuf->size;
}

static void membuf_close(void *handle)
{
}

//------------------------------------------------------------------------------

static struct file_callbacks const file_callback_table[] = {
    [FILE_SOURCE_DEFAULT] = {
        .read = fread_,
        .tell = ftell_,
        .seek = fseek_,
        .get_size = get_file_size,
        .close = fclose_,
    },
    [FILE_SOURCE_SYSTEM] = {
        .read = qx_sys_fread,
        .tell = qx_sys_ftell,
        .seek = qx_sys_fseek,
        .get_size = qx_sys_get_file_size,
        .close = qx_sys_fclose,
    },
    [FILE_SOURCE_MEMORY] = {
        .read = membuf_read,
        .tell = membuf_tell,
        .seek = membuf_seek,
        .get_size = membuf_get_size,
        .close = membuf_close,
    },
};

//------------------------------------------------------------------------------

qx_file *qx_fopen(char const *path)
{
    qx_file *file = malloc(sizeof(qx_file));

    if (!file) {
        QU_ERROR("malloc() returned NULL!\n");
        return NULL;
    }

    strncpy(file->name, path, sizeof(file->name) - 1);

    void *sys_file = qx_sys_fopen(path);

    if (sys_file) {
        file->source = FILE_SOURCE_SYSTEM;
        file->handle = sys_file;

        QU_INFO("Successfully opened file %s using system-specific interface.\n", path);

        return file;
    }

    FILE *std_file = fopen(path, "rb");

    if (std_file) {
        file->source = FILE_SOURCE_DEFAULT;
        file->handle = std_file;

        QU_INFO("Successfully opened file %s.\n", path);

        return file;
    }

    free(file);

    QU_ERROR("File %s is not found.\n", path);

    return NULL;
}

qx_file *qx_membuf_to_file(void const *data, size_t size)
{
    qx_file *file = malloc(sizeof(qx_file));
    struct membuf *buffer = malloc(sizeof(struct membuf));

    if (!file || !buffer) {
        QU_ERROR("malloc() returned NULL!\n");
        return NULL;
    }

    buffer->data = data;
    buffer->size = size;
    buffer->offset = 0;

    file->source = FILE_SOURCE_MEMORY;
    file->handle = buffer;

    snprintf(file->name, sizeof(file->name) - 1, "%p", data);

    return file;
}

int64_t qx_fread(void *buffer, size_t size, qx_file *file)
{
    return file_callback_table[file->source].read(buffer, size, file->handle);
}

int64_t qx_ftell(qx_file *file)
{
    return file_callback_table[file->source].tell(file->handle);
}

int64_t qx_fseek(qx_file *file, int64_t offset, int origin)
{
    return file_callback_table[file->source].seek(file->handle, offset, origin);
}

size_t qx_file_get_size(qx_file *file)
{
    return file_callback_table[file->source].get_size(file->handle);
}

char const *qx_file_get_name(qx_file *file)
{
    return file->name;
}

void qx_fclose(qx_file *file)
{
    file_callback_table[file->source].close(file->handle);

    if (file->source == FILE_SOURCE_MEMORY) {
        free(file->handle);
    }

    free(file);
}
