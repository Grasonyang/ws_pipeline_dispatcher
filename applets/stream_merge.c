/*
 * stream_merge.c -- growing blob reader for clip JSON objects.
 */

#include "libpipeline.h"
#include "stream_logger.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s --src <src_dir> --session <session_id>\n", prog);
}

static int path_join(char *out, size_t out_size, const char *dir, const char *name)
{
    int n = snprintf(out, out_size, "%s/%s", dir, name);
    return n >= 0 && (size_t)n < out_size ? 0 : -1;
}

static int sentinel_exists(const char *src)
{
    char path[PATH_MAX];
    if (path_join(path, sizeof(path), src, PIPELINE_SENTINEL_NAME) != 0) {
        return 0;
    }
    return access(path, F_OK) == 0;
}

static void consume_inotify_fd(int fd, int *saw_sentinel)
{
    char buf[4096];
    for (;;) {
        ssize_t got = read(fd, buf, sizeof(buf));
        if (got < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            return;
        }
        if (got == 0) {
            return;
        }
        for (ssize_t off = 0; off + (ssize_t)sizeof(struct inotify_event) <= got;) {
            const struct inotify_event *event = (const struct inotify_event *)(buf + off);
            if (event->len > 0 && pipeline_path_is_sentinel(event->name)) {
                *saw_sentinel = 1;
            }
            off += (ssize_t)sizeof(struct inotify_event) + (ssize_t)event->len;
        }
    }
}

static int looks_like_clip(const char *json, size_t len)
{
    const char *needle = "\"type\"";
    const char *clip = "\"clip\"";
    for (size_t i = 0; i + strlen(needle) <= len; ++i) {
        if (memcmp(json + i, needle, strlen(needle)) == 0) {
            for (size_t j = i + strlen(needle); j + strlen(clip) <= len && j < i + 80; ++j) {
                if (memcmp(json + j, clip, strlen(clip)) == 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int emit_complete_objects(pipeline_buffer_t *buf)
{
    size_t start = 0;
    int have_start = 0;
    int depth = 0;
    int in_string = 0;
    int escaped = 0;

    for (size_t i = 0; i < buf->len; ++i) {
        char c = buf->data[i];
        if (!have_start) {
            if (c == '{') {
                start = i;
                have_start = 1;
                depth = 1;
            }
            continue;
        }

        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (c == '\\') {
                escaped = 1;
            } else if (c == '"') {
                in_string = 0;
            }
            continue;
        }

        if (c == '"') {
            in_string = 1;
        } else if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0) {
                size_t len = i - start + 1;
                if (looks_like_clip(buf->data + start, len)) {
                    if (fwrite(buf->data + start, 1, len, stdout) != len || fputc('\n', stdout) == EOF) {
                        return -1;
                    }
                }
                size_t rest = buf->len - i - 1;
                memmove(buf->data, buf->data + i + 1, rest);
                buf->len = rest;
                buf->data[buf->len] = '\0';
                i = (size_t)-1;
                have_start = 0;
                depth = 0;
                in_string = 0;
                escaped = 0;
            }
        }
    }
    return fflush(stdout);
}

static int drain_file(int fd, pipeline_buffer_t *buf)
{
    char chunk[4096];
    for (;;) {
        ssize_t got = read(fd, chunk, sizeof(chunk));
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (got == 0) {
            return emit_complete_objects(buf);
        }
        if (pipeline_buffer_append_mem(buf, chunk, (size_t)got) != 0 || emit_complete_objects(buf) != 0) {
            return -1;
        }
    }
}

int main(int argc, char *argv[])
{
    stream_logger_set_tag("stream_merge");

    const char *src = NULL;
    const char *session = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--src") == 0 && i + 1 < argc) {
            src = argv[++i];
        } else if (strcmp(argv[i], "--session") == 0 && i + 1 < argc) {
            session = argv[++i];
        } else {
            usage(argv[0]);
            return 1;
        }
    }
    if (src == NULL || session == NULL) {
        usage(argv[0]);
        return 1;
    }

    char file_name[PATH_MAX];
    char file_path[PATH_MAX];
    if (snprintf(file_name, sizeof(file_name), "%s.bin", session) < 0 ||
        strlen(session) + 5 >= sizeof(file_name) ||
        path_join(file_path, sizeof(file_path), src, file_name) != 0) {
        LOG_ERROR("source path too long");
        return 1;
    }

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        LOG_ERROR("open %s failed: %s", file_path, strerror(errno));
        return 1;
    }

    int dir_wd = -1;
    int dir_fd = pipeline_open_dir_watch(src, &dir_wd);
    if (dir_fd < 0) {
        LOG_ERROR("watch %s failed: %s", src, strerror(errno));
        close(fd);
        return 1;
    }
    int file_wd = -1;
    int watch_fd = pipeline_open_file_watch(file_path, &file_wd);
    if (watch_fd < 0) {
        LOG_ERROR("watch %s failed: %s", file_path, strerror(errno));
        close(dir_fd);
        close(fd);
        return 1;
    }

    pipeline_buffer_t buf = {0};
    int saw_sentinel = sentinel_exists(src);
    int rc = 0;
    if (drain_file(fd, &buf) != 0) {
        rc = 1;
    }

    while (rc == 0 && !saw_sentinel) {
        struct pollfd pfds[2] = {
            { .fd = watch_fd, .events = POLLIN },
            { .fd = dir_fd, .events = POLLIN }
        };
        int prc = poll(pfds, 2, 1000);
        if (prc < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_ERROR("poll failed: %s", strerror(errno));
            rc = 1;
            break;
        }
        if (pfds[0].revents != 0) {
            int ignored = 0;
            consume_inotify_fd(watch_fd, &ignored);
            if (drain_file(fd, &buf) != 0) {
                rc = 1;
            }
        }
        if (pfds[1].revents != 0) {
            consume_inotify_fd(dir_fd, &saw_sentinel);
        }
        if (sentinel_exists(src)) {
            saw_sentinel = 1;
        }
    }

    if (rc == 0 && drain_file(fd, &buf) != 0) {
        rc = 1;
    }

    pipeline_buffer_free(&buf);
    close(watch_fd);
    close(dir_fd);
    close(fd);
    return rc;
}
