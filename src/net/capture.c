/*
 * capture.c – libpcap wrapper (Milestone 1)
 */
#include "net/capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#define SNAPLEN      65535
#define PROMISC      1
#define READ_TIMEOUT 1000   /* ms */

struct capture_ctx {
    pcap_t *handle;
    int     datalink;
    char    errbuf[PCAP_ERRBUF_SIZE];
};

/* --- internal: shared BPF setup ------------------------------------------ */

/* Apply a filter by combining with default port, or use custom filter */
static int apply_filter_extra(capture_ctx_t *ctx, const char *bpf_extra)
{
    char expr[512];
    if (bpf_extra && bpf_extra[0])
        snprintf(expr, sizeof(expr), "tcp port 5432 and (%s)", bpf_extra);
    else
        snprintf(expr, sizeof(expr), "tcp port 5432");

    struct bpf_program fp;
    if (pcap_compile(ctx->handle, &fp, expr, 1, PCAP_NETMASK_UNKNOWN) < 0) {
        fprintf(stderr, "[capture] pcap_compile failed: %s\n",
                pcap_geterr(ctx->handle));
        return -1;
    }
    if (pcap_setfilter(ctx->handle, &fp) < 0) {
        fprintf(stderr, "[capture] pcap_setfilter failed: %s\n",
                pcap_geterr(ctx->handle));
        pcap_freecode(&fp);
        return -1;
    }
    pcap_freecode(&fp);
    return 0;
}

/* Apply a custom BPF filter as-is */
static int apply_filter_custom(capture_ctx_t *ctx, const char *bpf)
{
    if (!bpf || !bpf[0]) {
        /* No filter – capture all */
        return 0;
    }

    struct bpf_program fp;
    if (pcap_compile(ctx->handle, &fp, bpf, 1, PCAP_NETMASK_UNKNOWN) < 0) {
        fprintf(stderr, "[capture] pcap_compile failed (filter: %s): %s\n",
                bpf, pcap_geterr(ctx->handle));
        return -1;
    }
    if (pcap_setfilter(ctx->handle, &fp) < 0) {
        fprintf(stderr, "[capture] pcap_setfilter failed (filter: %s): %s\n",
                bpf, pcap_geterr(ctx->handle));
        pcap_freecode(&fp);
        return -1;
    }
    pcap_freecode(&fp);
    return 0;
}

/* --- SIGINT handler: break pcap_loop -------------------------------------- */

static pcap_t *g_sigint_handle = NULL;

static void sigint_handler(int sig)
{
    (void)sig;
    if (g_sigint_handle)
        pcap_breakloop(g_sigint_handle);
}

/* --- public API ----------------------------------------------------------- */

capture_ctx_t *capture_open_live(const char *iface, const char *bpf_extra)
{
    capture_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;

    ctx->handle = pcap_open_live(iface, SNAPLEN, PROMISC,
                                 READ_TIMEOUT, ctx->errbuf);
    if (!ctx->handle) {
        fprintf(stderr, "[capture] pcap_open_live(%s): %s\n",
                iface, ctx->errbuf);
        free(ctx);
        return NULL;
    }
    ctx->datalink = pcap_datalink(ctx->handle);

    if (apply_filter_extra(ctx, bpf_extra) < 0) {
        pcap_close(ctx->handle);
        free(ctx);
        return NULL;
    }

    /* Register SIGINT handler for graceful shutdown */
    g_sigint_handle = ctx->handle;
    signal(SIGINT, sigint_handler);

    fprintf(stderr, "[capture] live capture on %s (DLT %d), filter: tcp port 5432\n",
            iface, ctx->datalink);
    return ctx;
}

capture_ctx_t *capture_open_file(const char *path, const char *bpf_extra)
{
    capture_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;

    ctx->handle = pcap_open_offline(path, ctx->errbuf);
    if (!ctx->handle) {
        fprintf(stderr, "[capture] pcap_open_offline(%s): %s\n",
                path, ctx->errbuf);
        free(ctx);
        return NULL;
    }
    ctx->datalink = pcap_datalink(ctx->handle);

    if (apply_filter_extra(ctx, bpf_extra) < 0) {
        pcap_close(ctx->handle);
        free(ctx);
        return NULL;
    }

    fprintf(stderr, "[capture] offline file %s (DLT %d), filter: tcp port 5432\n",
            path, ctx->datalink);
    return ctx;
}

pcap_t *capture_pcap(capture_ctx_t *ctx) { return ctx->handle; }
int     capture_datalink(capture_ctx_t *ctx) { return ctx->datalink; }

capture_ctx_t *capture_open_live_with_filter(const char *iface, const char *bpf)
{
    capture_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;

    ctx->handle = pcap_open_live(iface, SNAPLEN, PROMISC,
                                 READ_TIMEOUT, ctx->errbuf);
    if (!ctx->handle) {
        fprintf(stderr, "[capture] pcap_open_live(%s): %s\n",
                iface, ctx->errbuf);
        free(ctx);
        return NULL;
    }
    ctx->datalink = pcap_datalink(ctx->handle);

    if (apply_filter_custom(ctx, bpf) < 0) {
        pcap_close(ctx->handle);
        free(ctx);
        return NULL;
    }

    /* Register SIGINT handler for graceful shutdown */
    g_sigint_handle = ctx->handle;
    signal(SIGINT, sigint_handler);

    if (bpf && bpf[0])
        fprintf(stderr, "[capture] live capture on %s (DLT %d), filter: %s\n",
                iface, ctx->datalink, bpf);
    else
        fprintf(stderr, "[capture] live capture on %s (DLT %d), no filter\n",
                iface, ctx->datalink);
    return ctx;
}

capture_ctx_t *capture_open_file_with_filter(const char *path, const char *bpf)
{
    capture_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;

    ctx->handle = pcap_open_offline(path, ctx->errbuf);
    if (!ctx->handle) {
        fprintf(stderr, "[capture] pcap_open_offline(%s): %s\n",
                path, ctx->errbuf);
        free(ctx);
        return NULL;
    }
    ctx->datalink = pcap_datalink(ctx->handle);

    if (apply_filter_custom(ctx, bpf) < 0) {
        pcap_close(ctx->handle);
        free(ctx);
        return NULL;
    }

    if (bpf && bpf[0])
        fprintf(stderr, "[capture] offline file %s (DLT %d), filter: %s\n",
                path, ctx->datalink, bpf);
    else
        fprintf(stderr, "[capture] offline file %s (DLT %d), no filter\n",
                path, ctx->datalink);
    return ctx;
}

void capture_loop(capture_ctx_t *ctx, packet_cb_t cb, void *user)
{
    int ret = pcap_loop(ctx->handle, -1,
                        (pcap_handler)cb, (u_char *)user);
    if (ret == PCAP_ERROR)
        fprintf(stderr, "[capture] pcap_loop error: %s\n",
                pcap_geterr(ctx->handle));
    /* ret == -2 means pcap_breakloop was called (SIGINT) – normal exit */
}

typedef struct {
    pcap_dumper_t *dumper;
} capture_dump_ctx_t;

static void dump_all_packets(u_char *user,
                             const struct pcap_pkthdr *hdr,
                             const uint8_t *pkt)
{
    capture_dump_ctx_t *ctx = (capture_dump_ctx_t *)user;
    if (ctx && ctx->dumper && hdr && pkt)
        pcap_dump((u_char *)ctx->dumper, hdr, pkt);
}

int capture_live_to_pcap(const char *iface,
                         const char *bpf_extra,
                         const char *out_path,
                         int duration_seconds)
{
    if (!out_path || !out_path[0]) {
        fprintf(stderr, "[capture] output pcap path cannot be empty\n");
        return -1;
    }
    if (duration_seconds <= 0) {
        fprintf(stderr, "[capture] duration must be greater than zero\n");
        return -1;
    }

    capture_ctx_t *cap = capture_open_live(iface, bpf_extra);
    if (!cap)
        return -1;

    pcap_t *pcap_handle = capture_pcap(cap);
    pcap_dumper_t *dumper = pcap_dump_open(pcap_handle, out_path);
    if (!dumper) {
        fprintf(stderr, "[capture] pcap_dump_open(%s) failed: %s\n",
                out_path, pcap_geterr(pcap_handle));
        capture_close(cap);
        return -1;
    }

    fprintf(stderr, "[capture] recording live traffic from %s to %s for %d second(s)\n",
            iface, out_path, duration_seconds);

    capture_dump_ctx_t dump_ctx = { .dumper = dumper };
    time_t end_time = time(NULL) + duration_seconds;

    while (time(NULL) < end_time) {
        int ret = pcap_dispatch(pcap_handle, 100,
                                (pcap_handler)dump_all_packets,
                                (u_char *)&dump_ctx);
        if (ret == PCAP_ERROR) {
            fprintf(stderr, "[capture] pcap_dispatch error: %s\n",
                    pcap_geterr(pcap_handle));
            pcap_dump_close(dumper);
            capture_close(cap);
            return -1;
        }
    }

    pcap_dump_flush(dumper);
    pcap_dump_close(dumper);
    capture_close(cap);

    fprintf(stderr, "[capture] live capture written to %s\n", out_path);
    return 0;
}

void capture_close(capture_ctx_t *ctx)
{
    if (!ctx) return;
    if (ctx->handle) pcap_close(ctx->handle);
    free(ctx);
}
