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
#ifndef __SF_SESSION_H__
#define __SF_SESSION_H__

typedef struct {
    int                 seh_size;
    sf_session_t      **seh_table;
} sf_session_hash_t;

typedef struct {
    int                 sei_max_sessions;
    int                 sei_session_count;
    sf_session_hash_t   sei_session_hash;
} sf_session_inst_t;

struct sf_session {
    uint64_t            se_sid;
    sf_sockaddr_t       se_peer;
    sf_socket_t        *se_sock;
    sf_timer_t          se_timer;
    sf_t                se_sf;
    void               *se_udata;
    sf_session_t       *se_hash_prev;
    sf_session_t       *se_hash_next;
};

int sf_init_session(sf_instance_t *inst);
sf_session_t *sf_session_create_start(sf_instance_t *inst, struct sockaddr *addr, sf_socket_t *sock, void *udata);
sf_session_t *sf_session_find_create_start(sf_instance_t *inst, struct sockaddr *addr, sf_socket_t *sock, void *udata);
void sf_session_destroy(sf_instance_t *inst, sf_session_t *session);
int sf_session_estlen(sf_instance_t *inst, sf_session_t *session, sf_pbuf_t *pbuf);
int sf_session_input(sf_instance_t *inst, sf_session_t *session, sf_pbuf_t *pbuf);
int sf_session_output(sf_instance_t *inst, sf_session_t *session);
int sf_session_output_bcast(sf_instance_t *inst, void *sock);
int sf_session_timeout(sf_instance_t *inst, sf_session_t *session);

#endif
