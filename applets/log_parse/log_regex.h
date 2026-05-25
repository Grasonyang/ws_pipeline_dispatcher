#ifndef LOG_REGEX_H
#define LOG_REGEX_H

#include "log_parse.h"

#include <regex.h>

/**
 * @brief Parse comma-separated regex field names into a reusable log_t schema.
 *
 * Splits arg in place and stores each token in log->names. Allocates log->names
 * and log->values arrays sized to the number of fields.
 *
 * @param arg Mutable comma-separated field list, e.g. "ts,level,msg".
 * @param log Output field container to initialize.
 * @return 0 on success, -1 for invalid fields or allocation failure.
 * @note arg is modified in place by replacing separators with NUL bytes.
 */
int log_regex_split_fields(char *arg, log_t *log);

/**
 * @brief Capture one input line into the fields described by log.
 *
 * Executes regex against line and copies capture groups 1..log->count into
 * log->values. The full match at group 0 is ignored.
 *
 * @param line NUL-terminated input line.
 * @param regex Compiled POSIX extended regular expression.
 * @param log Field container initialized by log_regex_split_fields().
 * @return 0 on match, 1 when the line does not match or a capture is missing,
 *         -1 on allocation failure.
 * @note Caller must call log_regex_free_values() after consuming a successful
 * parse before reusing log for another line.
 */
int log_regex_parse_line(const char *line, regex_t *regex, log_t *log);

/**
 * @brief Release per-line capture values while preserving the field schema.
 *
 * @param log Field container whose values should be cleared.
 * @note This keeps log->names, log->values, and log->count intact for reuse.
 */
void log_regex_free_values(log_t *log);

/**
 * @brief Release all storage owned by a regex-mode log_t.
 *
 * Frees any remaining values plus the names and values arrays allocated by
 * log_regex_split_fields(). This function does not call regfree(); the caller
 * still owns the compiled regex_t.
 *
 * @param log Field container to release.
 */
void log_regex_free(log_t *log);

#endif
