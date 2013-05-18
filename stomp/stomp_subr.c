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
#include "mqcore/mqcore.h"
#include "stomp_subr.h"

typedef struct {
    int           ss_state;
    binding_t    *ss_bind;
    msgqueue_t   *ss_msgq;
    char         *ss_sbuf;
    int           ss_slen;
} stomp_data_t;

static binding_t *stomp_new_binding(char *dest, msgsink_t *sink);
static void stomp_push_notify(void *param);

int
stomp_create_session(sf_t *sf)
{
    stomp_data_t *ss;

    if (sf_get_udata(sf) == NULL) {
        if ((ss = (stomp_data_t *) calloc(1, sizeof(stomp_data_t))) == NULL) {
            plog(LOG_DEBUG, "%s: calloc() failed", __func__);
            return -1;
        }

        sf_set_udata(sf, ss);
    }

    return 0;
}

void
stomp_destroy_session(sf_t *sf)
{
    stomp_data_t *ss;

    if ((ss = (stomp_data_t *) sf_get_udata(sf)) == NULL)
        return;

    if (ss->ss_bind != NULL) {
        binding_unsubscribe(ss->ss_bind, MSGQUEUE_SINK(ss->ss_msgq));
        ss->ss_bind = NULL;
    }

    if (ss->ss_msgq != NULL) {
        msgqueue_destroy(ss->ss_msgq);
        ss->ss_msgq = NULL;
    }

    sf_set_udata(sf, NULL);
    free(ss);
}

int
stomp_get_state(sf_t *sf)
{
    stomp_data_t *ss;

    ss = (stomp_data_t *) sf_get_udata(sf);
    return (ss == NULL) ? -1 : ss->ss_state;
}

void
stomp_set_state(sf_t *sf, int state)
{
    stomp_data_t *ss;

    ss = (stomp_data_t *) sf_get_udata(sf);
    ss->ss_state = state;
}

int
stomp_subscribe(sf_t *sf, char *dest)
{
    binding_t *bi;
    msgqueue_t *mq;
    stomp_data_t *ss;

    if ((ss = (stomp_data_t *) sf_get_udata(sf)) == NULL) {
        plog(LOG_ERR, "%s: udata == NULL. why?", __func__);
        return -1;
    }

    if (ss->ss_bind != NULL) {
        plog(LOG_ERR, "%s: XXX only one subscription is allowed per connection", __func__);
        return -1;
    }

    if ((mq = ss->ss_msgq) == NULL) {
        if ((mq = msgqueue_create(1024 * 1024 * 8, /* XXX */
                                  stomp_push_notify, sf)) == NULL) {
            plog(LOG_ERR, "%s: msgqueue_create() failed", __func__);
            return -1;
        }
    }

    if ((bi = binding_hash_lookup(&BindingHash, dest)) != NULL) {
        if (binding_subscribe(bi, MSGQUEUE_SINK(mq)) < 0) {
            plog(LOG_ERR, "%s: can't subscribe binding \"%s\"", __func__, dest);
            msgqueue_destroy(mq);
            ss->ss_msgq = NULL;
            return -1;
        }
    } else {
        if ((bi = stomp_new_binding(dest, MSGQUEUE_SINK(mq))) == NULL) {
            plog(LOG_ERR, "%s: can't create new binding \"%s\"", __func__, dest);
            msgqueue_destroy(mq);
            ss->ss_msgq = NULL;
            return -1;
        }
    }

    ss->ss_bind = bi;
    ss->ss_msgq = mq;

    return 0;
}

int
stomp_unsubscribe(sf_t *sf, char *dest)
{
    binding_t *bi;
    stomp_data_t *ss;

    if ((ss = (stomp_data_t *) sf_get_udata(sf)) == NULL) {
        plog(LOG_ERR, "%s: udata == NULL. why?", __func__);
        return -1;
    }

    if ((bi = binding_hash_lookup(&BindingHash, dest)) == NULL) {
        plog(LOG_DEBUG, "%s: can't find binding \"%s\"", __func__, dest);
        return 0;   /* silent discard */
    }

    if (ss->ss_bind != bi) {
        plog(LOG_DEBUG, "%s: binding \"%s\" is not bound for this session", __func__, dest);
        return 0;   /* silent discard */
    }

    if (ss->ss_msgq == NULL) {
        plog(LOG_DEBUG, "%s: this session has no message queue", __func__);
        return 0;   /* silent discard */
    }

    binding_unsubscribe(bi, MSGQUEUE_SINK(ss->ss_msgq));
    ss->ss_bind = NULL;

    return 0;
}

int
stomp_enqueue(sf_t *sf, char *dest, struct iovec *iov, int iovcnt)
{
    binding_t *bi;

    if ((bi = binding_hash_lookup(&BindingHash, dest)) == NULL) {
        plog(LOG_DEBUG, "%s: discard message due to no binding found", __func__);
        return 0;   /* silent discard */
    }

    if (binding_push_msg(bi, iov, iovcnt) < 0) {
        plog(LOG_DEBUG, "%s: binding_push_msg() failed", __func__);
        return 0;   /* silent discard */
    }

    return 0;
}

int
stomp_send_resume(sf_t *sf)
{
    int sent_len;
    stomp_data_t *ss;

    if ((ss = (stomp_data_t *) sf_get_udata(sf)) == NULL)
        return -1;

    if (ss->ss_msgq == NULL) {
        plog(LOG_ERR, "%s: no msgq. why?", __func__);
        return -1;
    }

    plog(LOG_DEBUG, "%s: msgq = %p", __func__, ss->ss_msgq);

    if (ss->ss_slen == 0) {
        if (msgqueue_peek(ss->ss_msgq, &ss->ss_sbuf, &ss->ss_slen) < 0) {
            plog(LOG_DEBUG, "%s: queue empty", __func__);
            return 0;
        }
    }

    if (ss->ss_slen > 0) {
        plog(LOG_DEBUG, "%s: [output] len = %d, buf = >>>\n%s<<<", __func__, ss->ss_slen, ss->ss_sbuf);

        if ((sent_len = sf_send(sf, ss->ss_sbuf, ss->ss_slen)) < 0) {
            plog(LOG_ERR, "%s: sf_send() failed", __func__);
            return -1;
        }

        ss->ss_sbuf += sent_len;
        ss->ss_slen -= sent_len;

        if (ss->ss_slen == 0)
            msgqueue_pop_msg(ss->ss_msgq);
    }

    return 0;
}

static binding_t *
stomp_new_binding(char *dest, msgsink_t *sink)
{
    binding_t *bi;

    if (strncmp(dest, "/queue/", 7) == 0) {
        if ((bi = binding_queue_create(dest, sink)) == NULL) {
            plog(LOG_ERR, "%s: binding_queue_create() failed", __func__);
            return NULL;
        }

        return bi;
    }

    if (strncmp(dest, "/topic/", 7) == 0) {
        if ((bi = binding_topic_create(dest, sink)) == NULL) {
            plog(LOG_ERR, "%s: binding_topic_create() failed", __func__);
            return NULL;
        }

        return bi;
    }

    return NULL;
}

static void
stomp_push_notify(void *param)
{
    stomp_send_resume((sf_t *) param);
}
