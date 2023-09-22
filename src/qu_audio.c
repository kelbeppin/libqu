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

#define QU_MODULE "audio"

#include "qu.h"

//------------------------------------------------------------------------------
// qu_audio.c: Audio module
//------------------------------------------------------------------------------

static struct qx_audio_impl const *supported_audio_impl_list[] = {

#ifdef QU_WIN32
    &qx_audio_xaudio2,
#endif

#ifdef QU_USE_OPENAL
    &qx_audio_openal,
#endif

    &qx_audio_null,
};

struct audio_priv
{
	struct qx_audio_impl const *impl;
};

static struct audio_priv priv;

//------------------------------------------------------------------------------

void qx_initialize_audio(qu_params const *params)
{
    memset(&priv, 0, sizeof(priv));

    int audio_impl_count = QU__ARRAY_SIZE(supported_audio_impl_list);

    if (audio_impl_count == 0) {
        QU_HALT("audio_impl_count == 0");
    }

    for (int i = 0; i < audio_impl_count; i++) {
        priv.impl = supported_audio_impl_list[i];

        QU_HALT_IF(!priv.impl->query);

        if (priv.impl->query(params)) {
            QU_DEBUG("Selected audio implementation #%d.\n", i);
            break;
        }
    }

    QU_HALT_IF(!priv.impl->initialize);
    QU_HALT_IF(!priv.impl->terminate);
    QU_HALT_IF(!priv.impl->set_master_volume);
    QU_HALT_IF(!priv.impl->load_sound);
    QU_HALT_IF(!priv.impl->delete_sound);
    QU_HALT_IF(!priv.impl->play_sound);
    QU_HALT_IF(!priv.impl->loop_sound);
    QU_HALT_IF(!priv.impl->open_music);
    QU_HALT_IF(!priv.impl->close_music);
    QU_HALT_IF(!priv.impl->play_music);
    QU_HALT_IF(!priv.impl->loop_music);
    QU_HALT_IF(!priv.impl->pause_voice);
    QU_HALT_IF(!priv.impl->unpause_voice);
    QU_HALT_IF(!priv.impl->stop_voice);

    priv.impl->initialize(params);
}

void qx_terminate_audio(void)
{
	priv.impl->terminate();
}

//------------------------------------------------------------------------------

void qx_set_master_volume(float volume)
{
    priv.impl->set_master_volume(volume);
}

int32_t qx_load_sound(qx_file *file)
{
    return priv.impl->load_sound(file);
}

void qx_delete_sound(int32_t id)
{
    priv.impl->delete_sound(id);
}

int32_t qx_play_sound(int32_t id)
{
    return priv.impl->play_sound(id);
}

int32_t qx_loop_sound(int32_t id)
{
    return priv.impl->loop_sound(id);
}

int32_t qx_open_music(qx_file *file)
{
    return priv.impl->open_music(file);
}

void qx_close_music(int32_t id)
{
    priv.impl->close_music(id);
}

int32_t qx_play_music(int32_t id)
{
    return priv.impl->play_music(id);
}

int32_t qx_loop_music(int32_t id)
{
    return priv.impl->loop_music(id);
}

void qx_pause_voice(int32_t id)
{
    priv.impl->pause_voice(id);
}

void qx_unpause_voice(int32_t id)
{
    priv.impl->unpause_voice(id);
}

void qx_stop_voice(int32_t id)
{
    priv.impl->stop_voice(id);
}
