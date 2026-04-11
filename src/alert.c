/*
 * alert.c – JSON alert output + optional PCAP dump (Milestone 7)
 */
#include "alert.h"
#include "util.h"
#include "pg_parser.h"   /* PG_MAX_QUERY */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* -------------------------------------------------------------------------- */
/* Internal: log rotation                                                     */
/* -------------------------------------------------------------------------- */

static void maybe_rotate(alert_ctx_t *ctx)
{
    if (!ctx->log_fp || ctx->max_log_bytes == 0) return;
    if (ctx->bytes_written < ctx->max_log_bytes) return;

    fclose(ctx->log_fp);
    /* Rotate: rename current file to .1 (simple overwrite rotation) */
    char bak[600];
    snprintf(bak, sizeof(bak), "%s.1", ctx->log_path);
    rename(ctx->log_path, bak);

    ctx->log_fp = fopen(ctx->log_path, "a");
    if (!ctx->log_fp)
        perror("[alert] log rotation failed");
    ctx->bytes_written = 0;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

int alert_open(alert_ctx_t *ctx,
               const char  *log_path,
               const char  *pcap_dump_path,
               int          datalink)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->max_log_bytes = ALERT_MAX_LOG_SIZE;

    util_strlcpy(ctx->log_path, log_path, sizeof(ctx->log_path));
    ctx->log_fp = fopen(log_path, "a");
    if (!ctx->log_fp) {
        perror(log_path);
        return -1;
    }

    if (pcap_dump_path && pcap_dump_path[0]) {
        ctx->pcap_dead = pcap_open_dead(datalink, 65535);
        if (ctx->pcap_dead) {
            ctx->pcap_dumper = pcap_dump_open(ctx->pcap_dead, pcap_dump_path);
            if (!ctx->pcap_dumper)
                fprintf(stderr, "[alert] pcap_dump_open failed for %s\n",
                        pcap_dump_path);
        }
    }
    return 0;
}

const char *alert_risk_level(int rule_score, double anomaly_score,
                             double anomaly_threshold)
{
    /* Anomaly score is negative log-prob; lower == more anomalous.
     * anomaly_threshold is typically something like -5.0. */
    int anom_hit = (anomaly_threshold != 0.0 && anomaly_score < anomaly_threshold);

    if (rule_score >= (int)SEV_CRITICAL || (anom_hit && rule_score >= (int)SEV_HIGH))
        return "CRITICAL";
    if (rule_score >= (int)SEV_HIGH || anom_hit)
        return "HIGH";
    if (rule_score >= (int)SEV_MEDIUM)
        return "MEDIUM";
    return "LOW";
}

void alert_emit(alert_ctx_t           *ctx,
                const flow_t          *flow,
                const char            *sql,
                const detect_result_t *result,
                double                 anomaly_score,
                const pg_row_t        *pg_row,
                const struct pcap_pkthdr *raw_hdr,
                const uint8_t            *raw_pkt)
{
    if (!ctx->log_fp) return;

    maybe_rotate(ctx);

    /* ---- Build flow_id ---- */
    char flow_id[17] = "0000000000000000";
    if (flow) reassembly_flow_id(flow, flow_id);

    /* ---- Timestamp ---- */
    char ts[32];
    util_iso8601_now(ts, sizeof(ts));

    /* ---- IP strings ---- */
    char src_ip[INET_ADDRSTRLEN] = "0.0.0.0";
    char dst_ip[INET_ADDRSTRLEN] = "0.0.0.0";
    if (flow) {
        struct in_addr sa, da;
        sa.s_addr = htonl(flow->src_ip);
        da.s_addr = htonl(flow->dst_ip);
        inet_ntop(AF_INET, &sa, src_ip, sizeof(src_ip));
        inet_ntop(AF_INET, &da, dst_ip, sizeof(dst_ip));
    }

    /* ---- Risk level ---- */
    const char *risk = alert_risk_level(result ? result->score : 0,
                                        anomaly_score, -5.0);

    /* ---- JSON escaping buffers ---- */
    char   sql_esc[PG_MAX_QUERY * 7 + 3];
    util_json_escape(sql ? sql : "", sql_esc, sizeof(sql_esc));

    /* ---- Emit JSON line ---- */
    int n = fprintf(ctx->log_fp,
        "{\"flow_id\":\"%s\","
        "\"timestamp\":\"%s\","
        "\"src_ip\":\"%s\",\"src_port\":%u,"
        "\"dst_ip\":\"%s\",\"dst_port\":%u,"
        "\"extracted_sql\":%s,"
        "\"rule_matches\":[",
        flow_id, ts,
        src_ip, flow ? flow->src_port : 0u,
        dst_ip, flow ? flow->dst_port : 0u,
        sql_esc);
    ctx->bytes_written += (uint64_t)(n > 0 ? n : 0);

    /* ---- Rule matches array ---- */
    if (result) {
        for (int i = 0; i < result->match_count; i++) {
            const match_t *m = &result->matches[i];
            n = fprintf(ctx->log_fp,
                "%s{\"rule\":\"%s\",\"offset\":%d,\"length\":%d}",
                i ? "," : "",
                m->rule_name, m->offset, m->length);
            ctx->bytes_written += (uint64_t)(n > 0 ? n : 0);
        }
    }

    n = fprintf(ctx->log_fp,
        "],"
        "\"rule_score\":%d,"
        "\"anomaly_score\":%.4f,"
        "\"risk_level\":\"%s\"",
        result ? result->score : 0,
        anomaly_score, risk);
    ctx->bytes_written += (uint64_t)(n > 0 ? n : 0);

    /* ---- Optional pg_stat_activity block ---- */
    if (pg_row && pg_row->pid[0]) {
        char q_esc[sizeof(pg_row->query) * 7 + 3];
        util_json_escape(pg_row->query, q_esc, sizeof(q_esc));
        n = fprintf(ctx->log_fp,
            ",\"pg_stat_activity\":{"
            "\"pid\":\"%s\","
            "\"usename\":\"%s\","
            "\"datname\":\"%s\","
            "\"state\":\"%s\","
            "\"wait_event\":\"%s\","
            "\"query\":%s,"
            "\"query_start\":\"%s\""
            "}",
            pg_row->pid, pg_row->usename, pg_row->datname,
            pg_row->state, pg_row->wait_event,
            q_esc, pg_row->query_start);
        ctx->bytes_written += (uint64_t)(n > 0 ? n : 0);
    }

    n = fprintf(ctx->log_fp, "}\n");
    ctx->bytes_written += (uint64_t)(n > 0 ? n : 0);
    fflush(ctx->log_fp);

    /* ---- Optional PCAP dump ---- */
    if (ctx->pcap_dumper && raw_hdr && raw_pkt)
        pcap_dump((u_char *)ctx->pcap_dumper, raw_hdr, raw_pkt);
}

void alert_close(alert_ctx_t *ctx)
{
    if (ctx->log_fp) { fclose(ctx->log_fp); ctx->log_fp = NULL; }
    if (ctx->pcap_dumper) { pcap_dump_close(ctx->pcap_dumper); ctx->pcap_dumper = NULL; }
    if (ctx->pcap_dead)   { pcap_close(ctx->pcap_dead);        ctx->pcap_dead   = NULL; }
}
