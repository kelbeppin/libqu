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
// qu_audio_sles.c: OpenSL ES Audio Implementation
//------------------------------------------------------------------------------

#define QU_MODULE "audio-sles"
#include "qu.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

//------------------------------------------------------------------------------

struct sl_priv
{
    int unused;
};

//------------------------------------------------------------------------------

static struct sl_priv priv;

//------------------------------------------------------------------------------

static qx_result sl_check(qu_params const *params)
{
    return QX_SUCCESS;
}

static qx_result sl_initialize(qu_params const *params)
{
    QU_INFO("Initialized.\n");

    return QX_SUCCESS;
}

static void sl_terminate(void)
{
    QU_INFO("Terminated.\n");
}

static void sl_set_master_volume(float volume)
{
}

static qx_result sl_create_source(qx_audio_source *source)
{
    return QX_SUCCESS;
}

static void sl_destroy_source(qx_audio_source *source)
{
}

static qx_result sl_queue_buffer(qx_audio_source *source, qx_audio_buffer *buffer)
{
    return QX_SUCCESS;
}

static int sl_get_queued_buffers(qx_audio_source *source)
{
    return 0;
}

static bool sl_is_source_used(qx_audio_source *source)
{
    return false;
}

static qx_result sl_start_source(qx_audio_source *source)
{
    return QX_SUCCESS;
}

static qx_result sl_stop_source(qx_audio_source *source)
{
    return QX_SUCCESS;
}

//------------------------------------------------------------------------------

qx_audio_impl const qx_audio_null = {
    .check = sl_check,
    .initialize = sl_initialize,
    .terminate = sl_terminate,
    .set_master_volume = sl_set_master_volume,
    .create_source = sl_create_source,
    .destroy_source = sl_destroy_source,
    .is_source_used = sl_is_source_used,
    .queue_buffer = sl_queue_buffer,
    .get_queued_buffers = sl_get_queued_buffers,
    .start_source = sl_start_source,
    .stop_source = sl_stop_source,
};
