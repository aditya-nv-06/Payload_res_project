/*
 * pg_parser.c – PostgreSQL wire-protocol parser (Milestone 3)
 *
 * PostgreSQL frontend message format (after the StartupMessage):
 *
 *   Byte   Content
 *   ----   ----------------------------------------------------------
 *   0      Message type tag (single ASCII char)
 *   1-4    Total message length including these 4 bytes (big-endian)
 *   5+     Message payload (depends on type)
 *
 * StartupMessage has NO type byte:
 *   0-3    Total length (big-endian, includes itself)
 *   4-7    Protocol version (3.0 = 0x00030000)
 *   8+     NUL-terminated key=value pairs
 *
 * Message types we care about:
 *   'Q' (0x51) – Simple Query:    length(4) + NUL-terminated query string
 *   'P' (0x50) – Parse:           length(4) + stmt_name + query + param_count + types
 *
 * All others are silently skipped.
 */
#include "pg_parser.h"
#include "util.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */
/* Flow-state lookup                                                          */
/* -------------------------------------------------------------------------- */

static uint32_t state_hash(const flow_t *f)
{
    uintptr_t p = (uintptr_t)f;
    return (uint32_t)((p >> 3) ^ (p >> 15)) & (PG_STATE_TABLE_SIZE - 1);
}

static pg_flow_state_t *state_find(pg_parser_ctx_t *ctx, flow_t *f)
{
    uint32_t h = state_hash(f);
    for (pg_flow_state_t *s = ctx->states[h]; s; s = s->next)
        if (s->flow_key == f) return s;
    return NULL;
}

static pg_flow_state_t *state_get_or_create(pg_parser_ctx_t *ctx, flow_t *f)
{
    pg_flow_state_t *s = state_find(ctx, f);
    if (s) return s;

    s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->flow_key = f;

    uint32_t h = state_hash(f);
    s->next = ctx->states[h];
    ctx->states[h] = s;
    return s;
}

/* -------------------------------------------------------------------------- */
/* StartupMessage handling                                                    */
/* -------------------------------------------------------------------------- */

/*
 * Attempt to consume a StartupMessage sitting at the front of buf/len.
 * Returns the number of bytes consumed, or 0 if we need more data.
 */
static size_t consume_startup(const uint8_t *buf, size_t len)
{
    if (len < 4) return 0;
    uint32_t msg_len = ntohl(*(const uint32_t *)buf);

    /* Sanity: typical startup message is < 1 KB */
    if (msg_len < 8 || msg_len > 4096) {
        /* Not a valid StartupMessage – skip 4 bytes and hope for the best */
        return 4;
    }
    if (len < msg_len) return 0;    /* need more data */
    return msg_len;
}

/* -------------------------------------------------------------------------- */
/* Regular message parsing                                                    */
/* -------------------------------------------------------------------------- */

/*
 * Process one complete message (type + body) and emit SQL if present.
 * tag    – message type byte
 * body   – message body (after the 4-byte length field)
 * blen   – length of body in bytes
 */
static void handle_message(pg_parser_ctx_t *ctx, flow_t *flow,
                            uint8_t tag,
                            const uint8_t *body, uint32_t blen)
{
    switch (tag) {

    case 'Q': {
        /* Simple query: NUL-terminated string */
        if (blen == 0) break;
        size_t qlen = strnlen((const char *)body, blen);
        if (qlen == 0 || qlen > PG_MAX_QUERY) break;
        if (ctx->on_query)
            ctx->on_query(flow, (const char *)body, qlen, ctx->user);
        break;
    }

    case 'P': {
        /*
         * Parse (extended query protocol):
         *   NUL-terminated prepared-statement name
         *   NUL-terminated query string
         *   int16 parameter count
         *   int32[] parameter type OIDs
         */
        if (blen < 2) break;
        /* Skip the prepared-statement name */
        size_t name_len = strnlen((const char *)body, blen);
        size_t offset   = name_len + 1;           /* +1 for NUL */
        if (offset >= blen) break;

        const char *sql  = (const char *)body + offset;
        size_t      qlen = strnlen(sql, blen - offset);
        if (qlen == 0 || qlen > PG_MAX_QUERY) break;
        if (ctx->on_query)
            ctx->on_query(flow, sql, qlen, ctx->user);
        break;
    }

    default:
        /* Ignore all other message types */
        break;
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

void pg_parser_ctx_init(pg_parser_ctx_t *ctx,
                        pg_query_cb_t    on_query,
                        void            *user)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->on_query = on_query;
    ctx->user     = user;
}

void pg_parser_feed(pg_parser_ctx_t *ctx,
                    flow_t          *flow,
                    const uint8_t   *data,
                    size_t           len)
{
    pg_flow_state_t *st = state_get_or_create(ctx, flow);
    if (!st) return;

    /* Append new bytes to the per-flow accumulation buffer */
    size_t space = PG_PARSE_BUF_SZ - st->buf_len;
    if (len > space) {
        /* Buffer full – reset to avoid stale data poison */
        fprintf(stderr, "[pg_parser] flow buffer overflow, resetting\n");
        st->buf_len      = 0;
        st->startup_done = 0;
        len = (len < PG_PARSE_BUF_SZ) ? len : PG_PARSE_BUF_SZ;
    }
    memcpy(st->buf + st->buf_len, data, len);
    st->buf_len += len;

    size_t pos = 0;

    /* ---- Consume StartupMessage if we haven't yet ---- */
    if (!st->startup_done) {
        size_t consumed = consume_startup(st->buf + pos, st->buf_len - pos);
        if (consumed == 0) return;   /* need more bytes */
        pos += consumed;
        st->startup_done = 1;
    }

    /* ---- Parse regular messages ---- */
    while (pos + 5 <= st->buf_len) {      /* need at least type(1) + len(4) */
        uint8_t  tag     = st->buf[pos];
        uint32_t msg_len = ntohl(*(const uint32_t *)(st->buf + pos + 1));

        /* msg_len includes the 4 length bytes themselves but NOT the tag */
        if (msg_len < 4 || msg_len > PG_PARSE_BUF_SZ) {
            /* Corrupt framing – skip one byte and resync */
            pos++;
            continue;
        }

        size_t total = 1 + msg_len;      /* tag(1) + length(4) + body */
        if (pos + total > st->buf_len)
            break;                        /* incomplete message, wait for more */

        const uint8_t *body = st->buf + pos + 5;
        uint32_t       blen = msg_len - 4;

        handle_message(ctx, flow, tag, body, blen);
        pos += total;
    }

    /* Compact: move unprocessed bytes to the front */
    if (pos > 0 && pos < st->buf_len) {
        memmove(st->buf, st->buf + pos, st->buf_len - pos);
        st->buf_len -= pos;
    } else if (pos >= st->buf_len) {
        st->buf_len = 0;
    }
}

void pg_parser_flow_remove(pg_parser_ctx_t *ctx, flow_t *flow)
{
    uint32_t h = state_hash(flow);
    pg_flow_state_t **pp = &ctx->states[h];
    while (*pp) {
        if ((*pp)->flow_key == flow) {
            pg_flow_state_t *del = *pp;
            *pp = del->next;
            free(del);
            return;
        }
        pp = &(*pp)->next;
    }
}

void pg_parser_ctx_free(pg_parser_ctx_t *ctx)
{
    for (int i = 0; i < PG_STATE_TABLE_SIZE; i++) {
        pg_flow_state_t *s = ctx->states[i];
        while (s) {
            pg_flow_state_t *next = s->next;
            free(s);
            s = next;
        }
        ctx->states[i] = NULL;
    }
}
