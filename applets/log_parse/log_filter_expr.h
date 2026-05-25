#ifndef LOG_FILTER_EXPR_H
#define LOG_FILTER_EXPR_H

#include "log_parse.h"

/**
 * @brief Operators supported by log_parse filter expressions.
 */
typedef enum {
    /** Exact string equality: key=value. */
    FILTER_EQUALS = 0,
    /** Exact string inequality: key!=value. */
    FILTER_NOT_EQUALS,
    /** Strict signed integer comparison: key>value. */
    FILTER_GREATER_THAN,
    /** Substring match: key~value. */
    FILTER_CONTAINS
} filter_operator_t;

/**
 * @brief Parsed filter expression.
 *
 * key and value point into the mutable expression buffer passed to
 * log_filter_parse(). The filter does not own those strings.
 */
typedef struct {
    /** Field name to read from a parsed log or JSON object. */
    char *key;
    /** Comparison value parsed from the expression. */
    char *value;
    /** Operator selected by expression syntax. */
    filter_operator_t op;
} filter_t;

/**
 * @brief Parse a filter expression into key, operator, and value.
 *
 * Supports key=value, key!=value, key>value, and key~value. The expression is
 * split in place and the parsed filter points into expr.
 *
 * @param expr Mutable filter expression.
 * @param filter Output filter descriptor.
 * @return 0 on success, -1 for invalid syntax or empty key/value.
 */
int log_filter_parse(char *expr, filter_t *filter);

/**
 * @brief Apply a filter to fields extracted by regex mode.
 *
 * @param log Parsed field/value container.
 * @param filter Filter to apply, or NULL to match all records.
 * @return 1 if the record matches, 0 otherwise.
 */
int log_filter_match_fields(const log_t *log, const filter_t *filter);

/**
 * @brief Apply a filter to one JSON Lines object.
 *
 * JSONL mode uses this helper for pass-through and count output. Scalar JSON
 * values are compared as text except for FILTER_GREATER_THAN, which requires an
 * integral JSON number.
 *
 * @param line NUL-terminated JSON object line.
 * @param filter Filter to apply, or NULL to match all objects.
 * @param matched Output flag set to 1 when the object matches.
 * @return 0 when line is valid JSON and matching completed, -1 for malformed
 * JSON or invalid arguments.
 */
int log_filter_match_jsonl(const char *line, const filter_t *filter, int *matched);

#endif
