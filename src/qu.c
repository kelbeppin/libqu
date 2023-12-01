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

#if defined(__EMSCRIPTEN__)

#include <emscripten.h>

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
    qu_flush_graphics();
    qu_swap_buffers();
}
