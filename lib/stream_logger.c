/*
 * Minimal stderr logger shared by command-line applets.
 *
 * The logger avoids heap allocation and global configuration beyond a short
 * process tag, which keeps it safe to use from error paths.
 */

#include "stream_logger.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Process-level tag set once at startup, identifies the log source. */
static char g_tag[32] = "applet";

static const char *lvl_name(log_level_t lvl) {
    switch (lvl) {
        case LOG_LVL_DEBUG:
            return "DEBUG";
        case LOG_LVL_INFO:
            return "INFO";
        case LOG_LVL_WARN:
            return "WARN";
        case LOG_LVL_ERROR:
            return "ERROR";
        default:
            return "?";
    }
}

void stream_logger_set_tag(const char *tag) {
    if (tag == NULL)
        return;
    /* Reserve the last byte for NUL so overly long tags are safely truncated. */
    strncpy(g_tag, tag, sizeof(g_tag) - 1);
    g_tag[sizeof(g_tag) - 1] = '\0';
}

void stream_logger_log(log_level_t lvl, const char *fmt, ...) {
    struct timespec ts;
    struct tm tm;
    char timestamp_buf[32];

    clock_gettime(CLOCK_REALTIME, &ts);
    gmtime_r(&ts.tv_sec, &tm);
    strftime(timestamp_buf, sizeof(timestamp_buf), "%Y-%m-%dT%H:%M:%S", &tm);

    /* Emit an RFC3339-like UTC timestamp with millisecond precision. */
    fprintf(stderr, "%s.%03ldZ [%s] %s: ", timestamp_buf, ts.tv_nsec/1000000, lvl_name(lvl), g_tag);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);
}
