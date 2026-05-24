#ifndef STREAM_LOGGER_H
#define STREAM_LOGGER_H

/**
 * @brief Severity label emitted in the logger prefix.
 */
typedef enum {
    LOG_LVL_DEBUG = 0,
    LOG_LVL_INFO  = 1,
    LOG_LVL_WARN  = 2,
    LOG_LVL_ERROR = 3
} log_level_t;

/**
 * @brief Set the process-level tag shown in subsequent log lines.
 *
 * @param tag Tag string to copy into logger state.
 * @note Passing NULL is ignored. Long tags are truncated to fit the internal buffer.
 */
void stream_logger_set_tag(const char *tag);

/**
 * @brief Write one timestamped printf-style diagnostic line to stderr.
 *
 * @param lvl Severity label for the emitted line.
 * @param fmt printf-style format string.
 * @param ... Arguments consumed by fmt.
 * @note The logger appends one trailing newline.
 */
void stream_logger_log(log_level_t lvl, const char *fmt, ...);

/**
 * @brief Convenience wrappers that preserve printf-style call sites.
 */
#define LOG_DEBUG(...) stream_logger_log(LOG_LVL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  stream_logger_log(LOG_LVL_INFO,  __VA_ARGS__)
#define LOG_WARN(...)  stream_logger_log(LOG_LVL_WARN,  __VA_ARGS__)
#define LOG_ERROR(...) stream_logger_log(LOG_LVL_ERROR, __VA_ARGS__)

#endif
