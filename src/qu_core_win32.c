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
#include <shellscalingapi.h>
#include <xinput.h>
#include <GL/gl.h>
#include <GL/wglext.h>

//------------------------------------------------------------------------------
// qu_core_win32.c: Win32-based core module
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Immersive Dark Mode

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

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
    LPCTSTR     class_name;
    LPCTSTR     window_name;
    DWORD       style;
    HWND        window;
    HDC         dc;
    HGLRC       rc;
    HCURSOR     cursor;
    BOOL        hide_cursor;
    BOOL        key_autorepeat;
    UINT        mouse_button_ordinal;
    UINT        mouse_buttons;

#if defined(UNICODE)
    WCHAR       wide_title[256];
#endif
} dpy;

static struct
{
    bool keyboard[QU_TOTAL_KEYS];
    uint8_t mouse_buttons;
    qu_vec2i mouse_wheel_delta;
    qu_vec2i mouse_cursor_position;
    qu_vec2i mouse_cursor_delta;
    
    struct {
        bool attached;
        float next_poll_time;
        XINPUT_STATE state;
    } joystick[4];
} input;

static struct
{
    qu_key_fn on_key_pressed;
    qu_key_fn on_key_repeated;
    qu_key_fn on_key_released;
    qu_mouse_button_fn on_mouse_button_pressed;
    qu_mouse_button_fn on_mouse_button_released;
    qu_mouse_wheel_fn on_mouse_wheel_scrolled;
    qu_mouse_cursor_fn on_mouse_cursor_moved;
} callbacks;

//------------------------------------------------------------------------------

static int init_wgl_extensions(void)
{
    // Create dummy window and OpenGL context and check for
    // available WGL extensions

    QU_INFO("WinAPI: Creating dummy invisible window to check supported WGL extensions.\n");

    WNDCLASS wc = {
        .style          = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc    = DefWindowProc,
        .hInstance      = GetModuleHandle(0),
        .lpszClassName  = TEXT("Trampoline"),
    };

    if (!RegisterClass(&wc)) {
        QU_ERROR("WinAPI: Unable to register dummy window class.\n");
        return -1;
    }

    HWND window = CreateWindowEx(0, wc.lpszClassName, wc.lpszClassName, 0,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, wc.hInstance, NULL);

    if (!window) {
        QU_ERROR("WinAPI: Unable to create dummy window.\n");
        return -1;
    }

    HDC dc = GetDC(window);

    if (!dc) {
        QU_ERROR("WinAPI: Dummy window has invalid Device Context.\n");

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
        QU_ERROR("WinAPI: Invalid pixel format.\n");

        ReleaseDC(window, dc);
        DestroyWindow(window);
        return -1;
    }

    HGLRC rc = wglCreateContext(dc);

    if (!rc || !wglMakeCurrent(dc, rc)) {
        QU_ERROR("WinAPI: Failed to create dummy OpenGL context.\n");

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

    QU_INFO("WGL: Pointers to required functions acquired:\n");
    QU_INFO(":: wglGetExtensionsStringARB -> %p\n", wgl.wglGetExtensionsStringARB);
    QU_INFO(":: wglGetPixelFormatAttribivARB -> %p\n", wgl.wglGetPixelFormatAttribivARB);
    QU_INFO(":: wglChoosePixelFormatARB -> %p\n", wgl.wglChoosePixelFormatARB);
    QU_INFO(":: wglCreateContextAttribsARB -> %p\n", wgl.wglCreateContextAttribsARB);

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
        QU_ERROR("WGL: No suitable Pixel Format found.\n");
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
        QU_HALT("WGL: Failed to fetch extension list.");
    }

    HDC dc = GetDC(window);

    if (!dc) {
        QU_HALT("WGL: Failed to get Device Context for main window.");
    }

    QU_INFO("WGL: available extensions:\n");
    QU_INFO(":: %s\n", wgl.wglGetExtensionsStringARB(dc));

    int format = choose_pixel_format(dc);

    if (format == -1) {
        QU_ERROR("WGL: Failed to choose appropriate Pixel Format.\n");

        ReleaseDC(window, dc);
        return -1;
    }

    PIXELFORMATDESCRIPTOR pfd;
    DescribePixelFormat(dc, format, sizeof(pfd), &pfd);

    if (!SetPixelFormat(dc, format, &pfd)) {
        QU_ERROR("WGL: Failed to set Pixel Format.\n");
        QU_ERROR(":: GetLastError -> 0x%08x\n", GetLastError());

        ReleaseDC(window, dc);
        return -1;
    }

    HGLRC rc = NULL;
    int gl_versions[] = { 320, 310, 300, 210 };

    for (unsigned int i = 0; i < ARRAYSIZE(gl_versions); i++) {
        int major = gl_versions[i] / 100;
        int minor = (gl_versions[i] % 100) / 10;

        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB,  major,
            WGL_CONTEXT_MINOR_VERSION_ARB,  minor,
            WGL_CONTEXT_PROFILE_MASK_ARB,   WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
            0,
        };

        rc = wgl.wglCreateContextAttribsARB(dc, NULL, attribs);

        if (rc && wglMakeCurrent(dc, rc)) {
            QU_INFO("WGL: OpenGL version %d.%d seems to be supported.\n", major, minor);
            break;
        }

        QU_ERROR("WGL: Unable to create OpenGL version %d.%d context.\n", major, minor);
    }

    if (!rc) {
        QU_ERROR("WGL: Neither of listed OpenGL versions is supported.\n");

        ReleaseDC(window, dc);
        return -1;
    }

    wgl.wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");

    if (wgl.wglSwapIntervalEXT) {
        wgl.wglSwapIntervalEXT(1);
    }

    dpy.dc = dc;
    dpy.rc = rc;

    QU_INFO("WGL: OpenGL context is successfully created.\n");

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

static void handle_key_press(WPARAM wp, LPARAM lp)
{
    qu_key key = key_conv(wp, lp);

    if (key == QU_KEY_INVALID) {
        return;
    }

    if (input.keyboard[key]) {
        if (callbacks.on_key_repeated) {
            callbacks.on_key_repeated(key);
        }
    } else {
        input.keyboard[key] = true;

        if (callbacks.on_key_pressed) {
            callbacks.on_key_pressed(key);
        }
    }
}

static void handle_key_release(WPARAM wp, LPARAM lp)
{
    qu_key key = key_conv(wp, lp);

    if (key == QU_KEY_INVALID) {
        return;
    }

    input.keyboard[key] = false;

    if (callbacks.on_key_released) {
        callbacks.on_key_released(key);
    }
}

static void handle_mouse_button_press(UINT msg, WPARAM wp)
{
    qu_mouse_button button = mb_conv(msg, wp);

    if (button == QU_MOUSE_BUTTON_INVALID) {
        return;
    }

    input.mouse_buttons |= (1 << button);

    if (callbacks.on_mouse_button_pressed) {
        callbacks.on_mouse_button_pressed(button);
    }
}

static void handle_mouse_button_release(UINT msg, WPARAM wp)
{
    qu_mouse_button button = mb_conv(msg, wp);

    if (button == QU_MOUSE_BUTTON_INVALID) {
        return;
    }

    input.mouse_buttons &= ~(1 << button);

    if (callbacks.on_mouse_button_released) {
        callbacks.on_mouse_button_released(button);
    }
}

static void handle_mouse_cursor_motion(int x, int y)
{
    input.mouse_cursor_delta.x = x - input.mouse_cursor_position.x;
    input.mouse_cursor_delta.y = y - input.mouse_cursor_position.y;
    input.mouse_cursor_position.x = x;
    input.mouse_cursor_position.y = y;
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
        qu_notify_display_resize(LOWORD(lp), HIWORD(lp));
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
        handle_key_press(wp, lp);
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        handle_key_release(wp, lp);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
        handle_mouse_button_press(msg, wp);
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
        handle_mouse_button_release(msg, wp);
        return 0;
    case WM_MOUSEMOVE:
        handle_mouse_cursor_motion(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSEWHEEL:
        input.mouse_wheel_delta.y += GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
        return 0;
    case WM_MOUSEHWHEEL:
        input.mouse_wheel_delta.x += GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
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
    GetMonitorInfo(monitor, &mi);

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

    HINSTANCE instance = GetModuleHandle(NULL);

    if (!instance) {
        QU_HALT("WinAPI: no module instance.");
    }

    // DPI awareness

    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    // Set cursor and keyboard

    dpy.cursor = LoadCursor(NULL, IDC_CROSS);
    dpy.hide_cursor = FALSE;
    dpy.key_autorepeat = FALSE;

    // Create window

#if defined(UNICODE)
    MultiByteToWideChar(CP_UTF8, 0, title, -1, dpy.wide_title, ARRAYSIZE(dpy.wide_title));

    dpy.class_name = dpy.wide_title;
    dpy.window_name = dpy.wide_title;
#else
    dpy.class_name = params->title;
    dpy.window_name = params->title;
#endif

    dpy.style = WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX;

    WNDCLASSEX wcex = {
        .cbSize         = sizeof(WNDCLASSEX),
        .style          = CS_VREDRAW | CS_HREDRAW | CS_OWNDC,
        .lpfnWndProc    = wndproc,
        .cbClsExtra     = 0,
        .cbWndExtra     = 0,
        .hInstance      = instance,
        .hIcon          = LoadIcon(NULL, IDI_WINLOGO),
        .hCursor        = NULL,
        .hbrBackground  = NULL,
        .lpszMenuName   = NULL,
        .lpszClassName  = dpy.class_name,
        .hIconSm        = NULL,
    };

    if (!RegisterClassEx(&wcex)) {
        QU_HALT("WinAPI: failed to register class.");
    }

    dpy.window = CreateWindow(dpy.class_name, dpy.window_name, dpy.style,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, instance, NULL);
    
    if (!dpy.window) {
        QU_HALT("WinAPI: failed to create window.");
    }

    // Enable dark mode on Windows 11.

    BOOL dark = TRUE;
    DwmSetWindowAttribute(dpy.window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    // Resize and show the window.

    set_size(params->display_width, params->display_height);

    // Done.

    QU_INFO("Win32 core module initialized.\n");
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

    QU_INFO("Win32 core module terminated.\n");
}

static bool process(void)
{
    input.mouse_cursor_delta.x = 0;
    input.mouse_cursor_delta.y = 0;

    input.mouse_wheel_delta.x = 0;
    input.mouse_wheel_delta.y = 0;

    MSG msg;

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (input.mouse_wheel_delta.x || input.mouse_wheel_delta.y) {
        if (callbacks.on_mouse_wheel_scrolled) {
            callbacks.on_mouse_wheel_scrolled(
                input.mouse_wheel_delta.x, input.mouse_wheel_delta.y
            );
        }
    }

    if (input.mouse_cursor_delta.x || input.mouse_cursor_delta.y) {
        if (callbacks.on_mouse_cursor_moved) {
            callbacks.on_mouse_cursor_moved(
                input.mouse_cursor_delta.x, input.mouse_cursor_delta.y
            );
        }
    }

    for (int i = 0; i < 4; i++) {
        if (!input.joystick[i].attached) {
            continue;
        }

        DWORD status = XInputGetState(i, &input.joystick[i].state);

        if (status != ERROR_SUCCESS) {
            if (status != ERROR_DEVICE_NOT_CONNECTED) {
                // TODO: report error
            }

            input.joystick[i].attached = false;
            input.joystick[i].next_poll_time = 0.f;
        }
;   }

    return true;
}

static void present(void)
{
    SwapBuffers(dpy.dc);
}

static enum qu_graphics get_graphics_type(void)
{
    return QU_GRAPHICS_GL2;
}

static enum qu_audio get_audio_type(void)
{
    // TODO: sumanue qaidaq ere talueaqtaaqtar. arai env var'uenan?

#ifdef QU_USE_OPENAL
    return QU_AUDIO_OPENAL;
#else
    return QU_AUDIO_XAUDIO2;
#endif
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

static bool const *get_keyboard_state(void)
{
    return input.keyboard;
}

static bool is_key_pressed(qu_key key)
{
    return input.keyboard[key];
}

//------------------------------------------------------------------------------

static uint8_t get_mouse_button_state(void)
{
    return input.mouse_buttons;
}

static bool is_mouse_button_pressed(qu_mouse_button button)
{
    return (input.mouse_buttons & (1 << button));
}

static qu_vec2i get_mouse_cursor_position(void)
{
    return input.mouse_cursor_position;
}

static qu_vec2i get_mouse_cursor_delta(void)
{
    return input.mouse_cursor_delta;
}

static qu_vec2i get_mouse_wheel_delta(void)
{
    return input.mouse_wheel_delta;
}

//------------------------------------------------------------------------------

static bool is_joystick_connected(int joystick)
{
    if (joystick < 0 || joystick >= 4) {
        return false;
    }

    if (input.joystick[joystick].attached) {
        return true;
    }

    float current_time = qu_get_time_mediump();

    if (input.joystick[joystick].next_poll_time > current_time) {
        return false;
    }

    input.joystick[joystick].next_poll_time = current_time + 1.f;

    ZeroMemory(&input.joystick[joystick].state, sizeof(XINPUT_STATE));
    DWORD status = XInputGetState(joystick, &input.joystick[joystick].state);

    if (status == ERROR_SUCCESS) {
        input.joystick[joystick].attached = true;
    }

    return input.joystick[joystick].attached;
}

static char const *get_joystick_id(int joystick)
{
    if (joystick < 0 || joystick >= 4 || !input.joystick[joystick].attached) {
        return NULL;
    }

    return "UNKNOWN";
}

static int get_joystick_button_count(int joystick)
{
    if (joystick < 0 || joystick >= 4 || !input.joystick[joystick].attached) {
        return 0;
    }

    return 10;
}

static int get_joystick_axis_count(int joystick)
{
    if (joystick < 0 || joystick >= 4 || !input.joystick[joystick].attached) {
        return 0;
    }

    return 8;
}

static char const *get_joystick_button_id(int joystick, int button)
{
    if (joystick < 0 || joystick >= 4 || !input.joystick[joystick].attached) {
        return NULL;
    }

    if (button < 0 || button >= 10) {
        return NULL;
    }

    char const *names[10] = {
        "START", "BACK", "LTHUMB", "RTHUMB",
        "LSHOULDER", "RSHOULDER",
        "A", "B", "X", "Y",
    };

    return names[button];
}

static char const *get_joystick_axis_id(int joystick, int axis)
{
    if (joystick < 0 || joystick >= 4 || !input.joystick[joystick].attached) {
        return NULL;
    }

    if (axis < 0 || axis >= 8) {
        return NULL;
    }

    char const *names[8] = {
        "DPADX", "DPADY",
        "LTHUMBX", "LTHUMBY",
        "RTHUMBX", "RTHUMBY",
        "LTRIGGER", "RTRIGGER",
    };

    return names[axis];
}

static bool is_joystick_button_pressed(int id, int button)
{
    if (id < 0 || id >= 4 || !input.joystick[id].attached) {
        return false;
    }

    if (button < 0 || button >= 10) {
        return false;
    }

    int mask[10] = {
        XINPUT_GAMEPAD_START,
        XINPUT_GAMEPAD_BACK,
        XINPUT_GAMEPAD_LEFT_THUMB,
        XINPUT_GAMEPAD_RIGHT_THUMB,
        XINPUT_GAMEPAD_LEFT_SHOULDER,
        XINPUT_GAMEPAD_RIGHT_SHOULDER,
        XINPUT_GAMEPAD_A,
        XINPUT_GAMEPAD_B,
        XINPUT_GAMEPAD_X,
        XINPUT_GAMEPAD_Y,
    };

    return input.joystick[id].state.Gamepad.wButtons & mask[button];
}

static float get_joystick_axis_value(int id, int axis)
{
    if (id < 0 || id >= 4 || !input.joystick[id].attached) {
        return 0.f;
    }

    switch (axis) {
    case 0:
        if (input.joystick[id].state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
            return -1.f;
        } else if (input.joystick[id].state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
            return +1.f;
        }
        break;
    case 1:
        if (input.joystick[id].state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
            return -1.f;
        } else if (input.joystick[id].state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) {
            return +1.f;
        }
        break;
    case 2:
        return input.joystick[id].state.Gamepad.sThumbLX / 32767.f;
    case 3:
        return input.joystick[id].state.Gamepad.sThumbLY / 32767.f;
    case 4:
        return input.joystick[id].state.Gamepad.sThumbRX / 32767.f;
    case 5:
        return input.joystick[id].state.Gamepad.sThumbRY / 32767.f;
    case 6:
        return input.joystick[id].state.Gamepad.bLeftTrigger / 255.f;
    case 7:
        return input.joystick[id].state.Gamepad.bRightTrigger / 255.f;
    default:
        break;
    }

    return 0.f;
}

//------------------------------------------------------------------------------

static void on_key_pressed(qu_key_fn fn)
{
    callbacks.on_key_pressed = fn;
}

static void on_key_repeated(qu_key_fn fn)
{
    callbacks.on_key_repeated = fn;
}

static void on_key_released(qu_key_fn fn)
{
    callbacks.on_key_released = fn;
}

static void on_mouse_button_pressed(qu_mouse_button_fn fn)
{
    callbacks.on_mouse_button_pressed = fn;
}

static void on_mouse_button_released(qu_mouse_button_fn fn)
{
    callbacks.on_mouse_button_released = fn;
}

static void on_mouse_cursor_moved(qu_mouse_cursor_fn fn)
{
    callbacks.on_mouse_cursor_moved = fn;
}

static void on_mouse_wheel_scrolled(qu_mouse_wheel_fn fn)
{
    callbacks.on_mouse_wheel_scrolled = fn;
}

//------------------------------------------------------------------------------

qu_core_module const qu__core_win32_module = {
    .initialize = initialize,
    .terminate = terminate,
    .process = process,
    .present = present,
    .get_graphics_type = get_graphics_type,
    .get_audio_type = get_audio_type,
    .gl_check_extension = gl_check_extension,
    .gl_proc_address = gl_proc_address,
    .get_keyboard_state = get_keyboard_state,
    .is_key_pressed = is_key_pressed,
    .get_mouse_button_state = get_mouse_button_state,
    .is_mouse_button_pressed = is_mouse_button_pressed,
    .get_mouse_cursor_position = get_mouse_cursor_position,
    .get_mouse_cursor_delta = get_mouse_cursor_delta,
    .get_mouse_wheel_delta = get_mouse_wheel_delta,
    .is_joystick_connected = is_joystick_connected,
    .get_joystick_id = get_joystick_id,
    .get_joystick_button_count = get_joystick_button_count,
    .get_joystick_axis_count = get_joystick_axis_count,
    .is_joystick_button_pressed = is_joystick_button_pressed,
    .get_joystick_axis_value = get_joystick_axis_value,
    .get_joystick_button_id = get_joystick_button_id,
    .get_joystick_axis_id = get_joystick_axis_id,
    .on_key_pressed = on_key_pressed,
    .on_key_repeated = on_key_repeated,
    .on_key_released = on_key_released,
    .on_mouse_button_pressed = on_mouse_button_pressed,
    .on_mouse_button_released = on_mouse_button_released,
    .on_mouse_cursor_moved = on_mouse_cursor_moved,
    .on_mouse_wheel_scrolled = on_mouse_wheel_scrolled,
};
