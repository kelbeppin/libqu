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

#include "qu.h"

//------------------------------------------------------------------------------
// qu_joystick_win32.cpp: Win32 joystick module
//------------------------------------------------------------------------------

static void joystick_win32__initialize(qu_params const *params)
{
}

static void joystick_win32__terminate(void)
{
}

static bool joystick_win32__is_connected(int id)
{
	return false;
}

static char const *joystick_win32__get_name(int id)
{
	return NULL;
}

static int joystick_win32__get_button_count(int id)
{
	return 0;
}

static int joystick_win32__get_axis_count(int id)
{
	return 0;
}

static char const *joystick_win32__get_button_name(int id, int button)
{
	return NULL;
}

static char const *joystick_win32__get_axis_name(int id, int axis)
{
	return NULL;
}

static bool joystick_win32__is_button_pressed(int id, int button)
{
	return false;
}

static float joystick_win32__get_axis_value(int id, int axis)
{
	return 0.f;
}

//------------------------------------------------------------------------------

struct qu__joystick const qu__joystick_win32 = {
	joystick_win32__initialize,
	joystick_win32__terminate,
	joystick_win32__is_connected,
	joystick_win32__get_name,
	joystick_win32__get_button_count,
	joystick_win32__get_axis_count,
	joystick_win32__get_button_name,
	joystick_win32__get_axis_name,
	joystick_win32__is_button_pressed,
	joystick_win32__get_axis_value,
};
