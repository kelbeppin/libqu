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

#define QU_MODULE "win32"
#include "qu.h"
#include <windowsx.h>
#include <dwmapi.h>
#include <GL/gl.h>
#include <GL/wglext.h>

//------------------------------------------------------------------------------
// qu_core_win32.c: Win32-based core module
//------------------------------------------------------------------------------

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

typedef HRESULT (*SETPROCESSDPIAWARENESSPROC)(int);

//------------------------------------------------------------------------------

static struct
{
    PFNWGLGETEXTENSIONSSTRINGARBPROC    wglGetExtensionsStringARB;
    PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB;
    PFNWGLCHOOSEPIXELFORMATARBPROC      wglChoosePixelFormatARB;
    PFNWGLCREATECONTEXTATTRIBSARBPROC   wglCreateContextAttribsARB;
    PFNWGLSWAPINTERVALEXTPROC           wglSwapIntervalEXT;
} wgl;

static struct
{
    WCHAR       wide_title[256];
    LPCWSTR     class_name;
    LPCWSTR     window_name;
    DWORD       style;
    HWND        window;
    HDC         dc;
    HGLRC       rc;
    HCURSOR     cursor;
    BOOL        hide_cursor;
    BOOL        key_autorepeat;
    UINT        mouse_button_ordinal;
    UINT        mouse_buttons;
} dpy;

//------------------------------------------------------------------------------

static int init_wgl_extensions(void)
{
    // Create dummy window and OpenGL context and check for
    // available WGL extensions

    QU_INFO("Creating dummy invisible window to check supported WGL extensions.\n");

    WNDCLASSEXW wc = {
        .cbSize         = sizeof(WNDCLASSEXW),
        .style          = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc    = DefWindowProcW,
        .hInstance      = GetModuleHandleW(0),
        .lpszClassName  = L"Trampoline",
    };

    if (!RegisterClassExW(&wc)) {
        QU_ERROR("Unable to register dummy window class.\n");
        return -1;
    }

    HWND window = CreateWindowExW(0, wc.lpszClassName, wc.lpszClassName, 0,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, wc.hInstance, NULL);

    if (!window) {
        QU_ERROR("Unable to create dummy window.\n");
        return -1;
    }

    HDC dc = GetDC(window);

    if (!dc) {
        QU_ERROR("Dummy window has invalid Device Context.\n");

        DestroyWindow(window);
        return -1;
    }

    PIXELFORMATDESCRIPTOR pfd = {
        .nSize          = sizeof(PIXELFORMATDESCRIPTOR),
        .nVersion       = 1,
        .dwFlags        = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL,
        .iPixelType     = PFD_TYPE_RGBA,
        .cColorBits     = 32,
        .cAlphaBits     = 8,
        .iLayerType     = PFD_MAIN_PLANE,
    };

    int format = ChoosePixelFormat(dc, &pfd);

    if (!format || !SetPixelFormat(dc, format, &pfd)) {
        QU_ERROR("Invalid pixel format.\n");

        ReleaseDC(window, dc);
        DestroyWindow(window);
        return -1;
    }

    HGLRC rc = wglCreateContext(dc);

    if (!rc || !wglMakeCurrent(dc, rc)) {
        QU_ERROR("Failed to create dummy OpenGL context.\n");

        if (rc) {
            wglDeleteContext(rc);
        }

        ReleaseDC(window, dc);
        DestroyWindow(window);
        return -1;
    }

    wgl.wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)
        wglGetProcAddress("wglGetExtensionsStringARB");
    wgl.wglGetPixelFormatAttribivARB = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)
        wglGetProcAddress("wglGetPixelFormatAttribivARB");
    wgl.wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)
        wglGetProcAddress("wglChoosePixelFormatARB");
    wgl.wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)
        wglGetProcAddress("wglCreateContextAttribsARB");

    wglMakeCurrent(dc, NULL);
    wglDeleteContext(rc);
    ReleaseDC(window, dc);
    DestroyWindow(window);

    QU_DEBUG("Pointers to required functions acquired:\n");
    QU_DEBUG(":: wglGetExtensionsStringARB -> %p\n", wgl.wglGetExtensionsStringARB);
    QU_DEBUG(":: wglGetPixelFormatAttribivARB -> %p\n", wgl.wglGetPixelFormatAttribivARB);
    QU_DEBUG(":: wglChoosePixelFormatARB -> %p\n", wgl.wglChoosePixelFormatARB);
    QU_DEBUG(":: wglCreateContextAttribsARB -> %p\n", wgl.wglCreateContextAttribsARB);

    // These functions are mandatory to create OpenGL Core Profile Context.

    if (!wgl.wglGetExtensionsStringARB) {
        QU_ERROR("WGL: required function wglGetExtensionsStringARB() is unavailable.\n");
        return -1;
    }

    if (!wgl.wglGetPixelFormatAttribivARB) {
        QU_ERROR("WGL: required function wglGetPixelFormatAttribivARB() is unavailable.\n");
        return -1;
    }

    if (!wgl.wglChoosePixelFormatARB) {
        QU_ERROR("WGL: required function wglChoosePixelFormatARB() is unavailable.\n");
        return -1;
    }

    if (!wgl.wglCreateContextAttribsARB) {
        QU_ERROR("WGL: required function wglCreateContextAttribsARB() is unavailable.\n");
        return -1;
    }

    return 0;
}

static int choose_pixel_format(HDC dc)
{
    int format_attribs[] = {
        WGL_DRAW_TO_WINDOW_ARB,     (TRUE),
        WGL_ACCELERATION_ARB,       (WGL_FULL_ACCELERATION_ARB),
        WGL_SUPPORT_OPENGL_ARB,     (TRUE),
        WGL_DOUBLE_BUFFER_ARB,      (TRUE),
        WGL_PIXEL_TYPE_ARB,         (WGL_TYPE_RGBA_ARB),
        WGL_COLOR_BITS_ARB,         (32),
        WGL_DEPTH_BITS_ARB,         (24),
        WGL_STENCIL_BITS_ARB,       (8),
        0,
    };

    int formats[256];
    UINT total_formats = 0;
    wgl.wglChoosePixelFormatARB(dc, format_attribs, NULL, 256, formats, &total_formats);

    if (!total_formats) {
        QU_ERROR("No suitable Pixel Format found.\n");
        return -1;
    }

    int best_format = -1;
    int best_samples = 0;

    for (unsigned int i = 0; i < total_formats; i++) {
        int attribs[] = {
            WGL_SAMPLE_BUFFERS_ARB,
            WGL_SAMPLES_ARB,
        };

        int values[2];

        wgl.wglGetPixelFormatAttribivARB(dc, formats[i], 0, 2, attribs, values);

        if (!values[0]) {
            continue;
        }

        if (values[1] > best_samples) {
            best_format = i;
            best_samples = values[1];
        }
    }

    return formats[(best_format < 0) ? 0 : best_format];
}

static int init_wgl_context(HWND window)
{
    if (init_wgl_extensions() == -1) {
        QU_HALT("Failed to fetch extension list.");
    }

    HDC dc = GetDC(window);

    if (!dc) {
        QU_HALT("Failed to get Device Context for main window.");
    }

    QU_INFO("Available WGL extensions: %s\n", wgl.wglGetExtensionsStringARB(dc));

    int format = choose_pixel_format(dc);

    if (format == -1) {
        QU_ERROR("Failed to choose appropriate Pixel Format.\n");

        ReleaseDC(window, dc);
        return -1;
    }

    PIXELFORMATDESCRIPTOR pfd;
    DescribePixelFormat(dc, format, sizeof(pfd), &pfd);

    if (!SetPixelFormat(dc, format, &pfd)) {
        QU_ERROR("Failed to set Pixel Format.\n");
        QU_ERROR(":: GetLastError -> 0x%08x\n", GetLastError());

        ReleaseDC(window, dc);
        return -1;
    }

    HGLRC rc = NULL;
    int gl_versions[] = { 460, 450, 440, 420, 410, 400, 330 };

    for (unsigned int i = 0; i < ARRAYSIZE(gl_versions); i++) {
        int major = gl_versions[i] / 100;
        int minor = (gl_versions[i] % 100) / 10;

        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB,  major,
            WGL_CONTEXT_MINOR_VERSION_ARB,  minor,
            WGL_CONTEXT_PROFILE_MASK_ARB,   WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0,
        };

        rc = wgl.wglCreateContextAttribsARB(dc, NULL, attribs);

        if (rc && wglMakeCurrent(dc, rc)) {
            QU_INFO("OpenGL version %d.%d seems to be supported.\n", major, minor);
            break;
        }

        QU_ERROR("Unable to create OpenGL version %d.%d context.\n", major, minor);
    }

    if (!rc) {
        QU_ERROR("Neither of listed OpenGL versions is supported.\n");

        ReleaseDC(window, dc);
        return -1;
    }

    wgl.wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");

    if (wgl.wglSwapIntervalEXT) {
        wgl.wglSwapIntervalEXT(1);
    }

    dpy.dc = dc;
    dpy.rc = rc;

    QU_INFO("OpenGL context is successfully created.\n");

    return 0;
}

static qu_key key_conv(WPARAM wp, LPARAM lp)
{
    UINT vk = (UINT) wp;
    UINT scancode = (lp & (0xff << 16)) >> 16;
    BOOL extended = (lp & (0x01 << 24));

    if (vk >= 'A' && vk <= 'Z') {
        return QU_KEY_A + (vk - 'A');
    }

    if (vk >= '0' && vk <= '9') {
        return QU_KEY_0 + (vk - '0');
    }

    if (vk >= VK_F1 && vk <= VK_F12) {
        return QU_KEY_F1 + (vk - VK_F1);
    }

    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return QU_KEY_KP_0 + (vk - VK_NUMPAD0);
    }

    if (vk == VK_RETURN) {
        if (extended) {
            return QU_KEY_KP_ENTER;
        }

        return QU_KEY_ENTER;
    }

    if (vk == VK_SHIFT) {
        UINT ex = MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX);
        if (ex == VK_LSHIFT) {
            return QU_KEY_LSHIFT;
        } else if (ex == VK_RSHIFT) {
            return QU_KEY_RSHIFT;
        }
    }

    if (vk == VK_CONTROL) {
        if (extended) {
            return QU_KEY_RCTRL;
        }

        return QU_KEY_LCTRL;
    }

    if (vk == VK_MENU) {
        if (extended) {
            return QU_KEY_RALT;
        }

        return QU_KEY_LALT;
    }

    switch (vk) {
    case VK_BACK:               return QU_KEY_BACKSPACE;
    case VK_TAB:                return QU_KEY_TAB;
    case VK_PAUSE:              return QU_KEY_PAUSE;
    case VK_CAPITAL:            return QU_KEY_CAPSLOCK;
    case VK_ESCAPE:             return QU_KEY_ESCAPE;
    case VK_SPACE:              return QU_KEY_SPACE;
    case VK_PRIOR:              return QU_KEY_PGUP;
    case VK_NEXT:               return QU_KEY_PGDN;
    case VK_END:                return QU_KEY_END;
    case VK_HOME:               return QU_KEY_HOME;
    case VK_LEFT:               return QU_KEY_LEFT;
    case VK_UP:                 return QU_KEY_UP;
    case VK_RIGHT:              return QU_KEY_RIGHT;
    case VK_DOWN:               return QU_KEY_DOWN;
    case VK_PRINT:              return QU_KEY_PRINTSCREEN;
    case VK_INSERT:             return QU_KEY_INSERT;
    case VK_DELETE:             return QU_KEY_DELETE;
    case VK_LWIN:               return QU_KEY_LSUPER;
    case VK_RWIN:               return QU_KEY_RSUPER;
    case VK_MULTIPLY:           return QU_KEY_KP_MUL;
    case VK_ADD:                return QU_KEY_KP_ADD;
    case VK_SUBTRACT:           return QU_KEY_KP_SUB;
    case VK_DECIMAL:            return QU_KEY_KP_POINT;
    case VK_DIVIDE:             return QU_KEY_KP_DIV;
    case VK_NUMLOCK:            return QU_KEY_NUMLOCK;
    case VK_SCROLL:             return QU_KEY_SCROLLLOCK;
    case VK_LMENU:              return QU_KEY_MENU;
    case VK_RMENU:              return QU_KEY_MENU;
    case VK_OEM_1:              return QU_KEY_SEMICOLON;
    case VK_OEM_PLUS:           return QU_KEY_EQUAL;
    case VK_OEM_COMMA:          return QU_KEY_COMMA;
    case VK_OEM_MINUS:          return QU_KEY_MINUS;
    case VK_OEM_PERIOD:         return QU_KEY_PERIOD;
    case VK_OEM_2:              return QU_KEY_SLASH;
    case VK_OEM_3:              return QU_KEY_GRAVE;
    case VK_OEM_4:              return QU_KEY_LBRACKET;
    case VK_OEM_6:              return QU_KEY_RBRACKET;
    case VK_OEM_5:              return QU_KEY_BACKSLASH;
    case VK_OEM_7:              return QU_KEY_APOSTROPHE;
    }

    return QU_KEY_INVALID;
}

static qu_mouse_button mb_conv(UINT msg, WPARAM wp)
{
    switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        return QU_MOUSE_BUTTON_LEFT;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        return QU_MOUSE_BUTTON_RIGHT;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
        return QU_MOUSE_BUTTON_MIDDLE;
    }

    if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP) {
        WORD button = GET_XBUTTON_WPARAM(wp);

        if (button == 0x0001) {
            return 4;
        } else if (button == 0x0002) {
            return 5;
        }
    }

    return QU_MOUSE_BUTTON_INVALID;
}

static LRESULT CALLBACK wndproc(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        return init_wgl_context(window);
    case WM_DESTROY:
        return 0;
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        qu__graphics_on_display_resize(LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_ACTIVATE:
        // if (LOWORD(wp) == WA_ACTIVE && HIWORD(wp) == 0) {
        //     // Send release message for all pressed buttons when
        //     // switching from another window
        //     for (int i = 0; i < QU_TOTAL_MOUSE_BUTTONS; i++) {
        //         if (priv.mouse_buttons & (1 << i)) {
        //             libtq_on_mouse_button_released(i);
        //         }
        //     }

        //     priv.mouse_buttons = 0;
        //     libtq_on_focus_gain();
        // } else if (LOWORD(wp) == WA_INACTIVE) {
        //     if (priv.mouse_button_ordinal > 0) {
        //         priv.mouse_button_ordinal = 0;
        //         ReleaseCapture();
        //     }

        //     libtq_on_focus_loss();
        // }
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        qu__core_on_key_pressed(key_conv(wp, lp));
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        qu__core_on_key_released(key_conv(wp, lp));
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
        qu__core_on_mouse_button_pressed(mb_conv(msg, wp));
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
        qu__core_on_mouse_button_released(mb_conv(msg, wp));
        return 0;
    case WM_MOUSEMOVE:
        qu__core_on_mouse_cursor_moved(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSEWHEEL:
        qu__core_on_mouse_wheel_scrolled(0, GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA);
        return 0;
    case WM_MOUSEHWHEEL:
        qu__core_on_mouse_wheel_scrolled(GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA, 0);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            SetCursor(dpy.hide_cursor ? NULL : dpy.cursor);
            return 0;
        }
        break;
    }

    return DefWindowProc(window, msg, wp, lp);
}

static void set_size(int width, int height)
{
    HMONITOR monitor = MonitorFromWindow(dpy.window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfoW(monitor, &mi);

    int dx = (mi.rcMonitor.right - mi.rcMonitor.left - width) / 2;
    int dy = (mi.rcMonitor.bottom - mi.rcMonitor.top - height) / 2;

    RECT rect = { dx, dy, dx + width, dy + height };
    AdjustWindowRect(&rect, dpy.style, FALSE);

    int x = rect.left;
    int y = rect.top;
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;

    SetWindowPos(dpy.window, NULL, x, y, w, h, SWP_SHOWWINDOW);
    UpdateWindow(dpy.window);
}

//------------------------------------------------------------------------------

static void initialize(qu_params const *params)
{
    // Check instance

    HINSTANCE instance = GetModuleHandleW(NULL);

    if (!instance) {
        QU_HALT("Invalid module instance.");
    }

    // DPI awareness

    qu__library shcore_dll = qu__platform_open_library("shcore.dll");

    if (shcore_dll) {
        SETPROCESSDPIAWARENESSPROC pfnSetProcessDpiAwareness =
            qu__platform_get_procedure(shcore_dll, "SetProcessDpiAwareness");

        if (pfnSetProcessDpiAwareness) {
            // 2: PROCESS_PER_MONITOR_DPI_AWARE
            pfnSetProcessDpiAwareness(2);
        }

        qu__platform_close_library(shcore_dll);
    }

    // Set cursor and keyboard

    dpy.cursor = LoadCursorW(NULL, IDC_CROSS);
    dpy.hide_cursor = FALSE;
    dpy.key_autorepeat = FALSE;

    // Create window

    MultiByteToWideChar(CP_UTF8, 0, params->title, -1, dpy.wide_title, ARRAYSIZE(dpy.wide_title));
    dpy.class_name = dpy.wide_title;
    dpy.window_name = dpy.wide_title;

    dpy.style = WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX;

    WNDCLASSEXW wcex = {
        .cbSize         = sizeof(WNDCLASSEXW),
        .style          = CS_VREDRAW | CS_HREDRAW | CS_OWNDC,
        .lpfnWndProc    = wndproc,
        .cbClsExtra     = 0,
        .cbWndExtra     = 0,
        .hInstance      = instance,
        .hIcon          = LoadIconW(NULL, IDI_WINLOGO),
        .hCursor        = NULL,
        .hbrBackground  = NULL,
        .lpszMenuName   = NULL,
        .lpszClassName  = dpy.class_name,
        .hIconSm        = NULL,
    };

    if (!RegisterClassExW(&wcex)) {
        QU_HALT("Failed to register class.");
    }

    dpy.window = CreateWindowW(dpy.class_name, dpy.window_name, dpy.style,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, instance, NULL);
    
    if (!dpy.window) {
        QU_HALT("Failed to create window.");
    }

    // Enable dark mode on Windows 11.

    BOOL dark = TRUE;
    DwmSetWindowAttribute(dpy.window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    // Resize and show the window.

    set_size(params->display_width, params->display_height);

    // Done.

    QU_INFO("Initialized.\n");
}

static void terminate(void)
{
    if (dpy.window) {
        if (dpy.dc) {
            if (dpy.rc) {
                wglMakeCurrent(dpy.dc, NULL);
                wglDeleteContext(dpy.rc);
            }

            ReleaseDC(dpy.window, dpy.dc);
        }
        
        DestroyWindow(dpy.window);
    }

    QU_INFO("Terminated.\n");
}

static bool process(void)
{
    MSG msg;

    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return true;
}

static void present(void)
{
    SwapBuffers(dpy.dc);
}

static enum qu__renderer get_renderer(void)
{
    return QU__RENDERER_GL_CORE;
}

static bool gl_check_extension(char const *name)
{
    if (!wgl.wglGetExtensionsStringARB) {
        return false;
    }

    char *list = qu_strdup(wgl.wglGetExtensionsStringARB(dpy.dc));

    if (!list) {
        return false;
    }

    char *token = strtok(list, " ");

    while (token) {
        if (strcmp(token, name) == 0) {
            return true;
        }

        token = strtok(NULL, " ");
    }

    free(list);

    return false;
}

static void *gl_proc_address(char const *name)
{
    return (void *) wglGetProcAddress(name);
}

//------------------------------------------------------------------------------

struct qu__core const qu__core_win32 = {
    .initialize = initialize,
    .terminate = terminate,
    .process = process,
    .present = present,
    .get_renderer = get_renderer,
    .gl_check_extension = gl_check_extension,
    .gl_proc_address = gl_proc_address,
};
