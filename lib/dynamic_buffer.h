#ifndef DYNAMIC_BUFFER_H
#define DYNAMIC_BUFFER_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    int failed;
} dynamic_buffer_t;

void dynamic_buffer_free(dynamic_buffer_t *buf);
void dynamic_buffer_reset(dynamic_buffer_t *buf);
int dynamic_buffer_has_failed(const dynamic_buffer_t *buf);
int dynamic_buffer_reserve(dynamic_buffer_t *buf, size_t extra);
int dynamic_buffer_append_char(dynamic_buffer_t *buf, char c);
int dynamic_buffer_append_str(dynamic_buffer_t *buf, const char *s);
int dynamic_buffer_append_mem(dynamic_buffer_t *buf, const void *src, size_t len);

#endif
