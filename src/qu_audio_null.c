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

#define QU_MODULE "audio-null"
#include "qu.h"

//------------------------------------------------------------------------------
// qu_audio_null.c: dummy audio module
//------------------------------------------------------------------------------

static bool query(qu_params const *params)
{
    return true;
}

static void initialize(qu_params const *params)
{
    QU_INFO("Null audio module initialized.\n");
}

static void terminate(void)
{
    QU_INFO("Null audio module terminated.\n");
}

static void set_master_volume(float volume)
{
}

static int32_t load_sound(qu_file *file)
{
    return 1;
}

static void delete_sound(int32_t sound_id)
{
}

static int32_t play_sound(int32_t sound_id)
{
    return 1;
}

static int32_t loop_sound(int32_t sound_id)
{
    return 1;
}

static int32_t open_music(qu_file *file)
{
    return 1;
}

static void close_music(int32_t music_id)
{
}

static int32_t play_music(int32_t music_id)
{
    return 1;
}

static int32_t loop_music(int32_t music_id)
{
    return 1;
}

static void pause_stream(int32_t stream_id)
{
}

static void unpause_stream(int32_t stream_id)
{
}

static void stop_stream(int32_t stream_id)
{
}

//------------------------------------------------------------------------------

void qu_construct_null_audio(qu_audio_module *audio)
{
    *audio = (qu_audio_module) {
        .query = query,
        .initialize = initialize,
        .terminate = terminate,
        .set_master_volume = set_master_volume,
        .load_sound = load_sound,
        .delete_sound = delete_sound,
        .play_sound = play_sound,
        .loop_sound = loop_sound,
        .open_music = open_music,
        .close_music = close_music,
        .play_music = play_music,
        .loop_music = loop_music,
        .pause_stream = pause_stream,
        .unpause_stream = unpause_stream,
        .stop_stream = stop_stream,
    };
}
