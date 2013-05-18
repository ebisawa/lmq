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
#include "binding.h"
#include "binding_hash.h"

static binding_t *binding_create(int size, char *name, msgsink_push_msg_t *push_msg);
static int binding_subscribe_register(binding_t *bi, msgsink_t *sink);
static int binding_extend(binding_t *bi);
static int binding_topic_push_msg(binding_topic_t *self, struct iovec *iov, int iovcnt);
static int binding_queue_push_msg(binding_queue_t *self, struct iovec *iov, int iovcnt);

binding_t *
binding_topic_create(char *name, msgsink_t *sink)
{
    binding_t *bi;

    if ((bi = binding_create(sizeof(binding_topic_t), name,
                             (msgsink_push_msg_t *) binding_topic_push_msg)) == NULL) {
        plog(LOG_ERR, "%s: binding_create() failed", __func__);
        return NULL;
    }

    if (binding_subscribe_register(bi, sink) < 0) {
        plog(LOG_ERR, "%s: binding_subscribe_register() failed", __func__);
        free(bi);
        return NULL;
    }

    plog(LOG_DEBUG, "%s: create topic binding %p, name \"%s\"", __func__, bi, name);

    return bi;
}

binding_t *
binding_queue_create(char *name, msgsink_t *sink)
{
    binding_t *bi;

    if ((bi = binding_create(sizeof(binding_queue_t), name,
                             (msgsink_push_msg_t *) binding_queue_push_msg)) == NULL) {
        plog(LOG_ERR, "%s: binding_create() failed", __func__);
        return NULL;
    }

    if (binding_subscribe_register(bi, sink) < 0) {
        plog(LOG_ERR, "%s: binding_subscribe_register() failed", __func__);
        free(bi);
        return NULL;
    }

    plog(LOG_DEBUG, "%s: create queue binding %p, name \"%s\"", __func__, bi, name);

    return bi;
}

void
binding_destroy(binding_t *bi)
{
    binding_hash_unregister(&BindingHash, bi->bi_name);
    free(bi->bi_members);
    free(bi);

    plog(LOG_DEBUG, "%s: destroy binding %p", __func__, bi);
}

int
binding_subscribe(binding_t *bi, msgsink_t *sink)
{
    int i;

    if (bi->bi_members_count == bi->bi_members_max) {
        plog(LOG_DEBUG, "%s: binding %p is full", __func__, bi);

        if (binding_extend(bi) < 0) {
            plog(LOG_ERR, "%s: binding_extend() failed", __func__);
            return -1;
        }
    }

    for (i = 0; i < bi->bi_members_max; i++) {
        if (bi->bi_members[i] == NULL) {
            plog(LOG_DEBUG, "%s: subscribe binding %p, msgsink %p", __func__, bi, sink);
            bi->bi_members[i] = sink;
            bi->bi_members_count++;
            return 0;
        }
    }

    plog(LOG_ERR, "%s: subscribing binding %p failed", __func__, bi);

    return -1;
}

int
binding_unsubscribe(binding_t *bi, msgsink_t *sink)
{
    int i;

    plog(LOG_DEBUG, "%s: unsubscribe binding %p, msgsink %p", __func__, bi, sink);

    for (i = 0; i < bi->bi_members_max; i++) {
        if (bi->bi_members[i] == sink) {
            bi->bi_members[i] = NULL;
            bi->bi_members_count--;

            if (bi->bi_members_count == 0)
                binding_destroy(bi);
        }
    }

    return 0;
}

int
binding_push_msg(binding_t *self, struct iovec *iov, int iovcnt)
{
    plog(LOG_DEBUG, "%s: push message", __func__);

    return self->bi_msgsink.ms_push_msg(self, iov, iovcnt);
}

static binding_t *
binding_create(int size, char *name, msgsink_push_msg_t *push_msg)
{
    binding_t *bi;

    if ((bi = calloc(1, size)) == NULL) {
        plog(LOG_ERR, "%s: calloc() failed", __func__);
        return NULL;
    }

    if ((bi->bi_members = calloc(1, sizeof(msgsink_t *) * BINDING_MEMBERS_MAX)) == NULL) {
        plog(LOG_ERR, "%s: calloc() failed", __func__);
        free(bi);
        return NULL;
    }

    MSGSINK_INIT(&bi->bi_msgsink, push_msg);
    strncpy(bi->bi_name, name, sizeof(bi->bi_name));
    bi->bi_members_max = BINDING_MEMBERS_MAX;

    return bi;
}

static int
binding_subscribe_register(binding_t *bi, msgsink_t *sink)
{
    if (binding_subscribe(bi, sink) < 0) {
        plog(LOG_ERR, "%s: binding_subscribe() failed", __func__);
        return -1;
    }

    binding_hash_register(&BindingHash, bi);

    return 0;
}

static int
binding_extend(binding_t *bi)
{
    int mmax;
    msgsink_t **newp;

    mmax = bi->bi_members_max;
    mmax += mmax / 2;

    plog(LOG_DEBUG, "%s: extend binding members: %d -> %d", __func__, bi->bi_members_max, mmax);

    if ((newp = realloc(bi->bi_members, sizeof(msgsink_t *) * mmax)) == NULL) {
        plog_error(LOG_ERR, __func__, "binding_subscribe() failed");
        return -1;
    }

    plog(LOG_DEBUG, "%s: old members = %p, new = %p", __func__, bi->bi_members, newp);

    memset(&newp[bi->bi_members_max], 0, mmax - bi->bi_members_max);
    bi->bi_members_max = mmax;
    bi->bi_members = newp;

    return 0;
}

static int
binding_topic_push_msg(binding_topic_t *self, struct iovec *iov, int iovcnt)
{
    int i, errors = 0;
    msgsink_t *sink;

    for (i = 0; i < self->bit_binding.bi_members_max; i++) {
        if ((sink = self->bit_binding.bi_members[i]) == NULL)
            continue;
        if (sink->ms_push_msg(sink, iov, iovcnt) < 0)
            errors++;
    }

    return (errors > 0) ? -1 : 0;
}

static int
binding_queue_push_msg(binding_queue_t *self, struct iovec *iov, int iovcnt)
{
    int i, index, members;
    msgsink_t *sink;

    members = self->biq_binding.bi_members_max;
    for (i = 0; i < members; i++) {
        index = self->biq_round;
        self->biq_round = (self->biq_round + 1) % members;

        if ((sink = self->biq_binding.bi_members[index]) == NULL)
            continue;

        return sink->ms_push_msg(sink, iov, iovcnt);
    }

    return -1;
}
