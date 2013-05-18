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
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <pthread.h>
#include "sf_plog.h"

static void plog_print(int level, char *msg, va_list ap);
static void plog_syslog(int level, char *msg, va_list ap);

static int LogFlags;
static int MaskLevel = LOG_INFO;

void
plog(int level, char *msg, ...)
{
    va_list ap;

    if (level > MaskLevel)
        return;

    va_start(ap, msg);

    if (LogFlags & PLOG_SYSLOG)
        plog_syslog(level, msg, ap);
    else
        plog_print(level, msg, ap);

    va_end(ap);
}

void
plog_error(int level, const char *msg, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, msg);
    vsnprintf(buf, sizeof(buf), msg, ap);
    va_end(ap);

    plog(level, "%s: %s", buf, strerror(errno));
}

void
plog_setflag(int flag)
{
    LogFlags |= flag;
}

void
plog_setmask(int upto)
{
    MaskLevel = upto;
}

void
plog_openlog(char *ident)
{
    openlog(ident, 0, LOG_DAEMON);
    LogFlags |= PLOG_SYSLOG;
}

static void
plog_syslog(int level, char *msg, va_list ap)
{
    vsyslog(level, msg, ap);
}

static void
plog_print(int level, char *msg, va_list ap)
{
    char buf[256];
    time_t now;
    pthread_t self = pthread_self();
    struct tm t;
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_mutex_lock(&mutex);

    now = time(NULL);
    localtime_r(&now, &t);
    strftime(buf, sizeof(buf), "%F %T", &t);

    if (self == 0)
        printf("%s ", buf);
    else
        printf("%s 0x%08x ", buf, (unsigned) pthread_self());

    switch (level) {
    case LOG_CRIT:      printf("[CRIT] ");      break;
    case LOG_ERR:       printf("[ERR] ");       break;
    case LOG_WARNING:   printf("[WARNING] ");   break;
    case LOG_NOTICE:    printf("[NOTICE] ");    break;
    case LOG_INFO:      printf("[INFO] ");      break;
    case LOG_DEBUG:     printf("[DEBUG] ");     break;
    }

    vsnprintf(buf, sizeof(buf), msg, ap);
    puts(buf);

    pthread_mutex_unlock(&mutex);
}
