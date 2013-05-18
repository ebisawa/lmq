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
#include <ctype.h>
#include <unistd.h>
#include "libsf/sf.h"
#include "mqcore/mqcore.h"
#include "stomp_proto.h"
#include "stomp_subr.h"

#define STOMP_HEADERS_MAX       8
#define STOMP_HEADER_LEN_MAX    80

static int stomp_session_start(sf_t *sf, void *udata);
static int stomp_session_end(sf_t *sf, void *udata);
static int stomp_msg_estimlen(sf_t *sf, char *buf, int len, void *udata);
static int stomp_msg_length(sf_t *sf, char *buf, int len, void *udata);
static int stomp_msg_input(sf_t *sf, char *buf, int len, void *udata);
static int stomp_msg_output(sf_t *sf, void *udata);

sf_protocb_t StompProtoCB = {
    NULL,  /* id */
    stomp_session_start,
    stomp_session_end,
    stomp_msg_estimlen,
    stomp_msg_length,
    stomp_msg_input,
    stomp_msg_output,
    NULL,  /* timeout */
};

#define STOMP_STATE_INITIAL     0
#define STOMP_STATE_CONNECTED   1

#define STOMP_ABORT             0
#define STOMP_ACK               1
#define STOMP_BEGIN             2
#define STOMP_COMMIT            3
#define STOMP_CONNECT           4
#define STOMP_DISCONNECT        5
#define STOMP_SEND              6
#define STOMP_SUBSCRIBE         7
#define STOMP_UNSUBSCRIBE       8

typedef struct stomp_msg stomp_msg_t;

typedef struct {
    int               sc_code;
    char             *sc_str;
    int               sc_next_state;
    int             (*sc_func)(sf_t *sf, void *udata, stomp_msg_t *msg);
} stomp_command_t;

typedef struct {
    stomp_command_t  *sct_cmds;
    int               sct_num;
} stomp_command_tables_t;

static int stomp_initial_connect(sf_t *sf, void *udata, stomp_msg_t *msg);
static int stomp_connected_disconnect(sf_t *sf, void *udata, stomp_msg_t *msg);
static int stomp_connected_send(sf_t *sf, void *udata, stomp_msg_t *msg);
static int stomp_connected_subscribe(sf_t *sf, void *udata, stomp_msg_t *msg);
static int stomp_connected_unsubscribe(sf_t *sf, void *udata, stomp_msg_t *msg);

stomp_command_t StompCommandsInitial[] = {
    { STOMP_CONNECT,     "CONNECT",      STOMP_STATE_CONNECTED,  stomp_initial_connect,      },
};

stomp_command_t StompCommandsConnected[] = {
    { STOMP_DISCONNECT,  "DISCONNECT",   STOMP_STATE_INITIAL,    stomp_connected_disconnect  },
    { STOMP_SEND,        "SEND",         STOMP_STATE_CONNECTED,  stomp_connected_send        },
    { STOMP_SUBSCRIBE,   "SUBSCRIBE",    STOMP_STATE_CONNECTED,  stomp_connected_subscribe   },
    { STOMP_UNSUBSCRIBE, "UNSUBSCRIBE",  STOMP_STATE_CONNECTED,  stomp_connected_unsubscribe },
};

stomp_command_tables_t StompCommands[] = {
    { StompCommandsInitial,   NELEMS(StompCommandsInitial)   },   /* STOMP_STATE_INITIAL */
    { StompCommandsConnected, NELEMS(StompCommandsConnected) },   /* STOMP_STATE_CONNECTED */
};

struct stomp_msg {
    stomp_command_t  *sm_cmd;
    char             *sm_buf;
    char             *sm_hdr[STOMP_HEADERS_MAX];
    char             *sm_body;
    int               sm_len;
};

static int stomp_read_header(char *buf, int bufmax, stomp_msg_t *msg, char *key);
static char *stomp_find_header(stomp_msg_t *msg, char *key);
static int stomp_parse(stomp_msg_t *msg, char *buf, int len, stomp_command_t *cmdtable, int numtable);
static stomp_command_t *stomp_parse_command(char *buf, int len, stomp_command_t *cmdtable, int numtable);
static void stomp_parse_lines(stomp_msg_t *msg, char *buf, int buflen);
static char *stomp_next_line(char *buf, int buflen);
static char *stomp_skip_command(char *buf);
static int stomp_is_header(char *buf);
static int stomp_make_connected(char *buf, int bufmax, unsigned session_id);
static int stomp_make_message(char *buf, int bufmax, unsigned message_id);

static unsigned SessionId, MessageId;

static int
stomp_session_start(sf_t *sf, void *udata)
{
    plog(LOG_DEBUG, "%s: session start", __func__);

    if (stomp_create_session(sf) < 0) {
        plog(LOG_ERR, "%s: stomp_create_session() failed", __func__);
        return -1;
    }

    return 0;
}

static int
stomp_session_end(sf_t *sf, void *udata)
{
    plog(LOG_DEBUG, "%s: session end", __func__);
    stomp_destroy_session(sf);

    return 0;
}

static int
stomp_msg_estimlen(sf_t *sf, char *buf, int len, void *udata)
{
    int est_len;
    char *p, clen[256];
    stomp_msg_t msg;

    if (stomp_parse(&msg, buf, len, NULL, 0) < 0) {
        plog(LOG_DEBUG, "%s: stomp_parse() failed", __func__);
        return -1;
    }

    if (stomp_read_header(clen, sizeof(clen), &msg, "content-length:") < 0)
        return -1;

    est_len = strtol(clen, &p, 10);
    if (*p != '\n' && *p != '\r' && *p != 0)
        return -1;
    if (msg.sm_body == NULL || msg.sm_buf == NULL)
        return -1;

    /* message body size + header size + terminator size */
    est_len += (msg.sm_body - msg.sm_buf) + 1;

    plog(LOG_DEBUG, "%s: estimated message length = %d", __func__, est_len);

    return est_len;
}

static int
stomp_msg_length(sf_t *sf, char *buf, int len, void *udata)
{
    int i, clen;

    if ((clen = stomp_msg_estimlen(sf, buf, len, udata)) > 0) {
        if (len >= clen) {
            if (buf[clen - 1] != 0)
                return -1;
            else
                return clen;
        }
    } else {
        for (i = 0; i < len; i++) {
            if (buf[i] == 0)
                return i + 1;
        }
    }

    return -1;
}

static int
stomp_msg_input(sf_t *sf, char *buf, int len, void *udata)
{
    int state;
    stomp_msg_t msg;
    stomp_command_tables_t *t;

    if (udata == NULL) {
        plog(LOG_ERR, "%s: udata == NULL. why?", __func__);
        return -1;
    }

    state = stomp_get_state(sf);
    t = &StompCommands[state];

    plog(LOG_DEBUG, "%s: [input] state = %d, buf = >>>\n%s<<<", __func__, state, buf);

    if (stomp_parse(&msg, buf, len, t->sct_cmds, t->sct_num) < 0) {
        plog(LOG_ERR, "%s: stomp_parse() failed", __func__);
        return -1;
    }

    if (msg.sm_cmd->sc_func(sf, udata, &msg) < 0)
        return -1;

    stomp_set_state(sf, msg.sm_cmd->sc_next_state);
    return 0;
}

static int
stomp_msg_output(sf_t *sf, void *udata)
{
    return stomp_send_resume(sf);
}

static int
stomp_initial_connect(sf_t *sf, void *udata, stomp_msg_t *msg)
{
    int len;
    char buf[256];

    len = stomp_make_connected(buf, sizeof(buf), ++SessionId);
    if (sf_send(sf, buf, len + 1) < 0) {
        plog(LOG_ERR, "%s: xp_send() failed", __func__);
        return -1;
    }
    
    return 0;
}

static int
stomp_connected_disconnect(sf_t *sf, void *udata, stomp_msg_t *msg)
{
    plog(LOG_DEBUG, "%s: disconnect", __func__);
    stomp_destroy_session(sf);

    return 0;
}

static int
stomp_connected_send(sf_t *sf, void *udata, stomp_msg_t *msg)
{
    int header_len, body_len;
    char dest[256], header[256], *body;
    struct iovec iov[2];

    if (stomp_read_header(dest, sizeof(dest), msg, "destination:") < 0) {
        plog(LOG_ERR, "%s: destination header is not found", __func__);
        return -1;
    }

    if ((body = stomp_skip_command(msg->sm_buf)) == NULL) {
        plog(LOG_ERR, "%s: stomp_skip_command() failed", __func__);
        return -1;
    }

    header_len = stomp_make_message(header, sizeof(header), ++MessageId);
    body_len = (msg->sm_len - (body - msg->sm_buf));

    iov[0].iov_base = header;
    iov[0].iov_len = header_len;
    iov[1].iov_base = body;
    iov[1].iov_len = body_len;

    return stomp_enqueue(sf, dest, iov, NELEMS(iov));
}

static int
stomp_connected_subscribe(sf_t *sf, void *udata, stomp_msg_t *msg)
{
    char dest[256];

    if (stomp_read_header(dest, sizeof(dest), msg, "destination:") < 0) {
        plog(LOG_ERR, "%s: destination header is not found", __func__);
        return -1;
    }

    return stomp_subscribe(sf, dest);
}

static int
stomp_connected_unsubscribe(sf_t *sf, void *udata, stomp_msg_t *msg)
{
    char dest[256];

    if (stomp_read_header(dest, sizeof(dest), msg, "destination:") < 0) {
        plog(LOG_ERR, "%s: destination header is not found", __func__);
        return -1;
    }

    return stomp_unsubscribe(sf, dest);
}

static int
stomp_read_header(char *buf, int bufmax, stomp_msg_t *msg, char *key)
{
    char *p, *hdr;

    if ((hdr = stomp_find_header(msg, key)) == NULL)
        return -1;

    if ((p = strchr(hdr, ':')) == NULL)
        return -1;

    for (p++; isspace(*p); p++)
        ;

    while (*p != '\n' && *p != '\r' && *p != 0) {
        if (bufmax <= 1) 
            break;
        else {
            *buf = *p;
            buf++; p++;
        }
    }

    *buf = 0;

    return 0;
}

static char *
stomp_find_header(stomp_msg_t *msg, char *key)
{
    int i, len;

    for (i = 0; i < NELEMS(msg->sm_hdr) && msg->sm_hdr[i] != NULL; i++) {
        len = strlen(key);
        if (strncmp(msg->sm_hdr[i], key, len) == 0)
            return msg->sm_hdr[i];
    }

    return NULL;
}

static int
stomp_parse(stomp_msg_t *msg, char *buf, int len, stomp_command_t *cmdtable, int numtable)
{
    memset(msg, 0, sizeof(*msg));
    msg->sm_buf = buf;
    msg->sm_len = len;
    stomp_parse_lines(msg, buf, len);

    if (cmdtable != NULL) {
        if ((msg->sm_cmd = stomp_parse_command(buf, len, cmdtable, numtable)) == NULL)
            return -1;
    }

    return 0;
}

static stomp_command_t *
stomp_parse_command(char *buf, int len, stomp_command_t *cmdtable, int numtable)
{
    int i, l;

    for (i = 0; i < numtable; i++) {
        if ((l = strlen(cmdtable[i].sc_str)) > len)
            continue;
        if (strncmp(cmdtable[i].sc_str, buf, l) == 0)
            return &cmdtable[i];
    }

    return NULL;
}

static void
stomp_parse_lines(stomp_msg_t *msg, char *buf, int buflen)
{
    int count = 0;
    char *p, *next;

    for (p = stomp_next_line(buf, buflen); p != NULL; p = next) {
        next = stomp_next_line(p, buflen - (p - buf));

        if (count < NELEMS(msg->sm_hdr)) {
            if (stomp_is_header(p))
                msg->sm_hdr[count++] = p;
            else {
                msg->sm_body = p;
                return;
            }
        }
    }
}

static char *
stomp_next_line(char *buf, int buflen)
{
    int i, flag = 0;

    for (i = 0; i < buflen; i++) {
        if (buf[i] == '\r' || buf[i] == '\n') {
            flag = 1;
            continue;
        }

        if (flag) {
            switch (buf[i]) {
            case '\r':
            case '\n':
                continue;

            default:
                return &buf[i];
            }
        }
    }

    return NULL;
}

static char *
stomp_skip_command(char *buf)
{
    char *p;

    if ((p = strchr(buf, '\n')) == NULL)
        return NULL;

    return p + 1;
}

static int
stomp_is_header(char *buf)
{
    int i;

    for (i = 0; i < STOMP_HEADER_LEN_MAX; i++, buf++) {
        if (*buf == '\n' || *buf == '\r' || *buf == 0)
            break;
        if (*buf == ':')
            return 1;
    }

    return 0;
}

static int
stomp_make_connected(char *buf, int bufmax, unsigned session_id)
{
    return snprintf(buf, bufmax,
                    "CONNECTED\n"
                    "session-id:%u\n\n", session_id);
}

static int
stomp_make_message(char *buf, int bufmax, unsigned message_id)
{
    return snprintf(buf, bufmax,
                    "MESSAGE\n"
                    "message-id:%u\n", message_id);
}
