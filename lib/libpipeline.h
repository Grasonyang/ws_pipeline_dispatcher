/*
 * libpipeline.h
 * version: v2.3
 *
 * Shared low-level helpers used by all applets:
 *   - inotify directory/file watching
 *   - monotonic time
 *   - dynamic byte buffer
 *   - sentinel filename detection
 *
 * See .docs/core/overview.md for the repo-level contract.
 */

#ifndef LIBPIPELINE_H
#define LIBPIPELINE_H

#include <stddef.h>
#include <stdint.h>

#include "dynamic_buffer.h"
#include "jsonl_codec.h"
#include "stream_logger.h"

// Sentinel filename created by the upstream writer when a session is complete.
#define PIPELINE_SENTINEL_NAME ".pipeline_end"

/**
 * @brief Open an inotify fd that watches a directory for completed writes.
 *
 * Watches at least IN_CLOSE_WRITE | IN_MOVED_TO. Callers own the returned fd
 * and must close it when the watch is no longer needed.
 *
 * @param dir_path absolute or relative directory path; must already exist
 * @param watch_descriptor out: inotify_add_watch() watch descriptor
 *
 * @return inotify fd (>= 0), or -1 on failure with errno set.
 */
int lp_watch_dir(const char *dir_path, int *watch_descriptor);

/**
 * @brief Open an inotify fd that watches a file for append/modify events.
 *
 * Watches at least IN_MODIFY. IN_MODIFY lets callers observe append/growing-file
 * updates before the writer closes the fd; IN_CLOSE_WRITE reports writer-close
 * completion. Callers own the returned fd and must close it when the watch is
 * no longer needed.
 *
 * @param file_path absolute or relative file path; must already exist
 * @param watch_descriptor out: inotify_add_watch() watch descriptor
 *
 * @return inotify fd (>= 0), or -1 on failure with errno set.
 */
int lp_watch_file(const char *file_path, int *watch_descriptor);

/**
 * @brief Read the current monotonic time in milliseconds.
 *
 * Uses CLOCK_MONOTONIC so it is not affected by wall-clock changes.
 *
 * @return monotonic timestamp in milliseconds, or 0 if unavailable.
 */
int64_t pipeline_get_monotonic_time_ms(void);

/**
 * @brief Check whether a filename/path refers to the pipeline sentinel.
 *
 * Compares the basename against PIPELINE_SENTINEL_NAME and does not read file contents.
 *
 * @param filename filename or path to check.
 * @return 1 for sentinel, 0 otherwise.
 */
int lp_is_completed_session(const char *filename);

/**
 * @brief Build a path by joining dir and name with a '/' separator.
 *
 * @param out   output buffer
 * @param sz    size of output buffer
 * @param dir   directory path (no trailing slash required)
 * @param name  filename to append
 * @return 0 on success, -1 if the result would overflow or args are NULL.
 */
int lp_build_artifact_path(char *out, size_t sz, const char *dir, const char *name);

/**
 * @brief Duplicate at most len bytes of src into a newly allocated string.
 *
 * Equivalent to POSIX strndup(). Always NUL-terminates the result.
 *
 * @param src  source buffer (need not be NUL-terminated within len bytes)
 * @param len  number of bytes to copy
 * @return pointer to new string, or NULL on allocation failure or NULL src.
 */
char *lp_strndup(const char *src, size_t len);

#endif
