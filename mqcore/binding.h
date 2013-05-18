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
#ifndef BINDING_H
#define BINDING_H
#include "msgsink.h"

#define BINDING_NAME_MAX     64
#define BINDING_MEMBERS_MAX   8

#define BINDING_SINK(p)   (&((binding_t *) (p))->bi_msgsink)

typedef struct binding binding_t;

struct binding {
    msgsink_t    bi_msgsink;
    char         bi_name[BINDING_NAME_MAX];
    int          bi_members_max;
    int          bi_members_count;
    msgsink_t  **bi_members;
    binding_t   *bi_hash_next;
    binding_t   *bi_hash_prev;
};

typedef struct {
    binding_t    bit_binding;
} binding_topic_t;

typedef struct {
    binding_t    biq_binding;
    int          biq_round;
} binding_queue_t;

binding_t *binding_topic_create(char *name, msgsink_t *sink);
binding_t *binding_queue_create(char *name, msgsink_t *sink);
void binding_destroy(binding_t *bi);
int binding_subscribe(binding_t *bi, msgsink_t *sink);
int binding_unsubscribe(binding_t *bi, msgsink_t *sink);
int binding_push_msg(binding_t *bi, struct iovec *iov, int iovcnt);

#endif
