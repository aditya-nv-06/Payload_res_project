/*
 * alert.h – JSON alert output + optional PCAP dump (Milestone 7)
 *
 * Writes one JSON-Lines record per flagged query to a configurable log file.
 * Optionally dumps the raw packets of flagged flows to a .pcap file.
 *
 * Alert JSON schema:
 * {
 *   "flow_id":        "<16 hex chars>",
 *   "timestamp":      "<ISO-8601 UTC>",
 *   "src_ip":         "<dotted-decimal>",
 *   "src_port":       <uint16>,
 *   "dst_ip":         "<dotted-decimal>",
 *   "dst_port":       <uint16>,
 *   "extracted_sql":  "<escaped string>",
 *   "rule_matches":   [ { "rule": "...", "offset": N, "length": N }, ... ],
 *   "rule_score":     <int>,
 *   "anomaly_score":  <float>,
 *   "risk_level":     "LOW|MEDIUM|HIGH|CRITICAL",
 *   "pg_stat_activity": {                         (omitted when unavailable)
 *     "pid":          "...",
 *     "usename":      "...",
 *     "datname":      "...",
 *     "state":        "...",
 *     "wait_event":   "...",
 *     "query":        "...",
 *     "query_start":  "..."
 *   }
 * }
 */
#ifndef ALERT_H
#define ALERT_H

#include <pcap.h>
#include "reassembly.h"
#include "detector.h"
#include "pg_correlate.h"

/* -------------------------------------------------------------------------- */

#define ALERT_MAX_LOG_SIZE  (64ULL * 1024 * 1024)   /* 64 MB log rotation     */

typedef struct {
    FILE            *log_fp;
    char             log_path[512];
    uint64_t         bytes_written;
    uint64_t         max_log_bytes;

    /* Optional PCAP dump for flagged flows */
    pcap_dumper_t   *pcap_dumper;
    pcap_t          *pcap_dead;       /* dead handle for pcap_dump_open */
} alert_ctx_t;

/* -------------------------------------------------------------------------- */

/*
 * Open the alert logger.
 * log_path: path to the JSON-Lines log file.
 * pcap_dump_path: path for the flagged-flow PCAP, or NULL to disable.
 * datalink: DLT_* value from the capture device.
 */
int alert_open(alert_ctx_t *ctx,
               const char  *log_path,
               const char  *pcap_dump_path,
               int          datalink);

/*
 * Emit a single alert record.
 *
 * flow          – originating connection (for IP:port and flow_id)
 * sql           – extracted SQL string
 * result        – rule match results from detector_check()
 * anomaly_score – n-gram anomaly score (0.0 if not used)
 * pg_row        – pg_stat_activity row, or NULL if unavailable
 * raw_pkt       – raw captured packet for PCAP dump, or NULL
 * raw_hdr       – pcap_pkthdr for raw_pkt, or NULL
 */
void alert_emit(alert_ctx_t           *ctx,
                const flow_t          *flow,
                const char            *sql,
                const detect_result_t *result,
                double                 anomaly_score,
                const pg_row_t        *pg_row,
                const struct pcap_pkthdr *raw_hdr,
                const uint8_t            *raw_pkt);

/* Flush and close. */
void alert_close(alert_ctx_t *ctx);

/* Convert rule score + anomaly score to a risk level string. */
const char *alert_risk_level(int rule_score, double anomaly_score,
                             double anomaly_threshold);

#endif /* ALERT_H */
