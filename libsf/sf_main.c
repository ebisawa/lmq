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
#include "sf.h"

static void destroy_session(sf_instance_t *inst, sf_session_t *session);

int
sf_init(sf_instance_t *inst)
{
    if (sf_init_pbuf() < 0)
        return -1;
    if (sf_init_socket(inst) < 0)
        return -1;
    if (sf_init_session(inst) < 0)
        return -1;

    if ((inst->inst_fd_poll = sf_socket_poll_create()) < 0) {
        plog_error(LOG_ERR, "%s: socket_poll_create() failed", __func__);
        return -1;
    }

    return 0;
}

int
sf_tcp_listen(sf_instance_t *inst, struct sockaddr *addr, sf_protocb_t *pcb)
{
    return sf_socket_tcp_listen(inst, addr, pcb);
}

int
sf_tcp_connect(sf_instance_t *inst, struct sockaddr *addr, sf_protocb_t *pcb, void *udata)
{
    return sf_socket_tcp_connect(inst, addr, pcb, udata);
}

void *
sf_udp_listen(sf_instance_t *inst, struct sockaddr *addr, sf_protocb_t *pcb)
{
    return sf_socket_udp_listen(inst, addr, pcb);
}

void *
sf_udp_connect(sf_instance_t *inst, struct sockaddr *addr, sf_protocb_t *pcb, void *udata)
{
    return sf_socket_udp_connect(inst, addr, pcb, udata);
}

int
sf_udp_mcast_join(sf_instance_t *inst, void *sock, struct sockaddr *addr, char *ifname)
{
    return sf_socket_udp_mcast_join(inst, (sf_socket_t *) sock, addr, ifname);
}

int
sf_udp_mcast_sendif(sf_instance_t *inst, void *sock, char *ifname)
{
    return sf_socket_udp_mcast_sendif(inst, (sf_socket_t *) sock, ifname);
}

void
sf_main(sf_instance_t *inst)
{
    struct timeval tv, *t;

    for (;;) {
        t = (sf_timer_timetonext(inst, &tv) < 0) ? NULL : &tv;
        sf_socket_poll_wait(inst, inst->inst_fd_poll, t);
        sf_timer_execute(inst);
    }
}

int
sf_send(sf_t *sf, char *buf, int len)
{
    sf_session_t *session;

    session = sf->sf_sess;
    return sf_socket_send(sf->sf_inst, session->se_sock,
                          (struct sockaddr *) &session->se_peer, buf, len);
}

int
sf_set_timeout(sf_t *sf, int msec)
{
    return sf_timer_request(sf->sf_inst, &sf->sf_sess->se_timer,
                            msec, (sf_timer_func_t *) sf_session_timeout,
                            sf->sf_inst, sf->sf_sess);
}

void *
sf_get_udata(sf_t *sf)
{
    return sf->sf_sess->se_udata;
}

void
sf_set_udata(sf_t *sf, void *udata)
{
    sf->sf_sess->se_udata = udata;
}

void
sf_get_peer(sf_t *sf, sf_sockaddr_t *peer)
{
    sf_socket_t *sock;

    sock = sf->sf_sess->se_sock;
    memcpy(peer, &sock->so_last_from, SALEN(&sock->so_last_from));
}

void
sf_close_session(sf_t *sf)
{
    sf_timer_cancel(sf->sf_inst, &sf->sf_sess->se_timer);
    sf_timer_request(sf->sf_inst, &sf->sf_sess->se_timer, 0,
                     (sf_timer_func_t *) destroy_session, sf->sf_inst, sf->sf_sess);
}

static void
destroy_session(sf_instance_t *inst, sf_session_t *session)
{
    sf_session_destroy(inst, session);
    sf_socket_destroy(inst, session->se_sock);
}
