/*
 * Copyright (c) 2011 Satoshi Ebisawa. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. The names of its contributors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#ifdef HAVE_EPOLL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include "sf.h"

static int socket_epoll(int poll_fd, int fd, int events, void *sock, int epcmd);
static int socket_epoll_add(int poll_fd, int fd, int events, void *sock);

int
sf_socket_poll_create(void)
{
    return epoll_create(1);
}

int
sf_socket_poll_add(int poll_fd, int fd, void *sock)
{
    return socket_epoll_add(poll_fd, fd, EPOLLIN | EPOLLOUT | EPOLLET, sock);
}

int
sf_socket_poll_wait(sf_instance_t *inst, int poll_fd, struct timeval *timeout)
{
    int i, count, millisec = -1;
    struct epoll_event eev[8];

    if (timeout != NULL) {
        millisec = timeout->tv_sec * 1000;
        millisec += (timeout->tv_usec + 999) / 1000;
    }

    if ((count = epoll_wait(poll_fd, eev, NELEMS(eev), millisec)) < 0) {
        plog_error(LOG_ERR, __func__, "epoll_wait() failed");
        return -1;
    }

    for (i = 0; i < count; i++) {
        if (eev[i].events & EPOLLOUT)
            sf_socket_write_event(inst, eev[i].data.ptr);
    }

    for (i = 0; i < count; i++) {
        if (eev[i].events & EPOLLIN)
            sf_socket_read_event(inst, eev[i].data.ptr);
    }

    return 0;
}

static int
socket_epoll(int poll_fd, int fd, int events, void *sock, int epcmd)
{
    struct epoll_event event;

    memset(&event, 0, sizeof(event));
    event.events = events;
    event.data.ptr = sock;

    if (epoll_ctl(poll_fd, epcmd, fd, &event) < 0) {
        plog_error(LOG_ERR, __func__, "epoll_ctl() failed");
        return -1;
    }

    return 0;
}

static int
socket_epoll_add(int poll_fd, int fd, int events, void *sock)
{
    return socket_epoll(poll_fd, fd, events, sock, EPOLL_CTL_ADD);
}

#endif  /* HAVE_EPOLL */
