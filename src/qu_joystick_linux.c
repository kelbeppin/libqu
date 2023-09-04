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

#define QU_MODULE "joystick-linux"

#include "qu.h"

#include <linux/joystick.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

//------------------------------------------------------------------------------
// qu_joystick_linux.c: Linux joystick module
//------------------------------------------------------------------------------

#define MAX_JOYSTICKS               4
#define MAX_JOYSTICK_ID             64

#define MAX_JOYSTICK_BUTTONS		(KEY_MAX - BTN_MISC + 1)
#define MAX_JOYSTICK_AXES			(ABS_MAX + 1)

struct qu__linux_js_device
{
	char id[MAX_JOYSTICK_ID];

	int button_count;
	int axis_count;

	bool button[MAX_JOYSTICK_BUTTONS];
	float axis[MAX_JOYSTICK_AXES];

	int fd;
	uint16_t button_map[MAX_JOYSTICK_BUTTONS];
	uint8_t axis_map[MAX_JOYSTICK_AXES];
};

struct qu__joystick_linux_priv
{
	float next_poll_time;
	struct qu__linux_js_device device[MAX_JOYSTICKS];
};

static struct qu__joystick_linux_priv priv;

//------------------------------------------------------------------------------

static void joystick_linux__initialize(qu_params const *params)
{
    memset(&priv, 0, sizeof(priv));

	for (int i = 0; i < MAX_JOYSTICKS; i++) {
        priv.device[i].fd = -1;
    }
}

static void joystick_linux__terminate(void)
{
}

static void joystick_linux__process(void)
{
    // Read joystick events.
    for (int i = 0; i < MAX_JOYSTICKS; i++) {
        if (priv.device[i].fd == -1) {
            continue;
        }

        struct js_event event;

        // -1 is returned if event queue is empty.
        while (read(priv.device[i].fd, &event, sizeof(event)) != -1) {
            // Event type can be JS_EVENT_BUTTON or JS_EVENT_AXIS.
            // Either of these events can be ORd together with JS_EVENT_INIT
            // if event is sent for the first time in order to give you
            // initial values for buttons and axes.

            if (event.type & JS_EVENT_BUTTON) {
                priv.device[i].button[event.number] = event.value;

                if (!(event.type & JS_EVENT_INIT)) {
                    // TODO: ???
                }
            } else if (event.type & JS_EVENT_AXIS) {
                priv.device[i].axis[event.number] = event.value / 32767.f;
            }
        }

        // errno is set to EAGAIN if joystick is still connected.
        // If it's not, then joystick has been disconnected.
        if (errno != EAGAIN) {
            close(priv.device[i].fd);

            QU_INFO("Joystick '%s' disconnected.\n", priv.device[i].id);

            memset(&priv.device[i], 0, sizeof(priv.device[i]));

            priv.device[i].fd = -1;
            priv.device[i].button_count = 0;
            priv.device[i].axis_count = 0;
        }
    }
}

static bool joystick_linux__is_connected(int id)
{
	if (id < 0 || id >= MAX_JOYSTICKS) {
        return false;
    }

    if (priv.device[id].fd != -1) {
        return true;
    }

    float current_time = qu_get_time_mediump();

    if (priv.next_poll_time > current_time) {
        return false;
    }

    priv.next_poll_time = current_time + 1.f;

    char path[64];
    snprintf(path, sizeof(path) - 1, "/dev/input/js%d", joystick);

    int fd = open(path, O_RDONLY | O_NONBLOCK);

    if (fd == -1) {
        return false;
    }

    ioctl(fd, JSIOCGNAME(MAX_JOYSTICK_ID), priv.device[id].id);
    ioctl(fd, JSIOCGBUTTONS, &priv.device[id].button_count);
    ioctl(fd, JSIOCGAXES, &priv.device[id].axis_count);
    ioctl(fd, JSIOCGBTNMAP, priv.device[id].button_map);
    ioctl(fd, JSIOCGAXMAP, priv.device[id].axis_map);

    priv.device[id].fd = fd;

    QU_INFO("Joystick '%s' connected.\n", priv.device[id].id);
    QU_INFO("# of buttons: %d.\n", priv.device[id].button_count);
    QU_INFO("# of axes: %d.\n", priv.device[id].axis_count);

    return true;
}

static char const *joystick_linux__get_name(int id)
{
	if (id < 0 || id >= MAX_JOYSTICKS) {
        return NULL;
    }

    return priv.device[id].id;
}

static int joystick_linux__get_button_count(int id)
{
	if (id < 0 || id >= MAX_JOYSTICKS) {
        return 0;
    }

    return priv.device[id].button_count;
}

static int joystick_linux__get_axis_count(int id)
{
	if (id < 0 || id >= MAX_JOYSTICKS) {
        return 0;
    }

    return priv.device[id].axis_count;
}

static char const *joystick_linux__get_button_name(int id, int button)
{
	if (id < 0 || id >= MAX_JOYSTICKS) {
        return NULL;
    }

    if (button < 0 || button >= MAX_JOYSTICK_BUTTONS) {
        return NULL;
    }

    uint16_t index = priv.device[id].button_map[button];

    switch (index) {
    case BTN_TRIGGER:       return "TRIGGER";
    case BTN_THUMB:         return "THUMB";
    case BTN_THUMB2:        return "THUMB2";
    case BTN_TOP:           return "TOP";
    case BTN_TOP2:          return "TOP2";
    case BTN_PINKIE:        return "PINKIE";
    case BTN_BASE:          return "BASE";
    case BTN_BASE2:         return "BASE2";
    case BTN_BASE3:         return "BASE3";
    case BTN_BASE4:         return "BASE4";
    case BTN_BASE5:         return "BASE5";
    case BTN_BASE6:         return "BASE6";
    case BTN_DEAD:          return "DEAD";
    case BTN_A:             return "A"; // also SOUTH
    case BTN_B:             return "B"; // also EAST
    case BTN_C:             return "C";
    case BTN_X:             return "X"; // also NORTH
    case BTN_Y:             return "Y"; // also WEST
    case BTN_Z:             return "Z";
    case BTN_TL:            return "TL";
    case BTN_TR:            return "TR";
    case BTN_TL2:           return "TL2";
    case BTN_TR2:           return "TR2";
    case BTN_SELECT:        return "SELECT";
    case BTN_START:         return "START";
    case BTN_MODE:          return "MODE";
    case BTN_THUMBL:        return "THUMBL";
    case BTN_THUMBR:        return "THUMBR";
    default:
        break;
    }

    return NULL;
}

static char const *joystick_linux__get_axis_name(int id, int axis)
{
	if (id < 0 || id >= MAX_JOYSTICKS) {
        return NULL;
    }

    if (axis < 0 || axis >= MAX_JOYSTICK_AXES) {
        return NULL;
    }

    uint16_t index = priv.device[id].axis_map[axis];

    switch (index) {
    case ABS_X:             return "X";
    case ABS_Y:             return "Y";
    case ABS_Z:             return "Z";
    case ABS_RX:            return "RX";
    case ABS_RY:            return "RY";
    case ABS_RZ:            return "RZ";
    case ABS_THROTTLE:      return "THROTTLE";
    case ABS_RUDDER:        return "RUDDER";
    case ABS_WHEEL:         return "WHEEL";
    case ABS_GAS:           return "GAS";
    case ABS_BRAKE:         return "BRAKE";
    case ABS_HAT0X:         return "HAT0X";
    case ABS_HAT0Y:         return "HAT0Y";
    case ABS_HAT1X:         return "HAT1X";
    case ABS_HAT1Y:         return "HAT1Y";
    case ABS_HAT2X:         return "HAT2X";
    case ABS_HAT2Y:         return "HAT2Y";
    case ABS_HAT3X:         return "HAT3X";
    case ABS_HAT3Y:         return "HAT3Y";
    case ABS_PRESSURE:      return "PRESSURE";
    case ABS_DISTANCE:      return "DISTANCE";
    case ABS_TILT_X:        return "TILT_X";
    case ABS_TILT_Y:        return "TILT_Y";
    case ABS_TOOL_WIDTH:    return "TOOL_WIDTH";
    case ABS_VOLUME:        return "VOLUME";
    case ABS_MISC:          return "MISC";
    default:
        break;
    }

    return NULL;
}

static bool joystick_linux__is_button_pressed(int id, int button)
{
	if (id < 0 || id >= MAX_JOYSTICKS) {
        return false;
    }

    if (button < 0 || button >= MAX_JOYSTICK_BUTTONS) {
        return false;
    }

    return priv.device[id].button[button];
}

static float joystick_linux__get_axis_value(int id, int axis)
{
	if (id < 0 || id >= MAX_JOYSTICKS) {
        return 0.f;
    }

    if (axis < 0 || axis >= MAX_JOYSTICK_AXES) {
        return 0.f;
    }

    return priv.device[id].axis[axis];
}

//------------------------------------------------------------------------------

struct qu__joystick const qu__joystick_linux = {
	.initialize = joystick_linux__initialize,
	.terminate = joystick_linux__terminate,
    .process = joystick_linux__process,
	.is_connected = joystick_linux__is_connected,
	.get_name = joystick_linux__get_name,
	.get_button_count = joystick_linux__get_button_count,
	.get_axis_count = joystick_linux__get_axis_count,
	.get_button_name = joystick_linux__get_button_name,
	.get_axis_name = joystick_linux__get_axis_name,
	.is_button_pressed = joystick_linux__is_button_pressed,
	.get_axis_value = joystick_linux__get_axis_value,
};
