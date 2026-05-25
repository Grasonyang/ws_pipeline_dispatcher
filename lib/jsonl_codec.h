#ifndef JSONL_CODEC_H
#define JSONL_CODEC_H

#include "dynamic_buffer.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief JSON Lines field helpers backed by cJSON.
 *
 * Each getter parses one JSON object line and extracts one top-level key with
 * strict JSON type checks. Numbers are accepted only when they are integral and
 * within cJSON's exact integer range (-(2^53 - 1) to 2^53 - 1).
 *
 * @note These helpers are intentionally typed getters, not a general JSON tree
 * API. Use cJSON directly if nested traversal or batch extraction is needed.
 */

/**
 * @brief Copy a top-level JSON string field into a caller buffer.
 *
 * @param line NUL-terminated JSON object line.
 * @param key Case-sensitive field name to read.
 * @param out Destination buffer.
 * @param out_size Size of destination buffer in bytes.
 * @return 0 on success, -1 for missing key, non-string value, invalid JSON, or overflow.
 */
int jsonl_get_string(const char *line, const char *key, char *out, size_t out_size);

/**
 * @brief Read a top-level signed integer field.
 *
 * @param line NUL-terminated JSON object line.
 * @param key Case-sensitive field name to read.
 * @param out Destination for the parsed value.
 * @return 0 on success, -1 for missing key, non-number value, fractional value, or unsafe range.
 * @note cJSON stores numbers as double, so values outside ±(2^53 - 1) are rejected.
 */
int jsonl_get_int64(const char *line, const char *key, int64_t *out);

/**
 * @brief Read a top-level unsigned integer field.
 *
 * @param line NUL-terminated JSON object line.
 * @param key Case-sensitive field name to read.
 * @param out Destination for the parsed value.
 * @return 0 on success, -1 for missing key, negative value, non-number value, fractional value, or unsafe range.
 * @note cJSON stores numbers as double, so values above 2^53 - 1 are rejected.
 */
int jsonl_get_uint64(const char *line, const char *key, uint64_t *out);

/**
 * @brief Read a top-level JSON boolean field.
 *
 * @param line NUL-terminated JSON object line.
 * @param key Case-sensitive field name to read.
 * @param out Destination integer; receives 1 for true or 0 for false.
 * @return 0 on success, -1 for missing key, non-boolean value, or invalid JSON.
 */
int jsonl_get_bool(const char *line, const char *key, int *out);

/**
 * @brief Validate that a JSONL line is a JSON object.
 *
 * @param line NUL-terminated JSON line.
 * @return 1 if line parses to a JSON object, otherwise 0.
 */
int jsonl_is_object(const char *line);

/**
 * @brief Copy a top-level scalar field as text.
 *
 * Strings are copied without surrounding quotes. Numbers and booleans are
 * rendered as JSON text, e.g. 42, true, false.
 *
 * @param line NUL-terminated JSON object line.
 * @param key Case-sensitive field name to read.
 * @param out Destination buffer.
 * @param out_size Size of destination buffer in bytes.
 * @return 0 on success, -1 for missing key, object/array value, invalid JSON, or overflow.
 */
int jsonl_get_scalar_text(const char *line, const char *key, char *out, size_t out_size);

/**
 * @brief Append value as one escaped JSON string token.
 *
 * @param buf Destination buffer.
 * @param value NUL-terminated string to encode.
 * @return 0 on success, -1 on invalid args or allocation failure.
 * @note The appended token includes surrounding quotes.
 */
int jsonl_write_string(dynamic_buffer_t *buf, const char *value);

#endif
