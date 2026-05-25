#include "libpipeline.h"
#include "log_filter_expr.h"
#include "log_output_format.h"
#include "log_regex.h"

#include <ctype.h>
#include <getopt.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    regex_t regex;
    log_t log;
} regex_state_t;

static void print_usage(FILE *stream, const char *prog_name) {
    fprintf(stream, "Usage: %s [OPTIONS]\n\n", prog_name);
    fprintf(stream, "Description:\n");
    fprintf(stream, "  A lightweight structured log processor for UNIX pipelines.\n");
    fprintf(stream, "  Reads from stdin and writes to stdout. Diagnostic logs to stderr.\n\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -r, --regex <pattern>  Extract fields using POSIX extended regular expressions\n");
    fprintf(stream, "  -e, --fields <f1,f2>   Comma-separated field names for regex mode\n");
    fprintf(stream, "  -f, --filter <expr>    Filter expression: k=v, k!=v, k>v, or k~v\n");
    fprintf(stream, "      --format <fmt>     Set output format (json|csv|count) (default: json)\n");
    fprintf(stream, "  -h, --help             Show this help message and exit\n");
}

static void trim_line(char *line) {
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

static int parse_format(const char *arg, log_output_format_t *format) {
    if (arg == NULL) {
        return 0;
    }
    if (strcmp(arg, "json") == 0) {
        *format = LOG_OUTPUT_JSON;
    } else if (strcmp(arg, "csv") == 0) {
        *format = LOG_OUTPUT_CSV;
    } else if (strcmp(arg, "count") == 0) {
        *format = LOG_OUTPUT_COUNT;
    } else {
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    stream_logger_set_tag("log_parse");

    char *regex_pattern = NULL;
    char *fields_arg = NULL;
    char *format_arg = NULL;
    char *filter_kv = NULL;
    filter_t filter = {0};
    log_output_format_t format = LOG_OUTPUT_JSON;
    regex_state_t regex_state = {0};
    int regex_mode = 0;
    int regex_ready = 0;
    int exit_code = 0;
    size_t matched_count = 0;

    int opt;
    static struct option long_options[] = {
        {"regex",  required_argument, 0, 'r'},
        {"fields", required_argument, 0, 'e'},
        {"filter", required_argument, 0, 'f'},
        {"format", required_argument, 0, 1000},
        {"help",   no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "r:e:f:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'r':
                regex_pattern = optarg;
                break;
            case 'e':
                fields_arg = optarg;
                break;
            case 'f':
                filter_kv = optarg;
                if (log_filter_parse(filter_kv, &filter) != 0) {
                    LOG_ERROR("invalid filter syntax; expected key=value, key!=value, key>value, or key~value");
                    return 1;
                }
                break;
            case 1000:
                format_arg = optarg;
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

    if (parse_format(format_arg, &format) != 0) {
        LOG_ERROR("unsupported format=%s", format_arg);
        return 1;
    }

    if (regex_pattern != NULL) {
        regex_mode = 1;
        if (fields_arg == NULL) {
            LOG_ERROR("--fields is required with --regex");
            return 1;
        }
        if (log_regex_split_fields(fields_arg, &regex_state.log) != 0 || regex_state.log.count == 0) {
            LOG_ERROR("invalid --fields value");
            log_regex_free(&regex_state.log);
            return 1;
        }

        int rc = regcomp(&regex_state.regex, regex_pattern, REG_EXTENDED);
        if (rc != 0) {
            char err[256];
            regerror(rc, &regex_state.regex, err, sizeof(err));
            LOG_ERROR("invalid regex: %s", err);
            log_regex_free(&regex_state.log);
            return 1;
        }
        regex_ready = 1;
    } else if (fields_arg != NULL || (format_arg != NULL && format != LOG_OUTPUT_COUNT)) {
        LOG_ERROR("--fields and --format json|csv require --regex");
        return 1;
    }

    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, stdin) > 0) {
        trim_line(line);

        if (regex_mode) {
            int rc = log_regex_parse_line(line, &regex_state.regex, &regex_state.log);
            if (rc < 0) {
                LOG_ERROR("out of memory while parsing line");
                exit_code = 2;
                break;
            }
            if (rc > 0) {
                LOG_WARN("line did not match regex; skipping");
                continue;
            }

            if (log_filter_match_fields(&regex_state.log, &filter)) {
                if (format == LOG_OUTPUT_JSON) {
                    if (log_output_emit_json(&regex_state.log) != 0) {
                        LOG_ERROR("out of memory while formatting JSON output");
                        log_regex_free_values(&regex_state.log);
                        exit_code = 2;
                        break;
                    }
                } else if (format == LOG_OUTPUT_CSV) {
                    if (log_output_emit_csv(&regex_state.log) != 0) {
                        LOG_ERROR("out of memory while formatting CSV output");
                        log_regex_free_values(&regex_state.log);
                        exit_code = 2;
                        break;
                    }
                } else {
                    ++matched_count;
                }
            }
            log_regex_free_values(&regex_state.log);
        } else {
            int matched = 0;
            if (log_filter_match_jsonl(line, &filter, &matched) != 0) {
                LOG_WARN("malformed JSON line; skipping");
                continue;
            }
            if (matched) {
                if (format == LOG_OUTPUT_COUNT) {
                    ++matched_count;
                } else {
                    puts(line);
                }
            }
        }
    }

    if (ferror(stdin)) {
        LOG_ERROR("stdin read error");
        exit_code = 2;
    }

    free(line);
    if (exit_code == 0 && format == LOG_OUTPUT_COUNT) {
        printf("%zu\n", matched_count);
    }
    if (regex_ready) {
        regfree(&regex_state.regex);
    }
    if (regex_mode) {
        log_regex_free(&regex_state.log);
    }
    fflush(stdout);

    return exit_code;
}
