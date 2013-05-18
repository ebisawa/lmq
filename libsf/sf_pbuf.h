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
#ifndef __SF_PBUF_H__
#define __SF_PBUF_H__

#define PBUF_SMALL_BUFSIZE  2048

typedef struct {
    int         pb_len;
    char       *pb_buf;
    char       *pb_head;
    char       *pb_tail;
    void      (*pb_release_func)(void *self);
} sf_pbuf_t;

typedef struct {
    sf_pbuf_t   pbs_hdr;
    char        pbs_small_buf[PBUF_SMALL_BUFSIZE];
} sf_pbuf_small_t;

int sf_init_pbuf(void);
int sf_pbuf_init(sf_pbuf_t *pbuf, int len);
int sf_pbuf_resize(sf_pbuf_t *pbuf, int len);
void sf_pbuf_init_small(sf_pbuf_small_t *pbuf);
void sf_pbuf_release(sf_pbuf_t *pbuf);
int sf_pbuf_buffer_len(sf_pbuf_t *pbuf);
int sf_pbuf_data_len(sf_pbuf_t *pbuf);
int sf_pbuf_free_len(sf_pbuf_t *pbuf);
char *sf_pbuf_head(sf_pbuf_t *pbuf);
char *sf_pbuf_tail(sf_pbuf_t *pbuf);
void sf_pbuf_adjust(sf_pbuf_t *pbuf, int adj_len);
void sf_pbuf_adjust_tail(sf_pbuf_t *pbuf, int adj_len);
int sf_pbuf_write_prepare(sf_pbuf_t *pbuf, int len);
int sf_pbuf_write(sf_pbuf_t *pbuf, char *buf, int len);

#endif
