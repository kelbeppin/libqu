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

#define QU_MODULE "joystick-null"

#include "qu.h"

//------------------------------------------------------------------------------
// qu_joystick_null.c: Dummy joystick module
//------------------------------------------------------------------------------

static qu_result joystick_null__precheck(void)
{
	return QU_SUCCESS;
}

static qu_result joystick_null__initialize(void)
{
	return QU_SUCCESS;
}

static void joystick_null__terminate(void)
{
}

static void joystick_null__process(void)
{
}

static bool joystick_null__is_connected(int id)
{
	return false;
}

static char const *joystick_null__get_name(int id)
{
	return NULL;
}

static int joystick_null__get_button_count(int id)
{
	return 0;
}

static int joystick_null__get_axis_count(int id)
{
	return 0;
}

static char const *joystick_null__get_button_name(int id, int button)
{
	return NULL;
}

static char const *joystick_null__get_axis_name(int id, int axis)
{
	return NULL;
}

static bool joystick_null__is_button_pressed(int id, int button)
{
	return false;
}

static float joystick_null__get_axis_value(int id, int axis)
{
	return 0.f;
}

//------------------------------------------------------------------------------

qu_joystick_impl const qu_null_joystick_impl = {
	.precheck = joystick_null__precheck,
	.initialize = joystick_null__initialize,
	.terminate = joystick_null__terminate,
	.process = joystick_null__process,
	.is_connected = joystick_null__is_connected,
	.get_name = joystick_null__get_name,
	.get_button_count = joystick_null__get_button_count,
	.get_axis_count = joystick_null__get_axis_count,
	.get_button_name = joystick_null__get_button_name,
	.get_axis_name = joystick_null__get_axis_name,
	.is_button_pressed = joystick_null__is_button_pressed,
	.get_axis_value = joystick_null__get_axis_value,
};
