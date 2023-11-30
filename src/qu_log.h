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
// qu_log.h: logging
//------------------------------------------------------------------------------

#ifndef QU_LOG_H_INC
#define QU_LOG_H_INC

//------------------------------------------------------------------------------

#include "qu.h"

//------------------------------------------------------------------------------

#if !defined(QU_MODULE)
#define QU_MODULE      "?"__FILE__
#endif

#if defined(NDEBUG)
#define QU_LOGD(...)
#else
#define QU_LOGD(...)    qu_log_printf(QU_LOG_LEVEL_DEBUG, QU_MODULE, __VA_ARGS__)
#endif

#define QU_LOGI(...)    qu_log_printf(QU_LOG_LEVEL_INFO, QU_MODULE, __VA_ARGS__)
#define QU_LOGW(...)    qu_log_printf(QU_LOG_LEVEL_WARNING, QU_MODULE, __VA_ARGS__)
#define QU_LOGE(...)    qu_log_printf(QU_LOG_LEVEL_ERROR, QU_MODULE, __VA_ARGS__)

//------------------------------------------------------------------------------

typedef enum qu_log_level
{
    QU_LOG_LEVEL_DEBUG,
    QU_LOG_LEVEL_INFO,
    QU_LOG_LEVEL_WARNING,
    QU_LOG_LEVEL_ERROR,
} qu_log_level;

//------------------------------------------------------------------------------

void qu_log_puts(qu_log_level level, char const *tag, char const *str);
void qu_log_printf(qu_log_level level, char const *tag, char const *fmt, ...);

//------------------------------------------------------------------------------

#endif // QU_LOG_H_INC

