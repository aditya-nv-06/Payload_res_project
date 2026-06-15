#ifndef DETECTOR_H
#define DETECTOR_H

#include <regex.h>
#include <stddef.h>

/* -------------------------------------------------------------------------- */

#define MAX_RULES        256
#define MAX_RULE_NAME     64
#define MAX_RULE_PATTERN 512
#define MAX_MATCHES       64

typedef enum {
    RULE_KEYWORD = 0,
    RULE_REGEX   = 1
} rule_type_t;

typedef enum {
    SEV_LOW      = 1,
    SEV_MEDIUM   = 2,
    SEV_HIGH     = 3,
    SEV_CRITICAL = 4
} severity_t;

typedef struct {
    char         name[MAX_RULE_NAME];
    char         pattern[MAX_RULE_PATTERN];
    rule_type_t  type;
    severity_t   severity;
    regex_t      regex;          /* compiled only when type == RULE_REGEX */
    int          regex_compiled;
} rule_t;

typedef struct {
    char rule_name[MAX_RULE_NAME];
    int  offset;    /* byte offset of match in (lowercased) query */
    int  length;    /* match length in bytes */
} match_t;

typedef struct {
    int       matched;
    int       match_count;
    match_t   matches[MAX_MATCHES];
    severity_t max_severity;
    int       score;             /* sum of severity values across matches */
} detect_result_t;

typedef struct {
    rule_t rules[MAX_RULES];
    int    rule_count;
} detector_ctx_t;

/* -------------------------------------------------------------------------- */

/*
 * Load rules from the config file at 'path'.
 * Returns 0 on success, -1 on failure.
 */
int detector_load(detector_ctx_t *ctx, const char *path);

/*
 * Run all rules against 'sql' and populate *result.
 * The function works on a lowercase copy, so the original is not modified.
 */
void detector_check(detector_ctx_t  *ctx,
                    const char      *sql,
                    detect_result_t *result);

/* Return a human-readable severity string */
const char *detector_severity_str(severity_t sev);

/* Free compiled regexes */
void detector_free(detector_ctx_t *ctx);

#endif /* DETECTOR_H */
