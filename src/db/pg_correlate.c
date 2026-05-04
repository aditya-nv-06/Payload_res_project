/*
 * pg_correlate.c – PostgreSQL host correlation via libpq (Milestone 6)
 *
 * Compiled only when WITH_LIBPQ=1.
 */
#include "db/pg_correlate.h"
#include "common/util.h"

#ifdef WITH_LIBPQ
#include <libpq-fe.h>
#endif

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */

struct pg_correlate_ctx {
#ifdef WITH_LIBPQ
    PGconn *conn;
#endif
    char connstr[256];
};

/* -------------------------------------------------------------------------- */

pg_correlate_ctx_t *pg_correlate_open(const char *connstr)
{
#ifndef WITH_LIBPQ
    (void)connstr;
    fprintf(stderr, "[pg_correlate] built without libpq support\n");
    return NULL;
#else
    pg_correlate_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    util_strlcpy(ctx->connstr, connstr, sizeof(ctx->connstr));

    ctx->conn = PQconnectdb(connstr);
    if (PQstatus(ctx->conn) != CONNECTION_OK) {
        fprintf(stderr, "[pg_correlate] connection failed: %s\n",
                PQerrorMessage(ctx->conn));
        PQfinish(ctx->conn);
        free(ctx);
        return NULL;
    }
    fprintf(stderr, "[pg_correlate] connected to PostgreSQL\n");
    return ctx;
#endif
}

int pg_correlate_lookup(pg_correlate_ctx_t *ctx,
                        const flow_t       *flow,
                        pg_row_t           *row)
{
    if (!ctx || !flow || !row) return 0;

#ifndef WITH_LIBPQ
    (void)ctx; (void)flow;
    memset(row, 0, sizeof(*row));
    return 0;
#else
    memset(row, 0, sizeof(*row));

    /* Reconnect if connection was lost */
    if (PQstatus(ctx->conn) != CONNECTION_OK) {
        PQreset(ctx->conn);
        if (PQstatus(ctx->conn) != CONNECTION_OK) {
            fprintf(stderr, "[pg_correlate] reconnect failed: %s\n",
                    PQerrorMessage(ctx->conn));
            return 0;
        }
    }

    /* Convert flow src_ip (client) to dotted-decimal string */
    char client_ip[INET_ADDRSTRLEN];
    struct in_addr addr;
    addr.s_addr = htonl(flow->src_ip);
    inet_ntop(AF_INET, &addr, client_ip, sizeof(client_ip));

    char client_port_str[8];
    snprintf(client_port_str, sizeof(client_port_str), "%u", flow->src_port);

    /*
     * Query pg_stat_activity for a matching row.
     * We match on client_addr and client_port; if that finds nothing we fall
     * back to the most-recently-active non-idle row.
     */
    const char *paramValues[2] = { client_ip, client_port_str };
    const char *sql =
        "SELECT pid::text, usename, datname, "
        "       client_addr::text, client_port::text, "
        "       state, wait_event_type || ':' || coalesce(wait_event,'') AS wait_event, "
        "       left(query, 1000), "
        "       to_char(query_start,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') "
        "FROM pg_stat_activity "
        "WHERE client_addr = $1::inet "
        "  AND client_port = $2::int "
        "  AND state != 'idle' "
        "ORDER BY query_start DESC NULLS LAST "
        "LIMIT 1";

    PGresult *res = PQexecParams(ctx->conn, sql, 2, NULL,
                                 paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[pg_correlate] query error: %s\n",
                PQerrorMessage(ctx->conn));
        PQclear(res);
        return 0;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        return 0;   /* no matching row */
    }

    /* Column order must match SELECT list above */
    util_strlcpy(row->pid,         PQgetvalue(res, 0, 0), sizeof(row->pid));
    util_strlcpy(row->usename,     PQgetvalue(res, 0, 1), sizeof(row->usename));
    util_strlcpy(row->datname,     PQgetvalue(res, 0, 2), sizeof(row->datname));
    util_strlcpy(row->client_addr, PQgetvalue(res, 0, 3), sizeof(row->client_addr));
    util_strlcpy(row->client_port, PQgetvalue(res, 0, 4), sizeof(row->client_port));
    util_strlcpy(row->state,       PQgetvalue(res, 0, 5), sizeof(row->state));
    util_strlcpy(row->wait_event,  PQgetvalue(res, 0, 6), sizeof(row->wait_event));
    util_strlcpy(row->query,       PQgetvalue(res, 0, 7), sizeof(row->query));
    util_strlcpy(row->query_start, PQgetvalue(res, 0, 8), sizeof(row->query_start));

    PQclear(res);
    return 1;
#endif /* WITH_LIBPQ */
}

void pg_correlate_close(pg_correlate_ctx_t *ctx)
{
    if (!ctx) return;
#ifdef WITH_LIBPQ
    if (ctx->conn) PQfinish(ctx->conn);
#endif
    free(ctx);
}
