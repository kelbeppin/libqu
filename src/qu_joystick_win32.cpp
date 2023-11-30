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

#define QU_MODULE "joystick-win32"

extern "C" {
#include "qu.h"
}

#include <Xinput.h>

//------------------------------------------------------------------------------
// qu_joystick_win32.cpp: Win32 joystick module
// TODO: add DirectInput support?
//------------------------------------------------------------------------------

static int const TOTAL_XINPUT_DEVICES = 4;
static int const TOTAL_XINPUT_DEVICE_BUTTONS = 10;
static int const TOTAL_XINPUT_DEVICE_AXES = 8;

struct qu__xinput_device
{
	bool attached;
	float next_poll_time;
	XINPUT_STATE state;
};

struct joystick_win32_priv
{
	struct qu__xinput_device xinput[TOTAL_XINPUT_DEVICES];
};

static joystick_win32_priv priv;

//------------------------------------------------------------------------------

static qu_result joystick_win32__precheck(void)
{
    return QU_SUCCESS;
}

static qu_result joystick_win32__initialize(void)
{
    return QU_SUCCESS;
}

static void joystick_win32__terminate(void)
{
    memset(&priv, 0, sizeof(priv));
}

static void joystick_win32__process(void)
{
    for (int i = 0; i < 4; i++) {
        if (!priv.xinput[i].attached) {
            continue;
        }

        DWORD status = XInputGetState(i, &priv.xinput[i].state);

        if (status != ERROR_SUCCESS) {
            if (status != ERROR_DEVICE_NOT_CONNECTED) {
                // TODO: report error
            }

            priv.xinput[i].attached = false;
            priv.xinput[i].next_poll_time = 0.f;
        }
    }
}

static bool joystick_win32__is_connected(int id)
{
	if (id < 0 || id >= TOTAL_XINPUT_DEVICES) {
        return false;
    }

    if (priv.xinput[id].attached) {
        return true;
    }

    float current_time = qu_get_time_mediump();

    if (priv.xinput[id].next_poll_time > current_time) {
        return false;
    }

    priv.xinput[id].next_poll_time = current_time + 1.f;

    ZeroMemory(&priv.xinput[id].state, sizeof(XINPUT_STATE));
    DWORD status = XInputGetState(id, &priv.xinput[id].state);

    if (status == ERROR_SUCCESS) {
        priv.xinput[id].attached = true;
    }

    return priv.xinput[id].attached;
}

static char const *joystick_win32__get_name(int id)
{
	if (id < 0 || id >= TOTAL_XINPUT_DEVICES || !priv.xinput[id].attached) {
        return nullptr;
    }

    return "UNKNOWN";
}

static int joystick_win32__get_button_count(int id)
{
	if (id < 0 || id >= TOTAL_XINPUT_DEVICES || !priv.xinput[id].attached) {
        return 0;
    }

    return TOTAL_XINPUT_DEVICE_BUTTONS;
}

static int joystick_win32__get_axis_count(int id)
{
	if (id < 0 || id >= TOTAL_XINPUT_DEVICES || !priv.xinput[id].attached) {
        return 0;
    }

    return TOTAL_XINPUT_DEVICE_AXES;
}

static char const *joystick_win32__get_button_name(int id, int button)
{
	if (id < 0 || id >= TOTAL_XINPUT_DEVICES || !priv.xinput[id].attached) {
        return nullptr;
    }

    if (button < 0 || button >= TOTAL_XINPUT_DEVICE_BUTTONS) {
        return nullptr;
    }

    char const *names[TOTAL_XINPUT_DEVICE_BUTTONS] = {
        "START",
        "BACK",
        "LTHUMB",
        "RTHUMB",
        "LSHOULDER",
        "RSHOULDER",
        "A",
        "B",
        "X",
        "Y",
    };

    return names[button];
}

static char const *joystick_win32__get_axis_name(int id, int axis)
{
	if (id < 0 || id >= TOTAL_XINPUT_DEVICES || !priv.xinput[id].attached) {
        return nullptr;
    }

    if (axis < 0 || axis >= TOTAL_XINPUT_DEVICE_AXES) {
        return nullptr;
    }

    char const *names[TOTAL_XINPUT_DEVICE_AXES] = {
        "DPADX",
        "DPADY",
        "LTHUMBX",
        "LTHUMBY",
        "RTHUMBX",
        "RTHUMBY",
        "LTRIGGER",
        "RTRIGGER",
    };

    return names[axis];
}

static bool joystick_win32__is_button_pressed(int id, int button)
{
	if (id < 0 || id >= TOTAL_XINPUT_DEVICES || !priv.xinput[id].attached) {
        return false;
    }

    if (button < 0 || button >= TOTAL_XINPUT_DEVICE_BUTTONS) {
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

    return priv.xinput[id].state.Gamepad.wButtons & mask[button];
}

static float joystick_win32__get_axis_value(int id, int axis)
{
	if (id < 0 || id >= TOTAL_XINPUT_DEVICES || !priv.xinput[id].attached) {
        return 0.f;
    }

    switch (axis) {
    case 0:
        if (priv.xinput[id].state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
            return -1.f;
        } else if (priv.xinput[id].state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
            return +1.f;
        }
        break;
    case 1:
        if (priv.xinput[id].state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
            return -1.f;
        } else if (priv.xinput[id].state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) {
            return +1.f;
        }
        break;
    case 2:
        return priv.xinput[id].state.Gamepad.sThumbLX / 32767.f;
    case 3:
        return priv.xinput[id].state.Gamepad.sThumbLY / 32767.f;
    case 4:
        return priv.xinput[id].state.Gamepad.sThumbRX / 32767.f;
    case 5:
        return priv.xinput[id].state.Gamepad.sThumbRY / 32767.f;
    case 6:
        return priv.xinput[id].state.Gamepad.bLeftTrigger / 255.f;
    case 7:
        return priv.xinput[id].state.Gamepad.bRightTrigger / 255.f;
    default:
        break;
    }

    return 0.f;
}

//------------------------------------------------------------------------------

qu_joystick_impl const qu_win32_joystick_impl = {
    joystick_win32__precheck,
	joystick_win32__initialize,
	joystick_win32__terminate,
    joystick_win32__process,
	joystick_win32__is_connected,
	joystick_win32__get_name,
	joystick_win32__get_button_count,
	joystick_win32__get_axis_count,
	joystick_win32__get_button_name,
	joystick_win32__get_axis_name,
	joystick_win32__is_button_pressed,
	joystick_win32__get_axis_value,
};
