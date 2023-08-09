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

struct qu__audio_priv
{
	qu_audio_module const *impl;
};

static struct qu__audio_priv priv;

//------------------------------------------------------------------------------

void qu__audio_initialize(qu_params const *params)
{
    memset(&priv, 0, sizeof(priv));

    priv.impl = &qu__audio_null_module;

    qu_audio_module *ideal_impl = NULL;

    switch (qu__core_get_audio_type()) {
    default:
        break;

#ifdef QU_USE_OPENAL
    case QU_AUDIO_OPENAL:
        ideal_impl = &qu__audio_openal_module;
        break;
#endif

#ifdef _WIN32
    case QU_AUDIO_XAUDIO2:
        ideal_impl = &qu__audio_xaudio2_module;
        break;
#endif
    }

    if (ideal_impl && ideal_impl->query(params)) {
        priv.impl = ideal_impl;
    }

    priv.impl->initialize(params);
}

void qu__audio_terminate(void)
{
	priv.impl->terminate();
}

void qu__audio_set_master_volume(float volume)
{
    priv.impl->set_master_volume(volume);
}

int32_t qu__audio_load_sound(qu_file *file)
{
    return priv.impl->load_sound(file);
}

void qu__audio_delete_sound(int32_t sound_id)
{
    priv.impl->delete_sound(sound_id);
}

int32_t qu__audio_play_sound(int32_t sound_id)
{
    return priv.impl->play_sound(sound_id);
}

int32_t qu__audio_loop_sound(int32_t sound_id)
{
    return priv.impl->loop_sound(sound_id);
}

int32_t qu__audio_open_music(qu_file *file)
{
    return priv.impl->open_music(file);
}

void qu__audio_close_music(int32_t music_id)
{
    priv.impl->close_music(music_id);
}

int32_t qu__audio_play_music(int32_t music_id)
{
    return priv.impl->play_music(music_id);
}

int32_t qu__audio_loop_music(int32_t music_id)
{
    return priv.impl->loop_music(music_id);
}

void qu__audio_pause_stream(int32_t stream_id)
{
    priv.impl->pause_stream(stream_id);
}

void qu__audio_unpause_stream(int32_t stream_id)
{
    priv.impl->unpause_stream(stream_id);
}

void qu__audio_stop_stream(int32_t stream_id)
{
    priv.impl->stop_stream(stream_id);
}
