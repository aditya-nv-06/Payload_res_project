/*
 * main.c – PostgreSQL payload fragmentation + SQLi detection sensor
 *
 * Pipeline:
 *   libpcap capture
 *     → TCP reassembly (per 5-tuple)
 *       → PostgreSQL wire-protocol parser (extracts SQL)
 *         → Rule-based detector + N-gram anomaly scorer
 *           → (optional) pg_stat_activity correlation
 *             → JSON-Lines alert log [+ PCAP dump]
 *
 * Usage:
 *   pgsql_ids [options]
 *
 * Options:
 *   -i <iface>        Network interface to capture on (default: any)
 *   -r <file.pcap>    Read from offline PCAP file instead of live capture
 *   -f <bpf>          Extra BPF filter expression (ANDed with "tcp port 5432")
 *   -R <rules.conf>   Path to rules config file (default: config/rules.conf)
 *   -o <alerts.jsonl> Alert log output file    (default: alerts.jsonl)
 *   -p <dump.pcap>    Write flagged-flow packets to PCAP (default: disabled)
 *   -m <model.txt>    N-gram model file; if absent, anomaly scoring is skipped
 *   -t <file>         Train n-gram model from file (one SQL per line) and save
 *                     to -m path, then exit
 *   -T <threshold>    Anomaly score threshold (default: -5.0; lower = stricter)
 *   -c <connstr>      libpq connection string for pg_stat_activity correlation
 *   -d <connstr>      Direct PostgreSQL session mode; execute and score SQL
 *   -e <sql>          Execute one SQL statement in -d mode, then exit
 *   -v                Verbose: also print alerts to stderr
 *   -h                Show this help
 */
#include <string.h>
#include <unistd.h>
#include <pcap.h>
#include <time.h>

#ifdef WITH_LIBPQ
#include <libpq-fe.h>
#endif

#include "common/util.h"
#include "app/cli.h"
#include "net/capture.h"
#include "net/reassembly.h"
#include "net/pg_parser.h"
#include "analysis/detector.h"
#include "analysis/ngram.h"
#include "net/packet_parse.h"
#include "analysis/query_eval.h"
#include "db/db_session.h"
#include "db/pg_correlate.h"
#include "output/alert.h"

/* ========================================================================= */
/* Global state passed between callbacks                                     */
/* ========================================================================= */

typedef struct {
    reassembly_ctx_t    reassembly;
    pg_parser_ctx_t     pg_parser;
    detector_ctx_t      detector;
    ngram_model_t       ngram;
    int                 ngram_loaded;
    double              anomaly_threshold;
    alert_ctx_t         alert;
    pg_correlate_ctx_t *pg_corr;
    query_eval_ctx_t    eval;

    /* Current raw packet for PCAP dump (set per packet in on_packet) */
    const struct pcap_pkthdr *cur_hdr;
    const uint8_t            *cur_pkt;

    int verbose;
    int datalink;
} app_ctx_t;

static app_ctx_t g_app;

/* ========================================================================= */
/* Detection callback (called from pg_parser when a SQL string is ready)    */
/* ========================================================================= */

static void on_query(flow_t *flow, const char *sql, size_t len, void *user)
{
    (void)len;
    app_ctx_t *app = (app_ctx_t *)user;
    pg_row_t pg_row;
    memset(&pg_row, 0, sizeof(pg_row));
    const pg_row_t *pg_row_ptr = NULL;
    if (app->pg_corr && pg_correlate_lookup(app->pg_corr, flow, &pg_row))
        pg_row_ptr = &pg_row;

    query_eval_run(&app->eval, flow, sql, 0, pg_row_ptr, app->cur_hdr, app->cur_pkt);
}

/* ========================================================================= */
/* Reassembly callback (called when in-order TCP data is ready)             */
/* ========================================================================= */

static void on_reassembled(flow_t *flow, const uint8_t *data,
                            size_t len, void *user)
{
    app_ctx_t *app = (app_ctx_t *)user;
    pg_parser_feed(&app->pg_parser, flow, data, len);
}

static void on_packet(u_char                   *user,
                      const struct pcap_pkthdr *hdr,
                      const uint8_t            *pkt)
{
    app_ctx_t  *app = (app_ctx_t *)user;
    static time_t last_expire = 0;

    /* Store for optional PCAP dump */
    app->cur_hdr = hdr;
    app->cur_pkt = pkt;

    packet_info_t info;
    if (!packet_parse(hdr, pkt, app->datalink, &info))
        return;

    time_t now = (time_t)hdr->ts.tv_sec;

    reassembly_feed(&app->reassembly,
                    info.src_ip, info.dst_ip, info.src_port, info.dst_port,
                    info.seq, info.flags, info.payload, info.payload_len, now);

    /* Periodic flow expiry (every 30 seconds of capture time) */
    if (now - last_expire > 30) {
        reassembly_expire(&app->reassembly, now);
        last_expire = now;
    }
}

int main(int argc, char *argv[])
{
    cli_options_t cli;
    cli_options_init(&cli);
    int cli_rc = cli_parse(argc, argv, &cli, argv[0]);
    if (cli_rc != 0)
        return (cli_rc > 0) ? 0 : 1;

    const char *iface       = cli.iface;
    const char *pcap_file   = cli.pcap_file;
    const char *bpf_extra   = cli.bpf_extra;
    const char *rules_path  = cli.rules_path;
    const char *alert_path  = cli.alert_path;
    const char *pcap_dump   = cli.pcap_dump;
    const char *model_path  = cli.model_path;
    const char *corpus_path = cli.corpus_path;
    const char *pg_connstr  = cli.pg_connstr;
    const char *db_connstr  = cli.db_connstr;
#ifdef WITH_LIBPQ
    const char *db_sql      = cli.db_sql;
#endif
    double      anomaly_thr = cli.anomaly_thr;
    int         verbose     = cli.verbose;

    /* ---- Validate arguments ---- */
    if (pcap_file && !pcap_file[0]) {
        fprintf(stderr, "[ERROR] PCAP file path cannot be empty\n");
        return 1;
    }

    if (rules_path && !rules_path[0]) {
        fprintf(stderr, "[ERROR] Rules path cannot be empty\n");
        return 1;
    }

    if (corpus_path && !model_path) {
        fprintf(stderr, "[ERROR] -t (training) requires -m (model path)\n");
        return 1;
    }

    /* ---- N-gram training mode ---- */
    if (corpus_path) {
        fprintf(stderr, "[INFO] Training n-gram model from corpus: %s\n", corpus_path);
        ngram_model_t m;
        ngram_init(&m, NGRAM_N, NGRAM_SMOOTHING);
        int n = ngram_train_file(&m, corpus_path);
        if (n < 0) {
            fprintf(stderr, "[ERROR] Failed to train model\n");
            return 1;
        }
        if (ngram_save(&m, model_path) < 0) {
            fprintf(stderr, "[ERROR] Failed to save model to %s\n", model_path);
            return 1;
        }
        fprintf(stderr, "[SUCCESS] Model saved to %s (%d queries)\n",
                model_path, n);
        ngram_free(&m);
        return 0;
    }

    /* ---- Initialise application context ---- */
    memset(&g_app, 0, sizeof(g_app));
    g_app.anomaly_threshold = anomaly_thr;
    g_app.verbose           = verbose;

    if (verbose) {
        if (db_connstr) {
            fprintf(stderr,
                "\n"
                "[CONFIG] Mode: database session\n"
                "[CONFIG] Rules File: %s\n"
                "[CONFIG] Alert Output: %s\n"
                "[CONFIG] Anomaly Threshold: %.2f\n"
                "\n",
                rules_path,
                alert_path,
                anomaly_thr);
        } else {
            fprintf(stderr,
                "\n"
                "[CONFIG] Capture Mode: %s\n"
                "[CONFIG] Rules File: %s\n"
                "[CONFIG] Alert Output: %s\n"
                "[CONFIG] Anomaly Threshold: %.2f\n"
                "\n",
                pcap_file ? pcap_file : iface,
                rules_path,
                alert_path,
                anomaly_thr);
        }
    }

    /* ---- Load detection rules ---- */
    if (detector_load(&g_app.detector, rules_path) < 0) {
        fprintf(stderr, "[main] warning: no rules loaded\n");
    }

    /* ---- Load n-gram model (optional) ---- */
    if (model_path) {
        ngram_init(&g_app.ngram, NGRAM_N, NGRAM_SMOOTHING);
        if (ngram_load(&g_app.ngram, model_path) == 0)
            g_app.ngram_loaded = 1;
    }

    query_eval_init(&g_app.eval,
                    &g_app.detector,
                    &g_app.ngram,
                    g_app.ngram_loaded,
                    g_app.anomaly_threshold,
                    &g_app.alert,
                    g_app.verbose);

#ifdef WITH_LIBPQ
    if (db_connstr) {
        if (alert_open(&g_app.alert, alert_path, pcap_dump, DLT_RAW) < 0) {
            detector_free(&g_app.detector);
            if (g_app.ngram_loaded) ngram_free(&g_app.ngram);
            return 1;
        }

        int rc = db_session_run(db_connstr, db_sql, &g_app.eval);
        alert_close(&g_app.alert);
        detector_free(&g_app.detector);
        if (g_app.ngram_loaded) ngram_free(&g_app.ngram);
        return rc;
    }
#else
    if (db_connstr) {
        fprintf(stderr, "[main] database session mode requires WITH_LIBPQ=1\n");
        detector_free(&g_app.detector);
        if (g_app.ngram_loaded) ngram_free(&g_app.ngram);
        return 1;
    }
#endif

    /* ---- PostgreSQL correlation (optional) ---- */
    if (pg_connstr) {
        g_app.pg_corr = pg_correlate_open(pg_connstr);
        if (!g_app.pg_corr)
            fprintf(stderr, "[main] pg_stat_activity correlation disabled\n");
    }

    /* ---- Open capture ---- */
    capture_ctx_t *cap = pcap_file
        ? capture_open_file(pcap_file, bpf_extra)
        : capture_open_live(iface,     bpf_extra);

    if (!cap) return 1;
    g_app.datalink = capture_datalink(cap);

    /* ---- Open alert logger ---- */
    if (alert_open(&g_app.alert, alert_path, pcap_dump,
                   g_app.datalink) < 0) {
        capture_close(cap);
        return 1;
    }

    /* ---- Wire up callbacks ---- */
    reassembly_ctx_init(&g_app.reassembly, on_reassembled, &g_app);
    pg_parser_ctx_init(&g_app.pg_parser,   on_query,       &g_app);

    fprintf(stderr, "[main] starting capture (rules=%d, ngram=%s, pgcorr=%s)\n",
            g_app.detector.rule_count,
            g_app.ngram_loaded ? "on" : "off",
            g_app.pg_corr      ? "on" : "off");

    /* ---- Main capture loop ---- */
    capture_loop(cap, on_packet, &g_app);

    /* ---- Cleanup ---- */
    fprintf(stderr, "\n[main] shutting down\n");
    capture_close(cap);
    alert_close(&g_app.alert);
    reassembly_ctx_free(&g_app.reassembly);
    pg_parser_ctx_free(&g_app.pg_parser);
    detector_free(&g_app.detector);
    if (g_app.ngram_loaded) ngram_free(&g_app.ngram);
    if (g_app.pg_corr) pg_correlate_close(g_app.pg_corr);

    return 0;
}
