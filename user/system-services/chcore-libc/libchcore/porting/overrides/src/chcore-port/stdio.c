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

/* File operation towards STDIN, STDOUT and STDERR */

#include "fd.h"
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <syscall_arch.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <chcore/memory.h>
#include <termios.h>

#include <chcore-internal/procmgr_defs.h>
#include <unistd.h>
#include <fcntl.h>
#include <raw_syscall.h>
#include "stdio.h"

static void put(char buffer[], unsigned size)
{
    chcore_syscall2(CHCORE_SYS_putstr, (vaddr_t)buffer, size);
}

#define MAX_LINE_SIZE 4095

int chcore_stdio_fcntl(int fd, int cmd, int arg)
{
    int new_fd, ret = 0;

    switch (cmd) {
    case F_DUPFD_CLOEXEC:
    case F_DUPFD: {
        new_fd = dup_fd_content(fd, arg);
        return new_fd;
    }
    default:
        return -EINVAL;
    }
    return -1;
}

/* STDIN */
static ssize_t chcore_stdio_read(int fd, void *buf, size_t count)
{
    return -EINVAL;
}

static ssize_t chcore_stdin_write(int fd, void *buf, size_t count)
{
    return -EINVAL;
}

static int chcore_stdin_close(int fd)
{
    free_fd(fd);
    return 0;
}

static int chcore_stdio_ioctl(int fd, unsigned long request, void *arg)
{
    /* A fake window size */
    if (request == TIOCGWINSZ) {
        struct winsize *ws;

        ws = (struct winsize *)arg;
        ws->ws_row = 30;
        ws->ws_col = 80;
        ws->ws_xpixel = 0;
        ws->ws_ypixel = 0;
        return 0;
    }
    struct fd_desc *fdesc = fd_dic[fd];
    assert(fdesc);
    switch (request) {
    case TCGETS: {
        struct termios *t = (struct termios *)arg;
        *t = fdesc->termios;
        return 0;
    }
    case TCSETS:
    case TCSETSW:
    case TCSETSF: {
        struct termios *t = (struct termios *)arg;
        fdesc->termios = *t;
        return 0;
    }
    }
    warn("Unsupported ioctl fd=%d, cmd=0x%lx, arg=0x%lx\n", fd, request, arg);
    return 0;
}

static int chcore_stdio_poll(int fd, struct pollarg *arg)
{
    return -EINVAL;
}

struct fd_ops stdin_ops = {
    .read = chcore_stdio_read,
    .write = chcore_stdin_write,
    .close = chcore_stdin_close,
    .poll = chcore_stdio_poll,
    .ioctl = chcore_stdio_ioctl,
    .fcntl = chcore_stdio_fcntl,
};

/* STDOUT */
#define STDOUT_BUFSIZE 1024

static ssize_t chcore_stdout_write(int fd, void *buf, size_t count)
{
    
    char buffer[STDOUT_BUFSIZE];
    size_t size = 0;

    for (char *p = buf; p < (char *)buf + count; p++) {
        if (size + 2 > STDOUT_BUFSIZE) {
            put(buffer, size);
            size = 0;
        }

        if (*p == '\n') {
            buffer[size++] = '\r';
        }
        buffer[size++] = *p;
    }

    if (size > 0) {
        put(buffer, size);
    }

    return count;
}

static int chcore_stdout_close(int fd)
{
    free_fd(fd);
    return 0;
}

struct fd_ops stdout_ops = {
    .read = chcore_stdio_read,
    .write = chcore_stdout_write,
    .close = chcore_stdout_close,
    .poll = chcore_stdio_poll,
    .ioctl = chcore_stdio_ioctl,
    .fcntl = chcore_stdio_fcntl,
};

/* STDERR */
static ssize_t chcore_stderr_read(int fd, void *buf, size_t count)
{
    return -EINVAL;
}

static ssize_t chcore_stderr_write(int fd, void *buf, size_t count)
{
    return chcore_stdout_write(fd, buf, count);
}

static int chcore_stderr_close(int fd)
{
    free_fd(fd);
    return 0;
}

struct fd_ops stderr_ops = {
    .read = chcore_stderr_read,
    .write = chcore_stderr_write,
    .close = chcore_stderr_close,
    .poll = NULL,
    .ioctl = chcore_stdio_ioctl,
    .fcntl = chcore_stdio_fcntl,
};