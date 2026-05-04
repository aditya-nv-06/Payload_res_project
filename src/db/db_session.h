#ifndef DB_SESSION_H
#define DB_SESSION_H

#include "analysis/query_eval.h"

int db_session_run(const char *connstr,
                   const char *sql,
                   query_eval_ctx_t *eval);

#endif /* DB_SESSION_H */