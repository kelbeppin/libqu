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
// qu_audio_openal.c: OpenAL-based audio module
//------------------------------------------------------------------------------

#define QU_MODULE "openal"

#include "qu.h"

#include <al.h>
#include <alc.h>

//------------------------------------------------------------------------------

#ifdef NDEBUG

#define CHECK_AL(_call)     _call

#else

static void _check_al(char const *call, char const *file, int line)
{
    ALenum error = alGetError();

    if (error == AL_NO_ERROR) {
        return;
    }
    
    QU_WARNING("OpenAL error(s) occured in %s:\n", file);
    QU_WARNING("%4d: %s\n", line, call);

    do {
        switch (error) {
        case AL_INVALID_NAME:
            QU_WARNING("-- AL_INVALID_NAME\n");
            break;
        case AL_INVALID_ENUM:
            QU_WARNING("-- AL_INVALID_ENUM\n");
            break;
        case AL_INVALID_VALUE:
            QU_WARNING("-- AL_INVALID_VALUE\n");
            break;
        case AL_INVALID_OPERATION:
            QU_WARNING("-- AL_INVALID_OPERATION\n");
            break;
        case AL_OUT_OF_MEMORY:
            QU_WARNING("-- AL_OUT_OF_MEMORY\n");
            break;
        default:
            QU_WARNING("-- 0x%04x\n", error);
            break;
        }
    } while ((error = alGetError()) != AL_NO_ERROR);
}

#define CHECK_AL(_call) \
    do { \
        _call; \
        _check_al(#_call, __FILE__, __LINE__); \
    } while (0);

#endif

//------------------------------------------------------------------------------

#define PRIV_OPENAL_ID                  (0)
#define PRIV_OPENAL_FORMAT              (1)
#define PRIV_OPENAL_LOOP_FLAG           (2)

//------------------------------------------------------------------------------

struct al_priv
{
    ALCdevice *device;
    ALCcontext *context;
};

//------------------------------------------------------------------------------

static struct al_priv priv;

//------------------------------------------------------------------------------

static qx_result al_check(qu_params const *params)
{
    priv.device = alcOpenDevice(NULL);

    if (!priv.device) {
        QU_WARNING("Failed to open device.\n");
        return QX_FAILURE;
    }

    return QX_SUCCESS;
}

static qx_result al_initialize(qu_params const *params)
{
    priv.context = alcCreateContext(priv.device, NULL);

    if (!priv.context) {
        QU_ERROR("Failed to create context.\n");
        return QX_FAILURE;
    }

    if (!alcMakeContextCurrent(priv.context)) {
        QU_ERROR("Failed to activate context.\n");
        return QX_FAILURE;
    }

    QU_INFO("Initialized.\n");

    QU_INFO("AL_VENDOR: %s\n", alGetString(AL_VENDOR));
    QU_INFO("AL_VERSION: %s\n", alGetString(AL_VERSION));
    QU_INFO("AL_RENDERER: %s\n", alGetString(AL_RENDERER));
    QU_INFO("AL_EXTENSIONS: %s\n", alGetString(AL_EXTENSIONS));

    return QX_SUCCESS;
}

static void al_terminate(void)
{
    alcDestroyContext(priv.context);
    alcCloseDevice(priv.device);

    QU_INFO("Terminated.\n");
}

//------------------------------------------------------------------------------

static void al_set_master_volume(float volume)
{
    CHECK_AL(alListenerf(AL_GAIN, volume));
}

//------------------------------------------------------------------------------

static qx_result al_create_source(qx_audio_source *source)
{
    ALenum format;

    if (source->channels == 1) {
        format = AL_FORMAT_MONO16;
    } else if (source->channels == 2) {
        format = AL_FORMAT_STEREO16;
    } else {
        return QX_FAILURE;
    }

    ALuint alSource;
    CHECK_AL(alGenSources(1, &alSource));

    QU_DEBUG("al_create_source -> %d\n", alSource);

    if (alSource == 0) {
        return QX_FAILURE;
    }

    source->priv[PRIV_OPENAL_ID] = (intptr_t) alSource;
    source->priv[PRIV_OPENAL_FORMAT] = (intptr_t) format;

    return QX_SUCCESS;
}

static void al_destroy_source(qx_audio_source *source)
{
    ALuint alSource = (ALuint) source->priv[PRIV_OPENAL_ID];

    CHECK_AL(alSourceStop(alSource));

    ALint buffersQueued;
    CHECK_AL(alGetSourcei(alSource, AL_BUFFERS_QUEUED, &buffersQueued));

    QU_DEBUG("destroy_source: buffersQueued = %d\n", buffersQueued);

    if (buffersQueued > 0) {
        ALuint *buffers = malloc(sizeof(ALuint) * buffersQueued);

        if (buffers) {
            CHECK_AL(alSourceUnqueueBuffers(alSource, buffersQueued, buffers));
            CHECK_AL(alDeleteBuffers(buffersQueued, buffers));
            free(buffers);
        }
    }

    CHECK_AL(alDeleteSources(1, &alSource));
}

static qx_result al_queue_buffer(qx_audio_source *source, qx_audio_buffer *buffer)
{
    ALuint alSource = (ALuint) source->priv[PRIV_OPENAL_ID];
    ALenum format = (ALenum) source->priv[PRIV_OPENAL_FORMAT];

    ALsizei bytes = sizeof(int16_t) * buffer->samples;
    ALsizei sampleRate = source->sample_rate;

    ALuint alBuffer;
    CHECK_AL(alGenBuffers(1, &alBuffer));

    if (alBuffer == 0) {
        return QX_FAILURE;
    }

    CHECK_AL(alBufferData(alBuffer, format, buffer->data, bytes, sampleRate));

    if (source->loop == -1) {
        if (source->priv[PRIV_OPENAL_LOOP_FLAG] == 1) {
            return QX_FAILURE;
        }

        CHECK_AL(alSourcei(alSource, AL_BUFFER, alBuffer));
        CHECK_AL(alSourcei(alSource, AL_LOOPING, AL_TRUE));

        source->priv[PRIV_OPENAL_LOOP_FLAG] = 1;

        return QX_SUCCESS;
    }

    ALint buffersProcessed;
    CHECK_AL(alGetSourcei(alSource, AL_BUFFERS_PROCESSED, &buffersProcessed));

    int buffersToRemove = QU_MIN(8, buffersProcessed);

    if (buffersToRemove > 0) {
        ALuint alUnqueuedBuffers[8];

        CHECK_AL(alSourceUnqueueBuffers(alSource, buffersToRemove, alUnqueuedBuffers));
        CHECK_AL(alDeleteBuffers(buffersToRemove, alUnqueuedBuffers));
    }

    CHECK_AL(alSourceQueueBuffers(alSource, 1, &alBuffer));

    return QX_SUCCESS;
}

static int al_get_queued_buffers(qx_audio_source *source)
{
    ALuint alSource = (ALuint) source->priv[PRIV_OPENAL_ID];

    ALint buffersQueued;
    ALint buffersProcessed;

    CHECK_AL(alGetSourcei(alSource, AL_BUFFERS_QUEUED, &buffersQueued));
    CHECK_AL(alGetSourcei(alSource, AL_BUFFERS_PROCESSED, &buffersProcessed));

    return buffersQueued - buffersProcessed;
}

static bool al_is_source_used(qx_audio_source *source)
{
    ALuint alSource = (ALuint) source->priv[PRIV_OPENAL_ID];

    ALint state;
    CHECK_AL(alGetSourcei(alSource, AL_SOURCE_STATE, &state));

    if (state == AL_PLAYING || state == AL_PAUSED) {
        return true;
    }

    return false;
}

static qx_result al_start_source(qx_audio_source *source)
{
    ALuint alSource = (ALuint) source->priv[PRIV_OPENAL_ID];
    CHECK_AL(alSourcePlay(alSource));

    ALint state;
    CHECK_AL(alGetSourcei(alSource, AL_SOURCE_STATE, &state));

    if (state == AL_PLAYING) {
        return QX_SUCCESS;
    }

    return QX_FAILURE;
}

static qx_result al_stop_source(qx_audio_source *source)
{
    ALuint alSource = (ALuint) source->priv[PRIV_OPENAL_ID];
    CHECK_AL(alSourcePause(alSource));

    ALint state;
    CHECK_AL(alGetSourcei(alSource, AL_SOURCE_STATE, &state));

    if (state == AL_PAUSED) {
        return QX_SUCCESS;
    }

    return QX_FAILURE;
}

//------------------------------------------------------------------------------

qx_audio_impl const qx_audio_openal = {
    .check = al_check,
    .initialize = al_initialize,
    .terminate = al_terminate,
    .set_master_volume = al_set_master_volume,
    .create_source = al_create_source,
    .destroy_source = al_destroy_source,
    .is_source_used = al_is_source_used,
    .queue_buffer = al_queue_buffer,
    .get_queued_buffers = al_get_queued_buffers,
    .start_source = al_start_source,
    .stop_source = al_stop_source,
};
