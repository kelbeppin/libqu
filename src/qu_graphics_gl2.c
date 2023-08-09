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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glext.h>

//------------------------------------------------------------------------------
// qu_graphics_gl2.c: OpenGL 2.1 graphics
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// OpenGL function pointers

static PFNGLATTACHSHADERPROC               pf_glAttachShader;
static PFNGLBINDATTRIBLOCATIONPROC         pf_glBindAttribLocation;
static PFNGLCOMPILESHADERPROC              pf_glCompileShader;
static PFNGLCREATEPROGRAMPROC              pf_glCreateProgram;
static PFNGLCREATESHADERPROC               pf_glCreateShader;
static PFNGLDELETEPROGRAMPROC              pf_glDeleteProgram;
static PFNGLDELETESHADERPROC               pf_glDeleteShader;
static PFNGLGETPROGRAMINFOLOGPROC          pf_glGetProgramInfoLog;
static PFNGLGETPROGRAMIVPROC               pf_glGetProgramiv;
static PFNGLGETSHADERIVPROC                pf_glGetShaderiv;
static PFNGLGETUNIFORMLOCATIONPROC         pf_glGetUniformLocation;
static PFNGLGETSHADERINFOLOGPROC           pf_glGetShaderInfoLog;
static PFNGLLINKPROGRAMPROC                pf_glLinkProgram;
static PFNGLSHADERSOURCEPROC               pf_glShaderSource;
static PFNGLUNIFORM4FVPROC                 pf_glUniform4fv;
static PFNGLUNIFORMMATRIX4FVPROC           pf_glUniformMatrix4fv;
static PFNGLUSEPROGRAMPROC                 pf_glUseProgram;

static PFNGLBINDBUFFERPROC                 pf_glBindBuffer;
static PFNGLBUFFERDATAPROC                 pf_glBufferData;
static PFNGLBUFFERSUBDATAPROC              pf_glBufferSubData;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC   pf_glDisableVertexAttribArray;
static PFNGLENABLEVERTEXATTRIBARRAYPROC    pf_glEnableVertexAttribArray;
static PFNGLGENBUFFERSPROC                 pf_glGenBuffers;
static PFNGLVERTEXATTRIBPOINTERPROC        pf_glVertexAttribPointer;

static PFNGLBINDFRAMEBUFFEREXTPROC         pf_glBindFramebufferEXT;
static PFNGLBINDRENDERBUFFEREXTPROC        pf_glBindRenderbufferEXT;
static PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC  pf_glCheckFramebufferStatusEXT;
static PFNGLDELETEFRAMEBUFFERSEXTPROC      pf_glDeleteFramebuffersEXT;
static PFNGLDELETERENDERBUFFERSEXTPROC     pf_glDeleteRenderbuffersEXT;
static PFNGLGENFRAMEBUFFERSEXTPROC         pf_glGenFramebuffersEXT;
static PFNGLGENRENDERBUFFERSEXTPROC        pf_glGenRenderbuffersEXT;
static PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC pf_glFramebufferRenderbufferEXT;
static PFNGLFRAMEBUFFERTEXTURE2DEXTPROC    pf_glFramebufferTexture2DEXT;
static PFNGLRENDERBUFFERSTORAGEEXTPROC     pf_glRenderbufferStorageEXT;

//------------------------------------------------------------------------------
// Adapter macros

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER                  GL_FRAMEBUFFER_EXT
#endif

#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER                 GL_RENDERBUFFER_EXT
#endif

#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0            GL_COLOR_ATTACHMENT0_EXT
#endif

#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT             GL_DEPTH_ATTACHMENT_EXT
#endif

#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE         GL_FRAMEBUFFER_COMPLETE_EXT
#endif

#ifndef GL_INVALID_FRAMEBUFFER_OPERATION
#define GL_INVALID_FRAMEBUFFER_OPERATION GL_INVALID_FRAMEBUFFER_OPERATION_EXT
#endif

#define glAttachShader                  pf_glAttachShader
#define glBindAttribLocation            pf_glBindAttribLocation
#define glCompileShader                 pf_glCompileShader
#define glCreateProgram                 pf_glCreateProgram
#define glCreateShader                  pf_glCreateShader
#define glDeleteProgram                 pf_glDeleteProgram
#define glDeleteShader                  pf_glDeleteShader
#define glGetProgramInfoLog             pf_glGetProgramInfoLog
#define glGetProgramiv                  pf_glGetProgramiv
#define glGetShaderiv                   pf_glGetShaderiv
#define glGetUniformLocation            pf_glGetUniformLocation
#define glGetShaderInfoLog              pf_glGetShaderInfoLog
#define glLinkProgram                   pf_glLinkProgram
#define glShaderSource                  pf_glShaderSource
#define glUniform4fv                    pf_glUniform4fv
#define glUniformMatrix4fv              pf_glUniformMatrix4fv
#define glUseProgram                    pf_glUseProgram

#define glBindBuffer                    pf_glBindBuffer
#define glBufferData                    pf_glBufferData
#define glBufferSubData                 pf_glBufferSubData
#define glDisableVertexAttribArray      pf_glDisableVertexAttribArray
#define glEnableVertexAttribArray       pf_glEnableVertexAttribArray
#define glGenBuffers                    pf_glGenBuffers
#define glVertexAttribPointer           pf_glVertexAttribPointer

#define glBindFramebuffer               pf_glBindFramebufferEXT
#define glBindRenderbuffer              pf_glBindRenderbufferEXT
#define glCheckFramebufferStatus        pf_glCheckFramebufferStatusEXT
#define glDeleteFramebuffers            pf_glDeleteFramebuffersEXT
#define glDeleteRenderbuffers           pf_glDeleteRenderbuffersEXT
#define glGenFramebuffers               pf_glGenFramebuffersEXT
#define glGenRenderbuffers              pf_glGenRenderbuffersEXT
#define glFramebufferRenderbuffer       pf_glFramebufferRenderbufferEXT
#define glFramebufferTexture2D          pf_glFramebufferTexture2DEXT
#define glRenderbufferStorage           pf_glRenderbufferStorageEXT

#define GL2_SHADER_VERTEX_SRC \
    "#version 120\n" \
    "attribute vec2 a_position;\n" \
    "attribute vec4 a_color;\n" \
    "attribute vec2 a_texCoord;\n" \
    "varying vec4 v_color;\n" \
    "varying vec2 v_texCoord;\n" \
    "uniform mat4 u_projection;\n" \
    "uniform mat4 u_modelView;\n" \
    "void main()\n" \
    "{\n" \
    "    v_texCoord = a_texCoord;\n" \
    "    v_color = a_color;\n" \
    "    vec4 position = vec4(a_position, 0.0, 1.0);\n" \
    "    gl_Position = u_projection * u_modelView * position;\n" \
    "}\n"

#define GL2_SHADER_SOLID_SRC \
    "#version 120\n" \
    "precision mediump float;\n" \
    "uniform vec4 u_color;\n" \
    "void main()\n" \
    "{\n" \
    "    gl_FragColor = u_color;\n" \
    "}\n"

#define GL2_SHADER_TEXTURED_SRC \
    "#version 120\n" \
    "precision mediump float;\n" \
    "varying vec2 v_texCoord;\n" \
    "uniform sampler2D u_texture;\n" \
    "uniform vec4 u_color;\n" \
    "void main()\n" \
    "{\n" \
    "    gl_FragColor = texture2D(u_texture, v_texCoord) * u_color;\n" \
    "}\n"

#define GL2_SHADER_CANVAS_SRC \
    "#version 120\n" \
    "attribute vec2 a_position;\n" \
    "attribute vec2 a_texCoord;\n" \
    "varying vec2 v_texCoord;\n" \
    "uniform mat4 u_projection;\n" \
    "void main()\n" \
    "{\n" \
    "    v_texCoord = a_texCoord;\n" \
    "    vec4 position = vec4(a_position, 0.0, 1.0);\n" \
    "    gl_Position = u_projection * position;\n" \
    "}\n"

//------------------------------------------------------------------------------
// Shared implementation

#define QU_MODULE "gl2"
#include "qu_graphics_gl2_impl.h"

//------------------------------------------------------------------------------
// Extension loader

static void load_glext(char const *extension)
{
    if (strcmp(extension, "GL_EXT_framebuffer_object") == 0) {
        pf_glBindFramebufferEXT = qu__core_get_gl_proc_address("glBindFramebufferEXT");
        pf_glBindRenderbufferEXT = qu__core_get_gl_proc_address("glBindRenderbufferEXT");
        pf_glCheckFramebufferStatusEXT = qu__core_get_gl_proc_address("glCheckFramebufferStatusEXT");
        pf_glDeleteFramebuffersEXT = qu__core_get_gl_proc_address("glDeleteFramebuffersEXT");
        pf_glDeleteRenderbuffersEXT = qu__core_get_gl_proc_address("glDeleteRenderbuffersEXT");
        pf_glGenFramebuffersEXT = qu__core_get_gl_proc_address("glGenFramebuffersEXT");
        pf_glGenRenderbuffersEXT = qu__core_get_gl_proc_address("glGenRenderbuffersEXT");
        pf_glFramebufferRenderbufferEXT = qu__core_get_gl_proc_address("glFramebufferRenderbufferEXT");
        pf_glFramebufferTexture2DEXT = qu__core_get_gl_proc_address("glFramebufferTexture2DEXT");
        pf_glRenderbufferStorageEXT = qu__core_get_gl_proc_address("glRenderbufferStorageEXT");
    }
}

static void load_gl_functions(void)
{
    pf_glAttachShader = qu__core_get_gl_proc_address("glAttachShader");
    pf_glBindAttribLocation = qu__core_get_gl_proc_address("glBindAttribLocation");
    pf_glCompileShader = qu__core_get_gl_proc_address("glCompileShader");
    pf_glCreateProgram = qu__core_get_gl_proc_address("glCreateProgram");
    pf_glCreateShader = qu__core_get_gl_proc_address("glCreateShader");
    pf_glDeleteProgram = qu__core_get_gl_proc_address("glDeleteProgram");
    pf_glDeleteShader = qu__core_get_gl_proc_address("glDeleteShader");
    pf_glGetProgramInfoLog = qu__core_get_gl_proc_address("glGetProgramInfoLog");
    pf_glGetProgramiv = qu__core_get_gl_proc_address("glGetProgramiv");
    pf_glGetShaderiv = qu__core_get_gl_proc_address("glGetShaderiv");
    pf_glGetUniformLocation = qu__core_get_gl_proc_address("glGetUniformLocation");
    pf_glGetShaderInfoLog = qu__core_get_gl_proc_address("glGetShaderInfoLog");
    pf_glLinkProgram = qu__core_get_gl_proc_address("glLinkProgram");
    pf_glShaderSource = qu__core_get_gl_proc_address("glShaderSource");
    pf_glUniform4fv = qu__core_get_gl_proc_address("glUniform4fv");
    pf_glUniformMatrix4fv = qu__core_get_gl_proc_address("glUniformMatrix4fv");
    pf_glUseProgram = qu__core_get_gl_proc_address("glUseProgram");

    pf_glBindBuffer = qu__core_get_gl_proc_address("glBindBuffer");
    pf_glBufferData = qu__core_get_gl_proc_address("glBufferData");
    pf_glBufferSubData = qu__core_get_gl_proc_address("glBufferSubData");
    pf_glDisableVertexAttribArray = qu__core_get_gl_proc_address("glDisableVertexAttribArray");
    pf_glEnableVertexAttribArray = qu__core_get_gl_proc_address("glEnableVertexAttribArray");
    pf_glGenBuffers = qu__core_get_gl_proc_address("glGenBuffers");
    pf_glVertexAttribPointer = qu__core_get_gl_proc_address("glVertexAttribPointer");

    char *extensions = qu_strdup((char const *) glGetString(GL_EXTENSIONS));

    QU_INFO("Supported OpenGL extensions:\n");
    QU_INFO("%s\n", extensions);

    char *token = strtok(extensions, " ");
    int count = 0;

    while (token) {
        load_glext(token);
        token = strtok(NULL, " ");
        count++;
    }

    QU_INFO("Total OpenGL extensions: %d\n", count);

    free(extensions);
}

static bool check_glext(char const *extension)
{
    char *extensions = qu_strdup((char const *) glGetString(GL_EXTENSIONS));
    char *token = strtok(extensions, " ");
    bool found = false;

    while (token) {
        if (strcmp(token, extension) == 0) {
            found = true;
            break;
        }

        token = strtok(NULL, " ");
    }

    free(extensions);

    return found;
}

//------------------------------------------------------------------------------

static bool query(qu_params const *params)
{
    load_gl_functions();

    if (!check_glext("GL_EXT_framebuffer_object")) {
        QU_ERROR("Required OpenGL extension GL_EXT_framebuffer_object is not supported.\n");
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// Constructor

struct qu__graphics const qu__graphics_gl2 = {
    .query = query,
    .initialize = gl2_initialize,
    .terminate = gl2_terminate,
    .refresh = gl2_refresh,
    .swap = gl2_swap,
    .on_display_resize = gl2_on_display_resize,
    .conv_cursor = gl2_conv_cursor,
    .conv_cursor_delta = gl2_conv_cursor_delta,
    .set_view = gl2_set_view,
    .reset_view = gl2_reset_view,
    .push_matrix = gl2_push_matrix,
    .pop_matrix = gl2_pop_matrix,
    .translate = gl2_translate,
    .scale = gl2_scale,
    .rotate = gl2_rotate,
    .clear = gl2_clear,
    .draw_point = gl2_draw_point,
    .draw_line = gl2_draw_line,
    .draw_triangle = gl2_draw_triangle,
    .draw_rectangle = gl2_draw_rectangle,
    .draw_circle = gl2_draw_circle,
    .create_texture = gl2_create_texture,
    .update_texture = gl2_update_texture,
    .load_texture = gl2_load_texture,
    .delete_texture = gl2_delete_texture,
    .set_texture_smooth = gl2_set_texture_smooth,
    .draw_texture = gl2_draw_texture,
    .draw_subtexture = gl2_draw_subtexture,
    .draw_text = gl2_draw_text,
    .create_surface = gl2_create_surface,
    .delete_surface = gl2_delete_surface,
    .set_surface = gl2_set_surface,
    .reset_surface = gl2_reset_surface,
    .draw_surface = gl2_draw_surface,
};
