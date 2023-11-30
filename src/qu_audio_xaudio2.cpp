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

extern "C" {
#include "qu.h"
}

#include <xaudio2.h>

//------------------------------------------------------------------------------
// qu_audio_xaudio2.cpp: XAudio2-based audio module
//------------------------------------------------------------------------------

#define TOTAL_STREAMS               64

#define STREAM_INACTIVE             0
#define STREAM_STATIC               1
#define STREAM_DYNAMIC              2

#define STREAM_CHECK_MASK           0x0000CC00
#define STREAM_INDEX_MASK           0xFF
#define STREAM_GEN_MASK             0x7FFF
#define STREAM_GEN_SHIFT            16

#define STREAM_VOICE_STOPPED        0
#define STREAM_VOICE_STARTED        1

#define BGM_TOTAL_BUFFERS           4
#define BGM_BUFFER_LENGTH           8192

//------------------------------------------------------------------------------

typedef HRESULT (APIENTRY *PFNXAUDIO2CREATEPROC)(IXAudio2 **, UINT32, XAUDIO2_PROCESSOR);

struct xa2_module
{
    char const *name;
    int major;
    int minor;
};

static struct xa2_module const xa2_modules[] = {
    { "XAudio2_9.dll", 2, 9 },
    { "XAudio2_9redist.dll", 2, 9 },
    { "XAudio2_8.dll", 2, 8 },
};

//------------------------------------------------------------------------------

class xa2_engine_callback : public IXAudio2EngineCallback
{
public:
    virtual void WINAPI OnProcessingPassStart()
    {
    }

    virtual void WINAPI OnProcessingPassEnd()
    {
    }

    virtual void WINAPI OnCriticalError(HRESULT error)
    {
    }
};

class xa2_source_callback : public IXAudio2VoiceCallback
{
public:
    virtual void WINAPI OnVoiceProcessingPassStart(UINT32 bytesRequired)
    {
    }

    virtual void WINAPI OnVoiceProcessingPassEnd()
    {
    }

    virtual void WINAPI OnStreamEnd()
    {
    }

    virtual void WINAPI OnBufferStart(void *context)
    {
    }

    virtual void WINAPI OnBufferEnd(void *context)
    {
    }

    virtual void WINAPI OnLoopEnd(void *context)
    {
    }

    virtual void WINAPI OnVoiceError(void *context, HRESULT error)
    {
    }
};

//------------------------------------------------------------------------------

struct xa2_priv
{
    void *library;
    PFNXAUDIO2CREATEPROC XAudio2Create;

    IXAudio2 *engine;
    IXAudio2MasteringVoice *mastering_voice;

    xa2_engine_callback engine_callback;
    xa2_source_callback source_callback;
};

//------------------------------------------------------------------------------

static struct xa2_priv priv;

//------------------------------------------------------------------------------

static qu_result xa2_check(void)
{
    for (int i = 0; i < ARRAYSIZE(xa2_modules); i++) {
        struct xa2_module const *module = &xa2_modules[i];
        priv.library = pl_open_dll(module->name);
        // priv.libver = module->major * 100 + module->minor;

        if (!priv.library) {
            QU_LOGW("Unable to load %s.\n", module->name);
            continue;
        }

        QU_LOGI("Loaded %s.\n", module->name);

        priv.XAudio2Create = (PFNXAUDIO2CREATEPROC)
            pl_get_dll_proc(priv.library, "XAudio2Create");

        if (!priv.XAudio2Create) {
            QU_LOGE("XAudio2Create() procedure is not found in %s.\n", module->name);
            continue;
        }

        QU_LOGI("XAudio %d.%d is found in %s.\n", module->major, module->minor, module->name);
        return QU_SUCCESS;
    }

    QU_LOGE("XAudio2 is not found.\n");
    return QU_FAILURE;
}

static qu_result xa2_initialize(void)
{
    HRESULT hResult;

    // Initialize COM. It's OK to call CoInitialize() multiple times
    // as long as there is corresponding CoUninitialize() call.

    hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if (FAILED(hResult)) {
        QU_LOGE("Failed to initialize COM [0x%08x].\n", hResult);
        return QU_FAILURE;
    }

    // Create XAudio2 engine.

    hResult = priv.XAudio2Create(&priv.engine, 0, XAUDIO2_DEFAULT_PROCESSOR);

    if (FAILED(hResult)) {
        QU_LOGE("Failed to create XAudio2 engine instance [0x%08x].\n", hResult);
        return QU_FAILURE;
    }

    priv.engine->RegisterForCallbacks(&priv.engine_callback);

    // Create mastering voice.

    hResult = priv.engine->CreateMasteringVoice(
        &priv.mastering_voice,
        XAUDIO2_DEFAULT_CHANNELS,
        XAUDIO2_DEFAULT_SAMPLERATE,
        0, NULL, NULL,
        AudioCategory_GameEffects
    );

    if (FAILED(hResult)) {
        QU_LOGE("Failed to create XAudio2 mastering voice [0x%08x].\n", hResult);
        return QU_FAILURE;
    }

    // Done.
    QU_LOGI("Initialized.\n");
    return QU_SUCCESS;
}

static void xa2_terminate(void)
{
    // Terminate XAudio2 and COM, destroy mutex.
    priv.mastering_voice->DestroyVoice();
    priv.engine->Release();
    CoUninitialize();

    // Done.
    QU_LOGI("Terminated.\n");
}

//------------------------------------------------------------------------------

static void xa2_set_master_volume(float volume)
{
    priv.mastering_voice->SetVolume(volume);
}

//------------------------------------------------------------------------------

static qu_result xa2_create_source(qu_audio_source *source)
{
    WAVEFORMATEX wfex = { 0 };

    wfex.wFormatTag = WAVE_FORMAT_PCM;
    wfex.nChannels = source->channels;
    wfex.nSamplesPerSec = source->sample_rate;
    wfex.nAvgBytesPerSec = source->channels * source->sample_rate * 2;
    wfex.nBlockAlign = (source->channels * 16) / 8;
    wfex.wBitsPerSample = 16;

    IXAudio2SourceVoice *x_source;

    HRESULT result = priv.engine->CreateSourceVoice(
        &x_source,                  // where to store new source pointer
        &wfex,                      // pointer to format table
        0,                          // flags
        XAUDIO2_DEFAULT_FREQ_RATIO, // maximum frequency ratio
        &priv.source_callback       // pointer to callback class instance
    );

    if (FAILED(result)) {
        return QU_FAILURE;
    }

    source->priv[0] = reinterpret_cast<intptr_t>(x_source);

    return QU_SUCCESS;
}

static void xa2_destroy_source(qu_audio_source *source)
{
    IXAudio2SourceVoice *x_source = reinterpret_cast<IXAudio2SourceVoice *>(source->priv[0]);

    x_source->DestroyVoice();
}

static bool xa2_is_source_used(qu_audio_source *source)
{
    IXAudio2SourceVoice *x_source = reinterpret_cast<IXAudio2SourceVoice *>(source->priv[0]);

    XAUDIO2_VOICE_STATE state = { 0 };
    x_source->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

    if (state.BuffersQueued > 0) {
        return true;
    }

    return false;
}

static qu_result xa2_queue_buffer(qu_audio_source *source, qu_audio_buffer *buffer)
{
    IXAudio2SourceVoice *x_source = reinterpret_cast<IXAudio2SourceVoice *>(source->priv[0]);

    XAUDIO2_BUFFER x_buffer = { 0 };

    x_buffer.AudioBytes = sizeof(int16_t) * buffer->samples;
    x_buffer.pAudioData = reinterpret_cast<BYTE *>(buffer->data);
    x_buffer.pContext = source;

    if (source->loop == -1) {
        if (source->priv[1] == 1) {
            return QU_FAILURE;
        }

        x_buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
        source->priv[1] = 1;
    }

    HRESULT result = x_source->SubmitSourceBuffer(&x_buffer);

    if (FAILED(result)) {
        return QU_FAILURE;
    }

    return QU_SUCCESS;
}

static int xa2_get_queued_buffers(qu_audio_source *source)
{
    IXAudio2SourceVoice *x_source = reinterpret_cast<IXAudio2SourceVoice *>(source->priv[0]);

    XAUDIO2_VOICE_STATE state = { 0 };
    x_source->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

    return state.BuffersQueued;
}

static qu_result xa2_start_source(qu_audio_source *source)
{
    IXAudio2SourceVoice *x_source = reinterpret_cast<IXAudio2SourceVoice *>(source->priv[0]);
    QU_LOGD("start_source(): %p\n", x_source);
    return SUCCEEDED(x_source->Start()) ? QU_SUCCESS : QU_FAILURE;
}

static qu_result xa2_stop_source(qu_audio_source *source)
{
    IXAudio2SourceVoice *x_source = reinterpret_cast<IXAudio2SourceVoice *>(source->priv[0]);
    QU_LOGD("stop_source(): %p\n", x_source);
    return SUCCEEDED(x_source->Stop()) ? QU_SUCCESS : QU_FAILURE;
}

//------------------------------------------------------------------------------

qu_audio_impl const qu_xaudio2_audio_impl = {
    xa2_check,
    xa2_initialize,
    xa2_terminate,
    xa2_set_master_volume,
    xa2_create_source,
    xa2_destroy_source,
    xa2_is_source_used,
    xa2_queue_buffer,
    xa2_get_queued_buffers,
    xa2_start_source,
    xa2_stop_source,
};
