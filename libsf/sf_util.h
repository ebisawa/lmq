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
#ifndef __SF_UTIL_H__
#define __SF_UTIL_H__

#define NELEMS(array)   (sizeof(array) / sizeof(array[0]))
#define SALEN(sa)       (((struct sockaddr *) (sa))->sa_family == AF_INET) ? sizeof(struct sockaddr_in) : \
                          ((((struct sockaddr *) (sa))->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : 0)

typedef union {
    struct sockaddr_in   sa_in;
    struct sockaddr_in6  sa_in6;
} sf_sockaddr_t;

uint32_t sf_util_random(void);
int sf_util_str2sa(struct sockaddr *sa, char *addr, uint16_t port);
int sf_util_sa2str(char *buf, int bufmax, struct sockaddr *sa);
int sf_util_sa2str_wop(char *buf, int bufmax, struct sockaddr *sa);
int sf_util_sagetport(struct sockaddr *sa);
void sf_util_sasetport(struct sockaddr *sa, uint16_t port);
int sf_util_sacmp(struct sockaddr *a, struct sockaddr *b);
int sf_util_ifnametoaddr(struct sockaddr_in *ifaddr, char *ifname);

#endif
