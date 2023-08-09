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

#ifdef __linux__
#define _GNU_SOURCE
#endif

#define QU_MODULE "platform-posix"
#include "qu.h"

#include <errno.h>
#include <pthread.h>

//------------------------------------------------------------------------------
// qu_platform_posix.c: POSIX-specific platform code 
//------------------------------------------------------------------------------

#define THREAD_NAME_LENGTH          256

//------------------------------------------------------------------------------

struct qu_thread
{
    pthread_t id;
    char name[THREAD_NAME_LENGTH];
    qu_thread_func func;
    void *arg;
};

struct qu_mutex
{
    pthread_mutex_t id;
};

//------------------------------------------------------------------------------

static uint64_t start_mediump;
static double start_highp;

//------------------------------------------------------------------------------

void qu_platform_initialize(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    start_mediump = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    start_highp = (double) ts.tv_sec + (ts.tv_nsec / 1.0e9);
}

void qu_platform_terminate(void)
{
}

//------------------------------------------------------------------------------
// Clock

float qu_get_time_mediump(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    unsigned long long msec = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    return (msec - start_mediump) / 1000.0f;
}

double qu_get_time_highp(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (double) ts.tv_sec + (ts.tv_nsec / 1.0e9) - start_highp;
}

//------------------------------------------------------------------------------
// Threads

static void *thread_main(void *thread_ptr)
{
    qu_thread *thread = thread_ptr;
    intptr_t retval = thread->func(thread->arg);
    free(thread);

    return (void *) retval;
}

qu_thread *qu_create_thread(char const *name, qu_thread_func func, void *arg)
{
    qu_thread *thread = calloc(1, sizeof(qu_thread));

    if (!thread) {
        QU_ERROR("Failed to allocate memory for thread \'%s\'.\n", name);
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
        QU_ERROR("Error (code %d) occured while attempting to create thread \'%s\'.\n", error, thread->name);
        free(thread);

        return NULL;
    }

#if defined(__linux__)
    pthread_setname_np(thread->id, thread->name);
#endif

    return thread;
}

void qu_detach_thread(qu_thread *thread)
{
    int error = pthread_detach(thread->id);

    if (error) {
        QU_ERROR("Failed to detach thread \'%s\', error code: %d.\n", thread->name, error);
    }
}

intptr_t qu_wait_thread(qu_thread *thread)
{
    void *retval;
    int error = pthread_join(thread->id, &retval);

    if (error) {
        QU_ERROR("Failed to join thread \'%s\', error code: %d.\n", thread->name, error);
    }

    return (intptr_t) retval;
}

qu_mutex *qu_create_mutex(void)
{
    qu_mutex *mutex = calloc(1, sizeof(qu_mutex));

    if (!mutex) {
        return NULL;
    }

    int error = pthread_mutex_init(&mutex->id, NULL);

    if (error) {
        QU_ERROR("Failed to create mutex, error code: %d.\n", error);
        return NULL;
    }

    return mutex;
}

void qu_destroy_mutex(qu_mutex *mutex)
{
    if (!mutex) {
        return;
    }

    int error = pthread_mutex_destroy(&mutex->id);

    if (error) {
        QU_ERROR("Failed to destroy mutex, error code: %d.\n", error);
    }

    free(mutex);
}

void qu_lock_mutex(qu_mutex *mutex)
{
    pthread_mutex_lock(&mutex->id);
}

void qu_unlock_mutex(qu_mutex *mutex)
{
    pthread_mutex_unlock(&mutex->id);
}

void qu_sleep(double seconds)
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
