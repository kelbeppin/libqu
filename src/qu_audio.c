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

#ifdef QU_ANDROID
    &qx_audio_sles,
#endif

#ifdef QU_USE_OPENAL
    &qx_audio_openal,
#endif

    &qx_audio_null,
};

//------------------------------------------------------------------------------

#define MUSIC_BUFFER_LENGTH             (4096)
#define TOTAL_MUSIC_BUFFERS             (8)

#define MAX_VOICES                      (64)

#define VOICE_STATE_INACTIVE            (0)
#define VOICE_STATE_PLAYING             (1)
#define VOICE_STATE_PAUSED              (2)
#define VOICE_STATE_DESTROYED           (3)

#define VOICE_TYPE_NONE                 (0)
#define VOICE_TYPE_SOUND                (1)
#define VOICE_TYPE_MUSIC                (2)

//------------------------------------------------------------------------------

struct sound
{
    int channels;
    int sample_rate;
    qx_audio_buffer buffer;
};

struct music
{
    struct qx_wave wave;
    struct voice *voice;
    qu_thread *thread;
    int loop_count;
};

struct voice
{
    int index;
    int gen;
    int type;
    int state;
    qx_audio_source source;
};

struct audio_priv
{
    struct qx_audio_impl const *impl;
    qu_mutex *mutex;

    qu_array *sounds;
    qu_array *music;
    struct voice voices[MAX_VOICES];
};

//------------------------------------------------------------------------------

static struct audio_priv priv;

//------------------------------------------------------------------------------

static void sound_dtor(void *ptr)
{
    struct sound *sound = ptr;

    free(sound->buffer.data);
}

static void music_dtor(void *ptr)
{
    struct music *music = (struct music *) ptr;

    // If the music is playing, wait until thread ends.
    if (music->thread) {
        qu_wait_thread(music->thread);
    }

    // Close sound reader.
    qx_close_wave(&music->wave);
}

//------------------------------------------------------------------------------

static struct voice *find_voice(void)
{
    struct voice *voice = NULL;

    for (int i = 0; i < MAX_VOICES; i++) {
        int type = priv.voices[i].type;
        int state = priv.voices[i].state;

        // This voice is doing something on the background, skip it.
        if (type == VOICE_TYPE_MUSIC) {
            continue;
        }

        // This voice is fresh (unused) or was stopped manually, can be reused.
        if (type == VOICE_TYPE_NONE || state == VOICE_STATE_DESTROYED) {
            voice = &priv.voices[i];
            break;
        }

        // This voice is probably reached its end and stopped, can be reused.
        if (!priv.impl->is_source_used(&priv.voices[i].source)) {
            priv.impl->destroy_source(&priv.voices[i].source);
            voice = &priv.voices[i];
            break;
        }
    }

    if (!voice) {
        QU_WARNING("Can't find free voice.\n");
        return NULL;
    }

    // Need to keep track of generations so that voice identifier
    // doesn't repeat too often.

    voice->gen = (voice->gen + 1) % 64;
    voice->type = VOICE_TYPE_NONE;
    voice->state = VOICE_STATE_INACTIVE;

    memset(&voice->source, 0, sizeof(qx_audio_source));

    return voice;
}

static int32_t voice_to_id(struct voice const *voice)
{
    return (voice->gen << 16) | 0x0000CC00 | voice->index;
}

static struct voice *id_to_voice(int32_t id)
{
    if ((id & 0x0000FF00) != 0x0000CC00) {
        return NULL;
    }

    int index = id & 0xFF;
    int gen = (id >> 16) & 0x7FFF;

    if (index >= MAX_VOICES) {
        return NULL;
    }

    if (gen != priv.voices[index].gen) {
        return NULL;
    }

    return &priv.voices[index];
}

//------------------------------------------------------------------------------

static intptr_t music_main(void *arg)
{
    struct music *music = (struct music *) arg;
    char const *name = qx_file_get_name(music->wave.file);

    // Rewind the music file.
    qx_seek_wave(&music->wave, 0);

    // Sample buffers. These will be updated on the fly.
    qx_audio_buffer buffers[TOTAL_MUSIC_BUFFERS];
    memset(buffers, 0, sizeof(buffers));

    music->voice->type = VOICE_TYPE_MUSIC;
    music->voice->state = VOICE_STATE_PLAYING;

    // Special case: don't even try to play music if
    // dummy audio engine is in use.
    if (priv.impl == &qx_audio_null) {
        qu_sleep(1.0);
        goto end;
    }

    // Decode first few buffers upfront.
    for (int i = 0; i < TOTAL_MUSIC_BUFFERS; i++) {
        buffers[i].data = malloc(sizeof(int16_t) * MUSIC_BUFFER_LENGTH);
        buffers[i].samples = qx_read_wave(&music->wave, buffers[i].data, MUSIC_BUFFER_LENGTH);

        if (buffers[i].samples == 0) {
            QU_ERROR("Music track %s is too short.\n", name);
            goto end;
        }

        if (priv.impl->queue_buffer(&music->voice->source, &buffers[i]) != QX_SUCCESS) {
            QU_ERROR("Failed to read music track %s.\n", name);
        }
    }

    // Start playing first buffers of audio.
    if (priv.impl->start_source(&music->voice->source) != QX_SUCCESS) {
        QU_ERROR("Failed to start music track %s.\n", name);
        goto end;
    }

    int current_buffer = 0;
    bool running = true;

    while (running) {
        bool paused = false;

        qu_lock_mutex(priv.mutex);

        // This way we determine if the voice state was
        // changed from another thread using API functions
        // like qu_pause_voice().
        switch (music->voice->state) {
        case VOICE_STATE_PAUSED:
            paused = true;
            break;
        case VOICE_STATE_DESTROYED:
            running = false;
            break;
        }

        qu_unlock_mutex(priv.mutex);

        if (!running) {
            break;
        }

        if (paused) {
            qu_sleep(0.1);
            continue;
        }

        // Query how many buffers are still queued...
        int queued = priv.impl->get_queued_buffers(&music->voice->source);

        // ...and determine amount of buffers which were played.
        int played = TOTAL_MUSIC_BUFFERS - queued;

        QU_DEBUG("queued=%d,played=%d\n", queued, played);

        // Keep reading audio file as we playing it.
        for (int i = 0; i < played; i++) {
            // Read another portion of samples.
            int64_t samples_read = qx_read_wave(
                &music->wave, buffers[current_buffer].data, MUSIC_BUFFER_LENGTH
            );

            QU_DEBUG("[%d, %d] read... %d\n", i, current_buffer, samples_read);

            // If reached end of file...
            if (samples_read == 0) {
                // If loop count reached 0, then we should stop.
                if (music->loop_count == 0) {
                    running = false;
                    break;
                }

                // If it's still greater than 0, decrease it by 1 and continue.
                if (music->loop_count > 0) {
                    music->loop_count--;
                }

                // Note that if loop count is -1, no condition from above
                // is true, therefore the loop never ends.

                // Rewind and go back, don't submit this empty buffer.
                qx_seek_wave(&music->wave, 0);
                continue;
            }

            // Place buffer to the end of audio source.
            buffers[current_buffer].samples = samples_read;
            priv.impl->queue_buffer(&music->voice->source, &buffers[current_buffer]);

            // Move on to the next buffer.
            current_buffer = (current_buffer + 1) % TOTAL_MUSIC_BUFFERS;
        }

        // Don't overload CPU.
        qu_sleep(0.25);
    }

end:
    // Free buffer memory.
    for (int i = 0; i < TOTAL_MUSIC_BUFFERS; i++) {
        free(buffers[i].data);
    }

    qu_lock_mutex(priv.mutex);

    // Set this pointer to NULL to indicate that the thread
    // is stopped.
    music->thread = NULL;

    // Mark voice as unused.
    music->voice->type = VOICE_TYPE_NONE;
    music->voice->state = VOICE_STATE_INACTIVE;

    // This will tell that this particular music track isn't used now.
    music->voice = NULL;

    qu_unlock_mutex(priv.mutex);

    return 0;
}

//------------------------------------------------------------------------------

void qx_initialize_audio(qu_params const *params)
{
    memset(&priv, 0, sizeof(priv));

    // Select audio engine implementation.

    int audio_impl_count = QU__ARRAY_SIZE(supported_audio_impl_list);

    if (audio_impl_count == 0) {
        QU_HALT("audio_impl_count == 0");
    }

    for (int i = 0; i < audio_impl_count; i++) {
        priv.impl = supported_audio_impl_list[i];

        QU_HALT_IF(!priv.impl->check);

        if (priv.impl->check(params) == QX_SUCCESS) {
            QU_DEBUG("Selected audio implementation #%d.\n", i);
            break;
        }
    }

    // Thoroughly check if function table is properly set.
    QU_HALT_IF(!priv.impl->initialize);
    QU_HALT_IF(!priv.impl->terminate);
    QU_HALT_IF(!priv.impl->set_master_volume);
    QU_HALT_IF(!priv.impl->create_source);
    QU_HALT_IF(!priv.impl->destroy_source);
    QU_HALT_IF(!priv.impl->is_source_used);
    QU_HALT_IF(!priv.impl->queue_buffer);
    QU_HALT_IF(!priv.impl->get_queued_buffers);
    QU_HALT_IF(!priv.impl->start_source);
    QU_HALT_IF(!priv.impl->stop_source);

    // Initialize audio engine.
    if (priv.impl->initialize(params) != QX_SUCCESS) {
        QU_HALT("Illegal audio engine state.");
    }

    // Initialize dynamic arrays which are to hold sound and music data.

    priv.sounds = qu_create_array(sizeof(struct sound), sound_dtor);

    if (!priv.sounds) {
        QU_HALT("Out of memory: unable to initialize sound array.");
    }

    priv.music = qu_create_array(sizeof(struct music), music_dtor);

    if (!priv.music) {
        QU_HALT("Out of memory: unable to initialize music array.");
    }

    // Initialize static array of voices.
    // Scary: .index field should be set correctly and must never change.
    for (int i = 0; i < MAX_VOICES; i++) {
        priv.voices[i].index = i;
        priv.voices[i].gen = 0;
    }

    // Initialize common mutex. It's used when altering voice state,
    // such as pausing and resuming. Thus we can assure that music
    // playback in the background is more robust.
    priv.mutex = qu_create_mutex();
}

void qx_terminate_audio(void)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        if (priv.voices[i].type == VOICE_TYPE_NONE) {
            continue;
        }

        priv.impl->stop_source(&priv.voices[i].source);
    }

    qu_destroy_array(priv.sounds);
    qu_destroy_array(priv.music);

    priv.impl->terminate();
}

//------------------------------------------------------------------------------

void qx_set_master_volume(float volume)
{
    priv.impl->set_master_volume(volume);
}

//------------------------------------------------------------------------------

int32_t qx_load_sound(qx_file *file)
{
    struct qx_wave wave = { 0 };

    // Initialize sound decoder to read from the given file.
    if (!qx_open_wave(&wave, file)) {
        return 0;
    }

    // Create sound object and allocate memory to store
    // all audio samples from the file.
    struct sound sound = {
        .channels = wave.num_channels,
        .sample_rate = wave.sample_rate,
        .buffer = {
            .data = malloc(sizeof(int16_t) * wave.num_samples),
            .samples = wave.num_samples,
        },
    };

    // Attempt to read and decode the whole file.
    // Some decoders return smaller number than .num_samples even
    // if the whole file is read, so we don't check its value.
    int samples_read = qx_read_wave(&wave, sound.buffer.data, sound.buffer.samples);

    // Close decoder as it won't be used anymore.
    qx_close_wave(&wave);

    // Bail out if no samples were read at all.
    if (samples_read == 0) {
        free(sound.buffer.data); // Don't forget to clean up.
        return 0;
    }

    // Copy sound object to the sound array.
    return qu_array_add(priv.sounds, &sound);
}

void qx_delete_sound(int32_t id)
{
    // Destructor will be triggered, see sound_dtor().
    qu_array_remove(priv.sounds, id);
}

int32_t qx_play_sound(int32_t id, int loop)
{
    // Get sound object from the sound array.
    struct sound *sound = qu_array_get(priv.sounds, id);

    // NULL is returned, id is invalid.
    if (!sound) {
        QU_ERROR("Invalid sound identifier: 0x%08x.\n", id);
        return 0;
    }

    // Search for unused voice.
    struct voice *voice = find_voice();

    // Nothing found.
    if (!voice) {
        QU_ERROR("Free voice not found. Can't play sound 0x%08x.\n", id);
        return 0;
    }

    // Clear audio source struct just in case.
    memset(&voice->source, 0, sizeof(qx_audio_source));

    // Set the correct format for source.
    voice->source.channels = sound->channels;
    voice->source.sample_rate = sound->sample_rate;

    // -1 means looping enabled, 0 is not.
    voice->source.loop = loop;

    // Attempt to create audio source.
    if (priv.impl->create_source(&voice->source) != QX_SUCCESS) {
        QU_ERROR("Failed to create audio source. Can't play sound 0x%08x.\n", id);
        return 0;
    }

    // Queue the only buffer.
    if (priv.impl->queue_buffer(&voice->source, &sound->buffer) != QX_SUCCESS) {
        QU_ERROR("Failed to queue sample buffer. Can't play sound 0x%08x.\n", id);
        priv.impl->destroy_source(&voice->source);
        return 0;
    }

    // Play voice now.
    if (priv.impl->start_source(&voice->source) != QX_SUCCESS) {
        QU_ERROR("Failed to play audio source. Can't play sound 0x%08x.\n", id);
        priv.impl->destroy_source(&voice->source);
        return 0;
    }

    // Set voice state and return its identifier.
    voice->type = VOICE_TYPE_SOUND;
    voice->state = VOICE_STATE_PLAYING;

    return voice_to_id(voice);
}

//------------------------------------------------------------------------------

int32_t qx_open_music(qx_file *file)
{
    struct music music = { 0 };

    if (!qx_open_wave(&music.wave, file)) {
        return 0;
    }

    return qu_array_add(priv.music, &music);
}

void qx_close_music(int32_t id)
{
    // Destructor will be triggered, see music_dtor().
    qu_array_remove(priv.music, id);
}

int32_t qx_play_music(int32_t id, int loop)
{
    struct music *music = qu_array_get(priv.music, id);

    if (!music) {
        return 0;
    }

    if (music->voice) {
        QU_WARNING("Music track \"%s\" is already playing.\n", qx_file_get_name(music->wave.file));
        return voice_to_id(music->voice);
    }

    // Search for unused voice.
    struct voice *voice = find_voice();

    // Nothing found.
    if (!voice) {
        QU_ERROR("Free voice not found. Can't play music 0x%08x.\n", id);
        return 0;
    }

    // Clear audio source struct just in case.
    memset(&voice->source, 0, sizeof(qx_audio_source));

    // Set correct source format.
    voice->source.channels = music->wave.num_channels;
    voice->source.sample_rate = music->wave.sample_rate;

    // This should always be 0 even if music is looped.
    voice->source.loop = 0;

    // Attempt to create audio source.
    if (priv.impl->create_source(&voice->source) != QX_SUCCESS) {
        QU_ERROR("Failed to create audio source. Can't play music 0x%08x.\n", id);
        return 0;
    }

    // Start music playback in another thread.
    music->voice = voice;
    music->loop_count = loop;
    music->thread = qu_create_thread("music", music_main, music);

    return voice_to_id(voice);
}

//------------------------------------------------------------------------------

void qx_pause_voice(int32_t id)
{
    struct voice *voice = id_to_voice(id);

    if (!voice) {
        QU_ERROR("Invalid voice identifier: 0x%08x. Can't pause.\n", id);
        return;
    }

    qu_lock_mutex(priv.mutex);

    if (voice->state == VOICE_STATE_PLAYING) {
        if (priv.impl->stop_source(&voice->source) != QX_SUCCESS) {
            QU_WARNING("Failed to pause voice 0x%08x.\n", id);
        } else {
            voice->state = VOICE_STATE_PAUSED;
        }
    } else {
        QU_WARNING("Voice 0x%08x is not playing, can't be paused.\n", id);
    }

    qu_unlock_mutex(priv.mutex);
}

void qx_unpause_voice(int32_t id)
{
    struct voice *voice = id_to_voice(id);

    if (!voice) {
        QU_ERROR("Invalid voice identifier: 0x%08x. Can't resume.\n", id);
        return;
    }

    qu_lock_mutex(priv.mutex);

    if (voice->state == VOICE_STATE_PAUSED) {
        if (priv.impl->start_source(&voice->source) != QX_SUCCESS) {
            QU_WARNING("Failed to resume voice 0x%08x.\n", id);
        } else {
            voice->state = VOICE_STATE_PLAYING;
        }
    } else {
        QU_WARNING("Voice 0x%08x is not paused, can't be resumed.\n", id);
    }

    qu_unlock_mutex(priv.mutex);
}

void qx_stop_voice(int32_t id)
{
    struct voice *voice = id_to_voice(id);

    if (!voice) {
        QU_ERROR("Invalid voice identifier: 0x%08x. Can't stop.\n", id);
        return;
    }

    if (voice->type == VOICE_TYPE_NONE) {
        QU_WARNING("Voice 0x%08x is not active, can't be stopped.\n", id);
        return;
    }

    qu_lock_mutex(priv.mutex);

    priv.impl->stop_source(&voice->source);
    priv.impl->destroy_source(&voice->source);
    voice->state = VOICE_STATE_DESTROYED;

    qu_unlock_mutex(priv.mutex);
}
