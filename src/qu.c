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
// qu.c: Gateway
//------------------------------------------------------------------------------

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#include "qu.h"
#include "qu_core.h"
#include "qu_graphics.h"
#include "qu_platform.h"

//------------------------------------------------------------------------------

#define STB_DS_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#define STBDS_REALLOC(ctx, ptr, size)   pl_realloc(ptr, size)
#define STBDS_FREE(ctx, ptr)            pl_free(ptr)

#define STBI_MALLOC(size)               pl_malloc(size)
#define STBI_REALLOC(ptr, size)         pl_realloc(ptr, size)
#define STBI_FREE(ptr)                  pl_free(ptr)

#include <stb_ds.h>
#include <stb_image.h>

//------------------------------------------------------------------------------

#define MAX_EXIT_HANDLERS       (32)

//------------------------------------------------------------------------------

struct gateway_priv
{
    bool initialized;
    void (*exit_handlers[MAX_EXIT_HANDLERS])(void);
    int total_exit_handlers;
};

static struct gateway_priv priv;

//------------------------------------------------------------------------------
// Game Loop

static struct
{
    int tick_rate;
    double frame_duration;
    double frame_start_time;
    double frame_lag_time;
    qu_update_fn update_fn;
    qu_draw_fn draw_fn;
} loop_state;

static void init_loop(int tick_rate, qu_update_fn update_fn, qu_draw_fn draw_fn)
{
    loop_state.tick_rate = tick_rate;
    loop_state.frame_duration = 1.0 / tick_rate;
    loop_state.frame_start_time = 0.0;
    loop_state.frame_lag_time = 0.0;
    loop_state.update_fn = update_fn;
    loop_state.draw_fn = draw_fn;
}

static int main_loop(void)
{
    double current_time = qu_get_time_highp();
    double elapsed_time = current_time - loop_state.frame_start_time;

    loop_state.frame_start_time = current_time;
    loop_state.frame_lag_time += elapsed_time;

    int rc = 0;

    while (loop_state.frame_lag_time >= loop_state.frame_duration) {
        rc = loop_state.update_fn();

        if (rc) {
            break;
        }

        loop_state.frame_lag_time -= loop_state.frame_duration;
    }

    double lag_offset = loop_state.frame_lag_time * loop_state.tick_rate;
    loop_state.draw_fn(lag_offset);

    return rc;
}

#if defined(__EMSCRIPTEN__)

static void main_loop_for_emscripten(void *arg)
{
    if (qu_process() && main_loop() == 0) {
        return;
    }

    emscripten_cancel_main_loop();
}

#endif

//------------------------------------------------------------------------------
// Internal API

void qu_atexit(void (*callback)(void))
{
    if (priv.total_exit_handlers == MAX_EXIT_HANDLERS) {
        return;
    }

    priv.exit_handlers[priv.total_exit_handlers++] = callback;
}

//------------------------------------------------------------------------------
// Public API

void qu_initialize(void)
{
    if (priv.initialized) {
        return;
    }

    qu_initialize_core();
    qu_initialize_graphics();

    priv.initialized = true;
}

void qu_terminate(void)
{
    for (int i = (priv.total_exit_handlers - 1); i >= 0; i--) {
        priv.exit_handlers[i]();
    }

    if (!priv.initialized) {
        return;
    }

    qu_terminate_graphics();
    qu_terminate_core();

    memset(&priv, 0, sizeof(priv));
}

bool qu_process(void)
{
    return qu_handle_events();
}

int qu_execute_game_loop(int tick_rate, qu_update_fn update_fn, qu_draw_fn draw_fn)
{
    init_loop(tick_rate, update_fn, draw_fn);

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop_arg(main_loop_for_emscripten, NULL, 0, 1);
#else
    while (qu_process()) {
        int rc = main_loop();

        if (rc) {
            return rc;
        }
    }
#endif

    return 0;
}

void qu_present(void)
{
    qu_flush_graphics();
    qu_swap_buffers();
}

