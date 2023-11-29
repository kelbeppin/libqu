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
// qu_math.h: linear algebra
//------------------------------------------------------------------------------

#ifndef QU_MATH_H_INC
#define QU_MATH_H_INC

//------------------------------------------------------------------------------

#include <math.h>
#include <libqu.h>

//------------------------------------------------------------------------------

typedef struct qu_mat4
{
    float m[16];
} qu_mat4;

//------------------------------------------------------------------------------

void qu_mat4_identity(qu_mat4 *mat);
void qu_mat4_copy(qu_mat4 *dst, qu_mat4 const *src);
void qu_mat4_multiply(qu_mat4 *a, qu_mat4 const *b);
void qu_mat4_ortho(qu_mat4 *mat, float l, float r, float b, float t);
void qu_mat4_translate(qu_mat4 *mat, float x, float y, float z);
void qu_mat4_scale(qu_mat4 *mat, float x, float y, float z);
void qu_mat4_rotate(qu_mat4 *mat, float rad, float x, float y, float z);
void qu_mat4_inverse(qu_mat4 *dst, qu_mat4 const *src);
qu_vec2f qu_mat4_transform_point(qu_mat4 const *mat, qu_vec2f p);

//------------------------------------------------------------------------------

#endif // QU_MATH_H_INC

