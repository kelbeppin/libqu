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
// qu_util.h: utility functions
//------------------------------------------------------------------------------

#ifndef QU_UTIL_H_INC
#define QU_UTIL_H_INC

//------------------------------------------------------------------------------

#include "qu_platform.h"

//------------------------------------------------------------------------------

#define QU_ARRAY_SIZE(array) \
    sizeof(array) / sizeof(array[0])

#define QU_ALLOC_ARRAY(ptr, size) \
    ptr = pl_malloc(sizeof(*(ptr)) * (size))

//------------------------------------------------------------------------------

typedef struct qu_handle_list qu_handle_list;

//------------------------------------------------------------------------------

char *qu_strdup(char const *str);

qu_handle_list *qu_create_handle_list(size_t element_size, void (*dtor)(void *));
void qu_destroy_handle_list(qu_handle_list *list);
int32_t qu_handle_list_add(qu_handle_list *list, void *data);
void qu_handle_list_remove(qu_handle_list *list, int32_t id);
void *qu_handle_list_get(qu_handle_list *list, int32_t id);
void *qu_handle_list_get_first(qu_handle_list *list);
void *qu_handle_list_get_next(qu_handle_list *list);

//------------------------------------------------------------------------------

#endif // QU_UTIL_H_INC

