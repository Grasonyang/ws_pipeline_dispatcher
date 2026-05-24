#ifndef JSONL_CODEC_H
#define JSONL_CODEC_H

#include "dynamic_buffer.h"

#include <stddef.h>
#include <stdint.h>

int jsonl_get_string(const char *line, const char *key, char *out, size_t out_size);
int jsonl_get_int64(const char *line, const char *key, int64_t *out);
int jsonl_get_uint64(const char *line, const char *key, uint64_t *out);
int jsonl_get_bool(const char *line, const char *key, int *out);
int jsonl_write_string(dynamic_buffer_t *buf, const char *value);

#endif
