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
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netdb.h>
#include "sf.h"
#ifdef HAVE_BSD_STRING_H
# include <bsd/string.h>
#endif

static int util_compare_sin(struct sockaddr_in *a, struct sockaddr_in *b);
static int util_compare_sin6(struct sockaddr_in6 *a, struct sockaddr_in6 *b);

uint32_t
sf_util_random(void)
{
    int fd;
    uint32_t value;

    if ((fd = open("/dev/urandom", O_RDONLY)) < 0)
        return 0;

    if (read(fd, &value, sizeof(value)) != sizeof(value)) {
        close(fd);
        return 0;
    }

    close(fd);
    return value;
}

int
sf_util_str2sa(struct sockaddr *sa, char *addr, uint16_t port)
{
    struct addrinfo hints, *res;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(addr, NULL, &hints, &res) != 0) {
        plog(LOG_ERR, "%s: getaddrinfo() failed", __func__);
        return -1;
    }

    memcpy(sa, res->ai_addr, SALEN(res->ai_addr));
    freeaddrinfo(res);

    sf_util_sasetport(sa, port);
    return 0;
}

int
sf_util_sa2str(char *buf, int bufmax, struct sockaddr *sa)
{
    int port;
    char host[256];

    if (sf_util_sa2str_wop(host, sizeof(host), sa) < 0)
        return -1;

    if ((port = sf_util_sagetport(sa)) == 0)
        strlcpy(buf, host, bufmax);
    else {
        if (sa->sa_family == AF_INET)
            snprintf(buf, bufmax, "%s:%d", host, port);
        if (sa->sa_family == AF_INET6)
            snprintf(buf, bufmax, "[%s]:%d", host, port);
    }

    return 0;
}

int
sf_util_sa2str_wop(char *buf, int bufmax, struct sockaddr *sa)
{
    if (getnameinfo(sa, SALEN(sa), buf, bufmax, NULL, 0, NI_NUMERICHOST) < 0) {
        plog(LOG_ERR, "%s: getnameinfo() failed", __func__);
        return -1;
    }

    return 0;
}

int
sf_util_sagetport(struct sockaddr *sa)
{
    struct sockaddr_in *sin;
    struct sockaddr_in6 *sin6;

    switch (sa->sa_family) {
    case AF_INET:
        sin = (struct sockaddr_in *) sa;
        return ntohs(sin->sin_port);

    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) sa;
        return ntohs(sin6->sin6_port);
    }

    return -1;
}

void
sf_util_sasetport(struct sockaddr *sa, uint16_t port)
{
    struct sockaddr_in *sin;
    struct sockaddr_in6 *sin6;

    switch (sa->sa_family) {
    case AF_INET:
        sin = (struct sockaddr_in *) sa;
        sin->sin_port = htons(port);
        break;

    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) sa;
        sin6->sin6_port = htons(port);
        break;
    }
}

int
sf_util_sacmp(struct sockaddr *a, struct sockaddr *b)
{
    switch (a->sa_family) {
    case AF_INET:
        return util_compare_sin((struct sockaddr_in *) a, (struct sockaddr_in *) b);
    case AF_INET6:
        return util_compare_sin6((struct sockaddr_in6 *) a, (struct sockaddr_in6 *) b);
    }

    return -1;
}

int
sf_util_ifnametoaddr(struct sockaddr_in *ifaddr, char *ifname)
{
    struct ifaddrs *ifap, *ifa;

    if (getifaddrs(&ifap) < 0) {
        plog_error(LOG_ERR, "%s: getifaddrs() failed", __func__);
        return -1;
    }

    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (strcmp(ifa->ifa_name, ifname) == 0) {
            if (ifa->ifa_addr->sa_family == AF_INET) {
                memcpy(ifaddr, ifa->ifa_addr, sizeof(*ifaddr));
                freeifaddrs(ifap);
                return 0;
            }
        }
    }

    freeifaddrs(ifap);
    return -1;
}

static int
util_compare_sin(struct sockaddr_in *a, struct sockaddr_in *b)
{
    int r;

    if ((r = memcmp(&a->sin_addr, &b->sin_addr, sizeof(a->sin_addr))) != 0)
        return r;
    if ((r = a->sin_port - b->sin_port) != 0)
        return r;

    return 0;
}

static int
util_compare_sin6(struct sockaddr_in6 *a, struct sockaddr_in6 *b)
{
    int r;

    if ((r = memcmp(&a->sin6_addr, &b->sin6_addr, sizeof(a->sin6_addr))) != 0)
        return r;
    if ((r = a->sin6_port - b->sin6_port) != 0)
        return r;

    return 0;
}
