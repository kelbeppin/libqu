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
// qu_platform_posix.c: POSIX-specific platform code 
//------------------------------------------------------------------------------

#ifdef __linux__
#define _GNU_SOURCE
#endif

#define QU_MODULE "platform-posix"

//------------------------------------------------------------------------------

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>

#include "qu.h"

//------------------------------------------------------------------------------

#define THREAD_NAME_LENGTH          256

//------------------------------------------------------------------------------

struct pl_thread
{
    pthread_t id;
    char name[THREAD_NAME_LENGTH];
    intptr_t (*func)(void *);
    void *arg;
};

struct pl_mutex
{
    pthread_mutex_t id;
};

//------------------------------------------------------------------------------

void pl_initialize(void)
{
}

void pl_terminate(void)
{
}

//------------------------------------------------------------------------------

void *pl_malloc(size_t size)
{
    return malloc(size);
}

void *pl_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

void *pl_realloc(void *data, size_t size)
{
    return realloc(data, size);
}

void pl_free(void *data)
{
    free(data);
}

//------------------------------------------------------------------------------
// Clock

uint32_t pl_get_ticks_mediump(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

uint64_t pl_get_ticks_highp(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (ts.tv_sec * 1000000000) + ts.tv_nsec;
}

//------------------------------------------------------------------------------
// Threads

static void *thread_main(void *thread_ptr)
{
    pl_thread *thread = thread_ptr;
    intptr_t retval = thread->func(thread->arg);
    pl_free(thread);

    return (void *) retval;
}

pl_thread *pl_create_thread(char const *name, intptr_t (*func)(void *), void *arg)
{
    pl_thread *thread = pl_calloc(1, sizeof(pl_thread));

    if (!thread) {
        QU_LOGE("Failed to allocate memory for thread \'%s\'.\n", name);
        return NULL;
    }

    if (!name || name[0] == '\0') {
        strncpy(thread->name, "(unnamed)", THREAD_NAME_LENGTH);
    } else {
        strncpy(thread->name, name, THREAD_NAME_LENGTH);
    }

    thread->func = func;
    thread->arg = arg;

    int error = pthread_create(&thread->id, NULL, thread_main, thread);

    if (error) {
        QU_LOGE("Error (code %d) occured while attempting to create thread \'%s\'.\n", error, thread->name);
        pl_free(thread);

        return NULL;
    }

#if defined(__linux__)
    pthread_setname_np(thread->id, thread->name);
#endif

    return thread;
}

void pl_detach_thread(pl_thread *thread)
{
    int error = pthread_detach(thread->id);

    if (error) {
        QU_LOGE("Failed to detach thread \'%s\', error code: %d.\n", thread->name, error);
    }
}

intptr_t pl_wait_thread(pl_thread *thread)
{
    void *retval;
    int error = pthread_join(thread->id, &retval);

    if (error) {
        QU_LOGE("Failed to join thread \'%s\', error code: %d.\n", thread->name, error);
    }

    return (intptr_t) retval;
}

pl_mutex *pl_create_mutex(void)
{
    pl_mutex *mutex = pl_calloc(1, sizeof(pl_mutex));

    if (!mutex) {
        return NULL;
    }

    int error = pthread_mutex_init(&mutex->id, NULL);

    if (error) {
        QU_LOGE("Failed to create mutex, error code: %d.\n", error);
        return NULL;
    }

    return mutex;
}

void pl_destroy_mutex(pl_mutex *mutex)
{
    if (!mutex) {
        return;
    }

    int error = pthread_mutex_destroy(&mutex->id);

    if (error) {
        QU_LOGE("Failed to destroy mutex, error code: %d.\n", error);
    }

    pl_free(mutex);
}

void pl_lock_mutex(pl_mutex *mutex)
{
    pthread_mutex_lock(&mutex->id);
}

void pl_unlock_mutex(pl_mutex *mutex)
{
    pthread_mutex_unlock(&mutex->id);
}

void pl_sleep(double seconds)
{
    uint64_t s = (uint64_t) floor(seconds);

    struct timespec ts = {
        .tv_sec = s,
        .tv_nsec = (uint64_t) ((seconds - s) * 1000000000),
    };

    while ((nanosleep(&ts, &ts) == -1) && (errno == EINTR)) {
        // Wait, do nothing.
    }
}

//------------------------------------------------------------------------------
// Dynamic loading libraries

void *pl_open_dll(char const *path)
{
    return dlopen(path, RTLD_LAZY);
}

void pl_close_dll(void *library)
{
    if (library) {
        dlclose(library);
    }
}

void *pl_get_dll_proc(void *library, char const *name)
{
    if (library) {
        return dlsym(library, name);
    }

    return NULL;
}
