/*
 * reassembly.h – TCP stream reassembly (Milestone 2)
 *
 * Maintains a per-connection flow table and reconstructs the ordered TCP
 * byte-stream so that SQL payloads split across multiple packets are seen
 * as a single contiguous buffer.
 *
 * The caller feeds raw IP/TCP packet data via reassembly_feed().  When a
 * contiguous chunk of new data is available, the registered on_data callback
 * is fired with (flow, data, len, user).
 */
#ifndef REASSEMBLY_H
#define REASSEMBLY_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

/* -------------------------------------------------------------------------- */

#define FLOW_TABLE_SIZE  4096   /* must be a power of 2                       */
#define MAX_FLOWS        8192   /* hard cap on concurrent tracked connections  */
#define FLOW_TIMEOUT_SEC  120   /* idle flow eviction after this many seconds  */
#define SEG_MAXBUF      65536   /* max bytes buffered per flow                 */
#define PG_DEFAULT_PORT  5432   /* PostgreSQL default TCP port                 */

/* -------------------------------------------------------------------------- */

/* A single out-of-order TCP segment waiting to be reassembled */
typedef struct seg {
    uint32_t    seq;    /* sequence number of the first byte of data[]  */
    uint8_t    *data;
    uint32_t    len;
    struct seg *next;   /* sorted singly-linked list (ascending seq)    */
} seg_t;

/* Per-connection flow state */
typedef struct flow {
    /* 5-tuple key */
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;

    /* reassembly state */
    uint32_t next_seq;          /* next expected sequence number          */
    int      seq_init;          /* 1 once next_seq has been seeded        */
    seg_t   *pending;           /* out-of-order segments (sorted by seq)  */
    size_t   pending_bytes;     /* total bytes in pending list            */

    /* lifetime */
    time_t   last_seen;
    int      fin_seen;

    /* hash chain */
    struct flow *hash_next;
} flow_t;

/* -------------------------------------------------------------------------- */

/*
 * Callback fired when new in-order payload bytes are available.
 *   flow  – the flow that produced the data (read-only)
 *   data  – reassembled bytes (valid only for the duration of the callback)
 *   len   – number of bytes
 *   user  – opaque pointer supplied to reassembly_ctx_init()
 */
typedef void (*reassembly_cb_t)(flow_t        *flow,
                                const uint8_t *data,
                                size_t         len,
                                void          *user);

/* Reassembly context */
typedef struct {
    flow_t         *table[FLOW_TABLE_SIZE];
    size_t          flow_count;
    reassembly_cb_t on_data;
    void           *user;
} reassembly_ctx_t;

/* -------------------------------------------------------------------------- */

void reassembly_ctx_init(reassembly_ctx_t *ctx,
                         reassembly_cb_t   on_data,
                         void             *user);

/*
 * Feed a TCP segment into the reassembler.
 *   src_ip / dst_ip / src_port / dst_port – extracted from IP+TCP headers
 *   seq      – TCP sequence number
 *   flags    – TCP flags byte (TH_FIN, TH_RST, TH_SYN …)
 *   data     – TCP payload (may be NULL / len==0 for pure ACKs)
 *   len      – TCP payload length
 *   now      – current time (for timeout tracking)
 */
void reassembly_feed(reassembly_ctx_t *ctx,
                     uint32_t src_ip,   uint32_t dst_ip,
                     uint16_t src_port, uint16_t dst_port,
                     uint32_t seq,      uint8_t  flags,
                     const uint8_t *data, size_t len,
                     time_t now);

/* Evict flows that have been idle for longer than FLOW_TIMEOUT_SEC */
void reassembly_expire(reassembly_ctx_t *ctx, time_t now);

/* Free all resources */
void reassembly_ctx_free(reassembly_ctx_t *ctx);

/* Return a stable flow-id string (16 hex chars) into buf (>= 17 bytes) */
void reassembly_flow_id(const flow_t *flow, char *buf);

#endif /* REASSEMBLY_H */
