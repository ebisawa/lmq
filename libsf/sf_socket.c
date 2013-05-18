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
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "sf.h"

static sf_socket_base_t *socket_tcp(sf_instance_t *inst, sf_protocb_t *pcb);
static int socket_tcp_accept(sf_instance_t *inst, int fd, sf_protocb_t *pcb);
static int socket_tcp_session(sf_instance_t *inst, int new_fd, struct sockaddr *addr, sf_protocb_t *pcb, void *udata);
static sf_socket_t *socket_udp(sf_instance_t *inst, sf_protocb_t *pcb);
static sf_socket_t *socket_create(sf_instance_t *inst, int fd, sf_protocb_t *pcb);
static sf_socket_base_t *socket_create_base(sf_instance_t *inst, int fd, sf_protocb_t *pcb);
static int socket_bind(int fd, struct sockaddr *addr);
static int socket_listen(int fd, struct sockaddr *addr);
static int socket_nonblock(int fd);
static int socket_keepalive(int fd);
static int socket_read_event_accept(sf_instance_t *inst, void *sock);
static int socket_read_event_receive(sf_instance_t *inst, void *sock);
static int socket_write_event(sf_instance_t *inst, void *sock);
static int socket_receive(sf_instance_t *inst, sf_socket_t *sock);
static int socket_do_receive(sf_instance_t *inst, sf_socket_t *sock, struct sockaddr *from, socklen_t from_len, int flags);
static int socket_do_receive2(sf_instance_t *inst, sf_socket_t *sock, struct sockaddr *from, socklen_t from_len, char *buf, int bufmax, int flags);
static sf_session_t *socket_get_session(sf_instance_t *inst, sf_socket_t *sock, struct sockaddr *from);
static int socket_extend_rbuf(sf_instance_t *inst, sf_socket_t *sock, int new_len);
static void socket_shrink_rbuf(sf_socket_t *sock);

int
sf_init_socket(sf_instance_t *inst)
{
    sf_socket_inst_t *soi = &inst->inst_sock;

    /* XXX configure */
    memset(soi, 0, sizeof(*soi));
    soi->soi_max_sockets = 64;
    soi->soi_max_msgsize = 1024 * 1024;

    return 0;
}

int
sf_socket_tcp_listen(sf_instance_t *inst, struct sockaddr *addr, sf_protocb_t *pcb)
{
    sf_socket_base_t *sb;

    if ((sb = socket_tcp(inst, pcb)) == NULL)
        return -1;

    if (socket_listen(sb->sb_fd, addr) < 0) {
        close(sb->sb_fd);
        free(sb);
        return -1;
    }

    return 0;
}

int
sf_socket_tcp_connect(sf_instance_t *inst, struct sockaddr *addr, sf_protocb_t *pcb, void *udata)
{
#if 0
    int fd;

    if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        plog_error(LOG_ERR, "%s: socket() failed", __func__);
        return -1;
    }

    /* XXX new socket */

    /* XXX connect */

    if (socket_tcp_session(inst, fd, addr, udata) < 0) {
        plog(LOG_ERR, "%s: socket_tcp_sessoin() failed");
        /* socket is closed by socket_tcp_session() */
        return -1;
    }
#endif

    return -1;
}

sf_socket_t *
sf_socket_udp_listen(sf_instance_t *inst, struct sockaddr *addr, sf_protocb_t *pcb)
{
    sf_socket_t *sock;

    if ((sock = socket_udp(inst, pcb)) == NULL)
        return NULL;
    if (socket_bind(sock->so_base.sb_fd, addr) < 0) {
        sf_socket_destroy(inst, sock);
        return NULL;
    }

    sock->so_flags |= SOCK_DONTCLOSE;
    return sock;
}

sf_socket_t *
sf_socket_udp_connect(sf_instance_t *inst, struct sockaddr *addr, sf_protocb_t *pcb, void *udata)
{
    sf_socket_t *sock;
    sf_session_t *session;

    if ((sock = socket_udp(inst, pcb)) == NULL)
        return NULL;
    if ((session = sf_session_find_create_start(inst, addr, sock, udata)) == NULL) {
        sf_socket_destroy(inst, sock);
        return NULL;
    }

    sock->so_session = session;
    return sock;
}

int
sf_socket_udp_mcast_join(sf_instance_t *inst, sf_socket_t *sock, struct sockaddr *addr, char *ifname)
{
    struct ip_mreqn mreq;
    struct sockaddr_in *sin = (struct sockaddr_in *) addr;

    memset(&mreq, 0, sizeof(mreq));
    memcpy(&mreq.imr_multiaddr, &sin->sin_addr, sizeof(mreq.imr_multiaddr));

    if (ifname != NULL)
        mreq.imr_ifindex = if_nametoindex(ifname);

    if (setsockopt(sock->so_base.sb_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        plog_error(LOG_ERR, "%s: setsockopt(IP_ADD_MEMBERSHIP) failed", __func__);
        return -1;
    }

    return 0;
}

int
sf_socket_udp_mcast_sendif(sf_instance_t *inst, sf_socket_t *sock, char *ifname)
{
    struct sockaddr_in ifaddr;

    if (sf_util_ifnametoaddr(&ifaddr, ifname) < 0)
        return -1;

    if (setsockopt(sock->so_base.sb_fd, IPPROTO_IP, IP_MULTICAST_IF,
                   &ifaddr.sin_addr, sizeof(ifaddr.sin_addr)) < 0) {
        plog_error(LOG_ERR, "%s: setsockopt(IP_MULTICAST_IF) failed", __func__);
        return -1;
    }

    return 0;
}

int
sf_socket_send(sf_instance_t *inst, sf_socket_t *sock, struct sockaddr *to, char *buf, int len)
{
    int sent_len;

    if (len == 0)
        return 0;

    if (sock->so_flags & SOCK_CONNECTED)
        sent_len = send(sock->so_base.sb_fd, buf, len, 0);
    else
        sent_len = sendto(sock->so_base.sb_fd, buf, len, 0, to, SALEN(to));

    if (sent_len < 0) {
        if (errno == EAGAIN)
            return 0;
        else {
            plog_error(LOG_ERR, "%s: send() failed", __func__);
            return -1;
        }
    }

    return sent_len;
}

void
sf_socket_destroy(sf_instance_t *inst, sf_socket_t *sock)
{
    if (sock->so_flags & SOCK_DONTCLOSE) {
        plog(LOG_DEBUG, "%s: don't close socket %p (fd %d)", __func__, sock, sock->so_base.sb_fd);
        return;
    }

    plog(LOG_DEBUG, "%s: destroy socket %p (fd %d)", __func__, sock, sock->so_base.sb_fd);

    sf_pbuf_release((sf_pbuf_t *) &sock->so_rbuf);
    sf_pbuf_init_small(&sock->so_rbuf);

    close(sock->so_base.sb_fd);
    free(sock);

    inst->inst_sock.soi_sock_count--;
}

void
sf_socket_read_event(sf_instance_t *inst, void *sock)
{
    sf_socket_base_t *sb = (sf_socket_base_t *) sock;

    for (;;) {
        if (sb->sb_func_read(inst, sock) < 0)
            break;
    }
}

void
sf_socket_write_event(sf_instance_t *inst, void *sock)
{
    sf_socket_base_t *sb = (sf_socket_base_t *) sock;

    if (sb != NULL && sb->sb_func_write != NULL)
        sb->sb_func_write(inst, sock);
}

static sf_socket_base_t *
socket_tcp(sf_instance_t *inst, sf_protocb_t *pcb)
{
    int fd;
    sf_socket_base_t *sb;

    if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        plog_error(LOG_ERR, "%s: socket() failed", __func__);
        return NULL;
    }

    if (socket_nonblock(fd) < 0)
        goto error;
    if ((sb = socket_create_base(inst, fd, pcb)) == NULL)
        goto error;

    if (sf_socket_poll_add(inst->inst_fd_poll, fd, sb) < 0) {
        free(sb);
        goto error;
    }

    return sb;

error:
    close(fd);
    return NULL;
}

static int
socket_tcp_accept(sf_instance_t *inst, int fd, sf_protocb_t *pcb)
{
    int new_fd;
    socklen_t addrlen;
    sf_sockaddr_t addr;

    plog(LOG_DEBUG, "%s: tcp accept on fd %d", __func__, fd);

    addrlen = sizeof(addr);
    if ((new_fd = accept(fd, (struct sockaddr *) &addr, &addrlen)) < 0) {
        if (errno != EAGAIN)
            plog_error(LOG_ERR, "%s: accept() failed", __func__);
        return -1;
    }

    if (socket_tcp_session(inst, new_fd, (struct sockaddr *) &addr, pcb, NULL) < 0) {
        plog(LOG_ERR, "%s: socket_tcp_sessoin() failed");
        return 0;   /* return 0 even if creating new session is failed */
    }

    return 0;
}

static int
socket_tcp_session(sf_instance_t *inst, int new_fd, struct sockaddr *addr, sf_protocb_t *pcb, void *udata)
{
    sf_socket_t *sock;
    sf_session_t *session;

    if ((sock = socket_create(inst, new_fd, pcb)) == NULL) {
        close(new_fd);
        return -1;
    }

    if (socket_nonblock(new_fd) < 0)
        goto error;
    if (socket_keepalive(new_fd) < 0)
        goto error;

    if (sf_socket_poll_add(inst->inst_fd_poll, new_fd, sock) < 0)
        goto error;
    if ((session = sf_session_create_start(inst, addr, sock, udata)) == NULL)
        goto error;

    sock->so_session = session;
    sock->so_flags |= SOCK_CONNECTED;
    return 0;

error:
    sf_socket_destroy(inst, sock);
    return -1;
}

static sf_socket_t *
socket_udp(sf_instance_t *inst, sf_protocb_t *pcb)
{
    int fd;
    sf_socket_t *sock;

    if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        plog_error(LOG_ERR, "%s: socket() failed");
        return NULL;
    }

    if (socket_nonblock(fd) < 0)
        goto error;
    if ((sock = socket_create(inst, fd, pcb)) == NULL)
        goto error;

    if (sf_socket_poll_add(inst->inst_fd_poll, fd, sock) < 0) {
        sf_socket_destroy(inst, sock);
        return NULL;
    }

    plog(LOG_DEBUG, "%s: sock = %p (fd %d)", __func__, sock, fd);

    sock->so_base.sb_pcb = pcb;
    return sock;

error:
    close(fd);
    return NULL;
}

static sf_socket_t *
socket_create(sf_instance_t *inst, int fd, sf_protocb_t *pcb)
{
    sf_socket_t *sock;

    if (inst->inst_sock.soi_sock_count >= inst->inst_sock.soi_max_sockets) {
        plog(LOG_ERR, "%s: too many open sockets", __func__);
        return NULL;
    }

    if ((sock = (sf_socket_t *) calloc(1, sizeof(*sock))) == NULL) {
        plog_error(LOG_ERR, "%s: calloc() failed", __func__);
        return NULL;
    }

    sock->so_base.sb_fd = fd;
    sock->so_base.sb_pcb = pcb;
    sock->so_base.sb_func_read = socket_read_event_receive;
    sock->so_base.sb_func_write = socket_write_event;

    sf_pbuf_init_small(&sock->so_rbuf);
    inst->inst_sock.soi_sock_count++;

    plog(LOG_DEBUG, "%s: new socket %p (fd %d)", __func__, sock, fd);

    return sock;
}

static sf_socket_base_t *
socket_create_base(sf_instance_t *inst, int fd, sf_protocb_t *pcb)
{
    sf_socket_base_t *sb;

    if (inst->inst_sock.soi_sock_count >= inst->inst_sock.soi_max_sockets) {
        plog(LOG_ERR, "%s: too many open sockets", __func__);
        return NULL;
    }

    if ((sb = (sf_socket_base_t *) calloc(1, sizeof(*sb))) == NULL) {
        plog_error(LOG_ERR, "%s: calloc() failed", __func__);
        return NULL;
    }

    sb->sb_fd = fd;
    sb->sb_pcb = pcb;
    sb->sb_func_read = socket_read_event_accept;
    sb->sb_func_write = NULL;

    inst->inst_sock.soi_sock_count++;

    plog(LOG_DEBUG, "%s: new socket_base %p (fd %d)", __func__, sb, fd);

    return sb;
}

static int
socket_bind(int fd, struct sockaddr *addr)
{
    if (bind(fd, addr, SALEN(addr)) < 0) {
        plog_error(LOG_ERR, "%s: bind() failed", __func__);
        return -1;
    }

    return 0;
}

static int
socket_listen(int fd, struct sockaddr *addr)
{
    int on = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        plog_error(LOG_ERR, "%s: setsockopt(SO_REUSEADDR) failed", __func__);
        return -1;
    }

    if (socket_bind(fd, addr) < 0)
        return -1;

    if (listen(fd, 8) < 0) {
        plog_error(LOG_ERR, "%s: listen() failed", __func__);
        return -1;
    }

    return 0;
}

static int
socket_nonblock(int fd)
{
    int flags;

    if ((flags = fcntl(fd, F_GETFL)) < 0) {
        plog_error(LOG_ERR, "%s: fcntl(F_GETFL) failed", __func__);
        return -1;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        plog_error(LOG_ERR, "%s: fcntl(F_SETFL) failed", __func__);
        return -1;
    }

    return 0;
}

static int
socket_keepalive(int fd)
{
    int on = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0) {
        plog_error(LOG_ERR, "%s: setsockopt(SO_KEEPALIVE) failed", __func__);
        return -1;
    }

    return 0;
}

static int
socket_read_event_accept(sf_instance_t *inst, void *sock)
{
    sf_socket_base_t *sb = (sf_socket_base_t *) sock;

    return socket_tcp_accept(inst, sb->sb_fd, sb->sb_pcb);
}

static int
socket_read_event_receive(sf_instance_t *inst, void *sock)
{
    int len;
    sf_socket_t *so = (sf_socket_t *) sock;

    plog(LOG_DEBUG, "%s: input event on socket %p (fd %d)", __func__, sock, so->so_base.sb_fd);

    if ((len = socket_receive(inst, so)) < 0) {
        sf_session_destroy(inst, so->so_session);
        sf_socket_destroy(inst, so);
        return -1;
    }

    if (len == 0)
        return -1;

    socket_shrink_rbuf(so);
    return 0;
}

static int
socket_write_event(sf_instance_t *inst, void *sock)
{
    sf_socket_t *so = (sf_socket_t *) sock;

    plog(LOG_DEBUG, "%s: output event on socket %p (fd %d)", __func__, so, so->so_base.sb_fd);

    if (so->so_session == NULL) {
        if (sf_session_output_bcast(inst, so) < 0) {
            plog(LOG_ERR, "%s: session_output_bcast() failed", __func__);
            return -1;
        }
    } else {
        if (sf_session_output(inst, so->so_session) < 0) {
            plog(LOG_ERR, "%s: session_output() failed", __func__);
            return -1;
        }
    }

    return 0;
}

static int
socket_receive(sf_instance_t *inst, sf_socket_t *sock)
{
    int len, msg_len;
    sf_pbuf_t *pbuf;
    sf_session_t *session;
    sf_sockaddr_t from;

    if ((len = socket_do_receive(inst, sock, (struct sockaddr *) &from, sizeof(from), MSG_PEEK)) < 0)
        return -1;
    if (len == 0)
        return 0;

    if ((session = socket_get_session(inst, sock, (struct sockaddr *) &from)) == NULL) {
        plog(LOG_ERR, "%s: socket_get_session() failed", __func__);
        return -1;
    }

    pbuf = (sf_pbuf_t *) &sock->so_rbuf;
    memcpy(&sock->so_last_from, &from, sizeof(sock->so_last_from));

    msg_len = sf_session_estlen(inst, session, pbuf);
    sf_pbuf_adjust_tail(pbuf, -len);

    if (msg_len > len) {
        plog(LOG_DEBUG, "%s: estimated message size %d", __func__, msg_len);
        if (socket_extend_rbuf(inst, sock, msg_len) < 0) {
            plog(LOG_ERR, "%s: receive failed due to message too big", __func__);
            return -1;
        }
    }

    if ((len = socket_do_receive(inst, sock, (struct sockaddr *) &from, sizeof(from), 0)) <= 0)
        return -1;

    if (sf_session_input(inst, session, pbuf) < 0) {
        plog(LOG_ERR, "%s: sf_session_input() failed", __func__);
        return -1;
    }

    return len;
}

static int
socket_do_receive(sf_instance_t *inst, sf_socket_t *sock, struct sockaddr *from, socklen_t from_len, int flags)
{
    int len, free_len;
    sf_pbuf_t *pbuf = (sf_pbuf_t *) &sock->so_rbuf;

    free_len = sf_pbuf_free_len(pbuf);
    if ((len = socket_do_receive2(inst, sock, from, from_len, sf_pbuf_tail(pbuf), free_len, flags)) < 0)
        return -1;

    sf_pbuf_adjust_tail(pbuf, len);
    return len;
}

static int
socket_do_receive2(sf_instance_t *inst, sf_socket_t *sock, struct sockaddr *from, socklen_t from_len, char *buf, int bufmax, int flags)
{
    int len;

    if ((len = recvfrom(sock->so_base.sb_fd, buf, bufmax, flags, from, &from_len)) < 0) {
        switch (errno) {
        case EAGAIN:
            return 0;
        case ECONNRESET:
            plog(LOG_DEBUG, "%s: connection reset by peer", __func__);
            return -1;
        default:
            plog_error(LOG_ERR, "%s: recvfrom() failed", __func__);
            return -1;
        }
    }

    if (len == 0) {
        plog(LOG_DEBUG, "%s: connection closed by peer", __func__);
        return -1;
    }

    return len;
}

static sf_session_t *
socket_get_session(sf_instance_t *inst, sf_socket_t *sock, struct sockaddr *from)
{
    sf_session_t *session;

    if (sock->so_session != NULL)
        return (sf_session_t *) sock->so_session;
    else {
        if ((session = sf_session_find_create_start(inst, from, sock, NULL)) == NULL) {
            plog(LOG_ERR, "%s: sf_session_find_create_start() failed", __func__);
            return NULL;
        }

        session->se_sock = sock;
    }

    return session;
}

static int
socket_extend_rbuf(sf_instance_t *inst, sf_socket_t *sock, int new_len)
{
    sf_pbuf_t *pbuf = (sf_pbuf_t *) &sock->so_rbuf;

    if (new_len > inst->inst_sock.soi_max_msgsize) {
        plog(LOG_DEBUG, "%s: too large message size", __func__);
        return -1;
    }

    plog(LOG_DEBUG, "%s: extend buffer to %d bytes", __func__, new_len);
    sf_pbuf_resize(pbuf, new_len);

    return 0;
}

static void
socket_shrink_rbuf(sf_socket_t *sock)
{
    sf_pbuf_t *pbuf = (sf_pbuf_t *) &sock->so_rbuf;

    if (sf_pbuf_data_len(pbuf) == 0) {
        sf_pbuf_release(pbuf);
        sf_pbuf_init_small(&sock->so_rbuf);
    }
}
