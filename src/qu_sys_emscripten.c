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

#define QU_MODULE "sys-emscripten"

#include "qu.h"

//------------------------------------------------------------------------------

void *qx_sys_fopen(char const *path)
{
	return NULL;
}

void qx_sys_fclose(void *file)
{
}

int64_t qx_sys_fread(void *buffer, size_t size, void *file)
{
	return -1;
}

int64_t qx_sys_ftell(void *file)
{
	return -1;
}

int64_t qx_sys_fseek(void *file, int64_t offset, int origin)
{
	return -1;
}

size_t qx_sys_get_file_size(void *file)
{
	return 0;
}
