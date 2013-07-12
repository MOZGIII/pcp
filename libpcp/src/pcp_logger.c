/*
 * Copyright (c) 2013 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "pcp.h"
#include "pcp_logger.h"
#ifdef _MSC_VER
#include "pcp_gettimeofday.h" //gettimeofday()
#else
#include "sys/time.h"
#endif //_MSC_VER

pcp_debug_mode_t pcp_log_level = PCP_MAX_LOG_LEVEL;

static void default_logfn(pcp_debug_mode_t mode, const char* msg)
{

    const char* prefix;
    static struct timeval prev_timestamp = { 0, 0 };
    struct timeval cur_timestamp;
    uint64_t diff;

    gettimeofday(&cur_timestamp, NULL);

    if ((prev_timestamp.tv_sec == 0) && (prev_timestamp.tv_usec == 0)) {
        prev_timestamp = cur_timestamp;
        diff = 0;
    } else {
        diff = (cur_timestamp.tv_sec - prev_timestamp.tv_sec) * 1000000
                + (cur_timestamp.tv_usec - prev_timestamp.tv_usec);
    }

    switch (mode) {
    case PCP_DEBUG_ERR:
    case PCP_DEBUG_PERR:
        prefix = "ERROR";
        break;
    case PCP_DEBUG_WARN:
        prefix = "WARNING";
        break;
    case PCP_DEBUG_INFO:
        prefix = "INFO";
        break;
    case PCP_DEBUG_DEBUG:
        prefix = "DEBUG";
        break;
    default:
        prefix = "UNKNOWN";
        break;
    }

    fprintf(stderr, "%3llus %03llums %03lluus %-7s: %s\n",
            (long long int)diff / 1000000,
            (long long int)(diff % 1000000) / 1000,
            (long long int)diff % 1000, prefix, msg);
}

external_logger logger = default_logfn;

void pcp_set_loggerfn(external_logger ext_log)
{
    logger = ext_log;
}

void pcp_logger(pcp_debug_mode_t log_level, const char* fmt, ...)
{
    int n;
    int size = 256; /* Guess we need no more than 256 bytes. */
    char *p, *np;
    va_list ap;

    if (log_level > pcp_log_level) {
        return;
    }

    if ((p = (char*) malloc(size)) == NULL) {
        return; //LCOV_EXCL_LINE
    }

    while (1) {

        /* Try to print in the allocated space. */

        va_start(ap, fmt);
        n = vsnprintf(p, size, fmt, ap);
        va_end(ap);

        /* If that worked, return the string. */

        if (n > -1 && n < size) {
            if (logger) {
                (*logger)(log_level, p);
                free(p);
                return;
            } else {
                printf("%s \n", p);
                free(p);
                return;
            }
        }

        /* Else try again with more space. */

        if (n > -1) /* glibc 2.1 */
            size = n + 1; /* precisely what is needed */
        else
            /* glibc 2.0 */
            size *= 2; /* twice the old size */  //LCOV_EXCL_LINE

        if ((np = (char*) realloc(p, size)) == NULL) {
            free(p);  //LCOV_EXCL_LINE
            return;   //LCOV_EXCL_LINE
        } else {
            p = np;
        }
    }

    free(p);
}

/* Wrapper function for srerror_s and strerror_r functions */
/* char *buf is set to zero with memset at the beginning of the function,
 *  so the contents from the previous use of the variable would be anulated */
/* @return: Function returns bufffer buf containing error message */
void pcp_strerror(int errnum, char *buf, size_t buflen)
{

    memset(buf, 0, buflen);

#ifdef WIN32

    strerror_s(buf, buflen, errnum);

#else //WIN32
    //according to strerror_r man page, the XSI-compliant function is used
    // if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
#if ((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! defined(_GNU_SOURCE))
    {
        strerror_r(errnum, buf, buflen);
    }
#else
    {
        strerror_r(errnum, buf, buflen);
    }
#endif // ((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE)
#endif //WIN32
}
