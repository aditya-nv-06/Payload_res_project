/*
 * net_config.h – Network monitoring configuration parser
 *
 * Parses a config file to extract:
 *   - ports to monitor (comma-separated list)
 *   - IPs to monitor (optional; comma-separated list)
 *   - output path (optional; overrides CLI -o)
 */

#ifndef NET_CONFIG_H
#define NET_CONFIG_H

#include <stddef.h>

/* Max number of ports or IPs to monitor */
#define NET_CONFIG_MAX_PORTS  32
#define NET_CONFIG_MAX_IPS    32

typedef struct {
    int    ports[NET_CONFIG_MAX_PORTS];
    int    port_count;
    
    char  *ips[NET_CONFIG_MAX_IPS];      /* dynamically allocated */
    int    ip_count;
    
    char  *output_path;                 /* dynamically allocated */
} net_config_t;

/* ====================================================================== */

/*
 * Load and parse a network config file.
 * Returns 0 on success, -1 on error.
 * Config format (simple key=value, one per line):
 *   ports=5432,5433,5434
 *   ips=10.0.0.5,192.168.1.100
 *   output_path=/var/log/pqcheck/alerts.jsonl
 */
int net_config_load(net_config_t *cfg, const char *path);

/*
 * Build a BPF port filter expression from the ports list.
 * e.g., "tcp port 5432" or "(tcp port 5432 or tcp port 5433)"
 * Returns a dynamically allocated string that must be freed by the caller.
 */
char *net_config_build_port_filter(const net_config_t *cfg);

/*
 * Build a combined BPF filter with IP and port constraints.
 * e.g., "(dst net 10.0.0.5 or dst net 192.168.1.100) and (tcp port 5432 or tcp port 5433)"
 * Returns a dynamically allocated string that must be freed by the caller.
 */
char *net_config_build_ip_port_filter(const net_config_t *cfg);

/* Free dynamically allocated resources in the config */
void net_config_free(net_config_t *cfg);

#endif /* NET_CONFIG_H */
