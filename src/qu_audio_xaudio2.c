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

struct Sound
{
    void *pData;
    size_t Size;
    WAVEFORMATEX Format;
};

struct BGM
{
    qu_file *pFile;
    qu_sound_reader *pSndReader;
    struct Stream *pStream;
};

struct Stream
{
    int Index;
    int Gen;
    int Type;

    IXAudio2SourceVoice *pSourceVoice;
    int VoiceState;

    HANDLE hThread;
    bool fShouldRelease;
    bool fLoop;
};

static CRITICAL_SECTION g_CriticalSection;

static IXAudio2 *g_pXAudio2;
static IXAudio2MasteringVoice *g_pMasteringVoice;

static qu_array *g_pSoundArray;
static qu_array *g_pBGMArray;
static struct Stream g_StreamArray[TOTAL_STREAMS];

//------------------------------------------------------------------------------

static void ReleaseStream(struct Stream *pStream);

//------------------------------------------------------------------------------
// Sound XAudio2 callbacks

static void Sound_OnVoiceProcessingPassStart(IXAudio2VoiceCallback *self, UINT32 samplesRequired)
{
}

static void Sound_OnVoiceProcessingPassEnd(IXAudio2VoiceCallback *self)
{
}

static void Sound_OnStreamEnd(IXAudio2VoiceCallback *self)
{
}

static void Sound_OnBufferStart(IXAudio2VoiceCallback *self, void *pBufferContext)
{
}

static void Sound_OnBufferEnd(IXAudio2VoiceCallback *self, void *pBufferContext)
{
    struct Stream *pStream = (struct Stream *) pBufferContext;

    if (pStream) {
        ReleaseStream(pStream);
    }
}

static void Sound_OnLoopEnd(IXAudio2VoiceCallback *self, void *pBufferContext)
{
}

static void Sound_OnVoiceError(IXAudio2VoiceCallback *self, void* pBufferContext, HRESULT hError)
{
}

static IXAudio2VoiceCallback g_SoundVoiceCallback = {
    .lpVtbl = &(IXAudio2VoiceCallbackVtbl) {
        .OnVoiceProcessingPassStart = Sound_OnVoiceProcessingPassStart,
        .OnVoiceProcessingPassEnd = Sound_OnVoiceProcessingPassEnd,
        .OnStreamEnd = Sound_OnStreamEnd,
        .OnBufferStart = Sound_OnBufferStart,
        .OnBufferEnd = Sound_OnBufferEnd,
        .OnLoopEnd = Sound_OnLoopEnd,
        .OnVoiceError = Sound_OnVoiceError,
    },
};

//------------------------------------------------------------------------------
// Music XAudio2 callbacks

static void BGM_OnVoiceProcessingPassEnd(IXAudio2VoiceCallback *self)
{
}

static void BGM_OnVoiceProcessingPassStart(IXAudio2VoiceCallback *self, UINT32 samplesRequired)
{
}

static void BGM_OnStreamEnd(IXAudio2VoiceCallback *self)
{
}

static void BGM_OnBufferStart(IXAudio2VoiceCallback *self, void *pBufferContext)
{
}

static void BGM_OnBufferEnd(IXAudio2VoiceCallback *self, void *pBufferContext)
{
}

static void BGM_OnLoopEnd(IXAudio2VoiceCallback *self, void *pBufferContext)
{
}

static void BGM_OnVoiceError(IXAudio2VoiceCallback *self, void* pBufferContext, HRESULT hError)
{
}

static IXAudio2VoiceCallback g_BGMVoiceCallback = {
    .lpVtbl = &(IXAudio2VoiceCallbackVtbl) {
        .OnVoiceProcessingPassStart = BGM_OnVoiceProcessingPassStart,
        .OnVoiceProcessingPassEnd = BGM_OnVoiceProcessingPassEnd,
        .OnStreamEnd = BGM_OnStreamEnd,
        .OnBufferStart = BGM_OnBufferStart,
        .OnBufferEnd = BGM_OnBufferEnd,
        .OnLoopEnd = BGM_OnLoopEnd,
        .OnVoiceError = BGM_OnVoiceError,
    },
};

//------------------------------------------------------------------------------

static void SndDtor(void *pData)
{
    struct Sound *pSound = (struct Sound *) pData;

    free(pSound->pData);
}

static void BGMDtor(void *pData)
{
    struct BGM *pMusic = (struct BGM *) pData;

    if (pMusic->pStream) {
        EnterCriticalSection(&g_CriticalSection);
        pMusic->pStream->fShouldRelease = true;
        LeaveCriticalSection(&g_CriticalSection);

        WaitForSingleObject(pMusic->pStream->hThread, INFINITE);
    }

    qu_close_sound_reader(pMusic->pSndReader);
    qu_fclose(pMusic->pFile);
}

static void SetWaveFormat(WAVEFORMATEX *pFormat, qu_sound_reader *pSndReader)
{
    pFormat->wFormatTag = WAVE_FORMAT_PCM;
    pFormat->nChannels = pSndReader->num_channels;
    pFormat->nSamplesPerSec = pSndReader->sample_rate;
    pFormat->nAvgBytesPerSec = pSndReader->num_channels * pSndReader->sample_rate * 2;
    pFormat->nBlockAlign = (pSndReader->num_channels * 16) / 8;
    pFormat->wBitsPerSample = 16;
}

static int32_t EncodeStreamId(struct Stream *pStream)
{
    return (pStream->Gen << STREAM_GEN_SHIFT) | 0x0000CC00 | pStream->Index;
}

static struct Stream *GetStreamById(int32_t streamId)
{
    // Check if stream id is valid and if it is, return corresponding
    // stream pointer.

    if ((streamId & STREAM_CHECK_MASK) != STREAM_CHECK_MASK) {
        return NULL;
    }

    int index = streamId & STREAM_INDEX_MASK;
    int gen = (streamId >> STREAM_GEN_SHIFT) & STREAM_GEN_MASK;

    if (index < 0 || index >= TOTAL_STREAMS) {
        return NULL;
    }

    if (g_StreamArray[index].Gen != gen) {
        return NULL;
    }

    return &g_StreamArray[index];
}

static void ReleaseStream(struct Stream *pStream)
{
    // Free stream for future use.

    IXAudio2SourceVoice_DestroyVoice(pStream->pSourceVoice);

    pStream->Gen = (pStream->Gen + 1) & STREAM_GEN_MASK;
    pStream->Type = STREAM_INACTIVE;
    pStream->pSourceVoice = NULL;
    pStream->VoiceState = STREAM_VOICE_STOPPED;
}

static struct Stream *FindFreeStream(void)
{
    // Find an unused audio stream.

    for (int i = 0; i < TOTAL_STREAMS; i++) {
        struct Stream *pStream = &g_StreamArray[i];

        if (pStream->Type == STREAM_INACTIVE) {
            return pStream;
        }
    }

    return NULL;
}

static int32_t StartStaticStream(int32_t soundId, bool fLoop)
{
    HRESULT hResult;

    // Get sound resource first.

    struct Sound *pSound = qu_array_get(g_pSoundArray, soundId);

    if (!pSound) {
        return 0;
    }

    // Then find an unused stream.

    struct Stream *pStream = FindFreeStream();

    if (!pStream) {
        return 0;
    }

    // Create new voice to play.
    // [?] Should we create them up front?

    hResult = IXAudio2_CreateSourceVoice(g_pXAudio2,
                                         &pStream->pSourceVoice,
                                         &pSound->Format,
                                         0,
                                         XAUDIO2_DEFAULT_FREQ_RATIO,
                                         &g_SoundVoiceCallback, NULL, NULL);

    if (FAILED(hResult)) {
        QU_ERROR("IXAudio2::CreateSourceVoice() failed [0x%04x].\n", hResult);
        return 0;
    }

    // Set up XAudio2 buffer and queue it into voice.
    // Sounds in libquack contain a single buffer.

    XAUDIO2_BUFFER Buffer = { 0 };

    Buffer.pAudioData = pSound->pData;
    Buffer.AudioBytes = pSound->Size;
    Buffer.Flags = XAUDIO2_END_OF_STREAM;
    Buffer.pContext = pStream;

    if (fLoop) {
        Buffer.LoopBegin = 0;
        Buffer.LoopLength = 0;
        Buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    }

    hResult = IXAudio2SourceVoice_SubmitSourceBuffer(pStream->pSourceVoice,
                                                     &Buffer,
                                                     NULL);

    if (FAILED(hResult)) {
        QU_ERROR("IXAudio2SourceVoice::SubmitSourceBuffer() failed [0x%04x].\n", hResult);
        IXAudio2SourceVoice_DestroyVoice(pStream->pSourceVoice);
        return 0;
    }

    // Set correct type of the stream and start playing voice.

    pStream->Type = STREAM_STATIC;
    pStream->VoiceState = STREAM_VOICE_STARTED;

    IXAudio2SourceVoice_Start(pStream->pSourceVoice, 0, XAUDIO2_COMMIT_NOW);

    return pStream ? EncodeStreamId(pStream) : 0;
}

static void StopStaticStream(struct Stream *pStream)
{
    IXAudio2SourceVoice_Stop(pStream->pSourceVoice, 0, XAUDIO2_COMMIT_NOW);
    ReleaseStream(pStream);
}

static int FillBuffer(IXAudio2SourceVoice *pSourceVoice,
                      qu_sound_reader *pSndReader, int16_t *pSampleData, bool fLoop)
{
    // Read audio data from a file to memory buffer.

    int64_t samplesRead = qu_sound_read(pSndReader, pSampleData, BGM_BUFFER_LENGTH);

    if (samplesRead == 0) {
        if (fLoop) {
            qu_sound_seek(pSndReader, 0);
            samplesRead = qu_sound_read(pSndReader, pSampleData, BGM_BUFFER_LENGTH);

            if (samplesRead == 0) {
                return -1;
            }
        } else {
            return -1;
        }
    }

    XAUDIO2_BUFFER buffer = { 0 };

    buffer.pAudioData = pSampleData;
    buffer.AudioBytes = samplesRead * sizeof(int16_t);

    HRESULT hResult = IXAudio2SourceVoice_SubmitSourceBuffer(pSourceVoice, &buffer, NULL);

    if (FAILED(hResult)) {
        return -1;
    }

    return 0;
}

static DWORD WINAPI BGMThread(LPVOID lpParam)
{
    struct BGM *pMusic = (struct BGM *) lpParam;

    // Determine audio format.
    WAVEFORMATEX format = { 0 };
    SetWaveFormat(&format, pMusic->pSndReader);

    // Create source voice.
    HRESULT hResult = IXAudio2_CreateSourceVoice(
        g_pXAudio2,
        &pMusic->pStream->pSourceVoice,
        &format,
        0,
        XAUDIO2_DEFAULT_FREQ_RATIO,
        &g_BGMVoiceCallback, NULL, NULL
    );

    if (FAILED(hResult)) {
        return 1;
    }

    // Allocate sample buffers.

    int16_t *sampleData[BGM_TOTAL_BUFFERS];

    for (int i = 0; i < BGM_TOTAL_BUFFERS; i++) {
        sampleData[i] = malloc(BGM_BUFFER_LENGTH * sizeof(int16_t));
    }

    // Rewind audio track. Just in case.

    qu_sound_seek(pMusic->pSndReader, 0);

    // Pre-load first buffer and start source voice.

    int nCurrentBuffer = 0;
    FillBuffer(pMusic->pStream->pSourceVoice, pMusic->pSndReader,
               sampleData[nCurrentBuffer], pMusic->pStream->fLoop);

    IXAudio2SourceVoice_Start(pMusic->pStream->pSourceVoice, 0, XAUDIO2_COMMIT_NOW);
    pMusic->pStream->VoiceState = STREAM_VOICE_STARTED;

    // Main BGM loop.

    bool fKeepRunning = true;

    while (fKeepRunning) {
        // Check if the stream should be paused or even stopped and act accordingly.

        EnterCriticalSection(&g_CriticalSection);

        bool fPaused = (pMusic->pStream->VoiceState == STREAM_VOICE_STOPPED);
        bool fStopped = pMusic->pStream->fShouldRelease;

        LeaveCriticalSection(&g_CriticalSection);

        if (fStopped) {
            break;
        }
        
        if (fPaused) {
            Sleep(10);
            continue;
        }

        // Check the source voice state.
        XAUDIO2_VOICE_STATE state = { 0 };
        IXAudio2SourceVoice_GetState(pMusic->pStream->pSourceVoice, &state,
                                     XAUDIO2_VOICE_NOSAMPLESPLAYED);

        // Load one buffer at a time.
        if (state.BuffersQueued < BGM_TOTAL_BUFFERS) {
            nCurrentBuffer = (nCurrentBuffer + 1) % BGM_TOTAL_BUFFERS;
            int status = FillBuffer(pMusic->pStream->pSourceVoice,
                                    pMusic->pSndReader,
                                    sampleData[nCurrentBuffer],
                                    pMusic->pStream->fLoop);

            // Exit if we've reached the end of file.
            if (status == -1) {
                fKeepRunning = false;
            }
        }

        // Don't overload CPU.
        Sleep(10);
    }

    // Free memory we allocated for sample buffers.
    for (int i = 0; i < BGM_TOTAL_BUFFERS; i++) {
        free(sampleData[i]);
    }

    // Mark stream as free.
    ReleaseStream(pMusic->pStream);
    pMusic->pStream = NULL;

    return 0;
}

static int32_t StartDynamicStream(int32_t musicId, bool fLoop)
{
    struct BGM *pMusic = qu_array_get(g_pBGMArray, musicId);

    if (!pMusic) {
        return 0;
    }

    if (pMusic->pStream) {
        QU_WARNING("Music track can't be played more than once at a time.\n");
        return EncodeStreamId(pMusic->pStream);
    }

    pMusic->pStream = FindFreeStream();

    if (pMusic->pStream) {
        pMusic->pStream->Type = STREAM_DYNAMIC;
        pMusic->pStream->hThread = CreateThread(
            NULL,
            0,
            BGMThread,
            pMusic,
            0,
            NULL
        );

        pMusic->pStream->fLoop = fLoop;
        pMusic->pStream->fShouldRelease = false;
    }

    return pMusic->pStream ? EncodeStreamId(pMusic->pStream) : 0;
}

static void StopDynamicStream(struct Stream *pStream)
{
    EnterCriticalSection(&g_CriticalSection);

    pStream->fShouldRelease = true;

    LeaveCriticalSection(&g_CriticalSection);
}

//------------------------------------------------------------------------------

static bool query(qu_params const *params)
{
    return true;
}

static void initialize(qu_params const *params)
{
    HRESULT hResult;

    // Initialize COM. It's OK to call CoInitialize() multiple times
    // as long as there is corresponding CoUninitialize() call.

    hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if (FAILED(hResult)) {
        QU_HALT("Failed to initialize COM [0x%08x].", hResult);
    }

    // Initialize mutex.

    InitializeCriticalSection(&g_CriticalSection);

    // Create XAudio2 engine.

    hResult = XAudio2Create(&g_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);

    if (FAILED(hResult)) {
        QU_HALT("Failed to create XAudio2 engine instance [0x%08x].", hResult);
    }

    // Create mastering voice.

    hResult = IXAudio2_CreateMasteringVoice(g_pXAudio2, &g_pMasteringVoice,
                                            XAUDIO2_DEFAULT_CHANNELS,
                                            XAUDIO2_DEFAULT_SAMPLERATE,
                                            0, NULL, NULL,
                                            AudioCategory_GameEffects);

    if (FAILED(hResult)) {
        QU_HALT("Failed to create XAudio2 mastering voice [0x%08x].", hResult);
    }

    // Initialize sound and music dynamic arrays.

    g_pSoundArray = qu_create_array(sizeof(struct Sound), SndDtor);
    g_pBGMArray = qu_create_array(sizeof(struct BGM), BGMDtor);

    if (!g_pSoundArray || !g_pBGMArray) {
        QU_HALT("qu_create_array() failed.");
    }

    // Initialize audio streams.
    // There is fixed amount of them.

    for (int i = 0; i < TOTAL_STREAMS; i++) {
        g_StreamArray[i].Index = i;
        g_StreamArray[i].Gen = 0;
        g_StreamArray[i].Type = STREAM_INACTIVE;
    }

    // Done.

    QU_INFO("XAudio2 audio module initialized.\n");
}

static void terminate(void)
{
    // Destroy remaining voices.

    for (int i = 0; i < TOTAL_STREAMS; i++) {
        if (g_StreamArray[i].pSourceVoice) {
            IXAudio2SourceVoice_DestroyVoice(g_StreamArray[i].pSourceVoice);
        }
    }

    // Free sound and music arrays.

    qu_destroy_array(g_pSoundArray);
    qu_destroy_array(g_pBGMArray);

    // Terminate XAudio2 and COM, destroy mutex.

    IXAudio2MasteringVoice_DestroyVoice(g_pMasteringVoice);
    IXAudio2_Release(g_pXAudio2);
    DeleteCriticalSection(&g_CriticalSection);
    CoUninitialize();

    // Done.

    QU_INFO("XAudio2 audio module terminated.\n");
}

//------------------------------------------------------------------------------

static void set_master_volume(float volume)
{
    IXAudio2MasteringVoice_SetVolume(g_pMasteringVoice, volume, XAUDIO2_COMMIT_NOW);
}

//------------------------------------------------------------------------------

static int32_t load_sound(qu_file *pFile)
{
    // Open sound reader for a given file.

    qu_sound_reader *pSndReader = qu_open_sound_reader(pFile);

    if (!pSndReader) {
        // TODO: better error reports
        QU_ERROR("Can't load sound: sound reader is NULL.\n");
        return 0;
    }

    // Decode entire audio file into memory buffer.

    size_t length = pSndReader->num_channels * pSndReader->num_samples * sizeof(int16_t);
    void *data = malloc(length);

    if (!data) {
        QU_HALT("Out of memory.");
    }

    qu_sound_read(pSndReader, data, pSndReader->num_samples);

    // Create sound object and put it into the array.

    struct Sound sound = { 0 };

    sound.pData = data;
    sound.Size = length;

    SetWaveFormat(&sound.Format, pSndReader);

    qu_close_sound_reader(pSndReader);

    return qu_array_add(g_pSoundArray, &sound);
}

static void delete_sound(int32_t soundId)
{
    qu_array_remove(g_pSoundArray, soundId);
}

static int32_t play_sound(int32_t soundId)
{
    return StartStaticStream(soundId, false);
}

static int32_t loop_sound(int32_t soundId)
{
    return StartStaticStream(soundId, true);
}

//------------------------------------------------------------------------------

static int32_t open_music(qu_file *pFile)
{
    qu_sound_reader *pSndReader = qu_open_sound_reader(pFile);

    if (!pSndReader) {
        // TODO: better error reports
        QU_ERROR("Can't open music: sound reader is NULL.\n");
        return 0;
    }

    struct BGM music = { 0 };

    music.pFile = pFile;
    music.pSndReader = pSndReader;

    return qu_array_add(g_pBGMArray, &music);
}

static void close_music(int32_t musicId)
{
    qu_array_remove(g_pBGMArray, musicId);
}

static int32_t play_music(int32_t musicId)
{
    return StartDynamicStream(musicId, false);
}

static int32_t loop_music(int32_t musicId)
{
    return StartDynamicStream(musicId, true);
}

//------------------------------------------------------------------------------

static void pause_stream(int32_t streamId)
{
    struct Stream *pStream = GetStreamById(streamId);

    EnterCriticalSection(&g_CriticalSection);

    if (pStream && pStream->VoiceState == STREAM_VOICE_STARTED) {
        IXAudio2SourceVoice_Stop(pStream->pSourceVoice, 0, XAUDIO2_COMMIT_NOW);
        pStream->VoiceState = STREAM_VOICE_STOPPED;
    }

    LeaveCriticalSection(&g_CriticalSection);
}

static void unpause_stream(int32_t streamId)
{
    struct Stream *pStream = GetStreamById(streamId);

    EnterCriticalSection(&g_CriticalSection);

    if (pStream && pStream->VoiceState == STREAM_VOICE_STOPPED) {
        IXAudio2SourceVoice_Start(pStream->pSourceVoice, 0, XAUDIO2_COMMIT_NOW);
        pStream->VoiceState = STREAM_VOICE_STARTED;
    }

    LeaveCriticalSection(&g_CriticalSection);
}

static void stop_stream(int32_t streamId)
{
    struct Stream *pStream = GetStreamById(streamId);

    if (pStream) {
        if (pStream->Type == STREAM_STATIC) {
            StopStaticStream(pStream);
        } else if (pStream->Type == STREAM_DYNAMIC) {
            StopDynamicStream(pStream);
        }
    }
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
