#ifndef LOG_OUTPUT_FORMAT_H
#define LOG_OUTPUT_FORMAT_H

#include "log_parse.h"

/**
 * @brief Output modes supported by log_parse.
 */
typedef enum {
    /** Emit regex-extracted records as JSON objects. */
    LOG_OUTPUT_JSON = 0,
    /** Emit regex-extracted values as CSV rows. */
    LOG_OUTPUT_CSV = 1,
    /** Emit one final count of matching records. */
    LOG_OUTPUT_COUNT = 2
} log_output_format_t;

/**
 * @brief Emit one regex-extracted record as a JSON object.
 *
 * @param log Field/value container to format.
 * @return 0 on success, -1 on allocation or output formatting failure.
 */
int log_output_emit_json(const log_t *log);

/**
 * @brief Emit one regex-extracted record as a CSV row.
 *
 * Values are emitted in log->names order, but the header row is intentionally
 * omitted so the applet remains stream-friendly.
 *
 * @param log Field/value container to format.
 * @return 0 on success, -1 on allocation or output formatting failure.
 */
int log_output_emit_csv(const log_t *log);

#endif
