/*
 * capture.c – libpcap wrapper (Milestone 1)
 */
#include "capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define SNAPLEN      65535
#define PROMISC      1
#define READ_TIMEOUT 1000   /* ms */

struct capture_ctx {
    pcap_t *handle;
    int     datalink;
    char    errbuf[PCAP_ERRBUF_SIZE];
};

/* --- internal: shared BPF setup ------------------------------------------ */

static int apply_filter(capture_ctx_t *ctx, const char *bpf_extra)
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

    if (apply_filter(ctx, bpf_extra) < 0) {
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

    if (apply_filter(ctx, bpf_extra) < 0) {
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

void capture_loop(capture_ctx_t *ctx, packet_cb_t cb, void *user)
{
    int ret = pcap_loop(ctx->handle, -1,
                        (pcap_handler)cb, (u_char *)user);
    if (ret == PCAP_ERROR)
        fprintf(stderr, "[capture] pcap_loop error: %s\n",
                pcap_geterr(ctx->handle));
    /* ret == -2 means pcap_breakloop was called (SIGINT) – normal exit */
}

void capture_close(capture_ctx_t *ctx)
{
    if (!ctx) return;
    if (ctx->handle) pcap_close(ctx->handle);
    free(ctx);
}
