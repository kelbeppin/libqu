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

#define QU_MODULE "xaudio2"
#include "qu.h"

#include <xaudio2.h>

//------------------------------------------------------------------------------
// qu_audio_xaudio2.c: XAudio2-based audio module
//------------------------------------------------------------------------------

static IXAudio2 *g_pXAudio2;
static IXAudio2MasteringVoice *g_pMasteringVoice;

//------------------------------------------------------------------------------

static bool query(qu_params const *params)
{
    return true;
}

static void initialize(qu_params const *params)
{
    HRESULT hResult;

    hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if (FAILED(hResult)) {
        QU_HALT("Failed to initialize COM.");
    }

    hResult = XAudio2Create(&g_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);

    if (FAILED(hResult)) {
        QU_HALT("Failed to create XAudio2 engine instance.");
    }

    hResult = IXAudio2_CreateMasteringVoice(g_pXAudio2, &g_pMasteringVoice,
                                            XAUDIO2_DEFAULT_CHANNELS,
                                            XAUDIO2_DEFAULT_SAMPLERATE,
                                            0, NULL, NULL, 0);

    if (FAILED(hResult)) {
        QU_HALT("Failed to create XAudio2 mastering voice.");
    }

    QU_INFO("XAudio2 audio module initialized.\n");
}

static void terminate(void)
{
    IXAudio2MasteringVoice_DestroyVoice(g_pMasteringVoice);
    IXAudio2_Release(g_pXAudio2);
    CoUninitialize();

    QU_INFO("XAudio2 audio module terminated.\n");
}

//------------------------------------------------------------------------------

static void set_master_volume(float volume)
{
}

//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------

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

void qu_construct_xaudio2(qu_audio_module *audio)
{
    audio->query = query;
    audio->initialize = initialize;
    audio->terminate = terminate;

    audio->set_master_volume = set_master_volume;

    audio->load_sound = load_sound;
    audio->delete_sound = delete_sound;
    audio->play_sound = play_sound;
    audio->loop_sound = loop_sound;

    audio->open_music = open_music;
    audio->close_music = close_music;
    audio->play_music = play_music;
    audio->loop_music = loop_music;

    audio->pause_stream = pause_stream;
    audio->unpause_stream = unpause_stream;
    audio->stop_stream = stop_stream;
}
