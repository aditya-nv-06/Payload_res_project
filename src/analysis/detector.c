/*
 * detector.c – Rule-based SQLi detection engine (Milestone 4)
 */
#include "analysis/detector.h"
#include "common/util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Rule loading                                                               */
/* -------------------------------------------------------------------------- */

static severity_t parse_severity(const char *s)
{
    if (strcasecmp(s, "CRITICAL") == 0) return SEV_CRITICAL;
    if (strcasecmp(s, "HIGH")     == 0) return SEV_HIGH;
    if (strcasecmp(s, "MEDIUM")   == 0) return SEV_MEDIUM;
    return SEV_LOW;
}

int detector_load(detector_ctx_t *ctx, const char *path)
{
    memset(ctx, 0, sizeof(*ctx));

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "[detector] cannot open rules file: %s\n", path);
        return -1;
    }

    char line[1024];
    int  lineno = 0;

    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        /* Strip trailing newline */
        line[strcspn(line, "\r\n")] = '\0';

        /* Skip comments and blank lines */
        if (line[0] == '#' || line[0] == '\0') continue;

        if (ctx->rule_count >= MAX_RULES) {
            fprintf(stderr, "[detector] rule limit (%d) reached at line %d\n",
                    MAX_RULES, lineno);
            break;
        }

        /* Parse: name|type|pattern|severity */
        char name[MAX_RULE_NAME], type_str[16],
             pattern[MAX_RULE_PATTERN], sev_str[16];

        /* Use strtok_r for safe parsing */
        char *saveptr, *tok;
        char  tmp[1024];
        strncpy(tmp, line, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';

        tok = strtok_r(tmp, "|", &saveptr);
        if (!tok) continue;
        strncpy(name, tok, sizeof(name) - 1); name[sizeof(name)-1] = '\0';

        tok = strtok_r(NULL, "|", &saveptr);
        if (!tok) continue;
        strncpy(type_str, tok, sizeof(type_str) - 1);

        tok = strtok_r(NULL, "|", &saveptr);
        if (!tok) continue;
        strncpy(pattern, tok, sizeof(pattern) - 1); pattern[sizeof(pattern)-1] = '\0';

        tok = strtok_r(NULL, "|", &saveptr);
        if (!tok) continue;
        strncpy(sev_str, tok, sizeof(sev_str) - 1);

        rule_t *r = &ctx->rules[ctx->rule_count];
        memset(r, 0, sizeof(*r));
        util_strlcpy(r->name,    name,    sizeof(r->name));
        util_strlcpy(r->pattern, pattern, sizeof(r->pattern));
        r->severity = parse_severity(sev_str);

        if (strcasecmp(type_str, "REGEX") == 0) {
            r->type = RULE_REGEX;
            /* Compile as case-insensitive extended regex */
            char lower_pat[MAX_RULE_PATTERN];
            util_strlcpy(lower_pat, pattern, sizeof(lower_pat));
            int ret = regcomp(&r->regex, lower_pat,
                              REG_EXTENDED | REG_ICASE | REG_NEWLINE);
            if (ret != 0) {
                char errbuf[128];
                regerror(ret, &r->regex, errbuf, sizeof(errbuf));
                fprintf(stderr, "[detector] regex compile error in rule '%s': %s\n",
                        name, errbuf);
                continue;   /* skip bad rule */
            }
            r->regex_compiled = 1;
        } else {
            r->type = RULE_KEYWORD;
            /* Lower-case the pattern for case-insensitive comparison */
            util_strlower(r->pattern);
        }

        ctx->rule_count++;
    }

    fclose(fp);
    fprintf(stderr, "[detector] loaded %d rules from %s\n",
            ctx->rule_count, path);
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Detection                                                                  */
/* -------------------------------------------------------------------------- */

void detector_check(detector_ctx_t  *ctx,
                    const char      *sql,
                    detect_result_t *result)
{
    memset(result, 0, sizeof(*result));

    if (!sql || !*sql) return;

    /* Working copy for case-insensitive keyword matching */
    size_t  slen  = strlen(sql);
    char   *lower = malloc(slen + 1);
    if (!lower) return;
    memcpy(lower, sql, slen + 1);
    util_strlower(lower);

    for (int i = 0; i < ctx->rule_count; i++) {
        rule_t *r = &ctx->rules[i];
        if (result->match_count >= MAX_MATCHES) break;

        if (r->type == RULE_KEYWORD) {
            /* Case-insensitive substring search */
            const char *hit = strstr(lower, r->pattern);
            if (hit) {
                match_t *m = &result->matches[result->match_count++];
                util_strlcpy(m->rule_name, r->name, sizeof(m->rule_name));
                m->offset = (int)(hit - lower);
                m->length = (int)strlen(r->pattern);
                result->matched = 1;
                if (r->severity > result->max_severity)
                    result->max_severity = r->severity;
                result->score += (int)r->severity;
            }
        } else {
            /* POSIX regex match against original sql (regex compiled REG_ICASE) */
            regmatch_t rm;
            if (regexec(&r->regex, sql, 1, &rm, 0) == 0) {
                match_t *m = &result->matches[result->match_count++];
                util_strlcpy(m->rule_name, r->name, sizeof(m->rule_name));
                m->offset = (int)rm.rm_so;
                m->length = (int)(rm.rm_eo - rm.rm_so);
                result->matched = 1;
                if (r->severity > result->max_severity)
                    result->max_severity = r->severity;
                result->score += (int)r->severity;
            }
        }
    }

    free(lower);
}

const char *detector_severity_str(severity_t sev)
{
    switch (sev) {
    case SEV_CRITICAL: return "CRITICAL";
    case SEV_HIGH:     return "HIGH";
    case SEV_MEDIUM:   return "MEDIUM";
    default:           return "LOW";
    }
}

void detector_free(detector_ctx_t *ctx)
{
    for (int i = 0; i < ctx->rule_count; i++) {
        rule_t *r = &ctx->rules[i];
        if (r->regex_compiled) {
            regfree(&r->regex);
            r->regex_compiled = 0;
        }
    }
    ctx->rule_count = 0;
}
