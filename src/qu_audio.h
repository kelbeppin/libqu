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
// qu_audio.h: audio
//------------------------------------------------------------------------------

#ifndef QU_AUDIO_H_INC
#define QU_AUDIO_H_INC

//------------------------------------------------------------------------------

#include "qu.h"

//------------------------------------------------------------------------------

typedef struct qu_audio_buffer
{
    int16_t *data;      // sample data
    size_t samples;     // number of samples
    intptr_t priv[4];   // implementation-specific data
} qu_audio_buffer;

typedef struct qu_audio_source
{
    int channels;       // number of channels (1 or 2)
    int sample_rate;    // sample rate (usually 44100)
    int loop;           // should this source loop (-1 if yes)
    intptr_t priv[4];   // implementation-specific data
} qu_audio_source;

typedef struct qu_audio_impl
{
    qu_result (*check)(void);
    qu_result (*initialize)(void);
    void (*terminate)(void);

    void (*set_master_volume)(float volume);

    qu_result (*create_source)(qu_audio_source *source);
    void (*destroy_source)(qu_audio_source *source);
    bool (*is_source_used)(qu_audio_source *source);
    qu_result (*queue_buffer)(qu_audio_source *source, qu_audio_buffer *buffer);
    int (*get_queued_buffers)(qu_audio_source *source);
    qu_result (*start_source)(qu_audio_source *source);
    qu_result (*stop_source)(qu_audio_source *source);
} qu_audio_impl;

//------------------------------------------------------------------------------

extern qu_audio_impl const qu_null_audio_impl;
extern qu_audio_impl const qu_openal_audio_impl;
extern qu_audio_impl const qu_xaudio2_audio_impl;
extern qu_audio_impl const qu_sles_audio_impl;

//------------------------------------------------------------------------------

void qu_initialize_audio(void);
void qu_terminate_audio(void);

//------------------------------------------------------------------------------

#endif // QU_AUDIO_H_INC

