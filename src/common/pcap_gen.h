#ifndef PCAP_GEN_H
#define PCAP_GEN_H

#include <stdint.h>
#include <time.h>

/**
 * Simple PCAP file generator for testing.
 * Writes synthetic PostgreSQL protocol packets to PCAP format.
 */

typedef struct {
    int num_clean;
    int num_injection;
    uint32_t seed;
} pcap_gen_opts_t;

/**
 * Generate a PCAP file with synthetic PostgreSQL traffic.
 *
 * @param filename Output PCAP file path
 * @param opts Generation options (num_clean, num_injection, seed)
 * @return 0 on success, -1 on error
 */
int pcap_gen_write(const char *filename, const pcap_gen_opts_t *opts);

#endif // PCAP_GEN_H
