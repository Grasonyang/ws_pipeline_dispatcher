/**
 * stream_logger.c - Simple logging module
 * Provides standardized log output with timestamps and log levels
 */

#include "stream_logger.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Process-level tag set once at startup, identifies the log source */
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
    /* Copy tag to global variable, reserve last byte for null terminator */
    strncpy(g_tag, tag, sizeof(g_tag) - 1);
    g_tag[sizeof(g_tag) - 1] = '\0';
}

void stream_logger_log(log_level_t lvl, const char *fmt, ...) {
    struct timespec ts;
    struct tm tm;
    char timestamp_buf[32];  // Buffer for formatted timestamp

    /* Get current system time with millisecond precision */
    clock_gettime(CLOCK_REALTIME, &ts);
    /* Convert seconds since epoch to broken-down time */
    gmtime_r(&ts.tv_sec, &tm);
    /* Format time as ISO 8601 format (YYYY-MM-DDTHH:MM:SS) */
    strftime(timestamp_buf, sizeof(timestamp_buf), "%Y-%m-%dT%H:%M:%S", &tm);
    /* Output log header: timestamp.milliseconds [level] tag: */
    fprintf(stderr, "%s.%03ldZ [%s] %s: ", timestamp_buf, ts.tv_nsec/1000000, lvl_name(lvl), g_tag);

    /* Process variadic argument list and output the actual log content */
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    /* Add newline at end of log message */
    fputc('\n', stderr);
}
