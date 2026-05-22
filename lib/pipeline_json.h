/*
 * pipeline_json.h -- shared JSON / line helpers extracted from
 *                    stream_merge.c, log_parse.c, clip_store.c
 *
 * Intentionally minimal: these are NOT a real JSON parser. They are the
 * small bag of tricks the three applets were each re-implementing.
 *
 * Depends on libpipeline.h (pipeline_buffer_t) for json_find_string_value.
 */
#ifndef PIPELINE_JSON_H
#define PIPELINE_JSON_H

#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- line / string helpers ---------- */

/*
 * In-place trim of leading and trailing ASCII whitespace.
 * Safe on NULL (no-op). Modifies the buffer.
 */
void pipeline_trim_line(char *line);

/*
 * In-place strip of a single trailing '\n' or '\r\n' (or '\r').
 * Equivalent to:  line[strcspn(line, "\r\n")] = '\0';
 * Provided as a name so the intent is obvious at call sites.
 */
void pipeline_chomp(char *line);

/*
 * Duplicate the first `len` bytes of `src` into a new NUL-terminated
 * heap string. Returns NULL on allocation failure.
 * (This is the old xstrndup() from log_parse.c.)
 */
char *pipeline_strndup(const char *src, size_t len);

/* ---------- minimal JSON-Lines lookups ---------- */

/*
 * Locate the value token for `"key":` in `line`. Returns a pointer INTO
 * `line` at the first non-space character of the value (which may be '"',
 * a digit, 't', 'f', 'n', '{', '['), or NULL if the key was not found
 * or the syntax was unexpected.
 *
 * This is the find_field() helper from stream_merge.c. Use it when you
 * need to read a numeric / bool / nested value yourself.
 */
const char *pipeline_json_find_field(const char *line, const char *key);

/*
 * Extract a JSON string value for `key` from `line`. The result is a
 * freshly allocated, NUL-terminated string with backslash escapes
 * collapsed to their following byte (i.e. "\\n" -> 'n', not newline --
 * this matches the original applets' behaviour; it is *not* a real JSON
 * string decoder).
 *
 * Returns NULL if the key is missing, the value is not a string, or on
 * allocation / unterminated-string failure. Caller frees with free().
 */
char *pipeline_json_find_string_value(const char *line, const char *key);

/*
 * Extract any scalar value (string or unquoted token) for `key` from
 * `line`, as a freshly allocated NUL-terminated string. For string
 * values this behaves like pipeline_json_find_string_value(); for
 * numbers / booleans / null it returns the raw token text.
 *
 * Returns NULL on missing key, empty token, or allocation failure.
 * Caller frees with free().
 */
char *pipeline_json_find_scalar_value(const char *line, const char *key);

/*
 * Heuristic check: does `line` (after trimming surrounding spaces/tabs)
 * begin with '{' and end with '}'? Used by log_parse.c to filter
 * obviously-not-JSON input lines before attempting key lookup.
 */
int pipeline_json_looks_like_object(const char *line);

#ifdef __cplusplus
}
#endif

#endif /* PIPELINE_JSON_H */
