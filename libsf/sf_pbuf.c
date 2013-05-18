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

static int pbuf_init_with_data(sf_pbuf_t *pbuf, int len, char *data, int data_len);
static void pbuf_sethdr(sf_pbuf_t *pbuf, char *buf, int len);
static void pbuf_release(sf_pbuf_t *pbuf);
static void pbuf_release_buf(sf_pbuf_t *pbuf);
static int pbuf_writable_len(sf_pbuf_t *pbuf);
static void pbuf_relocate(sf_pbuf_t *pbuf);

int
sf_init_pbuf(void)
{
    return 0;
}

int
sf_pbuf_init(sf_pbuf_t *pbuf, int len)
{
    return pbuf_init_with_data(pbuf, len, NULL, 0);
}

int
sf_pbuf_resize(sf_pbuf_t *pbuf, int len)
{
    char *data;
    int data_len;
    sf_pbuf_t new_pbuf;

    data = pbuf->pb_buf;
    data_len = sf_pbuf_data_len(pbuf);

    if (pbuf_init_with_data(&new_pbuf, len, data, data_len) < 0)
        return -1;

    pbuf_release(pbuf);
    memcpy(pbuf, &new_pbuf, sizeof(*pbuf));

    return 0;
}

void
sf_pbuf_init_small(sf_pbuf_small_t *pbuf)
{
    pbuf_sethdr(&pbuf->pbs_hdr, pbuf->pbs_small_buf, sizeof(pbuf->pbs_small_buf));
}

void
sf_pbuf_release(sf_pbuf_t *pbuf)
{
    pbuf_release(pbuf);
}

int
sf_pbuf_buffer_len(sf_pbuf_t *pbuf)
{
    return pbuf->pb_len;
}

int
sf_pbuf_data_len(sf_pbuf_t *pbuf)
{
    return pbuf->pb_tail - pbuf->pb_head;
}

int
sf_pbuf_free_len(sf_pbuf_t *pbuf)
{
    return sf_pbuf_buffer_len(pbuf) - sf_pbuf_data_len(pbuf);
}

char *
sf_pbuf_head(sf_pbuf_t *pbuf)
{
    return pbuf->pb_head;
}

char *
sf_pbuf_tail(sf_pbuf_t *pbuf)
{
    return pbuf->pb_tail;
}

void
sf_pbuf_adjust(sf_pbuf_t *pbuf, int adj_len)
{
    if (sf_pbuf_data_len(pbuf) >= adj_len)
        pbuf->pb_head += adj_len;

    pbuf_relocate(pbuf);
}

void
sf_pbuf_adjust_tail(sf_pbuf_t *pbuf, int adj_len)
{
    pbuf->pb_tail += adj_len;
}

int
sf_pbuf_write_prepare(sf_pbuf_t *pbuf, int len)
{
    if (sf_pbuf_free_len(pbuf) < len)
        return -1;
    if (pbuf_writable_len(pbuf) < len)
        pbuf_relocate(pbuf);

    return 0;
}

int
sf_pbuf_write(sf_pbuf_t *pbuf, char *buf, int len)
{
    if (sf_pbuf_write_prepare(pbuf, len) < 0)
        return -1;

    memcpy(pbuf->pb_tail, buf, len);
    pbuf->pb_tail += len;
    
    return 0;
}

static int
pbuf_init_with_data(sf_pbuf_t *pbuf, int len, char *data, int data_len)
{
    char *p = NULL;

    if (len > 0 && (p = malloc(len)) == NULL) {
        plog_error(LOG_ERR, "%s: malloc() failed", __func__);
        return -1;
    }

    if (data != NULL)
        memcpy(p, data, data_len);

    pbuf_sethdr(pbuf, p, len);
    pbuf->pb_release_func = (void (*)(void *)) pbuf_release_buf;
    pbuf->pb_tail += data_len;

    return 0;
}

static void
pbuf_sethdr(sf_pbuf_t *pbuf, char *buf, int len)
{
    pbuf->pb_buf = buf;
    pbuf->pb_head = buf;
    pbuf->pb_tail = buf;
    pbuf->pb_len = len;
    pbuf->pb_release_func = NULL;

}

static void
pbuf_release(sf_pbuf_t *pbuf)
{
    if (pbuf->pb_release_func != NULL)
        pbuf->pb_release_func(pbuf);

    pbuf_sethdr(pbuf, NULL, 0);
}

static void
pbuf_release_buf(sf_pbuf_t *pbuf)
{
    if (pbuf->pb_buf != NULL)
        free(pbuf->pb_buf);
}

static int
pbuf_writable_len(sf_pbuf_t *pbuf)
{
    return pbuf->pb_len - (pbuf->pb_tail - pbuf->pb_buf);
}

static void
pbuf_relocate(sf_pbuf_t *pbuf)
{
    int copylen;

    if (pbuf->pb_head == pbuf->pb_buf)
        return;
    if ((copylen = sf_pbuf_data_len(pbuf)) > 0)
        memmove(pbuf->pb_buf, pbuf->pb_head, copylen);

    pbuf->pb_head = pbuf->pb_buf;
    pbuf->pb_tail = pbuf->pb_buf + copylen;
}
