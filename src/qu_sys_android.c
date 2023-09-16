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
// qu_sys_android.c: Android native activity implementation
//------------------------------------------------------------------------------

#define QU_MODULE "sys-android"

#include "qu.h"

#include <android/native_activity.h>
#include <EGL/egl.h>
#include <pthread.h>
#include <unistd.h>

//------------------------------------------------------------------------------

/**
 * Android application entry point.
 */
void android_main(void);

//------------------------------------------------------------------------------

#define COMMAND_SET_ACTIVITY_STATE      0x00
#define COMMAND_SET_INPUT_QUEUE         0x01
#define COMMAND_SET_WINDOW              0x02
#define COMMAND_RESIZE                  0x03

#define ACTIVITY_STATE_CREATED          0
#define ACTIVITY_STATE_STARTED          1
#define ACTIVITY_STATE_RESUMED          2
#define ACTIVITY_STATE_PAUSED           3
#define ACTIVITY_STATE_STOPPED          4
#define ACTIVITY_STATE_ON_DESTROY       5
#define ACTIVITY_STATE_DESTROYED        6

//------------------------------------------------------------------------------

struct sys_android_egl
{
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;

    int width;
    int height;
    int samples;
};

struct sys_android_priv
{
    int running;
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    int msgread;
    int msgwrite;

    ANativeActivity *activity;
    ALooper *looper;

    int activity_state;
    int next_activity_state;

    AInputQueue *input_queue;
    AInputQueue *next_input_queue;

    ANativeWindow *window;
    ANativeWindow *next_window;
};

//------------------------------------------------------------------------------

static struct sys_android_egl egl;
static struct sys_android_priv priv;

//------------------------------------------------------------------------------
// EGL

/**
 * Creates OpenGL context using EGL. Returns 0 on success, -1 otherwise.
 */
static int initialize_egl(void)
{
    if (!priv.window) {
        QU_ERROR("NativeWindow is unavailable, can't use EGL.\n");
        return -1;
    }

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    eglInitialize(dpy, NULL, NULL);

    EGLint attribs[] = {
        EGL_SURFACE_TYPE,   EGL_WINDOW_BIT,
        EGL_RED_SIZE,       8,
        EGL_BLUE_SIZE,      8,
        EGL_GREEN_SIZE,     8,
        EGL_NONE,
    };

    EGLint config_count;
    EGLConfig configs[32];

    eglChooseConfig(dpy, attribs, configs, 32, &config_count);

    if (config_count == 0) {
        QU_ERROR("No suitable EGL configuration found.\n");
        return -1;
    }

    int best_config = -1;
    int best_samples = 0;

    for (int i = 0; i < config_count; i++) {
        EGLConfig config = configs[i];
        EGLint red, green, blue, samples;

        eglGetConfigAttrib(dpy, config, EGL_RED_SIZE, &red);
        eglGetConfigAttrib(dpy, config, EGL_GREEN_SIZE, &green);
        eglGetConfigAttrib(dpy, config, EGL_BLUE_SIZE, &blue);
        eglGetConfigAttrib(dpy, config, EGL_SAMPLES, &samples);

        if (red == 8 && green == 8 && blue == 8 && samples > best_samples) {
            best_config = i;
            best_samples = samples;
        }
    }

    EGLConfig config = configs[(best_config == -1) ? 0 : best_config];

    EGLint format;
    eglGetConfigAttrib(dpy, config, EGL_NATIVE_VISUAL_ID, &format);

    EGLSurface surface = eglCreateWindowSurface(dpy, config, priv.window, NULL);

    EGLint ctx_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 2,
        EGL_CONTEXT_MINOR_VERSION, 0,
        EGL_NONE,
    };

    EGLContext context = eglCreateContext(dpy, config, NULL, ctx_attribs);

    if (!eglMakeCurrent(dpy, surface, surface, context)) {
        QU_ERROR("Failed to activate OpenGL context.\n");
        return -1;
    }

    egl.display = dpy;
    egl.surface = surface;
    egl.context = context;

    eglQuerySurface(dpy, surface, EGL_WIDTH, &egl.width);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &egl.height);

    egl.samples = best_samples;

    QU_INFO("Successfully created OpenGL context using EGL.\n");

    qu__graphics_on_context_restored();

    return 0;
}

/**
 * Destroys OpenGL context if it's present.
 */
static void terminate_egl(void)
{
    qu__graphics_on_context_lost();

    if (egl.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (egl.context != EGL_NO_CONTEXT) {
            eglDestroyContext(egl.display, egl.context);
        }

        if (egl.surface != EGL_NO_SURFACE) {
            eglDestroySurface(egl.display, egl.surface);
        }

        eglTerminate(egl.display);

        QU_INFO("EGL terminated.\n");
    }

    egl.display = EGL_NO_DISPLAY;
    egl.surface = EGL_NO_SURFACE;
    egl.context = EGL_NO_CONTEXT;
}

//------------------------------------------------------------------------------
// Application thread

/**
 * Input callback of the ALooper. Attached when AInputQueue is available.
 * Should return 0 to stop receiving events, and 1 to continue.
 */
static int input_looper_callback(int fd, int events, void *arg)
{
    AInputEvent *event = NULL;

    while (AInputQueue_getEvent(priv.input_queue, &event) >= 0) {
        QU_DEBUG("input event: %d\n", AInputEvent_getType(event));

        if (AInputQueue_preDispatchEvent(priv.input_queue, event)) {
            continue;
        }

        // TODO: handle events
        // handled is 1 if event is consumed

        AInputQueue_finishEvent(priv.input_queue, event, 0);
    }

    return 1;
}

/**
 * Main callback of the ALooper.
 * Should return 0 to stop receiving events, and 1 to continue.
 */
static int main_looper_callback(int fd, int events, void *arg)
{
    char command;

    if (read(priv.msgread, &command, sizeof(char)) != sizeof(char)) {
        return 0;
    }

    switch (command) {
    case COMMAND_SET_ACTIVITY_STATE:
        pthread_mutex_lock(&priv.mutex);
        {
            priv.activity_state = priv.next_activity_state;
            pthread_cond_broadcast(&priv.cond);
        }
        pthread_mutex_unlock(&priv.mutex);
        break;
    case COMMAND_SET_INPUT_QUEUE:
        pthread_mutex_lock(&priv.mutex);
        {
            if (priv.input_queue) {
                AInputQueue_detachLooper(priv.input_queue);
            }

            priv.input_queue = priv.next_input_queue;

            if (priv.input_queue) {
                AInputQueue_attachLooper(
                    priv.input_queue, priv.looper, ALOOPER_EVENT_INPUT,
                    input_looper_callback, NULL
                );
            }

            pthread_cond_broadcast(&priv.cond);
        }
        pthread_mutex_unlock(&priv.mutex);
        break;
    case COMMAND_SET_WINDOW:
        terminate_egl();

        pthread_mutex_lock(&priv.mutex);
        {
            priv.window = priv.next_window;
            pthread_cond_broadcast(&priv.cond);
        }
        pthread_mutex_unlock(&priv.mutex);

        if (priv.window) {
            initialize_egl();
        }
        break;
    case COMMAND_RESIZE:
        if (priv.window) {
            int width = ANativeWindow_getWidth(priv.window);
            int height = ANativeWindow_getHeight(priv.window);
            qu__graphics_on_display_resize(width, height);
        }
        break;
    }

    return 1;
}

/**
 * Entry point of application thread.
 */
static void *thread_main(void *arg)
{
    priv.looper = ALooper_prepare(0);

    ALooper_addFd(
        priv.looper, priv.msgread, ALOOPER_POLL_CALLBACK,
        ALOOPER_EVENT_INPUT, main_looper_callback, NULL
    );

    // Notify the main thread that we're ready to start.
    pthread_mutex_lock(&priv.mutex);
    {
        priv.running = 1;
        pthread_cond_broadcast(&priv.cond);
    }
    pthread_mutex_unlock(&priv.mutex);

    // Give control to the user.
    android_main();

    if (priv.input_queue) {
        AInputQueue_detachLooper(priv.input_queue);
    }

    pthread_mutex_lock(&priv.mutex);
    {
        priv.activity_state = ACTIVITY_STATE_DESTROYED;
        pthread_cond_broadcast(&priv.cond);
    }
    pthread_mutex_unlock(&priv.mutex);

    return NULL;
}

//------------------------------------------------------------------------------
// Commands

static void write_command(char command)
{
    write(priv.msgwrite, &command, sizeof(char));
}

static void set_activity_state(int state)
{
    pthread_mutex_lock(&priv.mutex);
    {
        priv.next_activity_state = state;
        write_command(COMMAND_SET_ACTIVITY_STATE);

        while (priv.activity_state != state) {
            pthread_cond_wait(&priv.cond, &priv.mutex);
        }
    }
    pthread_mutex_unlock(&priv.mutex);
}

static void set_input_queue(AInputQueue *queue)
{
    pthread_mutex_lock(&priv.mutex);
    {
        priv.next_input_queue = queue;
        write_command(COMMAND_SET_INPUT_QUEUE);

        while (priv.input_queue != queue) {
            pthread_cond_wait(&priv.cond, &priv.mutex);
        }
    }
    pthread_mutex_unlock(&priv.mutex);
}

static void set_window(ANativeWindow *window)
{
    pthread_mutex_lock(&priv.mutex);
    {
        priv.next_window = window;
        write_command(COMMAND_SET_WINDOW);

        while (priv.window != window) {
            pthread_cond_wait(&priv.cond, &priv.mutex);
        }
    }
    pthread_mutex_unlock(&priv.mutex);
}

//------------------------------------------------------------------------------
// Native activity

static void onStart(ANativeActivity *activity)
{
    QU_DEBUG("Start: %p\n", activity);
    set_activity_state(ACTIVITY_STATE_STARTED);
}

static void onResume(ANativeActivity *activity)
{
    QU_DEBUG("Resume: %p\n", activity);
    set_activity_state(ACTIVITY_STATE_RESUMED);
}

static void* onSaveInstanceState(ANativeActivity *activity, size_t *outLen)
{
    QU_DEBUG("SaveInstanceState: %p\n", activity);
    return NULL;
}

static void onPause(ANativeActivity *activity)
{
    QU_DEBUG("Pause: %p\n", activity);
    set_activity_state(ACTIVITY_STATE_PAUSED);
}

static void onStop(ANativeActivity *activity)
{
    QU_DEBUG("Stop: %p\n", activity);
    set_activity_state(ACTIVITY_STATE_STOPPED);
}

static void onDestroy(ANativeActivity *activity)
{
    QU_DEBUG("onDestroy: %p\n", activity);

    set_activity_state(ACTIVITY_STATE_ON_DESTROY);

    pthread_mutex_lock(&priv.mutex);
    {
        while (priv.activity_state != ACTIVITY_STATE_DESTROYED) {
            pthread_cond_wait(&priv.cond, &priv.mutex);
        }
    }
    pthread_mutex_unlock(&priv.mutex);

    close(priv.msgread);
    close(priv.msgwrite);

    pthread_cond_destroy(&priv.cond);
    pthread_mutex_destroy(&priv.mutex);
}

static void onWindowFocusChanged(ANativeActivity *activity, int focused)
{
    QU_DEBUG("WindowFocusChanged: %p -- %d\n", activity, focused);
}

static void onNativeWindowCreated(ANativeActivity *activity, ANativeWindow *window)
{
    QU_DEBUG("NativeWindowCreated: %p -- %p\n", activity, window);
    set_window(window);
}

static void onNativeWindowResized(ANativeActivity *activity, ANativeWindow *window)
{
    QU_DEBUG("NativeWindowResized: %p -- %p\n", activity, window);
    write_command(COMMAND_RESIZE);
}

static void onNativeWindowRedrawNeeded(ANativeActivity *activity, ANativeWindow *window)
{
    QU_DEBUG("NativeWindowRedrawNeeded: %p -- %p\n", activity, window);
}

static void onNativeWindowDestroyed(ANativeActivity *activity, ANativeWindow *window)
{
    QU_DEBUG("NativeWindowDestroyed: %p -- %p\n", activity, window);
    set_window(NULL);
}

static void onInputQueueCreated(ANativeActivity *activity, AInputQueue *queue)
{
    QU_DEBUG("InputQueueCreated: %p -- %p\n", activity, queue);
    set_input_queue(queue);
}

static void onInputQueueDestroyed(ANativeActivity *activity, AInputQueue *queue)
{
    QU_DEBUG("InputQueueDestroyed: %p -- %p\n", activity, queue);
    set_input_queue(NULL);
}

static void onContentRectChanged(ANativeActivity *activity, const ARect* r)
{
    QU_DEBUG("ContentRectChanged: l=%d,t=%d,r=%d,b=%d\n", r->left, r->top, r->right, r->bottom);
}

static void onConfigurationChanged(ANativeActivity *activity)
{
    QU_DEBUG("ConfigurationChanged: %p\n", activity);
}

static void onLowMemory(ANativeActivity *activity)
{
    QU_DEBUG("LowMemory: %p\n", activity);
}

/**
 * This is basically an entry point of any Android C/C++ app.
 */
JNIEXPORT void __unused ANativeActivity_onCreate(ANativeActivity *activity, void *savedState, size_t savedStateSize)
{
    activity->callbacks->onStart = onStart;
    activity->callbacks->onResume = onResume;
    activity->callbacks->onSaveInstanceState = onSaveInstanceState;
    activity->callbacks->onPause = onPause;
    activity->callbacks->onStop = onStop;
    activity->callbacks->onDestroy = onDestroy;
    activity->callbacks->onWindowFocusChanged = onWindowFocusChanged;
    activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
    activity->callbacks->onNativeWindowResized = onNativeWindowResized;
    activity->callbacks->onNativeWindowRedrawNeeded = onNativeWindowRedrawNeeded;
    activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
    activity->callbacks->onInputQueueCreated = onInputQueueCreated;
    activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;
    activity->callbacks->onContentRectChanged = onContentRectChanged;
    activity->callbacks->onConfigurationChanged = onConfigurationChanged;
    activity->callbacks->onLowMemory = onLowMemory;

    memset(&priv, 0, sizeof(priv));

    pthread_mutex_init(&priv.mutex, NULL);
    pthread_cond_init(&priv.cond, NULL);

    int msgpipe[2];

    if (pipe(msgpipe)) {
        return;
    }

    priv.msgread = msgpipe[0];
    priv.msgwrite = msgpipe[1];

    priv.activity = activity;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&priv.thread, &attr, thread_main, NULL);

    // Wait until the thread is started running.
    pthread_mutex_lock(&priv.mutex);
    {
        while (!priv.running) {
            pthread_cond_wait(&priv.cond, &priv.mutex);
        }
    }
    pthread_mutex_unlock(&priv.mutex);
}

//------------------------------------------------------------------------------

int qx_android_poll_events(void)
{
    // Everything is handled in callbacks.
    ALooper_pollAll(0, NULL, NULL, NULL);

    if (priv.activity_state == ACTIVITY_STATE_ON_DESTROY) {
        return -1;
    }

    return 0;
}

int qx_android_swap_buffers(void)
{
    if (egl.display == EGL_NO_DISPLAY) {
        return -1;
    }

    eglSwapBuffers(egl.display, egl.surface);
    return 0;
}

int qx_android_get_renderer(void)
{
    if (egl.display == EGL_NO_DISPLAY) {
        return QU__RENDERER_NULL;
    }

    return QU__RENDERER_ES2;
}

void *qx_android_gl_get_proc_address(char const *name)
{
    return eglGetProcAddress(name);
}

int qx_android_gl_get_sample_count(void)
{
    if (egl.display == EGL_NO_DISPLAY) {
        return 0;
    }

    return egl.samples;
}

//------------------------------------------------------------------------------

void *qx_sys_fopen(char const *path)
{
    return AAssetManager_open(priv.activity->assetManager, path, AASSET_MODE_UNKNOWN);
}

void qx_sys_fclose(void *file)
{
    AAsset_close((AAsset *) file);
}

int64_t qx_sys_fread(void *buffer, size_t size, void *file)
{
    return AAsset_read((AAsset *) file, buffer, size);
}

int64_t qx_sys_ftell(void *file)
{
    int64_t length = AAsset_getLength64((AAsset *) file);
    return length - AAsset_getRemainingLength64((AAsset *) file);
}

int64_t qx_sys_fseek(void *file, int64_t offset, int origin)
{
    return AAsset_seek64((AAsset *) file, offset, origin);
}

size_t qx_sys_get_file_size(void *file)
{
    return AAsset_getLength64((AAsset *) file);
}
