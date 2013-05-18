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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libsf/sf.h"
#include "binding.h"
#include "binding_hash.h"

static unsigned binding_calc_hash(char *name);

binding_hash_t BindingHash;

void
binding_hash_register(binding_hash_t *bh, binding_t *bi)
{
    binding_t *next;
    unsigned value, index;

    value = binding_calc_hash(bi->bi_name);
    index = value % NELEMS(bh->bh_hashtab);

    if ((next = bh->bh_hashtab[index]) != NULL)
        next->bi_hash_prev = bi;

    bi->bi_hash_next = next;
    bi->bi_hash_prev = NULL;
    bh->bh_hashtab[index] = bi;

    plog(LOG_DEBUG, "%s: register %p, name \"%s\", hash %u", __func__, bi, bi->bi_name, index);
}

void
binding_hash_unregister(binding_hash_t *bh, char *name)
{
    binding_t *bi;
    unsigned value, index;

    value = binding_calc_hash(name);
    index = value % NELEMS(bh->bh_hashtab);

    if ((bi = binding_hash_lookup(bh, name)) == NULL)
        return;

    if (bi->bi_hash_prev != NULL)
        bi->bi_hash_prev->bi_hash_next = bi->bi_hash_next;
    if (bi->bi_hash_next != NULL)
        bi->bi_hash_next->bi_hash_prev = bi->bi_hash_prev;
    if (bh->bh_hashtab[index] == bi)
        bh->bh_hashtab[index] = bi->bi_hash_next;

    plog(LOG_DEBUG, "%s: unregister %p, name \"%s\", hash %u", __func__, bi, bi->bi_name, index);
}

binding_t *
binding_hash_lookup(binding_hash_t *bh, char *name)
{
    binding_t *p;
    unsigned value, index;

    value = binding_calc_hash(name);
    index = value % NELEMS(bh->bh_hashtab);

    plog(LOG_DEBUG, "%s: name \"%s\", hash %u", __func__, name, index);

    p = bh->bh_hashtab[index];
    while (p != NULL) {
        if (strcmp(p->bi_name, name) == 0)
            return p;

        p = p->bi_hash_next;
    }

    return NULL;
}

static unsigned
binding_calc_hash(char *name)
{
    unsigned hash = 2166136261U;

    /* FNV-1a hash */
    for (; *name != 0; name++) {
        hash ^= *((unsigned char *) name);
        hash *= 16777619;
    }

    return hash;
}
