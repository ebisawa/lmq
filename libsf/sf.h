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
#ifndef __SF_H__
#define __SF_H__
#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "sf_util.h"
#include "sf_plog.h"

typedef struct sf_instance sf_instance_t;
typedef struct sf_session sf_session_t;

typedef struct {
    sf_instance_t       *sf_inst;
    sf_session_t        *sf_sess;
} sf_t;

typedef struct {
    int (*pc_session_id)(uint64_t *sid, char *buf, int len);
    int (*pc_session_start)(sf_t *sf, void *udata);
    int (*pc_session_end)(sf_t *sf, void *udata);
    int (*pc_msg_estlen)(sf_t *sf, char *buf, int len, void *udata);
    int (*pc_msg_length)(sf_t *sf, char *buf, int len, void *udata);
    int (*pc_msg_input)(sf_t *sf, char *buf, int len, void *udata);
    int (*pc_msg_output)(sf_t *sf, void *udata);
    int (*pc_timeout)(sf_t *sf, void *udata);
} sf_protocb_t;

#include "sf_timer.h"
#include "sf_pbuf.h"
#include "sf_socket.h"
#include "sf_session.h"
#include "sf_proto.h"
#include "sf_main.h"

struct sf_instance {
    int                  inst_fd_poll;
    sf_socket_inst_t     inst_sock;
    sf_session_inst_t    inst_sess;
    sf_timer_inst_t      inst_timer;
};

#endif
