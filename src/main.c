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
 *   pqCheck [options]
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
 *   --auto-baseline   In live mode without -m, capture baseline traffic and train
 *                     an in-memory model before monitoring (default: enabled)
 *   --auto-baseline-duration N  Baseline capture duration in seconds (default: 30)
 *   --no-auto-baseline Disable live auto-baseline behavior
 *   -c <connstr>      libpq connection string for pg_stat_activity correlation
 *   -d <connstr>      Direct PostgreSQL session mode; execute and score SQL
 *   -e <sql>          Execute one SQL statement in -d mode, then exit
 *   -v                Verbose: also print alerts to stderr
 *   -h                Show this help
 */
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pcap.h>
#include <time.h>

#ifdef WITH_LIBPQ
#include <libpq-fe.h>
#endif

#include "common/util.h"
#include "app/cli.h"
#include "common/logger.h"
#include "net/capture.h"
#include "net/reassembly.h"
#include "net/pg_parser.h"
#include "analysis/detector.h"
#include "analysis/audit.h"
#include "analysis/ngram.h"
#include "net/packet_parse.h"
#include "analysis/query_eval.h"
#include "db/db_session.h"
#include "db/pg_correlate.h"
#include "output/alert.h"
#include "common/net_config.h"
#include "common/pcap_gen.h"

#ifdef WITH_TUI
#include "ui/tui.h"
#endif

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

/* ------------------------------------------------------------------------- */
/* Training-from-PCAP helpers (auto mode)                                    */
/* ------------------------------------------------------------------------- */

typedef struct {
    reassembly_ctx_t reasm;
    pg_parser_ctx_t  parser;
    int              datalink;
    time_t           last_expire;
} train_ctx_t;

static void train_on_query(flow_t *flow, const char *sql, size_t len, void *user)
{
    (void)flow; (void)len;
    ngram_model_t *m = (ngram_model_t *)user;
    if (m && sql)
        ngram_train(m, sql);
}

static void train_on_reassembled(flow_t *flow, const uint8_t *data,
                                 size_t len, void *user)
{
    pg_parser_ctx_t *parser = (pg_parser_ctx_t *)user;
    if (parser)
        pg_parser_feed(parser, flow, data, len);
}

static void train_on_packet(u_char                   *user,
                            const struct pcap_pkthdr *hdr,
                            const uint8_t            *pkt)
{
    train_ctx_t *ctx = (train_ctx_t *)user;
    if (!ctx) return;

    packet_info_t info;
    if (!packet_parse(hdr, pkt, ctx->datalink, &info))
        return;

    time_t now = (time_t)hdr->ts.tv_sec;

    reassembly_feed(&ctx->reasm,
                    info.src_ip, info.dst_ip, info.src_port, info.dst_port,
                    info.seq, info.flags, info.payload, info.payload_len, now);

    if (now - ctx->last_expire > 30) {
        reassembly_expire(&ctx->reasm, now);
        ctx->last_expire = now;
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
    int         tui_mode    = cli.tui_mode;

    if (cli.audit_mode) {
        audit_report_t report;
#ifdef WITH_LIBPQ
        /* Prefer PostgreSQL system-level audit when built with libpq. If a
         * connection string is provided via -c/--pg-conn use it, otherwise
         * attempt to connect to localhost using libpq defaults. */
        int rc = audit_run_postgres(cli.pg_connstr, cli.audit_json_out, &report, stdout);
#else
        int rc = audit_run(cli.audit_root,
                           cli.audit_include_docs,
                           cli.audit_json_out,
                           &report,
                           stdout);
#endif

#ifdef WITH_TUI
        if (tui_mode) {
            tui_ctx_t tui;
            if (tui_init(&tui) == 0) {
                tui.current_screen = TUI_SCREEN_AUDIT;
                tui.stats.capture_active = 0;
                tui.stats.rules_loaded = 0;
                tui.stats.ngram_loaded = 0;
                tui.stats.total_queries = report.total_files_scanned;
                tui.stats.total_alerts = report.total_findings;
                tui.stats.rule_alerts = report.high_count;
                tui.stats.anomaly_alerts = report.medium_count + report.low_count;
                tui.audit_files_scanned = report.total_files_scanned;
                tui.audit_total_findings = report.total_findings;
                tui.audit_high = report.high_count;
                tui.audit_medium = report.medium_count;
                tui.audit_low = report.low_count;
                tui_run(&tui);
                tui_cleanup(&tui);
            }
        }
#else
        if (tui_mode) {
            fprintf(stderr, "[ERROR] TUI mode (--tui) requires ncurses support\n");
            fprintf(stderr, "[ERROR] Rebuild with: make WITH_TUI=1\n");
            return 1;
        }
#endif

        return rc;
    }

    char capture_path_buf[256];
    memset(capture_path_buf, 0, sizeof(capture_path_buf));
    char auto_baseline_path_buf[256];
    memset(auto_baseline_path_buf, 0, sizeof(auto_baseline_path_buf));
    int auto_baseline_temp_file = 0;

    if (cli.capture_mode) {
        if (pcap_file) {
            fprintf(stderr, "[ERROR] --capture cannot be combined with -r/--pcap input\n");
            return 1;
        }

        if (cli.capture_seconds <= 0) {
            fprintf(stderr, "[ERROR] --duration must be greater than zero\n");
            return 1;
        }

        if (cli.capture_pcap && cli.capture_pcap[0]) {
            pcap_file = cli.capture_pcap;
        } else {
            snprintf(capture_path_buf, sizeof(capture_path_buf),
                     "/tmp/pqcheck_capture_%ld_%ld.pcap",
                     (long)time(NULL), (long)getpid());
            pcap_file = capture_path_buf;
        }

        if (capture_live_to_pcap(iface, bpf_extra, pcap_file, cli.capture_seconds) < 0)
            return 1;

        cli.auto_pcap = pcap_file;
        cli.pcap_file = pcap_file;
    }

    /* ---- First-time live mode: auto-capture baseline and auto-train ---- */
    if (!pcap_file &&
        !model_path &&
        !corpus_path &&
        !cli.auto_pcap &&
        !cli.capture_mode &&
        !db_connstr &&
        !cli.audit_mode &&
        !cli.gen_test_output &&
        cli.auto_baseline_enabled) {
        if (cli.auto_baseline_seconds <= 0) {
            fprintf(stderr, "[ERROR] --auto-baseline-duration must be greater than zero\n");
            return 1;
        }

        snprintf(auto_baseline_path_buf, sizeof(auto_baseline_path_buf),
                 "/tmp/pqcheck_autobaseline_%ld_%ld.pcap",
                 (long)time(NULL), (long)getpid());
        fprintf(stderr,
                "[main] no model provided in live mode; collecting baseline traffic for %d second(s)\n",
                cli.auto_baseline_seconds);
        fprintf(stderr,
                "[main] baseline assumes initial capture is clean; disable with --no-auto-baseline\n");

        if (capture_live_to_pcap(iface, bpf_extra, auto_baseline_path_buf,
                                 cli.auto_baseline_seconds) == 0) {
            cli.auto_pcap = auto_baseline_path_buf;
            auto_baseline_temp_file = 1;
        } else {
            fprintf(stderr, "[main] warning: baseline capture failed; continuing without anomaly model\n");
        }
    }

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

    /* ---- TUI mode validation ---- */
    if (tui_mode) {
#ifndef WITH_TUI
        fprintf(stderr, "[ERROR] TUI mode (--tui) requires ncurses support\n");
        fprintf(stderr, "[ERROR] Rebuild with: make WITH_TUI=1\n");
        return 1;
#endif
        fprintf(stderr, "[INFO] Starting in TUI (interactive) mode\n");
    }

    /* ---- PCAP generation mode ---- */
    if (cli.gen_test_output) {
        if (!alert_path || alert_path[0] == '\0') {
            fprintf(stderr, "[ERROR] --gen-test requires -o (output file)\n");
            return 1;
        }
        fprintf(stderr, "[INFO] Generating test PCAP: %s\n", alert_path);
        pcap_gen_opts_t opts = {
            .num_clean = cli.gen_test_clean_count,
            .num_injection = cli.gen_test_injection_count,
            .seed = 0  /* auto-seed */
        };
        if (pcap_gen_write(alert_path, &opts) < 0) {
            fprintf(stderr, "[ERROR] Failed to generate PCAP\n");
            return 1;
        }
        fprintf(stderr, "[SUCCESS] Test PCAP created: %s\n", alert_path);
        fprintf(stderr, "[NEXT] Train model: pqCheck -A %s -m test.model\n", alert_path);
        return 0;
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

    /* Initialise logger from environment or verbose flag */
    const char *logfile = getenv("PQCHECK_LOG_FILE");
    const char *lvl = getenv("PQCHECK_LOG_LEVEL");
    if (verbose) {
        logger_init(logfile, LOG_DEBUG);
    } else if (lvl) {
        if (strcasecmp(lvl, "debug") == 0)
            logger_init(logfile, LOG_DEBUG);
        else if (strcasecmp(lvl, "info") == 0)
            logger_init(logfile, LOG_INFO);
        else if (strcasecmp(lvl, "warn") == 0 || strcasecmp(lvl, "warning") == 0)
            logger_init(logfile, LOG_WARN);
        else
            logger_init(logfile, LOG_INFO);
    } else {
        logger_init(logfile, LOG_INFO);
    }

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

    /* ---- Auto-train n-gram model from PCAP (optional) ---- */
    if (cli.auto_pcap) {
        if (model_path) {
            fprintf(stderr, "[main] warning: both -A (auto) and -m (model file) provided; skipping auto-train and using -m model\n");
        } else {
            fprintf(stderr, "[main] auto-training n-gram model from PCAP: %s\n", cli.auto_pcap);

            /* Initialise model in g_app and train into it */
            ngram_init(&g_app.ngram, NGRAM_N, NGRAM_SMOOTHING);

            train_ctx_t tctx;
            memset(&tctx, 0, sizeof(tctx));
            pg_parser_ctx_init(&tctx.parser, train_on_query, &g_app.ngram);
            reassembly_ctx_init(&tctx.reasm, train_on_reassembled, &tctx.parser);

            capture_ctx_t *tcap = capture_open_file(cli.auto_pcap, bpf_extra);
            if (!tcap) {
                fprintf(stderr, "[main] failed to open auto PCAP: %s\n", cli.auto_pcap);
            } else {
                tctx.datalink = capture_datalink(tcap);
                tctx.last_expire = 0;
                capture_loop(tcap, train_on_packet, &tctx);
                capture_close(tcap);
                /* Cleanup parser/reassembly contexts */
                pg_parser_ctx_free(&tctx.parser);
                reassembly_ctx_free(&tctx.reasm);

                g_app.ngram_loaded = 1;
                fprintf(stderr, "[main] auto-trained model: total=%llu vocab=%llu\n",
                        (unsigned long long)g_app.ngram.total,
                        (unsigned long long)g_app.ngram.vocab_size);
            }
        }

        if (auto_baseline_temp_file) {
            if (remove(auto_baseline_path_buf) != 0)
                fprintf(stderr, "[main] warning: failed to remove temp baseline pcap: %s\n",
                        auto_baseline_path_buf);
        }
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

    /* ---- Load network config (optional) ---- */
    net_config_t net_cfg;
    memset(&net_cfg, 0, sizeof(net_cfg));
    char *custom_bpf = NULL;

    if (cli.net_config_path) {
        if (net_config_load(&net_cfg, cli.net_config_path) < 0) {
            fprintf(stderr, "[main] warning: failed to load network config: %s\n",
                    cli.net_config_path);
        } else {
            /* Override alert_path if specified in config */
            if (net_cfg.output_path) {
                alert_path = net_cfg.output_path;
                fprintf(stderr, "[main] alert_path overridden from network config: %s\n",
                        alert_path);
            }
            /* Build custom BPF filter from network config */
            custom_bpf = net_config_build_ip_port_filter(&net_cfg);
            if (custom_bpf) {
                fprintf(stderr, "[main] custom BPF filter from network config: %s\n",
                        custom_bpf);
            }
        }
    }

    /* ---- Open capture ---- */
    capture_ctx_t *cap;
    if (custom_bpf) {
        /* Use custom BPF from network config */
        cap = pcap_file
            ? capture_open_file_with_filter(pcap_file, custom_bpf)
            : capture_open_live_with_filter(iface, custom_bpf);
    } else {
        /* Use default BPF (tcp port 5432 + optional bpf_extra) */
        cap = pcap_file
            ? capture_open_file(pcap_file, bpf_extra)
            : capture_open_live(iface,     bpf_extra);
    }

    if (custom_bpf) free(custom_bpf);

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
    net_config_free(&net_cfg);

    return 0;
}
