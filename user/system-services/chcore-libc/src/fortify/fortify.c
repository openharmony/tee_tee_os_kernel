/*
 * Copyright (c) 2023 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define _GNU_SOURCE
#define _BSD_SOURCE

#include "fortify.h"
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
void __fortify_error(const char* info, ...)
{
    va_list ap;
    va_start(ap, info);
    fprintf(stderr, FORTIFY_RUNTIME_ERROR_PREFIX);
    vfprintf(stderr, info, ap);
    va_end(ap);
    abort();
}

static inline void __diagnose_buffer_access(const char* fn, const char* action,
                                            size_t claim, size_t actual)
{
    if (__DIAGNOSE_PREDICT_FALSE(claim > actual)) {
        __fortify_error("%s: avoid %zu-byte %s %zu-byte buffer\n",
                        fn,
                        claim,
                        action,
                        actual);
    }
}

char* __fgets_chk(char* dest, int supplied_size, FILE* stream,
                  size_t dst_len_from_compiler)
{
    __diagnose_buffer_access(
        "fgets", "write into", supplied_size, dst_len_from_compiler);
    return __DIAGNOSE_CALL_BYPASSING_FORTIFY(
        fgets)(dest, supplied_size, stream);
}
int __vsprintf_chk(char* dest, int flags, size_t dst_len_from_compiler,
                   const char* format, va_list va)
{
    // The compiler has SIZE_MAX, But vsnprintf cannot use such a large
    // size.
    int result = __DIAGNOSE_CALL_BYPASSING_FORTIFY(vsnprintf)(
        dest,
        dst_len_from_compiler == SIZE_MAX ? SSIZE_MAX : dst_len_from_compiler,
        format,
        va);

    // Attempts to find out after the fact fail.
    __diagnose_buffer_access(
        "vsprintf", "write into", result + 1, dst_len_from_compiler);
    return result;
}
