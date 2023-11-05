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

#define _GNU_SOURCE

#if defined(__linux__)
#   define _POSIX_C_SOURCE 199309L
#   define _XOPEN_SOURCE 500
#endif

#define QU_MODULE "x11"
#include "qu.h"

#include <errno.h>
#include <pthread.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <GL/glx.h>

//------------------------------------------------------------------------------
// qu_core_x11.c: Xlib-based core module
//------------------------------------------------------------------------------

enum
{
    WM_PROTOCOLS,
    WM_DELETE_WINDOW,
    UTF8_STRING,
    NET_WM_NAME,
    TOTAL_ATOMS,
};

enum
{
    ARB_CREATE_CONTEXT = (1 << 0),
    ARB_CREATE_CONTEXT_PROFILE = (1 << 1),
    EXT_SWAP_CONTROL = (1 << 2),
    EXT_CREATE_CONTEXT_ES2_PROFILE = (1 << 3),
};

static struct
{
    int gl_profile;

    // Xlib variables

    Display *display;
    int screen;
    Window root;
    Colormap colormap;
    long event_mask;
    Window window;
    Atom atoms[TOTAL_ATOMS];

    // GLX-related variables

    GLXContext context;
    GLXDrawable surface;
    unsigned long extensions;

    // GLX extension function pointers

    PFNGLXSWAPINTERVALEXTPROC ext_glXSwapIntervalEXT;
    PFNGLXCREATECONTEXTATTRIBSARBPROC ext_glXCreateContextAttribsARB;

    // Window structure state

    int width;
    int height;
} impl;

//------------------------------------------------------------------------------

static qu_key key_conv(KeySym sym)
{
    switch (sym)
    {
    case XK_0:              return QU_KEY_0;
    case XK_1:              return QU_KEY_1;
    case XK_2:              return QU_KEY_2;
    case XK_3:              return QU_KEY_3;
    case XK_4:              return QU_KEY_4;
    case XK_5:              return QU_KEY_5;
    case XK_6:              return QU_KEY_6;
    case XK_7:              return QU_KEY_7;
    case XK_8:              return QU_KEY_8;
    case XK_9:              return QU_KEY_9;
    case XK_a:              return QU_KEY_A;
    case XK_b:              return QU_KEY_B;
    case XK_c:              return QU_KEY_C;
    case XK_d:              return QU_KEY_D;
    case XK_e:              return QU_KEY_E;
    case XK_f:              return QU_KEY_F;
    case XK_g:              return QU_KEY_G;
    case XK_h:              return QU_KEY_H;
    case XK_i:              return QU_KEY_I;
    case XK_j:              return QU_KEY_J;
    case XK_k:              return QU_KEY_K;
    case XK_l:              return QU_KEY_L;
    case XK_m:              return QU_KEY_M;
    case XK_n:              return QU_KEY_N;
    case XK_o:              return QU_KEY_O;
    case XK_p:              return QU_KEY_P;
    case XK_q:              return QU_KEY_Q;
    case XK_r:              return QU_KEY_R;
    case XK_s:              return QU_KEY_S;
    case XK_t:              return QU_KEY_T;
    case XK_u:              return QU_KEY_U;
    case XK_v:              return QU_KEY_V;
    case XK_w:              return QU_KEY_W;
    case XK_x:              return QU_KEY_X;
    case XK_y:              return QU_KEY_Y;
    case XK_z:              return QU_KEY_Z;
    case XK_grave:          return QU_KEY_GRAVE;
    case XK_apostrophe:     return QU_KEY_APOSTROPHE;
    case XK_minus:          return QU_KEY_MINUS;
    case XK_equal:          return QU_KEY_EQUAL;
    case XK_bracketleft:    return QU_KEY_LBRACKET;
    case XK_bracketright:   return QU_KEY_RBRACKET;
    case XK_comma:          return QU_KEY_COMMA;
    case XK_period:         return QU_KEY_PERIOD;
    case XK_semicolon:      return QU_KEY_SEMICOLON;
    case XK_slash:          return QU_KEY_SLASH;
    case XK_backslash:      return QU_KEY_BACKSLASH;
    case XK_space:          return QU_KEY_SPACE;
    case XK_Escape:         return QU_KEY_ESCAPE;
    case XK_BackSpace:      return QU_KEY_BACKSPACE;
    case XK_Tab:            return QU_KEY_TAB;
    case XK_Return:         return QU_KEY_ENTER;
    case XK_F1:             return QU_KEY_F1;
    case XK_F2:             return QU_KEY_F2;
    case XK_F3:             return QU_KEY_F3;
    case XK_F4:             return QU_KEY_F4;
    case XK_F5:             return QU_KEY_F5;
    case XK_F6:             return QU_KEY_F6;
    case XK_F7:             return QU_KEY_F7;
    case XK_F8:             return QU_KEY_F8;
    case XK_F9:             return QU_KEY_F9;
    case XK_F10:            return QU_KEY_F10;
    case XK_F11:            return QU_KEY_F11;
    case XK_F12:            return QU_KEY_F12;
    case XK_Up:             return QU_KEY_UP;
    case XK_Down:           return QU_KEY_DOWN;
    case XK_Left:           return QU_KEY_LEFT;
    case XK_Right:          return QU_KEY_RIGHT;
    case XK_Shift_L:        return QU_KEY_LSHIFT;
    case XK_Shift_R:        return QU_KEY_RSHIFT;
    case XK_Control_L:      return QU_KEY_LCTRL;
    case XK_Control_R:      return QU_KEY_RCTRL;
    case XK_Alt_L:          return QU_KEY_LALT;
    case XK_Alt_R:          return QU_KEY_RALT;
    case XK_Super_L:        return QU_KEY_LSUPER;
    case XK_Super_R:        return QU_KEY_RSUPER;
    case XK_Menu:           return QU_KEY_MENU;
    case XK_Page_Up:        return QU_KEY_PGUP;
    case XK_Page_Down:      return QU_KEY_PGDN;
    case XK_Home:           return QU_KEY_HOME;
    case XK_End:            return QU_KEY_END;
    case XK_Insert:         return QU_KEY_INSERT;
    case XK_Delete:         return QU_KEY_DELETE;
    case XK_Print:          return QU_KEY_PRINTSCREEN;
    case XK_Pause:          return QU_KEY_PAUSE;
    case XK_Caps_Lock:      return QU_KEY_CAPSLOCK;
    case XK_Scroll_Lock:    return QU_KEY_SCROLLLOCK;
    case XK_Num_Lock:       return QU_KEY_NUMLOCK;
    case XK_KP_0:           return QU_KEY_KP_0;
    case XK_KP_1:           return QU_KEY_KP_1;
    case XK_KP_2:           return QU_KEY_KP_2;
    case XK_KP_3:           return QU_KEY_KP_3;
    case XK_KP_4:           return QU_KEY_KP_4;
    case XK_KP_5:           return QU_KEY_KP_5;
    case XK_KP_6:           return QU_KEY_KP_6;
    case XK_KP_7:           return QU_KEY_KP_7;
    case XK_KP_8:           return QU_KEY_KP_8;
    case XK_KP_9:           return QU_KEY_KP_9;
    case XK_KP_Multiply:
        return QU_KEY_KP_MUL;
    case XK_KP_Add:
        return QU_KEY_KP_ADD;
    case XK_KP_Subtract:
        return QU_KEY_KP_SUB;
    case XK_KP_Decimal:
        return QU_KEY_KP_POINT;
    case XK_KP_Divide:
        return QU_KEY_KP_DIV;
    case XK_KP_Enter:
        return QU_KEY_KP_ENTER;
    default:
        return QU_KEY_INVALID;
    }
}

static qu_mouse_button mouse_button_conv(unsigned int button)
{
    switch (button) {
    case Button1:           return QU_MOUSE_BUTTON_LEFT;
    case Button2:           return QU_MOUSE_BUTTON_MIDDLE;
    case Button3:           return QU_MOUSE_BUTTON_RIGHT;
    default:                return QU_MOUSE_BUTTON_INVALID;
    }
}

//------------------------------------------------------------------------------

static void set_title(char const *title)
{
    XStoreName(impl.display, impl.window, title);
    XChangeProperty(impl.display, impl.window,
        impl.atoms[NET_WM_NAME],
        impl.atoms[UTF8_STRING],
        8, PropModeReplace,
        (unsigned char const *) title, strlen(title));
}

static void set_display_size(int width, int height)
{
    int x = (DisplayWidth(impl.display, impl.screen) / 2) - (width / 2);
    int y = (DisplayHeight(impl.display, impl.screen) / 2) - (height / 2);

    XMoveResizeWindow(impl.display, impl.window, x, y, width, height);

#if 0
    XSizeHints hints = {
        .flags = PMinSize | PMaxSize,
        .min_width = params->display_width,
        .max_width = params->display_width,
        .min_height = params->display_height,
        .max_height = params->display_height,
    };

    XMoveWindow(impl.display, impl.window, x, y);
    XSetWMNormalHints(impl.display, impl.window, &hints);
#endif
}

static void initialize(qu_params const *params)
{
    memset(&impl, 0, sizeof(impl));

    // (0) Open display

    impl.display = XOpenDisplay(NULL);

    if (!impl.display) {
        QU_HALT("Failed to open X11 display.");
    }

    impl.screen = DefaultScreen(impl.display);
    impl.root = DefaultRootWindow(impl.display);

    // (1) Choose GLX framebuffer configuration

    int fbc_attribs[] = {
        GLX_RED_SIZE,       (8),
        GLX_GREEN_SIZE,     (8),
        GLX_BLUE_SIZE,      (8),
        GLX_ALPHA_SIZE,     (8),
        GLX_RENDER_TYPE,    (GLX_RGBA_BIT),
        GLX_DRAWABLE_TYPE,  (GLX_WINDOW_BIT),
        GLX_X_RENDERABLE,   (True),
        GLX_DOUBLEBUFFER,   (True),
        None,
    };

    int fbc_total = 0;
    GLXFBConfig *fbc_list = glXChooseFBConfig(impl.display, impl.screen,
                                              fbc_attribs, &fbc_total);

    if (!fbc_list || !fbc_total) {
        QU_HALT("No suitable framebuffer configuration found.");
    }

    int best_fbc = -1;
    int best_sample_count = 0;

    for (int i = 0; i < fbc_total; i++) {
        int has_sample_buffers = 0;
        glXGetFBConfigAttrib(impl.display, fbc_list[i], GLX_SAMPLE_BUFFERS, &has_sample_buffers);

        if (!has_sample_buffers) {
            continue;
        }

        int total_samples = 0;
        glXGetFBConfigAttrib(impl.display, fbc_list[i], GLX_SAMPLES, &total_samples);

        if (total_samples > best_sample_count) {
            best_fbc = i;
            best_sample_count = total_samples;
        }
    }

    if (best_fbc >= 0) {
        QU_LOGI("Selected FBConfig with %d samples.\n", best_sample_count);
    }

    GLXFBConfig fbconfig = fbc_list[(best_fbc < 0) ? 0 : best_fbc];
    XFree(fbc_list);

    // (2) Create window

    XVisualInfo *vi = glXGetVisualFromFBConfig(impl.display, fbconfig);

    impl.colormap = XCreateColormap(impl.display, impl.root, vi->visual, AllocNone);
    impl.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
        PointerMotionMask | ButtonPressMask | ButtonReleaseMask |
        StructureNotifyMask;

    impl.window = XCreateWindow(
        impl.display,   // display
        impl.root,      // parent
        0, 0,           // position
        params->display_width,
        params->display_height,
        0,              // border width
        CopyFromParent, // depth
        InputOutput,    // class
        vi->visual,     // visual
        CWColormap | CWEventMask,
        &(XSetWindowAttributes) {
            .colormap = impl.colormap,
            .event_mask = impl.event_mask,
        }
    );

    if (!impl.window) {
        QU_HALT("Failed to create X11 window.");
    }

    // (3) Store X11 atoms and set protocols

    char *atom_names[TOTAL_ATOMS] = {
        [WM_PROTOCOLS] = "WM_PROTOCOLS",
        [WM_DELETE_WINDOW] = "WM_DELETE_WINDOW",
        [UTF8_STRING] = "UTF8_STRING",
        [NET_WM_NAME] = "_NET_WM_NAME",
    };

    XInternAtoms(impl.display, atom_names, TOTAL_ATOMS, False, impl.atoms);

    Atom protocols[] = { impl.atoms[WM_DELETE_WINDOW] };
    XSetWMProtocols(impl.display, impl.window, protocols, 1);

    // (4) WM_CLASS

    XClassHint *class_hint = XAllocClassHint();
    class_hint->res_class = "libqu application";
    XSetClassHint(impl.display, impl.window, class_hint);
    XFree(class_hint);

    // (5) Parse GLX extension list

    char *glx_extension_list = strdup(glXQueryExtensionsString(impl.display, impl.screen));
    char *glx_extension = strtok(glx_extension_list, " ");

    while (glx_extension) {
        if (!strcmp(glx_extension, "GLX_ARB_create_context")) {
            impl.extensions |= ARB_CREATE_CONTEXT;
            impl.ext_glXCreateContextAttribsARB =
                (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddressARB(
                    (GLubyte const *)"glXCreateContextAttribsARB");

            QU_LOGI("GLX_ARB_create_context is supported.\n");
        } else if (!strcmp(glx_extension, "GLX_ARB_create_context_profile")) {
            impl.extensions |= ARB_CREATE_CONTEXT_PROFILE;

            QU_LOGI("GLX_ARB_create_context_profile is supported.\n");
        } else if (!strcmp(glx_extension, "GLX_EXT_swap_control")) {
            impl.extensions |= EXT_SWAP_CONTROL;
            impl.ext_glXSwapIntervalEXT =
                (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddressARB(
                    (GLubyte const *)"glXSwapIntervalEXT");

            QU_LOGI("GLX_EXT_swap_control is supported.\n");
        } else if (!strcmp(glx_extension, "GLX_EXT_create_context_es2_profile")) {
            impl.extensions |= EXT_CREATE_CONTEXT_ES2_PROFILE;
            QU_LOGI("GLX_EXT_create_context_es2_profile is supported.\n");
        }

        glx_extension = strtok(NULL, " ");
    }

    free(glx_extension_list);

    // (6) Create GLX context and surface

    if (impl.extensions & ARB_CREATE_CONTEXT_PROFILE) {
        int major_version = 3;
        int minor_version = 3;
        int profile_mask = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;

#ifdef QU_USE_ES2
        if (impl.extensions & EXT_CREATE_CONTEXT_ES2_PROFILE) {
            major_version = 2;
            minor_version = 0;
            profile_mask = GLX_CONTEXT_ES2_PROFILE_BIT_EXT;
            impl.gl_profile = 1;
        }
#endif

        QU_LOGI("Creating %s %d.%d context...\n",
            impl.gl_profile == 1 ? "OpenGL ES" : "OpenGL",
            major_version, minor_version);

        int ctx_attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB,  major_version,
            GLX_CONTEXT_MINOR_VERSION_ARB,  minor_version,
            GLX_CONTEXT_PROFILE_MASK_ARB,   profile_mask,
            None,
        };

        impl.context = impl.ext_glXCreateContextAttribsARB(
            impl.display, fbconfig, NULL, True, ctx_attribs
        );
    }

    if (!impl.context) {
        QU_LOGI("Creating legacy OpenGL context...\n");
        impl.context = glXCreateContext(impl.display, vi, NULL, True);
        impl.gl_profile = -1;
    }

    XFree(vi);

    if (!impl.context) {
        QU_HALT("Failed to create OpenGL context.");
    }

    impl.surface = glXCreateWindow(impl.display, fbconfig, impl.window, NULL);
    glXMakeContextCurrent(impl.display, impl.surface, impl.surface, impl.context);

    // (7) Set detectable key auto-repeat

    XkbSetDetectableAutoRepeat(impl.display, True, NULL);

    // (8) Set title and size of the window

    XMapWindow(impl.display, impl.window);

    set_title(params->title);
    set_display_size(params->display_width, params->display_height);

    XSync(impl.display, False);

    // (8.1) ...

    XWindowAttributes xwa;
    XGetWindowAttributes(impl.display, impl.window, &xwa);

    impl.width = xwa.width;
    impl.height = xwa.height;

    // (9) Done.

    QU_LOGI("Xlib-based core module initialized.\n");
}

static void terminate(void)
{
    if (impl.display) {
        glXDestroyWindow(impl.display, impl.surface);
        glXDestroyContext(impl.display, impl.context);

        XDestroyWindow(impl.display, impl.window);
        XFreeColormap(impl.display, impl.colormap);
        XCloseDisplay(impl.display);
    }

    QU_LOGI("Xlib-based core module terminated.\n");
}

static bool process(void)
{
    XEvent event;

    while (XCheckTypedWindowEvent(impl.display, impl.window, ClientMessage, &event)) {
        if (event.xclient.data.l[0] == (long) impl.atoms[WM_DELETE_WINDOW]) {
            return false;
        }
    }

    while (XCheckWindowEvent(impl.display, impl.window, impl.event_mask, &event)) {
        switch (event.type) {
        case Expose:
            break;
        case KeyPress:
            qu_enqueue_event(&(qu_event) {
                .type = QU_EVENT_TYPE_KEY_PRESSED,
                .data.keyboard.key = key_conv(XLookupKeysym(&event.xkey, ShiftMapIndex)),
            });
            break;
        case KeyRelease:
            qu_enqueue_event(&(qu_event) {
                .type = QU_EVENT_TYPE_KEY_RELEASED,
                .data.keyboard.key = key_conv(XLookupKeysym(&event.xkey, ShiftMapIndex)),
            });
            break;
        case MotionNotify:
            qu_enqueue_event(&(qu_event) {
                .type = QU_EVENT_TYPE_MOUSE_CURSOR_MOVED,
                .data.mouse.x_cursor = event.xmotion.x,
                .data.mouse.y_cursor = event.xmotion.y,
            });
            break;
        case ButtonPress:
            if (event.xbutton.button == Button4) {
                qu_enqueue_event(&(qu_event) {
                    .type = QU_EVENT_TYPE_MOUSE_WHEEL_SCROLLED,
                    .data.mouse.dx_wheel = 0,
                    .data.mouse.dy_wheel = 1,
                });
            } else if (event.xbutton.button == Button5) {
                qu_enqueue_event(&(qu_event) {
                    .type = QU_EVENT_TYPE_MOUSE_WHEEL_SCROLLED,
                    .data.mouse.dx_wheel = 0,
                    .data.mouse.dy_wheel = -1,
                });
            } else if (event.xbutton.button == 6) {
                qu_enqueue_event(&(qu_event) {
                    .type = QU_EVENT_TYPE_MOUSE_WHEEL_SCROLLED,
                    .data.mouse.dx_wheel = -1,
                    .data.mouse.dy_wheel = 0,
                });
            } else if (event.xbutton.button == 7) {
                qu_enqueue_event(&(qu_event) {
                    .type = QU_EVENT_TYPE_MOUSE_WHEEL_SCROLLED,
                    .data.mouse.dx_wheel = 1,
                    .data.mouse.dy_wheel = 0,
                });
            } else {
                qu_enqueue_event(&(qu_event) {
                    .type = QU_EVENT_TYPE_MOUSE_BUTTON_PRESSED,
                    .data.mouse.button = mouse_button_conv(event.xbutton.button),
                });
            }
            break;
        case ButtonRelease:
            qu_enqueue_event(&(qu_event) {
                .type = QU_EVENT_TYPE_MOUSE_BUTTON_RELEASED,
                .data.mouse.button = mouse_button_conv(event.xbutton.button),
            });
            break;
        case ConfigureNotify:
            if (event.xconfigure.width != impl.width || event.xconfigure.height != impl.height) {
                impl.width = event.xconfigure.width;
                impl.height = event.xconfigure.height;
                qu_event_window_resize(impl.width, impl.height);
            }

            break;
        default:
            QU_LOGD("Unhandled event: 0x%04x\n", event.type);
            break;
        }
    }

    return true;
}

static void present(void)
{
    glXSwapBuffers(impl.display, impl.surface);
}

static char const *get_graphics_context_name(void)
{
    switch (impl.gl_profile) {
    case 0:
        return "OpenGL";
    case 1:
        return "OpenGL ES 2.0";
    default:
        return "OpenGL (Compatibility Profile)";
    }
}

static void *gl_proc_address(char const *name)
{
    return glXGetProcAddress((GLubyte const *) name);
}

static int get_gl_multisample_samples(void)
{
    return 1;
}

static bool set_window_title(char const *title)
{
    return false;
}

static bool set_window_size(int width, int height)
{
    return false;
}

//------------------------------------------------------------------------------

qu_core_impl const qu_x11_core_impl = {
    .initialize = initialize,
    .terminate = terminate,
    .process_input = process,
    .swap_buffers = present,
    .get_graphics_context_name = get_graphics_context_name,
    .gl_proc_address = gl_proc_address,
    .get_gl_multisample_samples = get_gl_multisample_samples,
    .set_window_title = set_window_title,
    .set_window_size = set_window_size,
};
