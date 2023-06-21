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

#include "syscall.h"
#include <sys/mman.h>
#include <unistd.h>
#include <chcore/oh_vfs.h>
#include "file.h"

int vfs_rename(int fd, const char *path)
{
    return chcore_vfs_rename(fd, path);
}

void *vfs_mmap(int32_t vfs_fd, uint64_t size, uint64_t off)
{
    return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, vfs_fd, off);
}
