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
#ifndef __SF_SOCKET_H__
#define __SF_SOCKET_H__

#define SOCK_DONTCLOSE   0x0001
#define SOCK_CONNECTED   0x0002

typedef struct {
    int               sb_fd;
    sf_protocb_t     *sb_pcb;
    int             (*sb_func_read)(sf_instance_t *inst, void *sock);
    int             (*sb_func_write)(sf_instance_t *inst, void *sock);
} sf_socket_base_t;

typedef struct {
    sf_socket_base_t  so_base;
    sf_pbuf_small_t   so_rbuf;
    sf_sockaddr_t     so_last_from;
    void             *so_session;   /* sf_session_t */
    unsigned          so_flags;
} sf_socket_t;

typedef struct {
    int               soi_sock_count;
    int               soi_max_sockets;
    size_t            soi_max_msgsize;
} sf_socket_inst_t;

int sf_init_socket(sf_instance_t *inst);
int sf_socket_tcp_listen(sf_instance_t *inst, struct sockaddr *addr, sf_protocb_t *pcb);
int sf_socket_tcp_connect(sf_instance_t *inst, struct sockaddr *addr, sf_protocb_t *pcb, void *udata);
sf_socket_t *sf_socket_udp_listen(sf_instance_t *inst, struct sockaddr *addr, sf_protocb_t *pcb);
sf_socket_t *sf_socket_udp_connect(sf_instance_t *inst, struct sockaddr *addr, sf_protocb_t *pcb, void *udata);
int sf_socket_udp_mcast_join(sf_instance_t *inst, sf_socket_t *sock, struct sockaddr *addr, char *ifname);
int sf_socket_udp_mcast_sendif(sf_instance_t *inst, sf_socket_t *sock, char *ifname);
int sf_socket_send(sf_instance_t *inst, sf_socket_t *sock, struct sockaddr *to, char *buf, int len);
void sf_socket_destroy(sf_instance_t *inst, sf_socket_t *sock);

void sf_socket_read_event(sf_instance_t *inst, void *sock);
void sf_socket_write_event(sf_instance_t *inst, void *sock);

int sf_socket_poll_create(void);
int sf_socket_poll_add(int poll_fd, int fd, void *sock);
int sf_socket_poll_wait(sf_instance_t *inst, int poll_fd, struct timeval *timeout);

#endif
