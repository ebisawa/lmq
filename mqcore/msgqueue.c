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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libsf/sf.h"
#include "msgqueue.h"

typedef struct {
    unsigned  mmh_len;
} msgqueue_msghdr_t;

static int msgqueue_wnext(msgqueue_t *self);
static int msgqueue_rnext(msgqueue_t *self);
static int msgqueue_total_len(struct iovec *iov, int iovcnt);
static int msgqueue_push_msg(msgqueue_t *self, struct iovec *iov, int iovcnt);

msgqueue_t *
msgqueue_create(size_t queue_size, void (*callback)(void *), void *param)
{
    int i;
    msgqueue_t *mq;
    
    if ((mq = malloc(sizeof(*mq))) == NULL) {
        plog(LOG_ERR, "%s: malloc() failed", __func__);
        return NULL;
    }

    for (i = 0; i < NELEMS(mq->mq_pbuf); i++)
        sf_pbuf_init(&mq->mq_pbuf[i], 0);

    if (sf_pbuf_init(&mq->mq_pbuf[0], queue_size / MSGQUEUE_PBUFS) < 0) {
        free(mq);
        return NULL;
    }

    MSGSINK_INIT(&mq->mq_msgsink, msgqueue_push_msg);
    mq->mq_pbuf_r = 0;
    mq->mq_pbuf_w = 0;
    mq->mq_queue_total_size = queue_size;

    mq->mq_push_callback = callback;
    mq->mq_push_cbparam = param;

    plog(LOG_DEBUG, "%s: create new msgqueue %p, queue_size %zu", __func__, mq, mq->mq_queue_total_size);

    return mq;
}

void
msgqueue_destroy(msgqueue_t *self)
{
    int i;

    plog(LOG_DEBUG, "%s: destroy msgqueue %p", __func__, self);

    for (i = 0; i < NELEMS(self->mq_pbuf); i++) {
        plog(LOG_DEBUG, "%s: pbuf_release %p", __func__, &self->mq_pbuf[i]);
        sf_pbuf_release(&self->mq_pbuf[i]);
    }

    free(self);
}

int
msgqueue_peek(msgqueue_t *self, char **buf, int *len)
{
    sf_pbuf_t *pbuf;
    msgqueue_msghdr_t *header;

redo:
    pbuf = &self->mq_pbuf[self->mq_pbuf_r];
    if (sf_pbuf_data_len(pbuf) < sizeof(*header)) {
        if (msgqueue_rnext(self) < 0)
            return -1;

        goto redo;
    }

    header = (msgqueue_msghdr_t *) sf_pbuf_head(pbuf);
    *len = header->mmh_len;
    *buf = (char *) (header + 1);

    return 0;
}

int
msgqueue_pop_msg(msgqueue_t *self)
{
    sf_pbuf_t *pbuf;
    msgqueue_msghdr_t *header;

redo:
    pbuf = &self->mq_pbuf[self->mq_pbuf_r];
    if (sf_pbuf_data_len(pbuf) < sizeof(*header)) {
        if (msgqueue_rnext(self) < 0)
            return -1;

        goto redo;
    }

    header = (msgqueue_msghdr_t *) sf_pbuf_head(pbuf);
    sf_pbuf_adjust(pbuf, sizeof(msgqueue_msghdr_t) + header->mmh_len);

    return 0;
}

static int
msgqueue_wnext(msgqueue_t *self)
{
    int wnext;

    wnext = (self->mq_pbuf_w + 1) % NELEMS(self->mq_pbuf);

    if (sf_pbuf_buffer_len(&self->mq_pbuf[wnext]) != 0)
        return -1;
    if (sf_pbuf_init(&self->mq_pbuf[wnext], self->mq_queue_total_size / MSGQUEUE_PBUFS) < 0)
        return -1;

    self->mq_pbuf_w = wnext;

    return 0;
}

static int
msgqueue_rnext(msgqueue_t *self)
{
    if (self->mq_pbuf_r == self->mq_pbuf_w)
        return -1;

    sf_pbuf_release(&self->mq_pbuf[self->mq_pbuf_r]);
    self->mq_pbuf_r++;
    self->mq_pbuf_r %= NELEMS(self->mq_pbuf);

    return 0;
}

static int
msgqueue_total_len(struct iovec *iov, int iovcnt)
{
    int i, len = 0;

    for (i = 0; i < iovcnt; i++)
        len += iov[i].iov_len;

    return len;
}

static int
msgqueue_push_msg(msgqueue_t *self, struct iovec *iov, int iovcnt)
{
    int i, total_len;
    sf_pbuf_t *pbuf;
    msgqueue_msghdr_t header;

    plog(LOG_DEBUG, "%s: push message %p", __func__, self);

    total_len = msgqueue_total_len(iov, iovcnt);
    header.mmh_len = total_len;

    if (total_len > self->mq_queue_total_size / MSGQUEUE_PBUFS) {
        plog(LOG_DEBUG, "%s: too big message size", __func__);
        return -1;
    }

redo:
    pbuf = &self->mq_pbuf[self->mq_pbuf_w];
    if (sf_pbuf_write_prepare(pbuf, sizeof(header) + total_len) < 0) {
        if (msgqueue_wnext(self) < 0) {
            plog(LOG_DEBUG, "%s: not enough space", __func__);
            return -1;
        }

        goto redo;
    }

    if (sf_pbuf_write(pbuf, (char *) &header, sizeof(header)) < 0)
        return -1;

    for (i = 0; i < iovcnt; i++) {
        if (sf_pbuf_write(pbuf, iov[i].iov_base, iov[i].iov_len) < 0)
            return -1;
    }

    plog(LOG_DEBUG, "%s: push ok", __func__);

    if (self->mq_push_callback != NULL)
        self->mq_push_callback(self->mq_push_cbparam);

    return 0;
}
