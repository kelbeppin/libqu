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
// [TODO] THIS IMPLEMENTATION IS FLAWED. IT SHOULD BE REWRITTEN.
//
// 1. Audio players are not released when the app loses focus.
// 2. Requests audio player for each source.
// 3. No query of optimal buffer size is made.
// 4. Probably I should implement mixing manually.
//------------------------------------------------------------------------------

#include "qu_audio.h"
#include "qu_log.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

//------------------------------------------------------------------------------

#define TOTAL_AUDIO_PLAYERS                     (16) // too much?

#define AUDIO_PLAYER_FLAG_ACTIVE                (0x0001)
#define AUDIO_PLAYER_FLAG_EXHAUSTED             (0x0002)

//------------------------------------------------------------------------------

struct sl_engine
{
    SLObjectItf object;
    SLEngineItf engine;
};

struct sl_output_mix
{
    SLObjectItf object;
};

struct sl_audio_player
{
    SLObjectItf object;
    SLPlayItf play;
    SLVolumeItf volume;
    SLAndroidSimpleBufferQueueItf bufferQueue;
    SLuint32 _flags;
    SLmillibel _maxMillibel;
};

//------------------------------------------------------------------------------

static qu_result engine_initialize(struct sl_engine *self)
{
    SLresult result;

    result = slCreateEngine(&self->object, 0, NULL, 0, NULL, NULL);

    if (result != SL_RESULT_SUCCESS) {
        QU_LOGE("Failed to create engine.\n");
        return QU_FAILURE;
    }

    result = (*self->object)->Realize(self->object, SL_BOOLEAN_FALSE);

    if (result != SL_RESULT_SUCCESS) {
        QU_LOGE("Failed to initialize engine.\n");
        return QU_FAILURE;
    }

    result = (*self->object)->GetInterface(self->object, SL_IID_ENGINE, &self->engine);

    if (result != SL_RESULT_SUCCESS) {
        QU_LOGE("Failed to obtain EngineItf interface from engine.\n");
        return QU_FAILURE;
    }

    return QU_SUCCESS;
}

static void engine_terminate(struct sl_engine *self)
{
    if (!self->object) {
        return;
    }

    (*self->object)->Destroy(self->object);
    memset(self, 0, sizeof(struct sl_engine));
}

//------------------------------------------------------------------------------

static qu_result output_mix_initialize(struct sl_output_mix *self, struct sl_engine *engine)
{
    SLInterfaceID const interfaceIdList[] = {
        SL_IID_VOLUME,
    };

    SLboolean const interfaceRequirementList[] = {
        SL_BOOLEAN_FALSE,
    };

    SLresult result;

    result = (*engine->engine)->CreateOutputMix(engine->engine,
                                                &self->object,
                                                1,
                                                interfaceIdList,
                                                interfaceRequirementList);

    if (result != SL_RESULT_SUCCESS) {
        QU_LOGE("Failed to create output mix.\n");
        return QU_FAILURE;
    }

    result = (*self->object)->Realize(self->object, SL_BOOLEAN_FALSE);

    if (result != SL_RESULT_SUCCESS) {
        QU_LOGE("Failed to initialize output mix.\n");
        return QU_FAILURE;
    }

    return QU_SUCCESS;
}

static void output_mix_terminate(struct sl_output_mix *self)
{
    if (!self->object) {
        return;
    }

    (*self->object)->Destroy(self->object);
    memset(self, 0, sizeof(struct sl_output_mix));
}

//------------------------------------------------------------------------------

static void buffer_queue_callback(SLAndroidSimpleBufferQueueItf bufferQueue, void *context)
{
    SLAndroidSimpleBufferQueueState state;
    (*bufferQueue)->GetState(bufferQueue, &state);

    if (state.count == 0) {
        struct sl_audio_player *audioPlayer = (struct sl_audio_player *) context;
        audioPlayer->_flags |= AUDIO_PLAYER_FLAG_EXHAUSTED;
    }
}

static qu_result audio_player_initialize(struct sl_audio_player *self, struct sl_engine *engine, struct sl_output_mix *outputMix, qu_audio_source *source)
{
    SLuint32 channelMask;

    if (source->channels == 1) {
        channelMask = SL_SPEAKER_FRONT_CENTER;
    } else if (source->channels == 2) {
        channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    } else {
        return QU_FAILURE;
    }

    SLDataLocator_AndroidSimpleBufferQueue dataLocatorBufferQueue = {
        .locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
        .numBuffers = 8, // should be the same as TOTAL_MUSIC_BUFFERS in qu_audio.c
    };

    // From docs: "The value of the samplesPerSec field is in units of milliHz,
    // despite the misleading name."
    // So the sample_rate should be multiplied by 1000.
    SLDataFormat_PCM dataFormatPCM = {
        .formatType = SL_DATAFORMAT_PCM,
        .numChannels = source->channels,
        .samplesPerSec = source->sample_rate * 1000,
        .bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16,
        .containerSize = SL_PCMSAMPLEFORMAT_FIXED_16,
        .channelMask = channelMask,
        .endianness = SL_BYTEORDER_LITTLEENDIAN,
    };

    SLDataSource audioSource = {
        .pLocator = &dataLocatorBufferQueue,
        .pFormat = &dataFormatPCM,
    };

    SLDataLocator_OutputMix dataLocatorOutputMix = {
        .locatorType = SL_DATALOCATOR_OUTPUTMIX,
        .outputMix = outputMix->object,
    };

    SLDataSink audioSink = {
        .pLocator = &dataLocatorOutputMix,
        .pFormat = NULL,
    };

    SLInterfaceID const interfaceIdList[] = {
        SL_IID_VOLUME,
        SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
    };

    SLboolean const interfaceRequirementList[] = {
        SL_BOOLEAN_TRUE,
        SL_BOOLEAN_TRUE,
    };

    SLresult result;

    result = (*engine->engine)->CreateAudioPlayer(engine->engine,
                                                  &self->object,
                                                  &audioSource,
                                                  &audioSink,
                                                  2,
                                                  interfaceIdList,
                                                  interfaceRequirementList);

    if (result != SL_RESULT_SUCCESS) {
        QU_LOGE("Failed to create AudioPlayer.\n");
        return QU_FAILURE;
    }

    result = (*self->object)->Realize(self->object, SL_BOOLEAN_FALSE);

    if (result != SL_RESULT_SUCCESS) {
        QU_LOGE("Failed to create AudioPlayer: can't initialize.\n");
        return QU_FAILURE;
    }

    result = (*self->object)->GetInterface(self->object, SL_IID_PLAY, &self->play);

    if (result != SL_RESULT_SUCCESS) {
        QU_LOGE("Failed to create AudioPlayer: can't obtain Play interface.\n");
        return QU_FAILURE;
    }

    result = (*self->object)->GetInterface(self->object, SL_IID_VOLUME, &self->volume);

    if (result != SL_RESULT_SUCCESS) {
        QU_LOGE("Failed to create AudioPlayer: can't obtain Volume interface.\n");
        return QU_FAILURE;
    }

    result = (*self->object)->GetInterface(self->object, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &self->bufferQueue);

    if (result != SL_RESULT_SUCCESS) {
        QU_LOGE("Failed to create AudioPlayer: can't obtain AndroidSimpleBufferQueue interface.\n");
        return QU_FAILURE;
    }

    result = (*self->bufferQueue)->RegisterCallback(self->bufferQueue, buffer_queue_callback, self);

    if (result != SL_RESULT_SUCCESS) {
        QU_LOGE("Failed to create AudioPlayer: can't register buffer queue callback.\n");
        return QU_FAILURE;
    }

    self->_flags = AUDIO_PLAYER_FLAG_ACTIVE;
    (*self->volume)->GetMaxVolumeLevel(self->volume, &self->_maxMillibel);

    return QU_SUCCESS;
}

static void audio_player_terminate(struct sl_audio_player *self)
{
    if (!self->object) {
        return;
    }

    SLuint32 playState;
    (*self->play)->GetPlayState(self->play, &playState);

    if (playState != SL_PLAYSTATE_STOPPED) {
        (*self->play)->SetPlayState(self->play, SL_PLAYSTATE_STOPPED);
    }

    (*self->object)->Destroy(self->object);
    memset(self, 0, sizeof(struct sl_audio_player));
}

static qu_result audio_player_enqueue_buffer(struct sl_audio_player *self, qu_audio_buffer *buffer)
{
    void const *data = buffer->data;
    SLuint32 size = sizeof(int16_t) * buffer->samples;

    SLresult result = (*self->bufferQueue)->Enqueue(self->bufferQueue, data, size);

    if (result != SL_RESULT_SUCCESS) {
        return QU_FAILURE;
    }

    return QU_SUCCESS;
}

static int audio_player_get_buffer_count(struct sl_audio_player *self)
{
    SLAndroidSimpleBufferQueueState state;

    (*self->bufferQueue)->GetState(self->bufferQueue, &state);

    return (int) state.count;
}

static void audio_player_set_volume(struct sl_audio_player *self, float volume)
{
    SLmillibel range = (SLmillibel) (self->_maxMillibel - SL_MILLIBEL_MIN);
    SLmillibel millibel = SL_MILLIBEL_MIN + ((float) range * volume);

    SLresult result = (*self->volume)->SetVolumeLevel(self->volume, millibel);

    if (result != SL_RESULT_SUCCESS) {
        QU_LOGW("Volume::SetVolumeLevel(0x%04x) failed.\n", millibel);
    }
}

static qu_result audio_player_play(struct sl_audio_player *self)
{
    SLresult result = (*self->play)->SetPlayState(self->play, SL_PLAYSTATE_PLAYING);

    if (result != SL_RESULT_SUCCESS) {
        return QU_FAILURE;
    }

    return QU_SUCCESS;
}

static qu_result audio_player_pause(struct sl_audio_player *self)
{
    SLresult result = (*self->play)->SetPlayState(self->play, SL_PLAYSTATE_PAUSED);

    if (result != SL_RESULT_SUCCESS) {
        return QU_FAILURE;
    }

    return QU_SUCCESS;
}

//------------------------------------------------------------------------------

struct sl_priv
{
    struct sl_engine engine;
    struct sl_output_mix outputMix;
    struct sl_audio_player audioPlayers[TOTAL_AUDIO_PLAYERS];
};

//------------------------------------------------------------------------------

static struct sl_priv priv;

//------------------------------------------------------------------------------

static qu_result sl_check(void)
{
    memset(&priv, 0, sizeof(struct sl_priv));

    return QU_SUCCESS;
}

static qu_result sl_initialize(void)
{
    if (engine_initialize(&priv.engine) != QU_SUCCESS) {
        return QU_FAILURE;
    }

    if (output_mix_initialize(&priv.outputMix, &priv.engine) != QU_SUCCESS) {
        return QU_FAILURE;
    }

    QU_LOGI("Initialized.\n");

    return QU_SUCCESS;
}

static void sl_terminate(void)
{
    for (int i = 0; i < TOTAL_AUDIO_PLAYERS; i++) {
        if (priv.audioPlayers[i]._flags & AUDIO_PLAYER_FLAG_ACTIVE) {
            audio_player_terminate(&priv.audioPlayers[i]);
        }
    }

    output_mix_terminate(&priv.outputMix);
    engine_terminate(&priv.engine);

    QU_LOGI("Terminated.\n");
}

static void sl_set_master_volume(float volume)
{
    for (int i = 0; i < TOTAL_AUDIO_PLAYERS; i++) {
        if (priv.audioPlayers[i]._flags & AUDIO_PLAYER_FLAG_ACTIVE) {
            audio_player_set_volume(&priv.audioPlayers[i], volume);
        }
    }
}

static qu_result sl_create_source(qu_audio_source *source)
{
    struct sl_audio_player *audioPlayer = NULL;

    for (int i = 0; i < TOTAL_AUDIO_PLAYERS; i++) {
        if (priv.audioPlayers[i]._flags & AUDIO_PLAYER_FLAG_ACTIVE) {
            audioPlayer = &priv.audioPlayers[i];
            break;
        }
    }

    if (!audioPlayer) {
        return QU_FAILURE;
    }

    if (audio_player_initialize(audioPlayer, &priv.engine, &priv.outputMix, source) != QU_SUCCESS) {
        return QU_FAILURE;
    }

    source->priv[0] = (intptr_t) audioPlayer;

    return QU_SUCCESS;
}

static void sl_destroy_source(qu_audio_source *source)
{
    struct sl_audio_player *audioPlayer = (struct sl_audio_player *) source->priv[0];

    audio_player_terminate(audioPlayer);

    source->priv[0] = (intptr_t) 0;
}

static qu_result sl_queue_buffer(qu_audio_source *source, qu_audio_buffer *buffer)
{
    struct sl_audio_player *audioPlayer = (struct sl_audio_player *) source->priv[0];

    return audio_player_enqueue_buffer(audioPlayer, buffer);
}

static int sl_get_queued_buffers(qu_audio_source *source)
{
    struct sl_audio_player *audioPlayer = (struct sl_audio_player *) source->priv[0];

    return audio_player_get_buffer_count(audioPlayer);
}

static bool sl_is_source_used(qu_audio_source *source)
{
    struct sl_audio_player *audioPlayer = (struct sl_audio_player *) source->priv[0];

    return (audioPlayer->_flags & AUDIO_PLAYER_FLAG_EXHAUSTED) == 0;
}

static qu_result sl_start_source(qu_audio_source *source)
{
    struct sl_audio_player *audioPlayer = (struct sl_audio_player *) source->priv[0];

    return audio_player_play(audioPlayer);
}

static qu_result sl_stop_source(qu_audio_source *source)
{
    struct sl_audio_player *audioPlayer = (struct sl_audio_player *) source->priv[0];

    return audio_player_pause(audioPlayer);
}

//------------------------------------------------------------------------------

qu_audio_impl const qu_sles_audio_impl = {
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
