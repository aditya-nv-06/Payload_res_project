#ifndef DB_SESSION_H
#define DB_SESSION_H

#include "analysis/query_eval.h"

int db_session_run(const char *connstr,
                   const char *sql,
                   query_eval_ctx_t *eval);

/* Validate a SQL query using the pqCheck detection stack.
 * Returns 1 if the query is flagged (rule match or anomaly), 0 otherwise.
 * Optionally returns the detect_result_t and anomaly score via out parameters.
 */
int db_validate_query(query_eval_ctx_t *eval,
                      const char *sql,
                      detect_result_t *out_dres,
                      double *out_score);

#endif /* DB_SESSION_H */