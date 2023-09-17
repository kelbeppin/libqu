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

#define QU_MODULE "log"
#include "qu.h"

#ifdef ANDROID
#include <android/log.h>
#endif

//------------------------------------------------------------------------------
// qu_log.c: logging
//------------------------------------------------------------------------------

void qu_log_printf(qu_log level, char const* module, char const* fmt, ...)
{
    char *labels[] = {
        [QU_LOG_DEBUG]   = "DBG ",
        [QU_LOG_INFO]    = "INFO",
        [QU_LOG_WARNING] = "WARN",
        [QU_LOG_ERROR]   = "ERR ",
    };

    va_list ap;
    char buffer[256];
    char *heap = NULL;

    va_start(ap, fmt);
    int count = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    if ((size_t) count >= sizeof(buffer)) {
        heap = malloc(count + 1);

        if (heap) {
            va_start(ap, fmt);
            vsnprintf(heap, count + 1, fmt, ap);
            va_end(ap);
        }
    }

    // Source file path: leave only last segment
    if (module[0] == '?') {
        for (int i = strlen(module) - 1; i >= 0; i--) {
            if (module[i] == '/' || module[i] == '\\') {
                module = module + i + 1;
                break;
            }
        }
    }

    fprintf((level == QU_LOG_ERROR) ? stderr : stdout,
            "(%8.3f) [%s] %s: %s", qu_get_time_mediump(),
            labels[level], module, heap ? heap : buffer);

    qx_sys_write_log(level, module, heap ? heap : buffer);

    free(heap);
}
