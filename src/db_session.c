#include "db_session.h"

#include <stdio.h>
#include <string.h>

#ifdef WITH_LIBPQ
#include <ctype.h>
#include <libpq-fe.h>
#endif

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
        fprintf(stderr, "[db] connection failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return 1;
    }

    fprintf(stderr, "[db] connected to PostgreSQL\n");

    int rc = 0;
    if (sql && sql[0]) {
        query_eval_run(eval, NULL, sql, 1, NULL, NULL, NULL);

        PGresult *res = PQexec(conn, sql);
        if (!res) {
            fprintf(stderr, "[db] query execution failed: %s\n",
                    PQerrorMessage(conn));
            rc = 1;
        } else {
            ExecStatusType status = PQresultStatus(res);
            if (status == PGRES_TUPLES_OK) {
                fprintf(stderr, "[db] rows=%d\n", PQntuples(res));
            } else if (status == PGRES_COMMAND_OK) {
                fprintf(stderr, "[db] command=%s\n", PQcmdStatus(res));
            } else {
                fprintf(stderr, "[db] query failed: %s\n",
                        PQresultErrorMessage(res));
                rc = 1;
            }
            PQclear(res);
        }

        PQfinish(conn);
        fprintf(stderr, "[db] disconnected\n");
        return rc;
    }

    char line[16384];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *query = trim_whitespace(line);

        if (*query == '\0')
            continue;
        if (is_disconnect_command(query)) {
            fprintf(stderr, "[db] disconnect requested\n");
            break;
        }

        query_eval_run(eval, NULL, query, 1, NULL, NULL, NULL);

        PGresult *res = PQexec(conn, query);
        if (!res) {
            fprintf(stderr, "[db] query execution failed: %s\n",
                    PQerrorMessage(conn));
            rc = 1;
            continue;
        }

        ExecStatusType status = PQresultStatus(res);
        if (status == PGRES_TUPLES_OK) {
            fprintf(stderr, "[db] rows=%d\n", PQntuples(res));
        } else if (status == PGRES_COMMAND_OK) {
            fprintf(stderr, "[db] command=%s\n", PQcmdStatus(res));
        } else {
            fprintf(stderr, "[db] query failed: %s\n",
                    PQresultErrorMessage(res));
            rc = 1;
        }
        PQclear(res);
    }

    PQfinish(conn);
    fprintf(stderr, "[db] disconnected\n");
    return rc;
#endif
}