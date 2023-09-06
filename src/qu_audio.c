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

static struct qu__audio const *supported_audio_impl_list[] = {

#ifdef QU_WIN32
    &qu__audio_xaudio2,
#endif

#ifdef QU_USE_OPENAL
    &qu__audio_openal,
#endif

    &qu__audio_null,
};

struct qu__audio_priv
{
	struct qu__audio const *impl;
};

static struct qu__audio_priv priv;

//------------------------------------------------------------------------------

void qu__audio_initialize(qu_params const *params)
{
    memset(&priv, 0, sizeof(priv));

    int audio_impl_count = QU__ARRAY_SIZE(supported_audio_impl_list);

    if (audio_impl_count == 0) {
        QU_HALT("audio_impl_count == 0");
    }

    for (int i = 0; i < audio_impl_count; i++) {
        priv.impl = supported_audio_impl_list[i];

        if (priv.impl->query(params)) {
            QU_DEBUG("Selected audio implementation #%d.\n", i);
            break;
        }
    }

    priv.impl->initialize(params);
}

void qu__audio_terminate(void)
{
	priv.impl->terminate();
}

//------------------------------------------------------------------------------

void qu__audio_set_master_volume(float volume)
{
    priv.impl->set_master_volume(volume);
}

int32_t qu__audio_load_sound(qu_file *file)
{
    return priv.impl->load_sound(file);
}

void qu__audio_delete_sound(int32_t id)
{
    priv.impl->delete_sound(id);
}

int32_t qu__audio_play_sound(int32_t id)
{
    return priv.impl->play_sound(id);
}

int32_t qu__audio_loop_sound(int32_t id)
{
    return priv.impl->loop_sound(id);
}

int32_t qu__audio_open_music(qu_file *file)
{
    return priv.impl->open_music(file);
}

void qu__audio_close_music(int32_t id)
{
    priv.impl->close_music(id);
}

int32_t qu__audio_play_music(int32_t id)
{
    return priv.impl->play_music(id);
}

int32_t qu__audio_loop_music(int32_t id)
{
    return priv.impl->loop_music(id);
}

void qu__audio_pause_voice(int32_t id)
{
    priv.impl->pause_voice(id);
}

void qu__audio_unpause_voice(int32_t id)
{
    priv.impl->unpause_voice(id);
}

void qu__audio_stop_voice(int32_t id)
{
    priv.impl->stop_voice(id);
}
