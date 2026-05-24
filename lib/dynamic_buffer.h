#ifndef DYNAMIC_BUFFER_H
#define DYNAMIC_BUFFER_H

#include <stddef.h>

/**
 * @brief Growable byte buffer used by applets and codecs.
 *
 * The buffer is always NUL-terminated after successful append/reserve calls,
 * even when callers append arbitrary bytes with dynamic_buffer_append_mem().
 *
 * @note Callers may inspect data as a C string only when appended content does
 * not contain embedded NUL bytes.
 * @note Once an operation fails, failed is latched until dynamic_buffer_reset()
 * or dynamic_buffer_free() is called.
 */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
    int failed;
} dynamic_buffer_t;

/**
 * @brief Release owned memory and reset all fields to the zero state.
 *
 * @param buf Buffer to release. Passing NULL is allowed.
 */
void dynamic_buffer_free(dynamic_buffer_t *buf);

/**
 * @brief Clear contents while keeping allocated storage.
 *
 * @param buf Buffer to reset. Passing NULL is allowed.
 * @note This clears the failure latch and preserves capacity when data exists.
 */
void dynamic_buffer_reset(dynamic_buffer_t *buf);

/**
 * @brief Check whether a buffer is in the latched failure state.
 *
 * @param buf Buffer to inspect. Passing NULL returns 0.
 * @return 1 if failed is set, otherwise 0.
 */
int dynamic_buffer_has_failed(const dynamic_buffer_t *buf);

/**
 * @brief Ensure room for extra bytes plus the trailing NUL terminator.
 *
 * @param buf Buffer to grow.
 * @param extra Number of additional payload bytes required.
 * @return 0 on success, -1 on invalid state, overflow, or allocation failure.
 * @note On failure, the buffer enters the latched failure state.
 */
int dynamic_buffer_reserve(dynamic_buffer_t *buf, size_t extra);

/**
 * @brief Append a single byte and preserve trailing NUL termination.
 *
 * @param buf Destination buffer.
 * @param c Byte to append.
 * @return 0 on success, -1 on reserve failure.
 */
int dynamic_buffer_append_char(dynamic_buffer_t *buf, char c);

/**
 * @brief Append a NUL-terminated string.
 *
 * @param buf Destination buffer.
 * @param s String to append. Must not be NULL.
 * @return 0 on success, -1 on invalid args or reserve failure.
 */
int dynamic_buffer_append_str(dynamic_buffer_t *buf, const char *s);

/**
 * @brief Append an exact byte range.
 *
 * @param buf Destination buffer.
 * @param src Source bytes. May be NULL only when len is zero.
 * @param len Number of bytes to append.
 * @return 0 on success, -1 on invalid args or reserve failure.
 * @note Embedded NUL bytes are preserved and counted in len.
 */
int dynamic_buffer_append_mem(dynamic_buffer_t *buf, const void *src, size_t len);

#endif
