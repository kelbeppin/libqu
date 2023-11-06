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
// qu_util.c: arbitrary functions that can do anything
//------------------------------------------------------------------------------

#define QU_MODULE "util"

//------------------------------------------------------------------------------

#include "qu.h"

//------------------------------------------------------------------------------

char *qu_strdup(char const *str)
{
    size_t size = strlen(str) + 1;
    char *dup = pl_malloc(size);

    if (dup) {
        memcpy(dup, str, size);
    }

    return dup;
}

//------------------------------------------------------------------------------
// Handle lists

struct handle_list_control
{
    uint8_t flags;
    uint8_t gen;
};

struct qu_handle_list
{
    size_t element_size;
    void (*dtor)(void *);

    size_t size;
    size_t used;
    int64_t last_free_index;
    struct handle_list_control *control;
    void *data;

    int64_t last_iter;
};

//----------------------------------------------------------

static int32_t handle_list_encode_id(int index, int gen)
{
    // Identifier layout:
    // - bits 0 to 17 hold index number
    // - bits 18 to 23 should be 1
    // - bits 24 to 30 hold generation number
    // - bit 31 is unused

    return ((gen & 0x7F) << 24) | 0x00FC0000 | (index & 0x3FFFF);
}

static bool handle_list_decode_id(int32_t id, int *index, int *gen)
{
    if ((id & 0x00FC0000) == 0) {
        return false;
    }

    *index = id & 0x3FFFF;
    *gen = (id >> 24) & 0x7F;

    return true;
}

static int handle_list_find_index(qu_handle_list *array)
{
    if (array->last_free_index >= 0) {
        int index = array->last_free_index;
        array->last_free_index = -1;
        return index;
    }

    for (size_t i = 0; i < array->size; i++) {
        if ((array->control[i].flags & 0x01) == 0) {
            return i;
        }
    }

    return -1;
}

static bool handle_list_grow_array(qu_handle_list *array)
{
    size_t next_size = array->size ? (array->size * 2) : 16;

    struct handle_list_control *next_control = pl_realloc(
        array->control,
        sizeof(struct handle_list_control) * next_size
    );

    void *next_data = pl_realloc(
        array->data,
        array->element_size * next_size
    );

    if (!next_control || !next_data) {
        return false;
    }

    QU_LOGD("grow_array(): %d -> %d\n", array->size, next_size);

    for (size_t i = array->size; i < next_size; i++) {
        next_control[i].flags = 0;
        next_control[i].gen = 0;
    }

    array->size = next_size;
    array->control = next_control;
    array->data = next_data;

    return true;
}

static void *handle_list_get_data(qu_handle_list *array, int index)
{
    return ((uint8_t *) array->data) + (array->element_size * index);
}

//----------------------------------------------------------

qu_handle_list *qu_create_handle_list(size_t element_size, void (*dtor)(void *))
{
    qu_handle_list *array = pl_malloc(sizeof(qu_handle_list));

    if (!array) {
        return NULL;
    }

    *array = (qu_handle_list) {
        .element_size = element_size,
        .dtor = dtor,
        .size = 0,
        .used = 0,
        .last_free_index = -1,
        .control = NULL,
        .data = NULL,
        .last_iter = -1,
    };

    return array;
}

void qu_destroy_handle_list(qu_handle_list *array)
{
    if (!array) {
        return;
    }

    if (array->dtor) {
        for (size_t i = 0; i < array->size; i++) {
            if (array->control[i].flags & 0x01) {
                array->dtor(handle_list_get_data(array, i));
            }
        }
    }

    pl_free(array->control);
    pl_free(array->data);
    pl_free(array);
}

int32_t qu_handle_list_add(qu_handle_list *array, void *data)
{
    if (array->used == array->size) {
        if (!handle_list_grow_array(array)) {
            if (array->dtor) {
                array->dtor(data);
            }

            return 0;
        }
    }

    int index = handle_list_find_index(array);

    if (index == -1) {
        if (array->dtor) {
            array->dtor(data);
        }
        
        return 0;
    }

    ++array->used;
    array->control[index].flags = 0x01;

    memcpy(handle_list_get_data(array, index), data, array->element_size);

    return handle_list_encode_id(index, array->control[index].gen);
}

void qu_handle_list_remove(qu_handle_list *array, int32_t id)
{
    int index, gen;

    if (!handle_list_decode_id(id, &index, &gen) || array->control[index].gen != gen) {
        return;
    }

    --array->used;

    if (array->dtor) {
        array->dtor(handle_list_get_data(array, index));
    }

    array->control[index].flags = 0x00;
    array->control[index].gen = (array->control[index].gen + 1) & 0x7F;

    array->last_free_index = index;
}

void *qu_handle_list_get(qu_handle_list *array, int32_t id)
{
    if (id == 0) {
        return NULL;
    }

    int index, gen;

    if (!handle_list_decode_id(id, &index, &gen) || array->control[index].gen != gen) {
        return NULL;
    }

    return handle_list_get_data(array, index);
}

void *qu_handle_list_get_first(qu_handle_list *array)
{
    if (!array) {
        return NULL;
    }

    array->last_iter = -1;

    for (size_t i = 0; i < array->size; i++) {
        if (array->control[i].flags & 0x01) {
            array->last_iter = i;
            return handle_list_get_data(array, i);
        }
    }

    return NULL;
}

void *qu_handle_list_get_next(qu_handle_list *array)
{
    if (!array) {
        return NULL;
    }

    if (array->last_iter < 0 || array->last_iter >= array->size) {
        return NULL;
    }

    for (size_t i = array->last_iter + 1; i < array->size; i++) {
        if (array->control[i].flags & 0x01) {
            array->last_iter = i;
            return handle_list_get_data(array, i);
        }
    }

    array->last_iter = -1;
    return NULL;
}
