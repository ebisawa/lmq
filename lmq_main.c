/*
 * Copyright (c) 2011-2013 Satoshi Ebisawa. All rights reserved.
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
#include <signal.h>
#include "libsf/sf.h"
#include "stomp/stomp_proto.h"

#define PROG_NAME  "leanmqd"

static void parse_args(int argc, char *argv[]);
static void usage(void);
static int init(void);
static void init_signal(void);
static void signal_handler(int signum);

static int Debug;
static sf_instance_t SFInstance;

int
main(int argc, char *argv[])
{
    parse_args(argc, argv);

    if (!Debug) {
        if (daemon(0, 0) < 0) {
            plog(LOG_ERR, "error: daemon() failed");
            return EXIT_FAILURE;
        }

        plog_openlog(PROG_NAME);
    }

    if (init() < 0)
        return EXIT_FAILURE;

    sf_main(&SFInstance);

    return EXIT_SUCCESS;
}

static void
parse_args(int argc, char *argv[])
{
    int i;

    for (i = 1; i < argc; i++) {
        if (*argv[i] == '-') {
            switch (*++argv[i]) {
            case 'd':
                plog_setmask(LOG_DEBUG);
                Debug = 1;
                break;
            case 'h':
                usage();
                break;

            default:
                plog(LOG_ERR, "error: invalid option: %s", argv[i]);
                usage();
                break;
            }
        }
    }
}

static void
usage(void)
{
    printf("usage: %s [options..]\n", PROG_NAME);
    puts("options:  -c [filename]   configuration file name");
    puts("          -d              debug");
    exit(EXIT_FAILURE);
}

static int
init(void)
{
    sf_sockaddr_t addr;

    if (sf_util_str2sa((struct sockaddr *) &addr, "0.0.0.0", STOMP_PORT) < 0) {
        plog(LOG_ERR, "sf_util_str2sa() failed");
        return -1;
    }

    if (sf_init(&SFInstance) < 0) {
        plog(LOG_ERR, "sf_init() failed");
        return -1;
    }

    if (sf_tcp_listen(&SFInstance, (struct sockaddr *) &addr, &StompProtoCB) < 0) {
        plog(LOG_ERR, "sf_tcp_listen() failed");
        return -1;
    }

    init_signal();

    return 0;
}

static void
init_signal(void)
{
    signal(SIGHUP, signal_handler);
    signal(SIGPIPE, SIG_IGN);
}

static void
signal_handler(int signum)
{
    switch (signum) {
    case SIGHUP:
        break;
    case SIGTERM:
        break;
    }
}
