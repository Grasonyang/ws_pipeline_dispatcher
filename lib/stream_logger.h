#ifndef STREAM_LOGGER_H
#define STREAM_LOGGER_H

typedef enum {
    LOG_LVL_DEBUG = 0,
    LOG_LVL_INFO  = 1,
    LOG_LVL_WARN  = 2,
    LOG_LVL_ERROR = 3
} log_level_t;

void stream_logger_set_tag(const char *tag);
void stream_logger_log(log_level_t lvl, const char *fmt, ...);

#define LOG_DEBUG(...) stream_logger_log(LOG_LVL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  stream_logger_log(LOG_LVL_INFO,  __VA_ARGS__)
#define LOG_WARN(...)  stream_logger_log(LOG_LVL_WARN,  __VA_ARGS__)
#define LOG_ERROR(...) stream_logger_log(LOG_LVL_ERROR, __VA_ARGS__)

#endif
