/*
 * stream_merge.c -- sidecar-driven session clip cutter (v2.1, GRA-24).
 *
 * Reads {session_id}.meta.jsonl to discover chunk byte ranges in the
 * session-level {session_id}.bin buffer, then emits clip metadata JSON
 * Lines to stdout based on a fixed time window and continuity rules.
 *
 * [v2.1 Upgrade]: Now actually extracts binary data using pread() 
 * and writes physical .bin clip files before emitting JSON metadata.
 */

#include "libpipeline.h"
#include "pipeline_json.h"
#include "stream_logger.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <getopt.h>

#define DEFAULT_CLIP_MS  5000
#define DEFAULT_IDLE_MS  2000

/* ── meta record ─────────────────────────────────────────────────────── */

typedef struct {
    char     kind[16];
    uint64_t seq;
    uint64_t offset;
    uint64_t length;
    int64_t  ts_ms;
    int      valid;
} meta_record_t;

static int parse_meta_record(const char *line, size_t len, meta_record_t *out)
{
    (void)len;
    memset(out, 0, sizeof(*out));

    /* kind */
    const char *p = pipeline_json_find_field(line, "kind");
    if (!p || *p != '"') return -1;
    ++p;
    size_t ki = 0;
    while (*p && *p != '"' && ki + 1 < sizeof(out->kind))
        out->kind[ki++] = *p++;
    out->kind[ki] = '\0';


    p = pipeline_json_find_field(line, "sequence");
    if (!p) return -1;
    char *end;
    out->seq = (uint64_t)strtoull(p, &end, 10);
    if (end == p) return -1;

    /* offset */
    p = pipeline_json_find_field(line, "offset");
    if (!p) return -1;
    out->offset = (uint64_t)strtoull(p, &end, 10);
    if (end == p) return -1;

    /* length */
    p = pipeline_json_find_field(line, "length");
    if (!p) return -1;
    out->length = (uint64_t)strtoull(p, &end, 10);
    if (end == p) return -1;

    /* ts_ms */
    p = pipeline_json_find_field(line, "ts_ms");
    if (!p) return -1;
    out->ts_ms = (int64_t)strtoll(p, &end, 10);
    if (end == p) return -1;

    out->valid = 1;
    return 0;
}

/* ── clip FSM state ──────────────────────────────────────────────────── */

typedef struct {
    int      active;
    int64_t  start_ts_ms;
    int64_t  end_ts_ms;
    uint64_t start_offset;
    uint64_t total_length;
    uint64_t next_seq;
    uint64_t next_offset;
    int64_t  last_chunk_wall_ms;
} clip_state_t;

static void clip_reset(clip_state_t *s)
{
    memset(s, 0, sizeof(*s));
}

/* v2.1 核心：傳入 bin_fd 進行實體檔案抽取 */
static int emit_clip(const clip_state_t *s, const char *src,
                     const char *session, int complete, int bin_fd)
{
    if (!s->active) return 0;

    long ts_sec = (long)(s->start_ts_ms / 1000);
    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/%s_%ld.bin", src, session, ts_sec);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        LOG_ERROR("path too long for session %s", session);
        return -1;
    }

    /* ── v2.1 Binary Extraction Logic (Pread) ── */
    if (s->total_length > 0) {
        int out_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) {
            LOG_ERROR("failed to create clip file %s: %s", path, strerror(errno));
            return -1;
        }

        char buf[8192];
        uint64_t remain = s->total_length;
        uint64_t current_offset = s->start_offset;

        while (remain > 0) {
            size_t to_read = remain < sizeof(buf) ? (size_t)remain : sizeof(buf);
            ssize_t nread = pread(bin_fd, buf, to_read, (off_t)current_offset);
            if (nread < 0) {
                if (errno == EINTR) continue;
                LOG_ERROR("failed to read bin file at offset %" PRIu64 ": %s", current_offset, strerror(errno));
                close(out_fd);
                return -1;
            }
            if (nread == 0) {
                LOG_WARN("unexpected EOF in bin file at offset %" PRIu64, current_offset);
                break; 
            }

            char *wptr = buf;
            ssize_t nwrite = nread;
            while (nwrite > 0) {
                ssize_t nw = write(out_fd, wptr, (size_t)nwrite);
                if (nw < 0) {
                    if (errno == EINTR) continue;
                    LOG_ERROR("failed to write to clip file %s: %s", path, strerror(errno));
                    close(out_fd);
                    return -1;
                }
                wptr += nw;
                nwrite -= nw;
            }
            remain -= (uint64_t)nread;
            current_offset += (uint64_t)nread;
        }
        close(out_fd);
    }
    /* ────────────────────────────────── */

    int rc = printf(
        "{\"type\":\"clip\","
        "\"session_id\":\"%s\","
        "\"ts\":%ld,"
        "\"path\":\"%s\","
        "\"offset\":%" PRIu64 ","
        "\"length\":%" PRIu64 ","
        "\"complete\":%s}\n",
        session, ts_sec, path,
        s->start_offset, s->total_length,
        complete ? "true" : "false");

    if (rc < 0) return -1;
    return fflush(stdout) == EOF ? -1 : 0;
}

/* ── FSM transitions ─────────────────────────────────────────────────── */

static int process_meta_record(const meta_record_t *rec, clip_state_t *s,
                                const char *src, const char *session,
                                int64_t clip_ms, int bin_fd)
{
    int64_t now_ms = pipeline_get_monotonic_time_ms();

    if (!s->active) {
        s->active           = 1;
        s->start_ts_ms      = rec->ts_ms;
        s->end_ts_ms        = rec->ts_ms;
        s->start_offset     = rec->offset;
        s->total_length     = rec->length;
        s->next_seq         = rec->seq + 1;
        s->next_offset      = rec->offset + rec->length;
        s->last_chunk_wall_ms = now_ms;
        return 0;
    }

    int seq_ok    = (rec->seq    == s->next_seq);
    int offset_ok = (rec->offset == s->next_offset);

    if (!seq_ok || !offset_ok) {
        LOG_WARN("continuity break seq=%" PRIu64 " (expected %" PRIu64
                 ") offset=%" PRIu64 " (expected %" PRIu64 ")",
                 rec->seq, s->next_seq, rec->offset, s->next_offset);
        if (emit_clip(s, src, session, 0, bin_fd) != 0) return -1;
        clip_reset(s);

        s->active           = 1;
        s->start_ts_ms      = rec->ts_ms;
        s->end_ts_ms        = rec->ts_ms;
        s->start_offset     = rec->offset;
        s->total_length     = rec->length;
        s->next_seq         = rec->seq + 1;
        s->next_offset      = rec->offset + rec->length;
        s->last_chunk_wall_ms = now_ms;
        return 0;
    }

    int64_t span = rec->ts_ms - s->start_ts_ms;
    if (span >= clip_ms) {
        if (emit_clip(s, src, session, 1, bin_fd) != 0) return -1;
        clip_reset(s);
        s->active             = 1;
        s->start_ts_ms        = rec->ts_ms;
        s->end_ts_ms          = rec->ts_ms;
        s->start_offset       = rec->offset;
        s->total_length       = rec->length;
        s->next_seq           = rec->seq + 1;
        s->next_offset        = rec->offset + rec->length;
        s->last_chunk_wall_ms = now_ms;
        return 0;
    }

    s->end_ts_ms      = rec->ts_ms;
    s->total_length  += rec->length;
    s->next_seq       = rec->seq + 1;
    s->next_offset    = rec->offset + rec->length;
    s->last_chunk_wall_ms = now_ms;
    return 0;
}

static int drain_meta(int meta_fd, pipeline_buffer_t *meta_buf,
                      clip_state_t *s, const char *src, const char *session,
                      int64_t clip_ms, int bin_fd)
{
    char chunk[4096];
    for (;;) {
        ssize_t got = read(meta_fd, chunk, sizeof(chunk));
        if (got < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return -1;
        }
        if (got == 0) break;
        if (pipeline_buffer_append_mem(meta_buf, chunk, (size_t)got) != 0)
            return -1;
    }

    for (;;) {
        char *nl = memchr(meta_buf->data, '\n', meta_buf->len);
        if (!nl) break;
        size_t line_len = (size_t)(nl - meta_buf->data);

        char saved = *nl;
        *nl = '\0';

        if (line_len > 0) {
            meta_record_t rec;
            if (parse_meta_record(meta_buf->data, line_len, &rec) == 0
                    && rec.valid
                    && strcmp(rec.kind, "data") == 0) {
                if (process_meta_record(&rec, s, src, session, clip_ms, bin_fd) != 0) {
                    *nl = saved;
                    return -1;
                }
            } else if (line_len > 0) {
                LOG_WARN("skipping meta line: %.80s", meta_buf->data);
            }
        }

        *nl = saved;
        size_t consumed = line_len + 1;
        size_t rest = meta_buf->len - consumed;
        memmove(meta_buf->data, meta_buf->data + consumed, rest);
        meta_buf->len = rest;
        meta_buf->data[rest] = '\0';
    }
    return 0;
}

static int check_idle(clip_state_t *s, int64_t idle_ms,
                      const char *src, const char *session, int bin_fd)
{
    if (!s->active) return 0;
    int64_t elapsed = pipeline_get_monotonic_time_ms() - s->last_chunk_wall_ms;
    if (elapsed >= idle_ms) {
        LOG_INFO("idle timeout after %" PRId64 "ms, emitting partial clip", elapsed);
        if (emit_clip(s, src, session, 0, bin_fd) != 0) return -1;
        clip_reset(s);
    }
    return 0;
}

/* ── helpers ─────────────────────────────────────────────────────────── */

static int path_join(char *out, size_t sz, const char *dir, const char *name)
{
    int n = snprintf(out, sz, "%s/%s", dir, name);
    return n >= 0 && (size_t)n < sz ? 0 : -1;
}

static int sentinel_exists(const char *src)
{
    char path[PATH_MAX];
    if (path_join(path, sizeof(path), src, PIPELINE_SENTINEL_NAME) != 0)
        return 0;
    return access(path, F_OK) == 0;
}

static void consume_inotify(int fd, int *saw_sentinel)
{
    char buf[4096];
    for (;;) {
        ssize_t got = read(fd, buf, sizeof(buf));
        if (got <= 0) {
            if (got < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                LOG_WARN("inotify read: %s", strerror(errno));
            return;
        }
        for (ssize_t off = 0;
             off + (ssize_t)sizeof(struct inotify_event) <= got;) {
            const struct inotify_event *ev =
                (const struct inotify_event *)(buf + off);
            if (ev->len > 0 && pipeline_path_is_sentinel(ev->name))
                *saw_sentinel = 1;
            off += (ssize_t)sizeof(struct inotify_event) + (ssize_t)ev->len;
        }
    }
}

static void print_usage(FILE *stream, const char *prog_name) {
    fprintf(stream, "Usage: %s [OPTIONS] <session_id> <src_dir>\n\n", prog_name);
    fprintf(stream, "Description:\n");
    fprintf(stream, "  Synchronizes and merges a growing binary stream with its metadata sidecar.\n");
    fprintf(stream, "  Extracts structured byte ranges and emits valid JSON Lines to stdout.\n\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "      --clip-secs <n>    Target clip duration in seconds (default: 5.0)\n");
    fprintf(stream, "      --idle-secs <n>    Idle timeout before emitting partial clip (default: 2.0)\n");
    fprintf(stream, "  -h, --help             Show this help message and exit\n");
}

/* ── main ────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    stream_logger_set_tag("stream_merge");

    int64_t clip_ms      = DEFAULT_CLIP_MS;
    int64_t idle_ms      = DEFAULT_IDLE_MS;
    const char *src      = NULL;   // <--- 補上這行
    const char *session  = NULL;
    int opt;
    static struct option long_options[] = {
        {"clip-secs", required_argument, 0, 1000},
        {"idle-secs", required_argument, 0, 1001},
        {"src",       required_argument, 0, 1002}, // 補上這個
        {"session",   required_argument, 0, 1003}, // 補上這個
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
        switch (opt) {
            case 1000:
                clip_ms = (int64_t)(atof(optarg) * 1000.0);
                break;
            case 1001:
                idle_ms = (int64_t)(atof(optarg) * 1000.0);
                break;
            case 1002: 
                src = optarg; 
                break;       // 抓取 --src
            case 1003: 
                session = optarg; 
                break;
            case 'h':
                print_usage(stdout, argv[0]);
                exit(EXIT_SUCCESS);
            case '?':
                print_usage(stderr, argv[0]);
                exit(EXIT_FAILURE);
            default:
                exit(EXIT_FAILURE);
        }
    }
    if (!session && optind < argc) session = argv[optind++];
    if (!src && optind < argc) src = argv[optind++];
    if (!session || !src) {
        fprintf(stderr, "Error: Missing required arguments <session_id> and/or <src_dir>\n\n");
        print_usage(stderr, argv[0]);
        exit(EXIT_FAILURE);
    }
    //if (!session || !src) return 1;
    /*if (optind + 2 > argc) {
        fprintf(stderr, "Error: Missing required arguments <session_id> and/or <src_dir>\n\n");
        print_usage(stderr, argv[0]);
        exit(EXIT_FAILURE);
    }*/

    //const char *session = argv[optind];
    //const char *src     = argv[optind + 1];

    /* open .bin */
    char bin_path[PATH_MAX], meta_path[PATH_MAX];
    {
        char bin_name[PATH_MAX], meta_name[PATH_MAX];
        if (snprintf(bin_name,  sizeof(bin_name),  "%s.bin",        session) < 0 ||
            snprintf(meta_name, sizeof(meta_name), "%s.meta.jsonl", session) < 0 ||
            path_join(bin_path,  sizeof(bin_path),  src, bin_name)  != 0 ||
            path_join(meta_path, sizeof(meta_path), src, meta_name) != 0) {
            LOG_ERROR("path construction failed");
            return 1;
        }
    }

    int bin_fd = open(bin_path, O_RDONLY);
    if (bin_fd < 0) {
        LOG_ERROR("open %s: %s", bin_path, strerror(errno));
        return 1;
    }

    int dir_wd = -1;
    int dir_fd = pipeline_open_dir_watch(src, &dir_wd);
    if (dir_fd < 0) {
        LOG_ERROR("inotify dir watch failed: %s", strerror(errno));
        close(bin_fd);
        return 1;
    }

    int meta_fd = open(meta_path, O_RDONLY);
    if (meta_fd < 0 && errno == ENOENT) {
        LOG_INFO("meta file not yet present, waiting up to 5s");
        int64_t deadline = pipeline_get_monotonic_time_ms() + 5000;
        while (meta_fd < 0) {
            int64_t remaining = deadline - pipeline_get_monotonic_time_ms();
            if (remaining <= 0) {
                LOG_ERROR("timed out waiting for %s", meta_path);
                close(dir_fd);
                close(bin_fd);
                return 1;
            }
            struct pollfd wpfd = { .fd = dir_fd, .events = POLLIN };
            int prc = poll(&wpfd, 1, (int)remaining);
            if (prc < 0) {
                if (errno == EINTR) continue;
                LOG_ERROR("poll waiting for meta: %s", strerror(errno));
                close(dir_fd);
                close(bin_fd);
                return 1;
            }
            if (prc > 0) {
                char ibuf[4096];
                ssize_t nr = read(dir_fd, ibuf, sizeof(ibuf));
                (void)nr;
            }
            meta_fd = open(meta_path, O_RDONLY);
        }
        LOG_INFO("meta file appeared");
    } else if (meta_fd < 0) {
        LOG_ERROR("open %s: %s", meta_path, strerror(errno));
        close(dir_fd);
        close(bin_fd);
        return 1;
    }

    int meta_wd = -1;
    int meta_wfd = pipeline_open_file_watch(meta_path, &meta_wd);
    if (meta_wfd < 0) {
        LOG_ERROR("inotify meta watch failed: %s", strerror(errno));
        close(dir_fd);
        close(meta_fd);
        close(bin_fd);
        return 1;
    }

    LOG_INFO("inotify ready");

    pipeline_buffer_t meta_buf = {0};
    clip_state_t      clip     = {0};
    int saw_sentinel = sentinel_exists(src);
    int rc = 0;

    /* Pass bin_fd through the call chain */
    if (drain_meta(meta_fd, &meta_buf, &clip, src, session, clip_ms, bin_fd) != 0)
        rc = 1;

    while (rc == 0 && !saw_sentinel) {
        int64_t now = pipeline_get_monotonic_time_ms();
        int64_t timeout = 1000;
        if (clip.active) {
            int64_t idle_left = idle_ms - (now - clip.last_chunk_wall_ms);
            if (idle_left < timeout)
                timeout = idle_left > 0 ? idle_left : 0;
        }

        struct pollfd pfds[3] = {
            { .fd = meta_wfd, .events = POLLIN },
            { .fd = dir_fd,   .events = POLLIN },
            { .fd = -1,       .events = 0      },
        };
        int prc = poll(pfds, 2, (int)timeout);
        if (prc < 0) {
            if (errno == EINTR) continue;
            LOG_ERROR("poll: %s", strerror(errno));
            rc = 1;
            break;
        }

        if (pfds[0].revents & POLLIN) {
            int ignored = 0;
            consume_inotify(meta_wfd, &ignored);
            if (drain_meta(meta_fd, &meta_buf, &clip, src, session, clip_ms, bin_fd) != 0) {
                rc = 1;
                break;
            }
        }
        if (pfds[1].revents & POLLIN)
            consume_inotify(dir_fd, &saw_sentinel);

        if (sentinel_exists(src))
            saw_sentinel = 1;

        if (check_idle(&clip, idle_ms, src, session, bin_fd) != 0) {
            rc = 1;
            break;
        }
    }

    if (rc == 0) {
        if (drain_meta(meta_fd, &meta_buf, &clip, src, session, clip_ms, bin_fd) != 0)
            rc = 1;
    }

    if (rc == 0 && clip.active) {
        if (emit_clip(&clip, src, session, 1, bin_fd) != 0)
            rc = 1;
        clip_reset(&clip);
    }

    pipeline_buffer_free(&meta_buf);
    close(meta_wfd);
    close(dir_fd);
    close(meta_fd);
    close(bin_fd);
    return rc;
}