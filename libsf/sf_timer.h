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
#ifndef __SF_TIMER_H__
#define __SF_TIMER_H__

typedef struct sf_timer sf_timer_t;
typedef void (sf_timer_func_t)(void *, void*);

struct sf_timer {
    struct timeval     t_time;
    sf_timer_t        *t_prev;
    sf_timer_t        *t_next;
    sf_timer_func_t   *t_func;
    void              *t_param1;
    void              *t_param2;
};

typedef struct {
    sf_timer_t        *ti_head;
    sf_timer_t        *ti_tail;
} sf_timer_inst_t;

int sf_timer_request(sf_instance_t *inst, sf_timer_t *timer, int msec, sf_timer_func_t *func, void *param1, void *param2);
int sf_timer_cancel(sf_instance_t *inst, sf_timer_t *timer);
int sf_timer_isregd(sf_instance_t *inst, sf_timer_t *timer);
int sf_timer_timetonext(sf_instance_t *inst, struct timeval *tv_ttn);
void sf_timer_execute(sf_instance_t *inst);

#endif
