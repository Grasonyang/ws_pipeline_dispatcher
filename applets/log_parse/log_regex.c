#include "log_regex.h"

#include "libpipeline.h"

#include <stdlib.h>
#include <string.h>

int log_regex_split_fields(char *arg, log_t *log) {
    size_t count = 1;
    for (char *p = arg; *p != '\0'; ++p) {
        if (*p == ',') {
            ++count;
        }
    }

    log->names = calloc(count, sizeof(log->names[0]));
    log->values = calloc(count, sizeof(log->values[0]));
    if (log->names == NULL || log->values == NULL) {
        free(log->names);
        free(log->values);
        log->names = NULL;
        log->values = NULL;
        return -1;
    }

    size_t idx = 0;
    char *save = NULL;
    char *tok = strtok_r(arg, ",", &save);
    while (tok != NULL) {
        if (*tok == '\0') {
            return -1;
        }
        log->names[idx++] = tok;
        tok = strtok_r(NULL, ",", &save);
    }
    log->count = idx;
    return idx == count ? 0 : -1;
}

void log_regex_free_values(log_t *log) {
    for (size_t i = 0; i < log->count; ++i) {
        free(log->values[i]);
        log->values[i] = NULL;
    }
}

void log_regex_free(log_t *log) {
    log_regex_free_values(log);
    free(log->names);
    free(log->values);
}

int log_regex_parse_line(const char *line, regex_t *regex, log_t *log) {
    size_t nmatch = log->count + 1;
    regmatch_t *matches = calloc(nmatch, sizeof(matches[0]));
    if (matches == NULL) {
        return -1;
    }

    int rc = regexec(regex, line, nmatch, matches, 0);
    if (rc != 0) {
        free(matches);
        return 1;
    }

    for (size_t i = 0; i < log->count; ++i) {
        regmatch_t m = matches[i + 1];
        if (m.rm_so < 0 || m.rm_eo < m.rm_so) {
            log_regex_free_values(log);
            free(matches);
            return 1;
        }
        log->values[i] = lp_strndup(line + m.rm_so, (size_t)(m.rm_eo - m.rm_so));
        if (log->values[i] == NULL) {
            log_regex_free_values(log);
            free(matches);
            return -1;
        }
    }

    free(matches);
    return 0;
}
