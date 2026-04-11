/*
 * pg_parser.h – PostgreSQL wire-protocol parser (Milestone 3)
 *
 * Parses the PostgreSQL frontend (client → server) message stream extracted
 * from the reassembled TCP byte-stream and emits the SQL query strings it
 * finds for further analysis.
 *
 * References:
 *   https://www.postgresql.org/docs/current/protocol-message-formats.html
 */
#ifndef PG_PARSER_H
#define PG_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include "reassembly.h"   /* flow_t */

/* -------------------------------------------------------------------------  */

#define PG_MAX_QUERY     16384  /* max SQL string we will handle */
#define PG_PARSE_BUF_SZ  32768  /* per-flow reassembly buffer    */
#define PG_STARTUP_MAX_LEN 4096 /* max valid StartupMessage size */

/* Per-flow PostgreSQL parser state */
typedef struct pg_flow_state {
    uint8_t  buf[PG_PARSE_BUF_SZ]; /* partial-message accumulation buffer */
    size_t   buf_len;
    int      startup_done;          /* have we consumed the StartupMessage? */

    /* intrusive list, keyed on the flow pointer */
    flow_t          *flow_key;
    struct pg_flow_state *next;
} pg_flow_state_t;

/* -------------------------------------------------------------------------  */

/*
 * Callback fired when a complete SQL query is extracted.
 *   flow  – originating flow
 *   sql   – NUL-terminated query string (valid only for callback duration)
 *   len   – strlen(sql)
 *   user  – opaque pointer supplied to pg_parser_ctx_init()
 */
typedef void (*pg_query_cb_t)(flow_t        *flow,
                              const char    *sql,
                              size_t         len,
                              void          *user);

/* Parser context */
typedef struct {
#define PG_STATE_TABLE_SIZE 1024
    pg_flow_state_t *states[PG_STATE_TABLE_SIZE];
    pg_query_cb_t    on_query;
    void            *user;
} pg_parser_ctx_t;

/* -------------------------------------------------------------------------  */

void pg_parser_ctx_init(pg_parser_ctx_t *ctx,
                        pg_query_cb_t    on_query,
                        void            *user);

/*
 * Feed reassembled client→server bytes to the parser.
 * Called from the reassembly on_data callback.
 */
void pg_parser_feed(pg_parser_ctx_t *ctx,
                    flow_t          *flow,
                    const uint8_t   *data,
                    size_t           len);

/* Remove per-flow state when a flow is torn down. */
void pg_parser_flow_remove(pg_parser_ctx_t *ctx, flow_t *flow);

/* Free all resources. */
void pg_parser_ctx_free(pg_parser_ctx_t *ctx);

#endif /* PG_PARSER_H */
