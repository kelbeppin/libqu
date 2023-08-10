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

#include "qu.h"

//------------------------------------------------------------------------------
// qu.c: Gateway
//------------------------------------------------------------------------------

enum qu_status
{
    QU_STATUS_CLEAN,
    QU_STATUS_INITIALIZED,
    QU_STATUS_TERMINATED,
};

struct qu
{
    enum qu_status status;
    qu_params params;
};

static struct qu qu;

//------------------------------------------------------------------------------

void qu_initialize(qu_params const *user_params)
{
    if (qu.status == QU_STATUS_INITIALIZED) {
        return;
    }
    
    if (qu.status == QU_STATUS_TERMINATED) {
        memset(&qu, 0, sizeof(struct qu));
    }

    if (user_params) {
        memcpy(&qu.params, user_params, sizeof(qu_params));
    }

    if (!qu.params.title) {
        qu.params.title = "libqu application";
    }

    if (!qu.params.display_width || !qu.params.display_height) {
        qu.params.display_width = 720;
        qu.params.display_height = 480;
    }

    if (!qu.params.canvas_width || !qu.params.canvas_height) {
        qu.params.canvas_width = qu.params.display_width;
        qu.params.canvas_height = qu.params.display_height;
    }

    qu_platform_initialize();
    qu__core_initialize(&qu.params);
    qu__graphics_initialize(&qu.params);
    qu__audio_initialize(&qu.params);
    qu_initialize_text();

    qu.status = QU_STATUS_INITIALIZED;
}

void qu_terminate(void)
{
    if (qu.status == QU_STATUS_INITIALIZED) {
        qu_terminate_text();
        qu__audio_terminate();
        qu__graphics_terminate();
        qu__core_terminate();
        qu_platform_terminate();

        qu.status = QU_STATUS_TERMINATED;
    }
}

bool qu_process(void)
{
    return qu__core_process();
}

#if defined(__EMSCRIPTEN__)

static void main_loop(void *arg)
{
    if (qu_process()) {
        qu_loop_fn loop_fn = (qu_loop_fn) arg;
        loop_fn();
    } else {
        qu_terminate();
        emscripten_cancel_main_loop();
    }
}

void qu_execute(qu_loop_fn loop_fn)
{
    emscripten_set_main_loop_arg(main_loop, loop_fn, 0, 1);
    exit(EXIT_SUCCESS);
}

#else

void qu_execute(qu_loop_fn loop_fn)
{
    while (qu_process() && loop_fn()) {
        // Intentionally left blank
    }

    qu_terminate();
    exit(EXIT_SUCCESS);
}

#endif

void qu_present(void)
{
    qu__graphics_swap();
    qu__core_present();
}

//------------------------------------------------------------------------------
// Audio

void qu_set_master_volume(float volume)
{
    qu__audio_set_master_volume(volume);
}

qu_sound qu_load_sound(char const *path)
{
    qu_file *file = qu_fopen(path);

    if (file) {
        int32_t id = qu__audio_load_sound(file);
        qu_fclose(file);

        return (qu_sound) { id };
    }

    return (qu_sound) { 0 };
}

void qu_delete_sound(qu_sound sound)
{
    qu__audio_delete_sound(sound.id);
}

qu_stream qu_play_sound(qu_sound sound)
{
    return (qu_stream) { qu__audio_play_sound(sound.id) };
}

qu_stream qu_loop_sound(qu_sound sound)
{
    return (qu_stream) { qu__audio_loop_sound(sound.id) };
}

qu_music qu_open_music(char const *path)
{
    qu_file *file = qu_fopen(path);

    if (file) {
        int32_t id = qu__audio_open_music(file);
        
        if (id == 0) {
            qu_fclose(file);
        }

        return (qu_music) { id };
    }

    return (qu_music) { 0 };
}

void qu_close_music(qu_music music)
{
    qu__audio_close_music(music.id);
}

qu_stream qu_play_music(qu_music music)
{
    return (qu_stream) { qu__audio_play_music(music.id) };
}

qu_stream qu_loop_music(qu_music music)
{
    return (qu_stream) { qu__audio_loop_music(music.id) };
}

void qu_pause_stream(qu_stream stream)
{
    qu__audio_pause_stream(stream.id);
}

void qu_unpause_stream(qu_stream stream)
{
    qu__audio_unpause_stream(stream.id);
}

void qu_stop_stream(qu_stream stream)
{
    qu__audio_stop_stream(stream.id);
}
