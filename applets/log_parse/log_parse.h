#ifndef LOG_PARSE_H
#define LOG_PARSE_H

#include <stddef.h>

/**
 * @brief Field/value view used by log_parse regex mode.
 *
 * names is the reusable field schema parsed from --fields. values contains the
 * per-line capture results produced by log_regex_parse_line(). Both arrays use
 * count as their length, and values[i] corresponds to names[i].
 *
 * @note names entries point into the mutable --fields buffer passed to
 * log_regex_split_fields(); only the names array itself is owned here.
 * @note values entries are allocated per matched input line and should be
 * released with log_regex_free_values() before parsing the next line.
 */
typedef struct {
    /** Field names parsed from --fields. */
    char **names;
    /** Captured field values for the current input line. */
    char **values;
    /** Number of entries in names and values. */
    size_t count;
} log_t;

#endif
