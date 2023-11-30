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
// qu_audio.c: abstract audio module
//------------------------------------------------------------------------------

#include "qu_audio.h"
#include "qu_log.h"
#include "qu_resource_loader.h"

//------------------------------------------------------------------------------

static struct qu_audio_impl const *supported_audio_impl_list[] = {

#ifdef QU_WIN32
    &qu_xaudio2_audio_impl,
#endif

#ifdef QU_ANDROID
    &qu_sles_audio_impl,
#endif

#ifdef QU_USE_OPENAL
    &qu_openal_audio_impl,
#endif

    &qu_null_audio_impl,
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
    qu_audio_buffer buffer;
    char name[QU_FILE_NAME_LENGTH];
};

struct music
{
    qu_audio_loader *loader;
    struct voice *voice;
    pl_thread *thread;
    int loop_count;
};

struct voice
{
    int index;
    int gen;
    int type;
    int state;
    qu_audio_source source;
};

struct audio_priv
{
    bool initialized;

    struct qu_audio_impl const *impl;
    pl_mutex *mutex;

    qu_handle_list *sounds;
    qu_handle_list *music;
    struct voice voices[MAX_VOICES];
};

//------------------------------------------------------------------------------

static struct audio_priv priv;

//------------------------------------------------------------------------------

static void sound_dtor(void *ptr)
{
    struct sound *sound = ptr;

    pl_free(sound->buffer.data);
}

static void music_dtor(void *ptr)
{
    struct music *music = (struct music *) ptr;

    // If the music is playing, wait until thread ends.
    if (music->thread) {
        pl_wait_thread(music->thread);
    }

    qu_file *file = music->loader->file;

    // Close sound reader.
    qu_close_audio_loader(music->loader);

    // Close music file.
    qu_close_file(file);
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
        QU_LOGW("Can't find free voice.\n");
        return NULL;
    }

    // Need to keep track of generations so that voice identifier
    // doesn't repeat too often.

    voice->gen = (voice->gen + 1) % 64;
    voice->type = VOICE_TYPE_NONE;
    voice->state = VOICE_STATE_INACTIVE;

    memset(&voice->source, 0, sizeof(qu_audio_source));

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

static int64_t read_sound_buffer(qu_audio_loader *loader, int16_t **data)
{
    // Try to allocate memory to store all audio samples.
    *data = pl_malloc(sizeof(*data) * loader->num_samples);

    // Out of memory?
    if (!(*data)) {
        return -1;
    }

    // Attempt to read and decode the whole file.
    // Some decoders return smaller number than .num_samples even
    // if the whole file is read, so we don't check its value.
    return qu_audio_loader_read(loader, *data, loader->num_samples);
}

static int32_t load_sound_from_file(qu_file *file)
{
    // Initialize sound decoder to read from the given file.
    qu_audio_loader *loader = qu_open_audio_loader(file);

    if (!loader) {
        return 0;
    }

    // Create sound object.
    struct sound sound = {
        .channels = loader->num_channels,
        .sample_rate = loader->sample_rate,
        .buffer = {
            .samples = loader->num_samples,
        },
    };

    // read_sound_buffer() will allocate memory.
    int64_t samples_read = read_sound_buffer(loader, &sound.buffer.data);

    // Close decoder as it won't be used anymore.
    qu_close_audio_loader(loader);

    if (samples_read == -1) {
        return 0;
    }

    strncpy(sound.name, file->name, QU_FILE_NAME_LENGTH - 1);

    // Copy sound object to the sound array.
    return qu_handle_list_add(priv.sounds, &sound);
}

static int32_t play_sound(struct sound *sound, int loop)
{
    // Search for unused voice.
    struct voice *voice = find_voice();

    // Nothing found.
    if (!voice) {
        QU_LOGE("Free voice not found. Can't play sound \"%s\".\n", sound->name);
        return 0;
    }

    // Clear audio source struct just in case.
    memset(&voice->source, 0, sizeof(qu_audio_source));

    // Set the correct format for source.
    voice->source.channels = sound->channels;
    voice->source.sample_rate = sound->sample_rate;

    // -1 means looping enabled, 0 is not.
    voice->source.loop = loop;

    // Attempt to create audio source.
    if (priv.impl->create_source(&voice->source) != QU_SUCCESS) {
        QU_LOGE("Failed to create audio source. Can't play sound \"%s\".\n", sound->name);
        return 0;
    }

    // Queue the only buffer.
    if (priv.impl->queue_buffer(&voice->source, &sound->buffer) != QU_SUCCESS) {
        QU_LOGE("Failed to queue sample buffer. Can't play sound \"%s\".\n", sound->name);
        priv.impl->destroy_source(&voice->source);
        return 0;
    }

    // Play voice now.
    if (priv.impl->start_source(&voice->source) != QU_SUCCESS) {
        QU_LOGE("Failed to play audio source. Can't play sound \"%s\".\n", sound->name);
        priv.impl->destroy_source(&voice->source);
        return 0;
    }

    // Set voice state and return its identifier.
    voice->type = VOICE_TYPE_SOUND;
    voice->state = VOICE_STATE_PLAYING;

    return voice_to_id(voice);
}

//------------------------------------------------------------------------------

static intptr_t music_main(void *arg)
{
    struct music *music = (struct music *) arg;
    char const *name = music->loader->file->name;

    // Rewind the music file.
    qu_audio_loader_seek(music->loader, 0);

    // Sample buffers. These will be updated on the fly.
    qu_audio_buffer buffers[TOTAL_MUSIC_BUFFERS];
    memset(buffers, 0, sizeof(buffers));

    music->voice->type = VOICE_TYPE_MUSIC;
    music->voice->state = VOICE_STATE_PLAYING;

    // Special case: don't even try to play music if
    // dummy audio engine is in use.
    if (priv.impl == &qu_null_audio_impl) {
        pl_sleep(1.0);
        goto end;
    }

    // Decode first few buffers upfront.
    for (int i = 0; i < TOTAL_MUSIC_BUFFERS; i++) {
        buffers[i].data = pl_malloc(sizeof(int16_t) * MUSIC_BUFFER_LENGTH);
        buffers[i].samples = qu_audio_loader_read(music->loader, buffers[i].data, MUSIC_BUFFER_LENGTH);

        if (buffers[i].samples == 0) {
            QU_LOGE("Music track %s is too short.\n", name);
            goto end;
        }

        if (priv.impl->queue_buffer(&music->voice->source, &buffers[i]) != QU_SUCCESS) {
            QU_LOGE("Failed to read music track %s.\n", name);
        }
    }

    // Start playing first buffers of audio.
    if (priv.impl->start_source(&music->voice->source) != QU_SUCCESS) {
        QU_LOGE("Failed to start music track %s.\n", name);
        goto end;
    }

    int current_buffer = 0;
    bool running = true;

    while (running) {
        bool paused = false;

        pl_lock_mutex(priv.mutex);

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

        pl_unlock_mutex(priv.mutex);

        if (!running) {
            break;
        }

        if (paused) {
            pl_sleep(0.1);
            continue;
        }

        // Query how many buffers are still queued...
        int queued = priv.impl->get_queued_buffers(&music->voice->source);

        // ...and determine amount of buffers which were played.
        int played = TOTAL_MUSIC_BUFFERS - queued;

        QU_LOGD("queued=%d,played=%d\n", queued, played);

        // Keep reading audio file as we playing it.
        for (int i = 0; i < played; i++) {
            // Read another portion of samples.
            int64_t samples_read = qu_audio_loader_read(
                music->loader, buffers[current_buffer].data, MUSIC_BUFFER_LENGTH
            );

            QU_LOGD("[%d, %d] read... %d\n", i, current_buffer, samples_read);

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
                qu_audio_loader_seek(music->loader, 0);
                continue;
            }

            // Place buffer to the end of audio source.
            buffers[current_buffer].samples = samples_read;
            priv.impl->queue_buffer(&music->voice->source, &buffers[current_buffer]);

            // Move on to the next buffer.
            current_buffer = (current_buffer + 1) % TOTAL_MUSIC_BUFFERS;
        }

        // Don't overload CPU.
        pl_sleep(0.25);
    }

    priv.impl->stop_source(&music->voice->source);

end:
    // Free buffer memory.
    for (int i = 0; i < TOTAL_MUSIC_BUFFERS; i++) {
        pl_free(buffers[i].data);
    }

    pl_lock_mutex(priv.mutex);

    // Release source. There won't be any chance where we can do that.
    priv.impl->destroy_source(&music->voice->source);

    // Set this pointer to NULL to indicate that the thread
    // is stopped.
    music->thread = NULL;

    // Mark voice as unused.
    music->voice->type = VOICE_TYPE_NONE;
    music->voice->state = VOICE_STATE_INACTIVE;

    // This will tell that this particular music track isn't used now.
    music->voice = NULL;

    pl_unlock_mutex(priv.mutex);

    return 0;
}

static int32_t play_music(struct music *music, int loop)
{
    char const *name = music->loader->file->name;

    if (music->voice) {
        QU_LOGW("Music track \"%s\" is already playing.\n", name);
        return voice_to_id(music->voice);
    }

    // Search for unused voice.
    struct voice *voice = find_voice();

    // Nothing found.
    if (!voice) {
        QU_LOGE("Free voice not found. Can't play music track \"%s\".\n", name);
        return 0;
    }

    // Clear audio source struct just in case.
    memset(&voice->source, 0, sizeof(qu_audio_source));

    // Set correct source format.
    voice->source.channels = music->loader->num_channels;
    voice->source.sample_rate = music->loader->sample_rate;

    // This should always be 0 even if music is looped.
    voice->source.loop = 0;

    // Attempt to create audio source.
    if (priv.impl->create_source(&voice->source) != QU_SUCCESS) {
        QU_LOGE("Failed to create audio source. Can't play music track \"%s\".\n", name);
        return 0;
    }

    // Start music playback in another thread.
    music->voice = voice;
    music->loop_count = loop;
    music->thread = pl_create_thread("music", music_main, music);

    return voice_to_id(voice);
}

//------------------------------------------------------------------------------

void qu_initialize_audio(void)
{
    if (priv.initialized) {
        QU_LOGW("Attempt to initialized audio, but it's initialized already.\n");
        return;
    }

    // Select audio engine implementation.

    int audio_impl_count = QU_ARRAY_SIZE(supported_audio_impl_list);

    if (audio_impl_count == 0) {
        QU_HALT("audio_impl_count == 0");
    }

    for (int i = 0; i < audio_impl_count; i++) {
        priv.impl = supported_audio_impl_list[i];

        QU_HALT_IF(!priv.impl->check);

        if (priv.impl->check() == QU_SUCCESS) {
            QU_LOGD("Selected audio implementation #%d.\n", i);
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
    if (priv.impl->initialize() != QU_SUCCESS) {
        QU_HALT("Illegal audio engine state.");
    }

    // Initialize dynamic arrays which are to hold sound and music data.

    priv.sounds = qu_create_handle_list(sizeof(struct sound), sound_dtor);

    if (!priv.sounds) {
        QU_HALT("Out of memory: unable to initialize sound array.");
    }

    priv.music = qu_create_handle_list(sizeof(struct music), music_dtor);

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
    priv.mutex = pl_create_mutex();

    // Mark as initialized.
    priv.initialized = true;

    // Terminate at exit.
    qu_atexit(qu_terminate_audio);

    QU_LOGI("Initialized.\n");
}

void qu_terminate_audio(void)
{
    if (!priv.initialized) {
        QU_LOGW("Can't terminate audio, not initialized.\n");
        return;
    }

    for (int i = 0; i < MAX_VOICES; i++) {
        if (priv.voices[i].type == VOICE_TYPE_NONE) {
            continue;
        }

        priv.impl->stop_source(&priv.voices[i].source);
    }

    qu_destroy_handle_list(priv.sounds);
    qu_destroy_handle_list(priv.music);

    priv.impl->terminate();

    pl_destroy_mutex(priv.mutex);

    memset(&priv, 0, sizeof(priv));

    QU_LOGI("Terminated.\n");
}

//------------------------------------------------------------------------------
// Public API

void qu_set_master_volume(float volume)
{
    if (!priv.initialized) {
        qu_initialize_audio();
    }

    priv.impl->set_master_volume(volume);
}

qu_sound qu_load_sound(char const *path)
{
    if (!priv.initialized) {
        qu_initialize_audio();
    }

    qu_file *file = qu_open_file_from_path(path);

    if (!file) {
        return (qu_sound) { 0 };
    }

    int32_t id = load_sound_from_file(file);

    qu_close_file(file);

    return (qu_sound) { id };
}

void qu_delete_sound(qu_sound handle)
{
    // How can we delete a sound if audio isn't even initialized?
    if (!priv.initialized) {
        return;
    }

    // Destructor will be triggered, see sound_dtor().
    qu_handle_list_remove(priv.sounds, handle.id);
}

qu_voice qu_play_sound(qu_sound handle)
{
    if (!priv.initialized) {
        return (qu_voice) { 0 };
    }

    // Get sound object from the sound array.
    struct sound *sound = qu_handle_list_get(priv.sounds, handle.id);

    // NULL is returned, id is invalid.
    if (!sound) {
        QU_LOGE("Invalid sound identifier: 0x%08x.\n", handle.id);
        return (qu_voice) { 0 };
    }

    return (qu_voice) { play_sound(sound, 0) };
}

qu_voice qu_loop_sound(qu_sound handle)
{
    if (!priv.initialized) {
        return (qu_voice) { 0 };
    }

    // Get sound object from the sound array.
    struct sound *sound = qu_handle_list_get(priv.sounds, handle.id);

    // NULL is returned, id is invalid.
    if (!sound) {
        QU_LOGE("Invalid sound identifier: 0x%08x.\n", handle.id);
        return (qu_voice) { 0 };
    }

    return (qu_voice) { play_sound(sound, -1) };
}

qu_music qu_open_music(char const *path)
{
    if (!priv.initialized) {
        qu_initialize_audio();
    }

    qu_file *file = qu_open_file_from_path(path);

    if (!file) {
        return (qu_music) { 0 };
    }

    qu_audio_loader *loader = qu_open_audio_loader(file);

    if (!loader) {
        QU_LOGE("Unable to open music file \"%s\".\n", file->name);
        qu_close_file(file);
        return (qu_music) { 0 };
    }

    int32_t id = qu_handle_list_add(priv.music, &(struct music) {
        .loader = loader,
    });

    return (qu_music) { id };
}

void qu_close_music(qu_music handle)
{
    if (!priv.initialized) {
        return;
    }

    // Destructor will be triggered, see music_dtor().
    qu_handle_list_remove(priv.music, handle.id);
}

qu_voice qu_play_music(qu_music handle)
{
    if (!priv.initialized) {
        return (qu_voice) { 0 };
    }

    struct music *music = qu_handle_list_get(priv.music, handle.id);

    if (!music) {
        QU_LOGW("Music track 0x%08x is invalid. Can't play.\n", handle.id);
        return (qu_voice) { 0 };
    }

    return (qu_voice) { play_music(music, 0) };
}

qu_voice qu_loop_music(qu_music handle)
{
    if (!priv.initialized) {
        return (qu_voice) { 0 };
    }

    struct music *music = qu_handle_list_get(priv.music, handle.id);

    if (!music) {
        QU_LOGW("Music track 0x%08x is invalid. Can't play.\n", handle.id);
        return (qu_voice) { 0 };
    }

    return (qu_voice) { play_music(music, -1) };
}

void qu_pause_voice(qu_voice handle)
{
    if (!priv.initialized) {
        return;
    }

    struct voice *voice = id_to_voice(handle.id);

    if (!voice) {
        QU_LOGE("Invalid voice identifier: 0x%08x. Can't pause.\n", handle.id);
        return;
    }

    pl_lock_mutex(priv.mutex);

    if (voice->state == VOICE_STATE_PLAYING) {
        if (priv.impl->stop_source(&voice->source) != QU_SUCCESS) {
            QU_LOGW("Failed to pause voice 0x%08x.\n", handle.id);
        } else {
            voice->state = VOICE_STATE_PAUSED;
        }
    } else {
        QU_LOGW("Voice 0x%08x is not playing, can't be paused.\n", handle.id);
    }

    pl_unlock_mutex(priv.mutex);
}

void qu_unpause_voice(qu_voice handle)
{
    if (!priv.initialized) {
        return;
    }

    struct voice *voice = id_to_voice(handle.id);

    if (!voice) {
        QU_LOGE("Invalid voice identifier: 0x%08x. Can't resume.\n", handle.id);
        return;
    }

    pl_lock_mutex(priv.mutex);

    if (voice->state == VOICE_STATE_PAUSED) {
        if (priv.impl->start_source(&voice->source) != QU_SUCCESS) {
            QU_LOGW("Failed to resume voice 0x%08x.\n", handle.id);
        } else {
            voice->state = VOICE_STATE_PLAYING;
        }
    } else {
        QU_LOGW("Voice 0x%08x is not paused, can't be resumed.\n", handle.id);
    }

    pl_unlock_mutex(priv.mutex);
}

void qu_stop_voice(qu_voice handle)
{
    if (!priv.initialized) {
        return;
    }

    struct voice *voice = id_to_voice(handle.id);

    if (!voice) {
        QU_LOGE("Invalid voice identifier: 0x%08x. Can't stop.\n", handle.id);
        return;
    }

    if (voice->type == VOICE_TYPE_NONE) {
        QU_LOGW("Voice 0x%08x is not active, can't be stopped.\n", handle.id);
        return;
    }

    pl_lock_mutex(priv.mutex);

    priv.impl->stop_source(&voice->source);
    priv.impl->destroy_source(&voice->source);
    voice->state = VOICE_STATE_DESTROYED;

    pl_unlock_mutex(priv.mutex);
}
