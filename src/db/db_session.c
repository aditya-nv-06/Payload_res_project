#include "db/db_session.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common/logger.h"

#ifdef WITH_LIBPQ
#include <ctype.h>
#include <libpq-fe.h>
#endif

/* Helper: validate a query using the detector and ngram model. Returns 1 if
 * the query is flagged (rule match or anomaly), 0 otherwise. This helper is
 * available regardless of libpq support so callers (and tests) can exercise
 * the validation logic without connecting to a database.
 */
int db_validate_query(query_eval_ctx_t *eval,
                      const char *sql,
                      detect_result_t *out_dres,
                      double *out_score)
{
    if (!eval || !sql) return 0;

    detect_result_t dres;
    memset(&dres, 0, sizeof(dres));
    double ascore = 0.0;

    if (eval->detector)
        detector_check(eval->detector, sql, &dres);

    if (eval->ngram && eval->ngram_loaded)
        ascore = ngram_score(eval->ngram, sql);

    if (out_dres) *out_dres = dres;
    if (out_score) *out_score = ascore;

    int flagged = dres.matched || (eval->ngram_loaded && eval->anomaly_threshold != 0.0 && ascore < eval->anomaly_threshold);
    return flagged;
}

#ifdef WITH_LIBPQ
static char *trim_whitespace(char *text)
{
    while (*text && isspace((unsigned char)*text)) text++;
    if (*text == '\0') return text;

    char *end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return text;
}

static int is_disconnect_command(const char *text)
{
    return strcmp(text, "\\q") == 0 ||
           strcmp(text, "\\disconnect") == 0 ||
           strcmp(text, "disconnect") == 0 ||
           strcmp(text, "quit") == 0 ||
           strcmp(text, "exit") == 0;
}
#endif

int db_session_run(const char *connstr,
                   const char *sql,
                   query_eval_ctx_t *eval)
{
#ifndef WITH_LIBPQ
    (void)connstr;
    (void)sql;
    (void)eval;
    fprintf(stderr, "[db] built without libpq support\n");
    return 1;
#else
    if (!connstr || !eval) return 1;

    PGconn *conn = PQconnectdb(connstr);
    if (PQstatus(conn) != CONNECTION_OK) {
        log_error("[db] connection failed: %s", PQerrorMessage(conn));
        PQfinish(conn);
        logger_set_module(NULL);
        return 1;
    }

    logger_set_module("db.session");
    log_info("[db] connected to PostgreSQL");

    int rc = 0;
    if (sql && sql[0]) {
        /* Pre-execute validation */
        detect_result_t dres;
        double ascore = 0.0;
        if (eval && eval->detector)
            detector_check(eval->detector, sql, &dres);
        if (eval && eval->ngram_loaded && eval->ngram)
            ascore = ngram_score(eval->ngram, sql);

        int flagged = dres.matched || (eval && eval->ngram_loaded && eval->anomaly_threshold != 0.0 && ascore < eval->anomaly_threshold);
        if (flagged) {
            log_warn("Query flagged by pqCheck: rules=%d severity=%s anomaly=%.3f sql=%.80s",
                     dres.match_count,
                     detector_severity_str(dres.max_severity),
                     ascore, sql);
            if (eval)
                query_eval_run(eval, NULL, sql, 1, NULL, NULL, NULL);

            const char *block = getenv("PQCHECK_BLOCK_ON_DETECT");
            if (block && (block[0] == '1' || strcasecmp(block, "true") == 0)) {
                log_warn("Execution blocked by PQCHECK_BLOCK_ON_DETECT for query: %.80s", sql);
                PQfinish(conn);
                logger_set_module(NULL);
                return 0;
            }
        }

        /* AUDIT_OK: session mode intentionally executes operator-provided SQL verbatim. */
        PGresult *res = PQexec(conn, sql);
        if (!res) {
            log_error("[db] query execution failed: %s", PQerrorMessage(conn));
            rc = 1;
        } else {
            ExecStatusType status = PQresultStatus(res);
            if (status == PGRES_TUPLES_OK) {
                log_info("[db] rows=%d", PQntuples(res));
            } else if (status == PGRES_COMMAND_OK) {
                log_info("[db] command=%s", PQcmdStatus(res));
            } else {
                log_error("[db] query failed: %s", PQresultErrorMessage(res));
                rc = 1;
            }
            PQclear(res);
        }

        PQfinish(conn);
        log_info("[db] disconnected");
        logger_set_module(NULL);
        return rc;
    }

    char line[16384];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *query = trim_whitespace(line);

        if (*query == '\0')
            continue;
        if (is_disconnect_command(query)) {
            log_info("[db] disconnect requested");
            break;
        }

        /* Pre-execute validation */
        detect_result_t dres;
        double ascore = 0.0;
        if (eval && eval->detector)
            detector_check(eval->detector, query, &dres);
        if (eval && eval->ngram_loaded && eval->ngram)
            ascore = ngram_score(eval->ngram, query);

        int flagged = dres.matched || (eval && eval->ngram_loaded && eval->anomaly_threshold != 0.0 && ascore < eval->anomaly_threshold);
        if (flagged) {
            log_warn("Query flagged by pqCheck: rules=%d severity=%s anomaly=%.3f sql=%.80s",
                     dres.match_count,
                     detector_severity_str(dres.max_severity),
                     ascore, query);
            if (eval)
                query_eval_run(eval, NULL, query, 1, NULL, NULL, NULL);

            const char *block = getenv("PQCHECK_BLOCK_ON_DETECT");
            if (block && (block[0] == '1' || strcasecmp(block, "true") == 0)) {
                    log_warn("Execution blocked by PQCHECK_BLOCK_ON_DETECT for query: %.80s", query);
                    continue;
                }
        }

        /* AUDIT_OK: interactive session intentionally executes operator-provided SQL verbatim. */
        PGresult *res = PQexec(conn, query);
        if (!res) {
            log_error("[db] query execution failed: %s", PQerrorMessage(conn));
            rc = 1;
            continue;
        }

        ExecStatusType status = PQresultStatus(res);
        if (status == PGRES_TUPLES_OK) {
            log_info("[db] rows=%d", PQntuples(res));
        } else if (status == PGRES_COMMAND_OK) {
            log_info("[db] command=%s", PQcmdStatus(res));
        } else {
            log_error("[db] query failed: %s", PQresultErrorMessage(res));
            rc = 1;
        }
        PQclear(res);
    }

    PQfinish(conn);
    log_info("[db] disconnected");
    logger_set_module(NULL);
    return rc;
#endif
}