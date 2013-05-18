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
#ifndef MSGQUEUE_H
#define MSGQUEUE_H
#include "libsf/sf.h"
#include "msgsink.h"

#define MSGQUEUE_PBUFS  4

typedef struct {
    msgsink_t  mq_msgsink;
    sf_pbuf_t  mq_pbuf[MSGQUEUE_PBUFS];
    int        mq_pbuf_r;
    int        mq_pbuf_w;
    size_t     mq_queue_total_size;
    void     (*mq_push_callback)(void *param);
    void      *mq_push_cbparam;
} msgqueue_t;

msgqueue_t *msgqueue_create(size_t queue_size, void (*callback)(void *), void *param);
void msgqueue_destroy(msgqueue_t *self);
int msgqueue_peek(msgqueue_t *self, char **buf, int *len);
int msgqueue_pop_msg(msgqueue_t *self);

#define MSGQUEUE_SINK(p)   (&(p)->mq_msgsink)

#endif
