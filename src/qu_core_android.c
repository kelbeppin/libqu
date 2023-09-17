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
// qu_core_android.c: Android core module
//------------------------------------------------------------------------------

#define QU_MODULE "core-android"

#include "qu.h"

//------------------------------------------------------------------------------

static void android_initialize(qu_params const *params)
{
}

static void android_terminate(void)
{
}

static bool android_process(void)
{
    return qx_android_poll_events() == 0;
}

static void android_present(void)
{
    qx_android_swap_buffers();
}

static enum qu__renderer android_get_renderer(void)
{
    return qx_android_get_renderer();
}

static void *android_gl_get_proc_address(char const *name)
{
    return qx_android_gl_get_proc_address(name);
}

static int android_gl_get_sample_count(void)
{
    return qx_android_gl_get_sample_count();
}

static bool android_set_window_title(char const *title)
{
    return false;
}

static bool android_set_window_size(int width, int height)
{
    return false;
}

//------------------------------------------------------------------------------

struct qu__core const qu__core_android = {
    .initialize = android_initialize,
    .terminate = android_terminate,
    .process = android_process,
    .present = android_present,
    .get_renderer = android_get_renderer,
    .gl_proc_address = android_gl_get_proc_address,
    .get_gl_multisample_samples = android_gl_get_sample_count,
    .set_window_title = android_set_window_title,
    .set_window_size = android_set_window_size,
};
