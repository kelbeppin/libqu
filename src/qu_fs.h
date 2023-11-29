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
// qu_fs.h: file system abstraction
//------------------------------------------------------------------------------

#ifndef QU_FS_H_INC
#define QU_FS_H_INC

//------------------------------------------------------------------------------

#include <stdint.h>

//------------------------------------------------------------------------------

#define QU_FILE_NAME_LENGTH     (256)

//------------------------------------------------------------------------------

typedef enum qu_file_source
{
    QU_FILE_SOURCE_STANDARD,
    QU_FILE_SOURCE_ANDROID_ASSET,
    QU_FILE_SOURCE_MEMORY_BUFFER,
    QU_TOTAL_FILE_SOURCES,
} qu_file_source;

typedef struct qu_file {
    qu_file_source source;

    char name[QU_FILE_NAME_LENGTH];
    size_t size;

    void *context;
} qu_file;

//------------------------------------------------------------------------------

qu_file *qu_open_file_from_path(char const *path);
qu_file *qu_open_file_from_buffer(void const *data, size_t size);
void qu_close_file(qu_file *file);

int64_t qu_file_read(void *buffer, size_t size, qu_file *file);
int64_t qu_file_tell(qu_file *file);
int64_t qu_file_seek(qu_file *file, int64_t offset, int origin);

//------------------------------------------------------------------------------

#endif // QU_FS_H_INC

