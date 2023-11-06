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

#define QU_MODULE "android"

//------------------------------------------------------------------------------

#include "qu.h"

#include <android/log.h>
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

struct android_egl_state
{
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;

    int width;
    int height;
    int samples;
};

struct android_priv_state
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

static struct android_egl_state egl;
static struct android_priv_state priv;

//------------------------------------------------------------------------------
// EGL

/**
 * Creates OpenGL context using EGL. Returns 0 on success, -1 otherwise.
 */
static int initialize_egl(void)
{
    if (!priv.window) {
        QU_LOGE("NativeWindow is unavailable, can't use EGL.\n");
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
        QU_LOGE("No suitable EGL configuration found.\n");
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
        QU_LOGE("Failed to activate OpenGL context.\n");
        return -1;
    }

    egl.display = dpy;
    egl.surface = surface;
    egl.context = context;

    eglQuerySurface(dpy, surface, EGL_WIDTH, &egl.width);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &egl.height);

    egl.samples = best_samples;

    QU_LOGI("Successfully created OpenGL context using EGL.\n");

    qu_event_context_restored();

    return 0;
}

/**
 * Destroys OpenGL context if it's present.
 */
static void terminate_egl(void)
{
    qu_event_context_lost();

    if (egl.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (egl.context != EGL_NO_CONTEXT) {
            eglDestroyContext(egl.display, egl.context);
        }

        if (egl.surface != EGL_NO_SURFACE) {
            eglDestroySurface(egl.display, egl.surface);
        }

        eglTerminate(egl.display);

        QU_LOGI("EGL terminated.\n");
    }

    egl.display = EGL_NO_DISPLAY;
    egl.surface = EGL_NO_SURFACE;
    egl.context = EGL_NO_CONTEXT;
}

//------------------------------------------------------------------------------
// Application thread

/**
 * Handle touchscreen events.
 */
static int handle_motion_event(AInputEvent const *event)
{
    int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;

    if (action == AMOTION_EVENT_ACTION_MOVE) {
        size_t pointerCount = AMotionEvent_getPointerCount(event);

        for (size_t i = 0; i < pointerCount; i++) {
            qu_enqueue_event(&(qu_event) {
                .type = QU_EVENT_TYPE_TOUCH_MOVED,
                .data.touch = {
                    .index = AMotionEvent_getPointerId(event, i),
                    .x = (int) AMotionEvent_getX(event, i),
                    .y = (int) AMotionEvent_getY(event, i),
                },
            });
        }

        return 1;
    }

    int type = -1;

    if (action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        type = QU_EVENT_TYPE_TOUCH_STARTED;
    }

    if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_POINTER_UP) {
        type = QU_EVENT_TYPE_TOUCH_ENDED;
    }

    if (type == -1) {
        return 0;
    }

    int index = 0;

    if (action == AMOTION_EVENT_ACTION_POINTER_UP || action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        int32_t mask = AMOTION_EVENT_ACTION_POINTER_INDEX_MASK;
        int32_t shift = AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        index = (AMotionEvent_getAction(event) & mask) >> shift;
    }

    qu_enqueue_event(&(qu_event) {
        .type = type,
        .data.touch = {
            .index = AMotionEvent_getPointerId(event, index),
            .x = (int) AMotionEvent_getX(event, index),
            .y = (int) AMotionEvent_getY(event, index),
        },
    });

    return 1;
}

/**
 * Handle key presses.
 * TODO: implement
 */
static int handle_key_event(AInputEvent const *event)
{
    return 0;
}

/**
 * Input callback of the ALooper. Attached when AInputQueue is available.
 * Should return 0 to stop receiving events, and 1 to continue.
 */
static int input_looper_callback(int fd, int events, void *arg)
{
    AInputEvent *event = NULL;

    while (AInputQueue_getEvent(priv.input_queue, &event) >= 0) {
        if (AInputQueue_preDispatchEvent(priv.input_queue, event)) {
            continue;
        }

        int handled = 0;

        switch (AInputEvent_getType(event)) {
        case AINPUT_EVENT_TYPE_MOTION:
            handled = handle_motion_event(event);
            break;
        case AINPUT_EVENT_TYPE_KEY:
            handled = handle_key_event(event);
            break;
        }

        AInputQueue_finishEvent(priv.input_queue, event, handled);
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

        if (priv.activity_state == ACTIVITY_STATE_RESUMED) {
            qu_enqueue_event(&(qu_event) { .type = QU_EVENT_TYPE_ACTIVATE });
        } else if (priv.activity_state == ACTIVITY_STATE_STOPPED) {
            qu_enqueue_event(&(qu_event) { .type = QU_EVENT_TYPE_DEACTIVATE });
        }
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
            qu_event_window_resize(width, height);
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
    QU_LOGD("Start: %p\n", activity);
    set_activity_state(ACTIVITY_STATE_STARTED);
}

static void onResume(ANativeActivity *activity)
{
    QU_LOGD("Resume: %p\n", activity);
    set_activity_state(ACTIVITY_STATE_RESUMED);
}

static void* onSaveInstanceState(ANativeActivity *activity, size_t *outLen)
{
    QU_LOGD("SaveInstanceState: %p\n", activity);
    return NULL;
}

static void onPause(ANativeActivity *activity)
{
    QU_LOGD("Pause: %p\n", activity);
    set_activity_state(ACTIVITY_STATE_PAUSED);
}

static void onStop(ANativeActivity *activity)
{
    QU_LOGD("Stop: %p\n", activity);
    set_activity_state(ACTIVITY_STATE_STOPPED);
}

static void onDestroy(ANativeActivity *activity)
{
    QU_LOGD("onDestroy: %p\n", activity);

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
    QU_LOGD("WindowFocusChanged: %p -- %d\n", activity, focused);
}

static void onNativeWindowCreated(ANativeActivity *activity, ANativeWindow *window)
{
    QU_LOGD("NativeWindowCreated: %p -- %p\n", activity, window);
    set_window(window);
}

static void onNativeWindowResized(ANativeActivity *activity, ANativeWindow *window)
{
    QU_LOGD("NativeWindowResized: %p -- %p\n", activity, window);
    write_command(COMMAND_RESIZE);
}

static void onNativeWindowRedrawNeeded(ANativeActivity *activity, ANativeWindow *window)
{
    QU_LOGD("NativeWindowRedrawNeeded: %p -- %p\n", activity, window);
}

static void onNativeWindowDestroyed(ANativeActivity *activity, ANativeWindow *window)
{
    QU_LOGD("NativeWindowDestroyed: %p -- %p\n", activity, window);
    set_window(NULL);
}

static void onInputQueueCreated(ANativeActivity *activity, AInputQueue *queue)
{
    QU_LOGD("InputQueueCreated: %p -- %p\n", activity, queue);
    set_input_queue(queue);
}

static void onInputQueueDestroyed(ANativeActivity *activity, AInputQueue *queue)
{
    QU_LOGD("InputQueueDestroyed: %p -- %p\n", activity, queue);
    set_input_queue(NULL);
}

static void onContentRectChanged(ANativeActivity *activity, const ARect* r)
{
    QU_LOGD("ContentRectChanged: l=%d,t=%d,r=%d,b=%d\n", r->left, r->top, r->right, r->bottom);
}

static void onConfigurationChanged(ANativeActivity *activity)
{
    QU_LOGD("ConfigurationChanged: %p\n", activity);
}

static void onLowMemory(ANativeActivity *activity)
{
    QU_LOGD("LowMemory: %p\n", activity);
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

static qu_result android_precheck(qu_params const *params)
{
    return QU_SUCCESS;
}

static qu_result android_initialize(qu_params const *params)
{
    return QU_SUCCESS;
}

static void android_terminate(void)
{
}

static bool android_process_events(void)
{
    // Everything is handled in callbacks.
    ALooper_pollAll(0, NULL, NULL, NULL);

    if (priv.activity_state == ACTIVITY_STATE_ON_DESTROY) {
        return false;
    }

    if (priv.activity_state == ACTIVITY_STATE_STOPPED) {
        nanosleep(&(struct timespec) { .tv_sec = 0, .tv_nsec = 100000000 }, NULL);
    }

    return true;
}

static void android_swap_buffers(void)
{
    if (egl.display == EGL_NO_DISPLAY) {
        return;
    }

    eglSwapBuffers(egl.display, egl.surface);
}

static char const *android_get_graphics_context_name(void)
{
    if (egl.display == EGL_NO_DISPLAY) {
        return NULL;
    }

    return "OpenGL ES 2.0";
}

static void *android_gl_get_proc_address(char const *name)
{
    return eglGetProcAddress(name);
}

static int android_gl_get_sample_count(void)
{
    if (egl.display == EGL_NO_DISPLAY) {
        return 0;
    }

    return egl.samples;
}

static char const *android_get_window_title(void)
{
    return NULL;
}

static bool android_set_window_title(char const *title)
{
    return false;
}

static qu_vec2i android_get_window_size(void)
{
    return (qu_vec2i) { -1, -1 };
}

static bool android_set_window_size(int width, int height)
{
    return false;
}

//------------------------------------------------------------------------------

qu_core_impl const qu_android_core_impl = {
    .precheck = android_precheck,
    .initialize = android_initialize,
    .terminate = android_terminate,
    .process_input = android_process_events,
    .swap_buffers = android_swap_buffers,
    .get_graphics_context_name = android_get_graphics_context_name,
    .gl_proc_address = android_gl_get_proc_address,
    .get_gl_multisample_samples = android_gl_get_sample_count,
    .get_window_title = android_get_window_title,
    .set_window_title = android_set_window_title,
    .get_window_size = android_get_window_size,
    .set_window_size = android_set_window_size,
};

//------------------------------------------------------------------------------

void np_android_write_log(int level, char const *tag, char const *message)
{
    int priorities[] = {
        [QU_LOG_LEVEL_DEBUG] = ANDROID_LOG_VERBOSE,
        [QU_LOG_LEVEL_INFO] = ANDROID_LOG_INFO,
        [QU_LOG_LEVEL_WARNING] = ANDROID_LOG_WARN,
        [QU_LOG_LEVEL_ERROR] = ANDROID_LOG_ERROR,
    };

    int priority;

    if (level >= QU_LOG_LEVEL_DEBUG && level <= QU_LOG_LEVEL_ERROR) {
        priority = priorities[level];
    } else {
        priority = ANDROID_LOG_UNKNOWN;
    }

    char decorated_tag[64];
    snprintf(decorated_tag, 64, "qu-%s", tag);

    __android_log_write(priority, decorated_tag, message);
}

void *np_android_asset_open(char const *path)
{
    return AAssetManager_open(priv.activity->assetManager, path, AASSET_MODE_UNKNOWN);
}

void np_android_asset_close(void *asset)
{
    AAsset_close((AAsset *) asset);
}

int64_t np_android_asset_read(void *dst, size_t size, void *asset)
{
    return AAsset_read((AAsset *) asset, dst, size);
}

int64_t np_android_asset_tell(void *asset)
{
    int64_t length = AAsset_getLength64((AAsset *) asset);
    int64_t remaining_length = AAsset_getRemainingLength64((AAsset *) asset);

    return length - remaining_length;
}

int64_t np_android_asset_seek(void *asset, int64_t offset, int whence)
{
    return AAsset_seek64((AAsset *) asset, offset, whence);
}
