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

#define QU_MODULE "math"
#include "qu.h"

//------------------------------------------------------------------------------
// qu_math.c: linear algebra
//------------------------------------------------------------------------------

void qu_mat4_identity(qu_mat4 *mat)
{
    static float const identity[] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    memcpy(mat->m, identity, sizeof(float) * 16);
}

void qu_mat4_copy(qu_mat4 *dst, qu_mat4 const *src)
{
    memcpy(dst->m, src->m, sizeof(float) * 16);
}

void qu_mat4_multiply(qu_mat4 *a, qu_mat4 const *b)
{
    float const result[] = {
        a->m[ 0] * b->m[ 0] + a->m[ 4] * b->m[ 1] + a->m[ 8] * b->m[ 2] + a->m[12] * b->m[ 3],
        a->m[ 1] * b->m[ 0] + a->m[ 5] * b->m[ 1] + a->m[ 9] * b->m[ 2] + a->m[13] * b->m[ 3],
        a->m[ 2] * b->m[ 0] + a->m[ 6] * b->m[ 1] + a->m[10] * b->m[ 2] + a->m[14] * b->m[ 3],
        a->m[ 3] * b->m[ 0] + a->m[ 7] * b->m[ 1] + a->m[11] * b->m[ 2] + a->m[15] * b->m[ 3],
        a->m[ 0] * b->m[ 4] + a->m[ 4] * b->m[ 5] + a->m[ 8] * b->m[ 6] + a->m[12] * b->m[ 7],
        a->m[ 1] * b->m[ 4] + a->m[ 5] * b->m[ 5] + a->m[ 9] * b->m[ 6] + a->m[13] * b->m[ 7],
        a->m[ 2] * b->m[ 4] + a->m[ 6] * b->m[ 5] + a->m[10] * b->m[ 6] + a->m[14] * b->m[ 7],
        a->m[ 3] * b->m[ 4] + a->m[ 7] * b->m[ 5] + a->m[11] * b->m[ 6] + a->m[15] * b->m[ 7],
        a->m[ 0] * b->m[ 8] + a->m[ 4] * b->m[ 9] + a->m[ 8] * b->m[10] + a->m[12] * b->m[11],
        a->m[ 1] * b->m[ 8] + a->m[ 5] * b->m[ 9] + a->m[ 9] * b->m[10] + a->m[13] * b->m[11],
        a->m[ 2] * b->m[ 8] + a->m[ 6] * b->m[ 9] + a->m[10] * b->m[10] + a->m[14] * b->m[11],
        a->m[ 3] * b->m[ 8] + a->m[ 7] * b->m[ 9] + a->m[11] * b->m[10] + a->m[15] * b->m[11],
        a->m[ 0] * b->m[12] + a->m[ 4] * b->m[13] + a->m[ 8] * b->m[14] + a->m[12] * b->m[15],
        a->m[ 1] * b->m[12] + a->m[ 5] * b->m[13] + a->m[ 9] * b->m[14] + a->m[13] * b->m[15],
        a->m[ 2] * b->m[12] + a->m[ 6] * b->m[13] + a->m[10] * b->m[14] + a->m[14] * b->m[15],
        a->m[ 3] * b->m[12] + a->m[ 7] * b->m[13] + a->m[11] * b->m[14] + a->m[15] * b->m[15],
    };

    memcpy(a->m, result, sizeof(float) * 16);
}

void qu_mat4_ortho(qu_mat4 *mat, float l, float r, float b, float t)
{
    float n = -1.f;
    float f = +1.f;

    mat->m[ 0] = 2.0f / (r - l);
    mat->m[ 1] = 0.0f;
    mat->m[ 2] = 0.0f;
    mat->m[ 3] = 0.0f;

    mat->m[ 4] = 0.0f;
    mat->m[ 5] = 2.0f / (t - b);
    mat->m[ 6] = 0.0f;
    mat->m[ 7] = 0.0f;

    mat->m[ 8] = 0.0f;
    mat->m[ 9] = 0.0f;
    mat->m[10] = -2.0f / (f - n);
    mat->m[11] = 0.0f;

    mat->m[12] = -(r + l) / (r - l);
    mat->m[13] = -(t + b) / (t - b);
    mat->m[14] = -(f + n) / (f - n);
    mat->m[15] = 1.0f;
}

void qu_mat4_translate(qu_mat4 *mat, float x, float y, float z)
{
    qu_mat4 const translation = {
        1.f,    0.f,    0.f,    0.f,
        0.f,    1.f,    0.f,    0.f,
        0.f,    0.f,    1.f,    0.f,
        x,      y,      z,      1.f,
    };

    qu_mat4_multiply(mat, &translation);
}

void qu_mat4_scale(qu_mat4 *mat, float x, float y, float z)
{
    qu_mat4 const scale = {
        x,      0.f,    0.f,    0.f,
        0.f,    y,      0.f,    0.f,
        0.f,    0.f,    z,      0.f,
        0.f,    0.f,    0.f,    1.f,
    };

    qu_mat4_multiply(mat, &scale);
}

void qu_mat4_rotate(qu_mat4 *mat, float rad, float x, float y, float z)
{
    float xx = x * x;
    float yy = y * y;
    float zz = z * z;
    float xy = x * y;
    float xz = x * z;
    float yz = y * z;

    float c = cosf(rad);
    float s = sinf(rad);
    float ci = 1.0f - c;

    float xs = x * s;
    float ys = y * s;
    float zs = z * s;

    qu_mat4 const rotation = {
        xx * ci + c,    y  * ci + zs,   xz * ci - ys,   0.f,
        xy * ci - zs,   yy * ci + c,    yz * ci + xs,   0.f,
        xz * ci + ys,   yz * ci - xs,   zz * ci + c,    0.f,
        0.f,            0.f,            0.f,            1.f,
    };

    qu_mat4_multiply(mat, &rotation);
}

void qu_mat4_inverse(qu_mat4 *dst, qu_mat4 const *src)
{
    float det =
        src->m[0] * (src->m[15] * src->m[5] - src->m[13] * src->m[7]) -
        src->m[4] * (src->m[15] * src->m[1] - src->m[13] * src->m[3]) +
        src->m[8] * (src->m[ 7] * src->m[1] - src->m[ 5] * src->m[3]);

    if (det == 0.f) {
        qu_mat4_identity(dst);
        return;
    }

    dst->m[ 0] = +(src->m[15] * src->m[5] - src->m[13] * src->m[7]) / det;
    dst->m[ 1] = -(src->m[15] * src->m[4] - src->m[12] * src->m[7]) / det;
    dst->m[ 2] = 0.0f;
    dst->m[ 3] = +(src->m[13] * src->m[4] - src->m[12] * src->m[5]) / det;
    dst->m[ 4] = -(src->m[15] * src->m[1] - src->m[13] * src->m[3]) / det;
    dst->m[ 5] = +(src->m[15] * src->m[0] - src->m[12] * src->m[3]) / det;
    dst->m[ 6] = 0.0f;
    dst->m[ 7] = -(src->m[13] * src->m[0] - src->m[12] * src->m[1]) / det;
    dst->m[ 8] = 0.0f;
    dst->m[ 9] = 0.0f;
    dst->m[10] = 1.0f;
    dst->m[11] = 0.0f;
    dst->m[12] = +(src->m[7] * src->m[1] - src->m[5] * src->m[3]) / det;
    dst->m[13] = -(src->m[7] * src->m[0] - src->m[4] * src->m[3]) / det;
    dst->m[14] = 0.0f;
    dst->m[15] = +(src->m[5] * src->m[0] - src->m[4] * src->m[1]) / det;
}

qu_vec2f qu_mat4_transform_point(qu_mat4 const *mat, qu_vec2f p)
{
    return (qu_vec2f) {
        .x = mat->m[0] * p.x + mat->m[1] * p.y + mat->m[3],
        .y = mat->m[4] * p.x + mat->m[5] * p.y + mat->m[7],
    };
}
