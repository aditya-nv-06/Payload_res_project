/*
 * reassembly.c – TCP stream reassembly (Milestone 2)
 */
#include "reassembly.h"
#include "util.h"

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static uint32_t flow_hash(uint32_t sa, uint32_t da,
                          uint16_t sp, uint16_t dp)
{
    uint8_t key[12];
    /* canonical ordering so A→B and B→A hash to different slots */
    memcpy(key,     &sa, 4);
    memcpy(key + 4, &da, 4);
    memcpy(key + 8, &sp, 2);
    memcpy(key + 10, &dp, 2);
    return util_hash_djb2(key, 12) & (FLOW_TABLE_SIZE - 1);
}

static flow_t *flow_find(reassembly_ctx_t *ctx,
                         uint32_t sa, uint32_t da,
                         uint16_t sp, uint16_t dp)
{
    uint32_t h = flow_hash(sa, da, sp, dp);
    for (flow_t *f = ctx->table[h]; f; f = f->hash_next) {
        if (f->src_ip   == sa && f->dst_ip   == da &&
            f->src_port == sp && f->dst_port == dp)
            return f;
    }
    return NULL;
}

static flow_t *flow_create(reassembly_ctx_t *ctx,
                            uint32_t sa, uint32_t da,
                            uint16_t sp, uint16_t dp,
                            time_t now)
{
    if (ctx->flow_count >= MAX_FLOWS) {
        fprintf(stderr, "[reassembly] flow table full (%zu), dropping new flow\n",
                ctx->flow_count);
        return NULL;
    }
    flow_t *f = calloc(1, sizeof(*f));
    if (!f) return NULL;

    f->src_ip   = sa;  f->dst_ip   = da;
    f->src_port = sp;  f->dst_port = dp;
    f->last_seen = now;

    uint32_t h = flow_hash(sa, da, sp, dp);
    f->hash_next = ctx->table[h];
    ctx->table[h] = f;
    ctx->flow_count++;
    return f;
}

static void flow_free_segs(flow_t *f)
{
    seg_t *s = f->pending;
    while (s) {
        seg_t *next = s->next;
        free(s->data);
        free(s);
        s = next;
    }
    f->pending = NULL;
    f->pending_bytes = 0;
}

static void flow_destroy(reassembly_ctx_t *ctx, flow_t *f)
{
    uint32_t h = flow_hash(f->src_ip, f->dst_ip, f->src_port, f->dst_port);
    flow_t **pp = &ctx->table[h];
    while (*pp && *pp != f) pp = &(*pp)->hash_next;
    if (*pp) *pp = f->hash_next;
    flow_free_segs(f);
    free(f);
    ctx->flow_count--;
}

/* Insert segment into the sorted pending list of f, merging overlaps */
static void flow_insert_seg(flow_t *f,
                            uint32_t seq,
                            const uint8_t *data, uint32_t len)
{
    if (len == 0) return;

    /* Drop if segment is entirely before next_seq (already seen).
     * Signed arithmetic is intentional: it handles TCP sequence number
     * wrap-around correctly (e.g. 0xFFFF0000 → 0x00010000). */
    if (f->seq_init) {
        uint32_t end = seq + len;
        if ((int32_t)(end - f->next_seq) <= 0) return;  /* fully old */
        /* Trim leading bytes already delivered */
        if ((int32_t)(seq - f->next_seq) < 0) {
            uint32_t trim = f->next_seq - seq;
            seq  += trim;
            data += trim;
            len  -= trim;
        }
    }

    /* Enforce per-flow buffer cap */
    if (f->pending_bytes + len > SEG_MAXBUF) {
        fprintf(stderr, "[reassembly] flow buffer overflow, resetting flow\n");
        flow_free_segs(f);
        f->seq_init = 0;
        return;
    }

    seg_t *ns = calloc(1, sizeof(*ns));
    if (!ns) return;
    ns->seq  = seq;
    ns->len  = len;
    ns->data = malloc(len);
    if (!ns->data) { free(ns); return; }
    memcpy(ns->data, data, len);

    /* Insert sorted by seq */
    seg_t **pp = &f->pending;
    while (*pp && (int32_t)((*pp)->seq - seq) < 0)
        pp = &(*pp)->next;
    ns->next = *pp;
    *pp = ns;
    f->pending_bytes += len;
}

/* Emit all contiguous data starting at f->next_seq, then fire callback */
static void flow_emit(reassembly_ctx_t *ctx, flow_t *f)
{
    while (f->pending) {
        seg_t *s = f->pending;

        /* Gap? stop. */
        if ((int32_t)(s->seq - f->next_seq) > 0)
            break;

        /* Overlap trim: segment starts before next_seq */
        uint32_t skip = 0;
        if ((int32_t)(s->seq - f->next_seq) < 0)
            skip = f->next_seq - s->seq;

        if (skip >= s->len) {
            /* Entirely consumed segment */
            f->pending = s->next;
            f->pending_bytes -= s->len;
            free(s->data);
            free(s);
            continue;
        }

        const uint8_t *payload = s->data + skip;
        uint32_t        plen   = s->len  - skip;

        /* Fire the callback */
        if (ctx->on_data)
            ctx->on_data(f, payload, plen, ctx->user);

        f->next_seq += plen;
        f->pending = s->next;
        f->pending_bytes -= s->len;
        free(s->data);
        free(s);
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

void reassembly_ctx_init(reassembly_ctx_t *ctx,
                         reassembly_cb_t   on_data,
                         void             *user)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->on_data = on_data;
    ctx->user    = user;
}

void reassembly_feed(reassembly_ctx_t *ctx,
                     uint32_t sa,   uint32_t da,
                     uint16_t sp,   uint16_t dp,
                     uint32_t seq,  uint8_t  flags,
                     const uint8_t *data, size_t len,
                     time_t now)
{
    /* Only track the client→server direction (client port != 5432) */
    if (dp != PG_DEFAULT_PORT) return;

    flow_t *f = flow_find(ctx, sa, da, sp, dp);

    /* RST: discard and remove the flow */
    if (flags & TH_RST) {
        if (f) flow_destroy(ctx, f);
        return;
    }

    /* SYN: new flow (or re-use after RST) */
    if (flags & TH_SYN) {
        if (f) flow_destroy(ctx, f);
        f = flow_create(ctx, sa, da, sp, dp, now);
        if (!f) return;
        f->next_seq = seq + 1;   /* SYN consumes one sequence number */
        f->seq_init = 1;
        return;
    }

    if (!f) {
        /* Mid-stream join (e.g., capture started after connection) */
        f = flow_create(ctx, sa, da, sp, dp, now);
        if (!f) return;
        /* Seed next_seq from the first segment we see */
        f->next_seq = seq;
        f->seq_init = 1;
    }

    f->last_seen = now;

    /* FIN: deliver buffered data then mark for cleanup */
    if (flags & TH_FIN) {
        if (len > 0) {
            flow_insert_seg(f, seq, data, (uint32_t)len);
            flow_emit(ctx, f);
        }
        flow_destroy(ctx, f);
        return;
    }

    if (len == 0) return;   /* pure ACK */

    flow_insert_seg(f, seq, data, (uint32_t)len);
    flow_emit(ctx, f);
}

void reassembly_expire(reassembly_ctx_t *ctx, time_t now)
{
    for (int i = 0; i < FLOW_TABLE_SIZE; i++) {
        flow_t **pp = &ctx->table[i];
        while (*pp) {
            flow_t *f = *pp;
            if (now - f->last_seen > FLOW_TIMEOUT_SEC) {
                *pp = f->hash_next;
                flow_free_segs(f);
                free(f);
                ctx->flow_count--;
            } else {
                pp = &f->hash_next;
            }
        }
    }
}

void reassembly_ctx_free(reassembly_ctx_t *ctx)
{
    for (int i = 0; i < FLOW_TABLE_SIZE; i++) {
        flow_t *f = ctx->table[i];
        while (f) {
            flow_t *next = f->hash_next;
            flow_free_segs(f);
            free(f);
            f = next;
        }
        ctx->table[i] = NULL;
    }
    ctx->flow_count = 0;
}

void reassembly_flow_id(const flow_t *f, char *buf)
{
    /* 16 hex chars derived from the 5-tuple */
    uint8_t key[12];
    memcpy(key,      &f->src_ip,   4);
    memcpy(key +  4, &f->dst_ip,   4);
    memcpy(key +  8, &f->src_port, 2);
    memcpy(key + 10, &f->dst_port, 2);
    uint32_t h1 = util_hash_djb2(key, 12);
    uint32_t h2 = util_hash_djb2(key + 4, 8);
    snprintf(buf, 17, "%08x%08x", h1, h2);
}
