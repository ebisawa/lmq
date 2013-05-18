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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "sf.h"

#define HASH_INITIAL_BASIS   2166136261U   /* 32bit FNV-1 hash */

static sf_session_t *session_find(sf_instance_t *inst, struct sockaddr *addr, uint64_t sid);
static sf_session_t *session_create(sf_instance_t *inst, struct sockaddr *addr, sf_socket_t *sock, uint64_t sid, void *udata);
static int session_hash_create(sf_session_hash_t *seh, int size);
static void session_hash_register(sf_session_hash_t *seh, sf_session_t *session);
static void session_hash_unregister(sf_session_hash_t *seh, sf_session_t *session);
static sf_session_t *session_hash_lookup(sf_session_hash_t *seh, struct sockaddr *sa, uint64_t sid);
static unsigned session_calc_hash(struct sockaddr *sa, uint64_t sid);
static unsigned session_calc_hash_in(struct sockaddr_in *sin);
static unsigned session_calc_hash_in6(struct sockaddr_in6 *sin6);
static unsigned session_calc_hash_buf(void *buf, int len, unsigned basis);
static int session_compare_sockaddr(struct sockaddr *a, struct sockaddr *b);
static int session_compare_sockaddr_in(struct sockaddr_in *a, struct sockaddr_in *b);
static int session_compare_sockaddr_in6(struct sockaddr_in6 *a, struct sockaddr_in6 *b);
static int session_euler_prime(int n);

int
sf_init_session(sf_instance_t *inst)
{
    int size;

    /* XXX */
    inst->inst_sess.sei_max_sessions = 1024;
    size = session_euler_prime(inst->inst_sess.sei_max_sessions);

    if (session_hash_create(&inst->inst_sess.sei_session_hash, size) < 0) {
        plog(LOG_ERR, "%s: session_hash_create() failed", __func__);
        return -1;
    }

    return 0;
}

sf_session_t *
sf_session_create_start(sf_instance_t *inst, struct sockaddr *addr, sf_socket_t *sock, void *udata)
{
    uint64_t sid;
    sf_session_t *session;

    if (sf_proto_sid(inst, &sid, sock) < 0)
        sid = 0;

    if ((session = session_create(inst, addr, sock, sid, udata)) == NULL) {
        plog(LOG_ERR, "%s: session_create() failed", __func__);
        return NULL;
    }

    if (sf_proto_start(inst, session) < 0) {
        plog(LOG_ERR, "%s: sf_proto_start() failed", __func__);
        sf_session_destroy(inst, session);
        return NULL;
    }

    return session;
}

sf_session_t *
sf_session_find_create_start(sf_instance_t *inst, struct sockaddr *addr, sf_socket_t *sock, void *udata)
{
    uint64_t sid;
    sf_session_t *session;

    if (sf_proto_sid(inst, &sid, sock) < 0)
        sid = 0;

    if ((session = session_find(inst, addr, sid)) == NULL) {
        if ((session = sf_session_create_start(inst, addr, sock, udata)) == NULL)
            return NULL;
    }

    return session;
}

void
sf_session_destroy(sf_instance_t *inst, sf_session_t *session)
{
    sf_session_inst_t *sei = &inst->inst_sess;

    if (session == NULL)
        return;

    plog(LOG_DEBUG, "%s: destroy session %p", __func__, session);

    sf_timer_cancel(inst, &session->se_timer);
    sf_proto_end(inst, session);

    session_hash_unregister(&inst->inst_sess.sei_session_hash, session);
    free(session);

    sei->sei_session_count--;
}

int
sf_session_estlen(sf_instance_t *inst, sf_session_t *session, sf_pbuf_t *pbuf)
{
    return sf_proto_estlen(inst, session, pbuf);
}

int
sf_session_input(sf_instance_t *inst, sf_session_t *session, sf_pbuf_t *pbuf)
{
    int msglen;

    while ((msglen = sf_proto_msglen(inst, session, pbuf)) > 0) {
        if (sf_proto_input(inst, session, pbuf, msglen) < 0)
            plog(LOG_ERR, "%s: sf_proto_input() failed", __func__);

        sf_pbuf_adjust(pbuf, msglen);
    }

    return 0;
}

int
sf_session_output(sf_instance_t *inst, sf_session_t *session)
{
    return sf_proto_output(inst, session);
}

int
sf_session_output_bcast(sf_instance_t *inst, void *sock)
{
    int i;
    sf_session_t *session;
    sf_session_hash_t *seh = &inst->inst_sess.sei_session_hash;

    for (i = 0; i < seh->seh_size; i++) {
        if (seh->seh_table[i] != NULL) {
            for (session = seh->seh_table[i]; session != NULL; ) {
                if (session->se_sock == sock) {
                    if (sf_session_output(inst, session) < 0)
                        return -1;
                }

                session = session->se_hash_next;
            }
        }
    }

    return 0;
}

int
sf_session_timeout(sf_instance_t *inst, sf_session_t *session)
{
    return sf_proto_timeout(inst, session);
}

static sf_session_t *
session_find(sf_instance_t *inst, struct sockaddr *addr, uint64_t sid)
{
    return session_hash_lookup(&inst->inst_sess.sei_session_hash, addr, sid);
}

static sf_session_t *
session_create(sf_instance_t *inst, struct sockaddr *addr, sf_socket_t *sock, uint64_t sid, void *udata)
{
    sf_session_t *session;
    sf_session_inst_t *sei = &inst->inst_sess;

    if (sei->sei_session_count >= sei->sei_max_sessions) {
        plog(LOG_ERR, "%s: too many sessions", __func__);
        return NULL;
    }

    if ((session = (sf_session_t *) calloc(1, sizeof(sf_session_t))) == NULL) {
        plog_error(LOG_ERR, "%s: calloc() failed", __func__);
        return NULL;
    }

    memcpy(&session->se_peer, addr, SALEN(addr));
    session->se_sf.sf_inst = inst;
    session->se_sf.sf_sess = session;
    session->se_sock = sock;
    session->se_sid = sid;
    session->se_udata = udata;

    session_hash_register(&sei->sei_session_hash, session);
    sei->sei_session_count++;

    plog(LOG_DEBUG, "%s: create new session %p (%d)", __func__, session, sei->sei_session_count);

    return session;
}

static int
session_hash_create(sf_session_hash_t *seh, int size)
{
    seh->seh_size = size;

    if ((seh->seh_table = (sf_session_t **) calloc(1, sizeof(sf_session_t *) * size)) == NULL) {
        plog_error(LOG_ERR, "%s: calloc() failed", __func__);
        return -1;
    }

    plog(LOG_DEBUG, "%s: create hash table (%d entries)", __func__, size);

    return 0;
}

static void
session_hash_register(sf_session_hash_t *seh, sf_session_t *session)
{
    sf_session_t *next;
    unsigned hash, index;

    hash = session_calc_hash((struct sockaddr *) &session->se_peer, session->se_sid);
    index = hash % seh->seh_size;

    if ((next = seh->seh_table[index]) != NULL)
        next->se_hash_prev = session;

    session->se_hash_next = next;
    session->se_hash_prev = NULL;
    seh->seh_table[index] = session;

    plog(LOG_DEBUG, "%s: register %p, hash %u", __func__, session, index);
}

static void
session_hash_unregister(sf_session_hash_t *seh, sf_session_t *session)
{
    unsigned hash, index;

    hash = session_calc_hash((struct sockaddr *) &session->se_peer, session->se_sid);
    index = hash % seh->seh_size;

    if (session->se_hash_prev != NULL)
        session->se_hash_prev->se_hash_next = session->se_hash_next;
    if (session->se_hash_next != NULL)
        session->se_hash_next->se_hash_prev = session->se_hash_prev;

    if (seh->seh_table[index] == session)
        seh->seh_table[index] = session->se_hash_next;

    plog(LOG_DEBUG, "%s: unregister %p, hash %u", __func__, session, index);
}

static sf_session_t *
session_hash_lookup(sf_session_hash_t *seh, struct sockaddr *sa, uint64_t sid)
{
    sf_session_t *s;
    unsigned hash, index;

    hash = session_calc_hash(sa, sid);
    index = hash % seh->seh_size;

    for (s = seh->seh_table[index]; s != NULL; s = s->se_hash_next) {
        if (s->se_sid == sid) {
            if (session_compare_sockaddr((struct sockaddr *) &s->se_peer, sa) == 0)
                return s;
        }
    }

    return NULL;
}

static unsigned
session_calc_hash(struct sockaddr *sa, uint64_t sid)
{
    unsigned value;

    switch (sa->sa_family) {
    case AF_INET:
        value = session_calc_hash_in((struct sockaddr_in *) sa);
        break;
    case AF_INET6:
        value = session_calc_hash_in6((struct sockaddr_in6 *) sa);
        break;
    default:
        return 0;
    }

    return session_calc_hash_buf(&sid, sizeof(sid), value);
}

static unsigned
session_calc_hash_in(struct sockaddr_in *sin)
{
    unsigned value = HASH_INITIAL_BASIS;

    value = session_calc_hash_buf(&sin->sin_family, sizeof(sin->sin_family), value);
    value = session_calc_hash_buf(&sin->sin_port, sizeof(sin->sin_port), value);
    value = session_calc_hash_buf(&sin->sin_addr, sizeof(sin->sin_addr), value);

    return value;
}

static unsigned
session_calc_hash_in6(struct sockaddr_in6 *sin6)
{
    unsigned value = HASH_INITIAL_BASIS;

    value = session_calc_hash_buf(&sin6->sin6_family, sizeof(sin6->sin6_family), value);
    value = session_calc_hash_buf(&sin6->sin6_port, sizeof(sin6->sin6_port), value);
    value = session_calc_hash_buf(&sin6->sin6_addr, sizeof(sin6->sin6_addr), value);

    return value;
}

static unsigned
session_calc_hash_buf(void *buf, int len, unsigned basis)
{
    int i;
    unsigned hash = basis;

    /* FNV-1a hash */
    for (i = 0; i < len; i++) {
        hash ^= *((unsigned char *) buf);
        hash *= 16777619;
        buf = ((unsigned char *) buf) + 1;
    }

    return hash;
}

static int
session_compare_sockaddr(struct sockaddr *a, struct sockaddr *b)
{
    switch (a->sa_family) {
    case AF_INET:
        return session_compare_sockaddr_in((struct sockaddr_in *) a, (struct sockaddr_in *) b);
    case AF_INET6:
        return session_compare_sockaddr_in6((struct sockaddr_in6 *) a, (struct sockaddr_in6 *) b);
    }

    return -1;
}

static int
session_compare_sockaddr_in(struct sockaddr_in *a, struct sockaddr_in *b)
{
    int r;

    if ((r = a->sin_family - b->sin_family) != 0)
        return r;
    if ((r = a->sin_port - b->sin_port) != 0)
        return r;
    if ((r = memcmp(&a->sin_addr, &b->sin_addr, sizeof(a->sin_addr))) != 0)
        return r;

    return 0;
}

static int
session_compare_sockaddr_in6(struct sockaddr_in6 *a, struct sockaddr_in6 *b)
{
    int r;

    if ((r = a->sin6_family - b->sin6_family) != 0)
        return r;
    if ((r = a->sin6_port - b->sin6_port) != 0)
        return r;
    if ((r = memcmp(&a->sin6_addr, &b->sin6_addr, sizeof(a->sin6_addr))) != 0)
        return r;

    return 0;
}

static int
session_euler_prime(int n)
{
    int i, value;

    for (i = 0; i < 1000; i++) {
        value = (i * i) + i + 41;
        if (value > n)
            return value;
    }

    return n;
}
