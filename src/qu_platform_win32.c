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

#define QU_MODULE "platform-win32"
#include "qu.h"

//------------------------------------------------------------------------------
// qu_platform_win32.c: Win32-specific platform code
//------------------------------------------------------------------------------

#define THREAD_FLAG_WAIT            0x01
#define THREAD_FLAG_DETACHED        0x02

//------------------------------------------------------------------------------

struct qu_thread
{
    DWORD id;
    HANDLE handle;
    CRITICAL_SECTION cs;
    UINT flags;
    char const *name;
    qu_thread_func func;
    void *arg;
};

struct qu_mutex
{
    CRITICAL_SECTION cs;
};

//------------------------------------------------------------------------------

static double      frequency_highp;
static double      start_highp;
static float       start_mediump;

//------------------------------------------------------------------------------

void qu_platform_initialize(void)
{
    LARGE_INTEGER perf_clock_frequency, perf_clock_count;

    QueryPerformanceFrequency(&perf_clock_frequency);
    QueryPerformanceCounter(&perf_clock_count);

    frequency_highp = (double) perf_clock_frequency.QuadPart;
    start_highp = (double) perf_clock_count.QuadPart / frequency_highp;
    start_mediump = (float) GetTickCount() / 1000.f;
}

void qu_platform_terminate(void)
{
}

enum qu_core qu_get_core_type(void)
{
    return QU_CORE_WIN32;
}

//------------------------------------------------------------------------------
// Clock

float qu_get_time_mediump(void)
{
    float seconds = (float) GetTickCount() / 1000.f;
    return seconds - start_mediump;
}

double qu_get_time_highp(void)
{
    LARGE_INTEGER perf_clock_counter;
    QueryPerformanceCounter(&perf_clock_counter);

    double seconds = (double) perf_clock_counter.QuadPart / frequency_highp;
    return seconds - start_highp;
}

//------------------------------------------------------------------------------
// Threads

static void thread_end(qu_thread *thread)
{
    // If the thread is detached, then only its info struct should be freed.
    if (!(thread->flags & THREAD_FLAG_DETACHED)) {
        EnterCriticalSection(&thread->cs);

        // If the thread is being waited in wait_thread(), do not release
        // its resources.
        if (thread->flags & THREAD_FLAG_WAIT) {
            LeaveCriticalSection(&thread->cs);
            return;
        }

        LeaveCriticalSection(&thread->cs);

        CloseHandle(thread->handle);
        DeleteCriticalSection(&thread->cs);
    }

    HeapFree(GetProcessHeap(), 0, thread);
}

static DWORD WINAPI thread_main(LPVOID param)
{
    qu_thread *thread = (qu_thread *) param;
    intptr_t retval = thread->func(thread->arg);

    thread_end(thread);

    return retval;
}

qu_thread *qu_create_thread(char const *name, qu_thread_func func, void *arg)
{
    qu_thread *thread = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(qu_thread));

    if (!thread) {
        return NULL;
    }

    thread->name = name;
    thread->func = func;
    thread->arg = arg;

    InitializeCriticalSection(&thread->cs);
    thread->flags = 0;

    thread->handle = CreateThread(NULL, 0, thread_main, thread, 0, &thread->id);

    if (!thread->handle) {
        HeapFree(GetProcessHeap(), 0, thread);
        return NULL;
    }

    return thread;
}

void qu_detach_thread(qu_thread *thread)
{
    EnterCriticalSection(&thread->cs);
    thread->flags |= THREAD_FLAG_DETACHED;
    LeaveCriticalSection(&thread->cs);

    CloseHandle(thread->handle);
    DeleteCriticalSection(&thread->cs);
}

intptr_t qu_wait_thread(qu_thread *thread)
{
    EnterCriticalSection(&thread->cs);

    if (thread->flags & THREAD_FLAG_DETACHED) {
        LeaveCriticalSection(&thread->cs);
        return -1;
    }

    thread->flags |= THREAD_FLAG_WAIT;
    LeaveCriticalSection(&thread->cs);

    DWORD retval;
    WaitForSingleObject(thread->handle, INFINITE);
    GetExitCodeThread(thread->handle, &retval);

    thread->flags &= ~THREAD_FLAG_WAIT;
    thread_end(thread);

    return (intptr_t) retval;
}

qu_mutex *qu_create_mutex(void)
{
    qu_mutex *mutex = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(qu_mutex));

    if (!mutex) {
        return NULL;
    }

    InitializeCriticalSection(&mutex->cs);

    return mutex;
}

void qu_destroy_mutex(qu_mutex *mutex)
{
    DeleteCriticalSection(&mutex->cs);
    HeapFree(GetProcessHeap(), 0, mutex);
}

void qu_lock_mutex(qu_mutex *mutex)
{
    EnterCriticalSection(&mutex->cs);
}

void qu_unlock_mutex(qu_mutex *mutex)
{
    LeaveCriticalSection(&mutex->cs);
}

void qu_sleep(double seconds)
{
    DWORD milliseconds = (DWORD) (seconds * 1000);
    Sleep(milliseconds);
}
