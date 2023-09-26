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

static qx_result check(qu_params const *params)
{
    return QX_SUCCESS;
}

static qx_result initialize(qu_params const *params)
{
    QU_INFO("Initialized.\n");

    return QX_SUCCESS;
}

static void terminate(void)
{
    QU_INFO("Terminated.\n");
}

static void set_master_volume(float volume)
{
}

static qx_result create_source(qx_audio_source *source)
{
    return QX_SUCCESS;
}

static void destroy_source(qx_audio_source *source)
{
}

static qx_result queue_buffer(qx_audio_source *source, qx_audio_buffer *buffer)
{
    return QX_SUCCESS;
}

static int get_queued_buffers(qx_audio_source *source)
{
    return 0;
}

static bool is_source_used(qx_audio_source *source)
{
    return false;
}

static qx_result start_source(qx_audio_source *source)
{
    return QX_SUCCESS;
}

static qx_result stop_source(qx_audio_source *source)
{
    return QX_SUCCESS;
}

//------------------------------------------------------------------------------

qx_audio_impl const qx_audio_null = {
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
