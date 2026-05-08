#ifndef AUDIT_H
#define AUDIT_H

#include <stdio.h>
#include <stddef.h>

#define AUDIT_MAX_FINDINGS 512
#define AUDIT_PATH_MAX_LEN 512
#define AUDIT_TEXT_MAX_LEN 256

typedef struct {
    char severity[8];
    char category[32];
    char title[AUDIT_TEXT_MAX_LEN];
    char file[AUDIT_PATH_MAX_LEN];
    int line;
    char snippet[AUDIT_TEXT_MAX_LEN];
    char recommendation[AUDIT_TEXT_MAX_LEN];
} audit_finding_t;

typedef struct {
    size_t total_files_scanned;
    size_t total_findings;
    size_t high_count;
    size_t medium_count;
    size_t low_count;
    int truncated;
    audit_finding_t findings[AUDIT_MAX_FINDINGS];
} audit_report_t;

void audit_report_init(audit_report_t *report);

int audit_run(const char *root,
              int include_docs,
              const char *json_out,
              audit_report_t *report,
              FILE *out);

#ifdef WITH_LIBPQ
/* Run a system-level PostgreSQL audit by connecting to the server via libpq.
 * If `pg_connstr` is NULL, libpq will use environment defaults (PGHOST/PGUSER/etc.)
 */
int audit_run_postgres(const char *pg_connstr,
                       const char *json_out,
                       audit_report_t *report,
                       FILE *out);
#endif

#endif /* AUDIT_H */
