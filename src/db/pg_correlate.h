/*
 * pg_correlate.h – PostgreSQL host correlation via libpq (Milestone 6)
 *
 * Queries pg_stat_activity to correlate a network-level alert with the
 * database server's view of active queries.  Compiled only when
 * WITH_LIBPQ=1 is set in the Makefile.
 */
#ifndef PG_CORRELATE_H
#define PG_CORRELATE_H

#include "net/reassembly.h"   /* flow_t */

/* -------------------------------------------------------------------------  */

typedef struct {
    char pid[16];
    char usename[64];
    char datname[64];
    char client_addr[48];
    char client_port[8];
    char state[32];
    char wait_event[64];
    char query[1024];
    char query_start[32];
} pg_row_t;

typedef struct pg_correlate_ctx pg_correlate_ctx_t;

/* -------------------------------------------------------------------------  */

/*
 * Connect to PostgreSQL with the given connection string.
 * connstr: libpq connection string, e.g. "host=localhost dbname=postgres sslmode=require"
 * Returns NULL on failure.
 */
pg_correlate_ctx_t *pg_correlate_open(const char *connstr);

/*
 * Look up the pg_stat_activity row that best matches the given flow.
 * Matching uses client_addr / client_port when available.
 * Returns 1 if a match was found and *row was populated; 0 otherwise.
 */
int pg_correlate_lookup(pg_correlate_ctx_t *ctx,
                        const flow_t       *flow,
                        pg_row_t           *row);

/* Close the connection and free ctx. */
void pg_correlate_close(pg_correlate_ctx_t *ctx);

#endif /* PG_CORRELATE_H */
