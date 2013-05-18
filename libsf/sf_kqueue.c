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
#ifdef HAVE_KQUEUE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "sf.h"

static int socket_kqueue_write_event(sf_instance_t *inst, struct kevent *kev, int count);
static int socket_kqueue_read_event(sf_instance_t *inst, struct kevent *kev, int count);

int
sf_socket_poll_create(void)
{
    return kqueue();
}

int
sf_socket_poll_add(int poll_fd, int fd, void *sock)
{
    struct kevent kev;

    plog(LOG_DEBUG, "%s: fd = %d", __func__, fd);

    EV_SET(&kev, fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, sock);
    if (kevent(poll_fd, &kev, 1, NULL, 0, NULL) < 0) {
        plog(LOG_DEBUG, "%s: kevent() failed", __func__);
        return -1;
    }

    EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, sock);
    if (kevent(poll_fd, &kev, 1, NULL, 0, NULL) < 0) {
        plog(LOG_DEBUG, "%s: kevent() failed", __func__);
        return -1;
    }

    return 0;
}

int
sf_socket_poll_wait(sf_instance_t *inst, int poll_fd, struct timeval *timeout)
{
    int i, count;
    struct kevent kev[8];
    struct timespec ts0, *ts = NULL;

    if (timeout != NULL) {
        ts0.tv_sec = timeout->tv_sec;
        ts0.tv_nsec = timeout->tv_usec * 1000;
        ts = &ts0;
    }

    if ((count = kevent(poll_fd, NULL, 0, kev, NELEMS(kev), ts)) < 0) {
        plog_error(LOG_ERR, "%s: kevent() failed", __func__);
        return -1;
    }

    for (i = 0; i < count; i++) {
        if (kev[i].filter == EVFILT_WRITE)
            sf_socket_write_event(inst, kev[i].udata);
    }

    for (i = 0; i < count; i++) {
        if (kev[i].filter == EVFILT_READ)
            sf_socket_read_event(inst, kev[i].udata);
    }

    return 0;
}

#endif  /* HAVE_KQUEUE */
