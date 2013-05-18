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
#include "sf.h"

#define PCB(session)    ((session)->se_sock->so_base.sb_pcb)
#define UDATA(session)  ((session == NULL) ? NULL : (session)->se_udata)

int
sf_proto_sid(sf_instance_t *inst, uint64_t *sid, sf_socket_t *sock)
{
    int len;
    sf_pbuf_t *pbuf;
    sf_protocb_t *pcb;

    if ((pcb = sock->so_base.sb_pcb) == NULL || pcb->pc_session_id == NULL)
        return -1;

    pbuf = (sf_pbuf_t *) &sock->so_rbuf;
    len = sf_pbuf_data_len(pbuf);

    if (len <= 0)
        return -1;

    return pcb->pc_session_id(sid, sf_pbuf_head(pbuf), len);
}

int
sf_proto_start(sf_instance_t *inst, sf_session_t *session)
{
    sf_protocb_t *pcb = PCB(session);

    if (pcb == NULL || pcb->pc_session_start == NULL)
        return 0;

    return pcb->pc_session_start(&session->se_sf, UDATA(session));
}

int
sf_proto_end(sf_instance_t *inst, sf_session_t *session)
{
    sf_protocb_t *pcb = PCB(session);

    if (pcb == NULL || pcb->pc_session_end == NULL)
        return 0;

    return pcb->pc_session_end(&session->se_sf, UDATA(session));
}

int
sf_proto_estlen(sf_instance_t *inst, sf_session_t *session, sf_pbuf_t *pbuf)
{
    int len;
    sf_protocb_t *pcb;

    if ((len = sf_pbuf_data_len(pbuf)) <= 0)
        return -1;
    if ((pcb = PCB(session)) == NULL || pcb->pc_msg_estlen == NULL)
        return -1;

    return pcb->pc_msg_estlen(&session->se_sf, sf_pbuf_head(pbuf), len, UDATA(session));
}

int
sf_proto_msglen(sf_instance_t *inst, sf_session_t *session, sf_pbuf_t *pbuf)
{
    int len;
    sf_protocb_t *pcb = PCB(session);

    if ((len = sf_pbuf_data_len(pbuf)) <= 0)
        return -1;
    if ((pcb = PCB(session)) == NULL || pcb->pc_msg_length == NULL)
        return len;

    return pcb->pc_msg_length(&session->se_sf, sf_pbuf_head(pbuf), len, UDATA(session));
}

int
sf_proto_input(sf_instance_t *inst, sf_session_t *session, sf_pbuf_t *pbuf, int msglen)
{
    sf_protocb_t *pcb = PCB(session);

    if (pcb == NULL || pcb->pc_msg_input == NULL)
        return -1;

    return pcb->pc_msg_input(&session->se_sf, sf_pbuf_head(pbuf), msglen, UDATA(session));
}

int
sf_proto_output(sf_instance_t *inst, sf_session_t *session)
{
    sf_protocb_t *pcb = PCB(session);

    if (pcb == NULL || pcb->pc_msg_output == NULL)
        return 0;

    return pcb->pc_msg_output(&session->se_sf, UDATA(session));
}

int
sf_proto_timeout(sf_instance_t *inst, sf_session_t *session)
{
    sf_protocb_t *pcb = PCB(session);

    if (pcb == NULL || pcb->pc_timeout == NULL)
        return 0;

    return pcb->pc_timeout(&session->se_sf, UDATA(session));
}
