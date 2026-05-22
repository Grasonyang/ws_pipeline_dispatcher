/*
 * pipeline_json.c -- shared JSON / line helpers.
 *
 * Extracted from stream_merge.c, log_parse.c, clip_store.c.
 * Behaviour is intentionally identical to the originals so callers
 * can be migrated one at a time without semantic surprises.
 */

#include "pipeline_json.h"
#include "libpipeline.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- line / string helpers ---------- */

void pipeline_trim_line(char *line)
{
    if (line == NULL) {
        return;
    }

    char *start = line;
    while (isspace((unsigned char)*start)) {
        ++start;
    }
    if (start != line) {
        memmove(line, start, strlen(start) + 1);
    }

    size_t len = strlen(line);
    while (len > 0 && isspace((unsigned char)line[len - 1])) {
        line[--len] = '\0';
    }
}

void pipeline_chomp(char *line)
{
    if (line == NULL) {
        return;
    }
    line[strcspn(line, "\r\n")] = '\0';
}

char *pipeline_strndup(const char *src, size_t len)
{
    char *out = malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, src, len);
    out[len] = '\0';
    return out;
}

/* ---------- minimal JSON-Lines lookups ---------- */

/*
 * Walk past `"key"` and the following ':' + spaces. Returns pointer to
 * the first byte of the value token on success, NULL on miss / syntax
 * error. The needle buffer matches the original applets (64/128 bytes);
 * we use 128 here so it works for all callers.
 */
const char *pipeline_json_find_field(const char *line, const char *key)
{
    if (line == NULL || key == NULL) {
        return NULL;
    }

    char needle[128];
    int n = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if (n < 0 || (size_t)n >= sizeof(needle)) {
        return NULL;
    }

    const char *p = strstr(line, needle);
    if (p == NULL) {
        return NULL;
    }
    p += (size_t)n;
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    if (*p != ':') {
        return NULL;
    }
    ++p;
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    return p;
}

char *pipeline_json_find_string_value(const char *line, const char *key)
{
    const char *p = pipeline_json_find_field(line, key);
    if (p == NULL || *p != '"') {
        return NULL;
    }
    ++p;  /* step over opening quote */

    pipeline_buffer_t out = {0};
    int escaped = 0;
    for (; *p != '\0'; ++p) {
        if (escaped) {
            if (pipeline_buffer_append_char(&out, *p) != 0) {
                pipeline_buffer_free(&out);
                return NULL;
            }
            escaped = 0;
        } else if (*p == '\\') {
            escaped = 1;
        } else if (*p == '"') {
            /* If we never appended anything, out.data is still NULL;
             * return an empty heap string instead so callers can free()
             * unconditionally. */
            if (out.data == NULL) {
                char *empty = malloc(1);
                if (empty == NULL) {
                    return NULL;
                }
                empty[0] = '\0';
                return empty;
            }
            return out.data;
        } else if (pipeline_buffer_append_char(&out, *p) != 0) {
            pipeline_buffer_free(&out);
            return NULL;
        }
    }

    /* unterminated string */
    pipeline_buffer_free(&out);
    return NULL;
}

char *pipeline_json_find_scalar_value(const char *line, const char *key)
{
    const char *p = pipeline_json_find_field(line, key);
    if (p == NULL) {
        return NULL;
    }
    if (*p == '"') {
        return pipeline_json_find_string_value(line, key);
    }

    const char *start = p;
    while (*p != '\0' && *p != ',' && *p != '}'
           && *p != ' ' && *p != '\t'
           && *p != '\n' && *p != '\r') {
        ++p;
    }
    if (p == start) {
        return NULL;
    }
    return pipeline_strndup(start, (size_t)(p - start));
}

int pipeline_json_looks_like_object(const char *line)
{
    if (line == NULL) {
        return 0;
    }

    const char *start = line;
    while (*start == ' ' || *start == '\t') {
        ++start;
    }
    if (*start != '{') {
        return 0;
    }

    const char *end = line + strlen(line);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
        --end;
    }
    return end > start && end[-1] == '}';
}
