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
 *   -v                Verbose: also print alerts to stderr
 *   -h                Show this help
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <pcap.h>
#include <time.h>

#include "util.h"
#include "capture.h"
#include "reassembly.h"
#include "pg_parser.h"
#include "detector.h"
#include "ngram.h"
#include "pg_correlate.h"
#include "alert.h"

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

    detect_result_t result;
    detector_check(&app->detector, sql, &result);

    double ascore = 0.0;
    if (app->ngram_loaded)
        ascore = ngram_score(&app->ngram, sql);

    /* Only emit an alert if something looks suspicious */
    int flag_rule  = result.matched;
    int flag_anom  = (app->ngram_loaded &&
                      app->anomaly_threshold != 0.0 &&
                      ascore < app->anomaly_threshold);

    if (!flag_rule && !flag_anom) return;

    /* pg_stat_activity correlation */
    pg_row_t pg_row;
    memset(&pg_row, 0, sizeof(pg_row));
    int have_pg = 0;
    if (app->pg_corr)
        have_pg = pg_correlate_lookup(app->pg_corr, flow, &pg_row);

    alert_emit(&app->alert,
               flow, sql, &result, ascore,
               have_pg ? &pg_row : NULL,
               app->cur_hdr, app->cur_pkt);

    if (app->verbose) {
        char flow_id[17];
        reassembly_flow_id(flow, flow_id);
        fprintf(stderr,
                "[ALERT] flow=%s risk=%s rules=%d anomaly=%.3f sql=%.80s\n",
                flow_id,
                alert_risk_level(result.score, ascore, app->anomaly_threshold),
                result.match_count, ascore, sql);
    }
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

/* ========================================================================= */
/* Packet callback (called by libpcap for every captured packet)            */
/* ========================================================================= */

/*
 * Extract src/dst IP and TCP segment info from a raw packet and feed it
 * to the reassembler.  Handles Ethernet (DLT_EN10MB) and Linux cooked
 * (DLT_LINUX_SLL) link layers.
 */
static void on_packet(const struct pcap_pkthdr *hdr,
                      const uint8_t            *pkt,
                      void                     *user)
{
    app_ctx_t  *app = (app_ctx_t *)user;
    static time_t last_expire = 0;

    /* Store for optional PCAP dump */
    app->cur_hdr = hdr;
    app->cur_pkt = pkt;

    const uint8_t *ip_start = NULL;
    uint32_t       caplen   = hdr->caplen;

    switch (app->datalink) {
    case DLT_EN10MB:
        /* 14-byte Ethernet header */
        if (caplen < 14) return;
        /* Check EtherType = IPv4 (0x0800) */
        if (pkt[12] != 0x08 || pkt[13] != 0x00) return;
        ip_start = pkt + 14;
        caplen  -= 14;
        break;
    case DLT_LINUX_SLL:
        /* 16-byte Linux cooked header */
        if (caplen < 16) return;
        /* Bytes 14-15: EtherType */
        if (pkt[14] != 0x08 || pkt[15] != 0x00) return;
        ip_start = pkt + 16;
        caplen  -= 16;
        break;
    case DLT_RAW:
        ip_start = pkt;
        break;
    default:
        return;
    }

    if (!ip_start || caplen < sizeof(struct ip)) return;

    const struct ip *iph = (const struct ip *)ip_start;
    if (iph->ip_v != 4) return;                     /* IPv4 only */
    if (iph->ip_p != IPPROTO_TCP) return;

    uint32_t ip_hlen = (uint32_t)(iph->ip_hl) * 4;
    if (ip_hlen < 20 || ip_hlen > caplen) return;

    const struct tcphdr *tcph = (const struct tcphdr *)(ip_start + ip_hlen);
    uint32_t remaining = caplen - ip_hlen;
    if (remaining < sizeof(struct tcphdr)) return;

    uint32_t tcp_hlen = (uint32_t)(tcph->th_off) * 4;
    if (tcp_hlen < 20 || tcp_hlen > remaining) return;

    const uint8_t *payload     = ip_start + ip_hlen + tcp_hlen;
    uint32_t       payload_len = remaining - tcp_hlen;

    uint32_t src_ip   = ntohl(iph->ip_src.s_addr);
    uint32_t dst_ip   = ntohl(iph->ip_dst.s_addr);
    uint16_t src_port = ntohs(tcph->th_sport);
    uint16_t dst_port = ntohs(tcph->th_dport);
    uint32_t seq      = ntohl(tcph->th_seq);
    uint8_t  flags    = (uint8_t)tcph->th_flags;

    time_t now = (time_t)hdr->ts.tv_sec;

    reassembly_feed(&app->reassembly,
                    src_ip, dst_ip, src_port, dst_port,
                    seq, flags, payload, payload_len, now);

    /* Periodic flow expiry (every 30 seconds of capture time) */
    if (now - last_expire > 30) {
        reassembly_expire(&app->reassembly, now);
        last_expire = now;
    }
}

/* ========================================================================= */
/* Version and Usage                                                          */
/* ========================================================================= */

#define PGSQL_IDS_VERSION "1.0.0"

static void print_version(void)
{
    printf("pgsql_ids v%s\n"
           "PostgreSQL Payload Fragmentation & SQLi Detection Sensor\n",
           PGSQL_IDS_VERSION);
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "\n"
        "  ╔════════════════════════════════════════════════════════════╗\n"
        "  ║     PostgreSQL IDS - SQLi Detection Sensor v%s              ║\n"
        "  ╚════════════════════════════════════════════════════════════╝\n"
        "\n"
        "Usage: %s [options]\n"
        "\n"
        "CAPTURE OPTIONS:\n"
        "  -i <iface>     Live capture interface (default: any)\n"
        "  -r <file>      Read from offline PCAP file\n"
        "  -f <bpf>       Extra BPF filter (AND-ed with tcp port 5432)\n"
        "\n"
        "ANALYSIS OPTIONS:\n"
        "  -R <rules>     Rules config file (default: config/rules.conf)\n"
        "  -m <model>     N-gram model file (enables anomaly scoring)\n"
        "  -T <thresh>    Anomaly threshold (default: -5.0)\n"
        "\n"
        "OUTPUT OPTIONS:\n"
        "  -o <file>      Alert log file (default: alerts.jsonl)\n"
        "  -p <file>      Dump flagged packets to PCAP file\n"
        "\n"
        "TRAINING:\n"
        "  -t <corpus>    Train n-gram model from corpus (requires -m)\n"
        "\n"
        "DATABASE OPTIONS:\n"
        "  -c <connstr>   libpq connection string for pg_stat_activity\n"
        "\n"
        "OTHER:\n"
        "  -v             Verbose output to stderr\n"
        "  --version      Show version information\n"
        "  -h, --help     Show this help message\n"
        "\n"
        "EXAMPLES:\n"
        "  # Live capture on eth0 with verbose output\n"
        "  %s -i eth0 -R config/rules.conf -v\n"
        "\n"
        "  # Analyze offline PCAP file\n"
        "  %s -r capture.pcap -R config/rules.conf -o alerts.jsonl\n"
        "\n"
        "  # Train anomaly model from SQL corpus\n"
        "  %s -t corpus.sql -m model.ngram -v\n"
        "\n"
        "  # Advanced: anomaly + rule detection + packet dump\n"
        "  %s -r pcap.file -m model.ngram -p flagged.pcap -o alerts.jsonl\n"
        "\n",
        PGSQL_IDS_VERSION, prog, prog, prog, prog, prog);
}

/* ========================================================================= */
/* main                                                                      */
/* ========================================================================= */

int main(int argc, char *argv[])
{
    /* ---- Defaults ---- */
    const char *iface       = "any";
    const char *pcap_file   = NULL;
    const char *bpf_extra   = NULL;
    const char *rules_path  = "config/rules.conf";
    const char *alert_path  = "alerts.jsonl";
    const char *pcap_dump   = NULL;
    const char *model_path  = NULL;
    const char *corpus_path = NULL;
    const char *pg_connstr  = NULL;
    double      anomaly_thr = -5.0;
    int         verbose     = 0;

    /* ---- Handle long options ---- */
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[1], "--version") == 0) {
            print_version();
            return 0;
        }
    }

    int opt;
    while ((opt = getopt(argc, argv, "i:r:f:R:o:p:m:t:T:c:vh")) != -1) {
        switch (opt) {
        case 'i': iface       = optarg; break;
        case 'r': pcap_file   = optarg; break;
        case 'f': bpf_extra   = optarg; break;
        case 'R': rules_path  = optarg; break;
        case 'o': alert_path  = optarg; break;
        case 'p': pcap_dump   = optarg; break;
        case 'm': model_path  = optarg; break;
        case 't': corpus_path = optarg; break;
        case 'T': 
            anomaly_thr = atof(optarg);
            if (anomaly_thr == 0.0) {
                fprintf(stderr, "[ERROR] Invalid threshold value: %s\n", optarg);
                return 1;
            }
            break;
        case 'c': pg_connstr  = optarg; break;
        case 'v': verbose     = 1; break;
        case 'h': usage(argv[0]); return 0;
        case '?':
        default:
            fprintf(stderr, "\n[ERROR] Invalid option: -%c\n", optopt);
            fprintf(stderr, "Use '%s -h' for help.\n\n", argv[0]);
            return 1;
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
