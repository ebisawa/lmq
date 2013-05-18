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
#include <sys/time.h>
#include "sf.h"

#define TIMER_SEC2USEC(s)   ((s) * 1000 * 1000)

static void timer_register(sf_instance_t *inst, sf_timer_t *timer);
static void timer_unregister(sf_instance_t *inst, sf_timer_t *timer);
static void timer_chain_next(sf_timer_t *parent, sf_timer_t *timer);
static void timer_tv_after(struct timeval *tv, int msec);
static int timer_tv_compare(struct timeval *a, struct timeval *b);

int
sf_timer_request(sf_instance_t *inst, sf_timer_t *timer, int msec,
                 void (*func)(void *, void *), void *param1, void *param2)
{
    plog(LOG_DEBUG, "%s: timeout request after %d ms", __func__, msec);

    if (sf_timer_isregd(inst, timer)) {
        plog(LOG_DEBUG, "%s: timer %p has already been registered. cancel it.", __func__, timer);
        timer_unregister(inst, timer);
    }

    gettimeofday(&timer->t_time, NULL);
    timer_tv_after(&timer->t_time, msec);
    timer->t_func = func;
    timer->t_param1 = param1;
    timer->t_param2 = param2;

    timer_register(inst, timer);
    return 0;
}

int
sf_timer_cancel(sf_instance_t *inst, sf_timer_t *timer)
{
    if (sf_timer_isregd(inst, timer)) {
        plog(LOG_DEBUG, "%s: cancel timer %p", __func__, timer);
        timer_unregister(inst, timer);
    }

    return 0;
}

int
sf_timer_isregd(sf_instance_t *inst, sf_timer_t *timer)
{
    sf_timer_t *t;
    sf_timer_inst_t *ti = &inst->inst_timer;

    for (t = ti->ti_head; t != NULL; t = t->t_next) {
        if (t == timer)
            return 1;
    }

    return 0;
}

int
sf_timer_timetonext(sf_instance_t *inst, struct timeval *tv_ttn)
{
    int s, u;
    sf_timer_t *next;
    struct timeval now;

    if ((next = inst->inst_timer.ti_head) == NULL)
        return -1;

    gettimeofday(&now, NULL);
    s = next->t_time.tv_sec - now.tv_sec;
    u = next->t_time.tv_usec - now.tv_usec;

    if (u < 0) {
        s--;
        u += TIMER_SEC2USEC(1);
    }

    if (s < 0) {
        s = 0;
        u = 1;
    }

    tv_ttn->tv_sec = s;
    tv_ttn->tv_usec = u;

    return 0;
}

void
sf_timer_execute(sf_instance_t *inst)
{
    sf_timer_t *t;
    sf_timer_inst_t *ti = &inst->inst_timer;
    struct timeval now;

    gettimeofday(&now, NULL);

    for (t = ti->ti_head; t != NULL; t = t->t_next) {
        if (timer_tv_compare(&now, &t->t_time) < 0)
            break;

        timer_unregister(inst, t);

        if (t->t_func != NULL)
            t->t_func(t->t_param1, t->t_param2);
    }
}

static void
timer_register(sf_instance_t *inst, sf_timer_t *timer)
{
    sf_timer_t *t;
    sf_timer_inst_t *ti = &inst->inst_timer;

    if (ti->ti_head == NULL) {
        timer->t_prev = NULL;
        timer->t_next = NULL;
        ti->ti_head = timer;
        ti->ti_tail = timer;
    } else if (timer_tv_compare(&timer->t_time, &ti->ti_head->t_time) < 0) {
        timer->t_prev = NULL;
        timer->t_next = ti->ti_head;
        ti->ti_head->t_prev = timer;
        ti->ti_head = timer;
    } else if (timer_tv_compare(&timer->t_time, &ti->ti_tail->t_time) > 0) {
        timer_chain_next(ti->ti_tail, timer);
        ti->ti_tail = timer;
    } else {
        for (t = ti->ti_head; t != NULL; t = t->t_next) {
            if (timer_tv_compare(&timer->t_time, &t->t_time) > 0) {
                timer_chain_next(t, timer);
                break;
            }
        }
    }
}

static void
timer_unregister(sf_instance_t *inst, sf_timer_t *timer)
{
    sf_timer_inst_t *ti = &inst->inst_timer;

    if (timer->t_prev != NULL)
        timer->t_prev->t_next = timer->t_next;
    if (timer->t_next != NULL)
        timer->t_next->t_prev = timer->t_prev;

    if (ti->ti_head == timer)
        ti->ti_head = timer->t_next;
    if (ti->ti_tail == timer)
        ti->ti_tail = timer->t_prev;
}

static void
timer_chain_next(sf_timer_t *parent, sf_timer_t *timer)
{
    timer->t_prev = parent;
    timer->t_next = parent->t_next;

    if (parent->t_next != NULL)
        parent->t_next->t_prev = timer;

    parent->t_next = timer;
}

static void
timer_tv_after(struct timeval *tv, int msec)
{
    tv->tv_usec += msec * 1000;

    if (tv->tv_usec > TIMER_SEC2USEC(1)) {
        tv->tv_usec -= TIMER_SEC2USEC(1);
        tv->tv_sec++;
    }
}

static int
timer_tv_compare(struct timeval *a, struct timeval *b)
{
    int r;

    if ((r = a->tv_sec - b->tv_sec) != 0)
        return r;
    if ((r = a->tv_usec - b->tv_usec) != 0)
        return r;

    return 0;
}
