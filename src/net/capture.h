/*
 * capture.h – libpcap wrapper (Milestone 1)
 *
 * Opens a live interface or an offline .pcap file, installs a BPF filter for
 * "tcp port 5432", and delivers raw packets to a callback.
 */
#ifndef CAPTURE_H
#define CAPTURE_H

#include <pcap.h>
#include <stdint.h>
#include <stddef.h>

/* Opaque capture context */
typedef struct capture_ctx capture_ctx_t;

/*
 * Callback invoked for every matching packet.
 *   hdr   – pcap packet header (timestamps, capture/wire lengths)
 *   pkt   – raw packet bytes starting at the link layer
 *   user  – opaque pointer supplied to capture_loop()
 */
typedef void (*packet_cb_t)(u_char                   *user,
                            const struct pcap_pkthdr *hdr,
                            const uint8_t            *pkt);

/*
 * Open a live capture on 'iface'.
 * bpf_extra: additional BPF expression ANDed with "tcp port 5432", or NULL.
 * Returns NULL and prints an error on failure.
 */
capture_ctx_t *capture_open_live(const char *iface, const char *bpf_extra);

/*
 * Open an offline .pcap file for replay.
 * bpf_extra: same as above.
 */
capture_ctx_t *capture_open_file(const char *path, const char *bpf_extra);

/*
 * Open a live capture with a custom BPF filter (no default port added).
 * bpf: complete BPF expression to use as-is, or NULL for no filter.
 */
capture_ctx_t *capture_open_live_with_filter(const char *iface, const char *bpf);

/*
 * Open an offline .pcap file with a custom BPF filter (no default port added).
 * bpf: complete BPF expression to use as-is, or NULL for no filter.
 */
capture_ctx_t *capture_open_file_with_filter(const char *path, const char *bpf);

/* Return the underlying pcap_t (needed for pcap_dump). */
pcap_t *capture_pcap(capture_ctx_t *ctx);

/* Return the link-layer type (DLT_*). */
int capture_datalink(capture_ctx_t *ctx);

/*
 * Enter the capture loop.  Runs until EOF (file) or SIGINT (live).
 * cb is invoked for every packet that passes the BPF filter.
 */
void capture_loop(capture_ctx_t *ctx, packet_cb_t cb, void *user);

/* Free resources. */
void capture_close(capture_ctx_t *ctx);

#endif /* CAPTURE_H */
