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
// qu_audio_null.c: dummy audio module
//------------------------------------------------------------------------------

#define QU_MODULE "audio-null"
#include "qu.h"

//------------------------------------------------------------------------------

static qu_result check(void)
{
    return QU_SUCCESS;
}

static qu_result initialize(void)
{
    QU_LOGI("Initialized.\n");

    return QU_SUCCESS;
}

static void terminate(void)
{
    QU_LOGI("Terminated.\n");
}

static void set_master_volume(float volume)
{
}

static qu_result create_source(qu_audio_source *source)
{
    return QU_SUCCESS;
}

static void destroy_source(qu_audio_source *source)
{
}

static qu_result queue_buffer(qu_audio_source *source, qu_audio_buffer *buffer)
{
    return QU_SUCCESS;
}

static int get_queued_buffers(qu_audio_source *source)
{
    return 0;
}

static bool is_source_used(qu_audio_source *source)
{
    return false;
}

static qu_result start_source(qu_audio_source *source)
{
    return QU_SUCCESS;
}

static qu_result stop_source(qu_audio_source *source)
{
    return QU_SUCCESS;
}

//------------------------------------------------------------------------------

qu_audio_impl const qu_null_audio_impl = {
    .check = check,
    .initialize = initialize,
    .terminate = terminate,
    .set_master_volume = set_master_volume,
    .create_source = create_source,
    .destroy_source = destroy_source,
    .is_source_used = is_source_used,
    .queue_buffer = queue_buffer,
    .get_queued_buffers = get_queued_buffers,
    .start_source = start_source,
    .stop_source = stop_source,
};
