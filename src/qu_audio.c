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
// API entries

void qu_set_master_volume(float volume)
{
    priv.impl->set_master_volume(volume);
}

qu_sound qu_load_sound(char const *path)
{
    qu_file *file = qu_fopen(path);

    if (!file) {
        return (qu_sound) { 0 };
    }

    int32_t id = priv.impl->load_sound(file);

    qu_fclose(file);

    return (qu_sound) { id };
}

void qu_delete_sound(qu_sound sound)
{
    priv.impl->delete_sound(sound.id);
}

qu_stream qu_play_sound(qu_sound sound)
{
    return (qu_stream) { priv.impl->play_sound(sound.id) };
}

qu_stream qu_loop_sound(qu_sound sound)
{
    return (qu_stream) { priv.impl->loop_sound(sound.id) };
}

qu_music qu_open_music(char const *path)
{
    qu_file *file = qu_fopen(path);

    if (!file) {
        return (qu_music) { 0 };
    }

    int32_t id = priv.impl->open_music(file);
        
    if (!id) {
        qu_fclose(file);
    }

    return (qu_music) { id };
}

void qu_close_music(qu_music music)
{
    priv.impl->close_music(music.id);
}

qu_stream qu_play_music(qu_music music)
{
    return (qu_stream) { priv.impl->play_music(music.id) };
}

qu_stream qu_loop_music(qu_music music)
{
    return (qu_stream) { priv.impl->loop_music(music.id) };
}

void qu_pause_stream(qu_stream stream)
{
    priv.impl->pause_stream(stream.id);
}

void qu_unpause_stream(qu_stream stream)
{
    priv.impl->unpause_stream(stream.id);
}

void qu_stop_stream(qu_stream stream)
{
    priv.impl->stop_stream(stream.id);
}
