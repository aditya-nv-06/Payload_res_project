/*
 * net_config.c – Network monitoring configuration parser
 */
#include "common/net_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Helper: trim whitespace from a string */
static char *trim_whitespace(char *str)
{
    if (!str) return str;
    while (*str && isspace((unsigned char)*str)) str++;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return str;
}

/* Helper: parse comma-separated integer list */
static int parse_port_list(const char *value, int *ports, int max_ports)
{
    if (!value || !ports || max_ports <= 0) return 0;

    int count = 0;
    char buf[256];
    strncpy(buf, value, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr = NULL;
    char *tok = strtok_r(buf, ",", &saveptr);
    while (tok && count < max_ports) {
        tok = trim_whitespace(tok);
        int port = atoi(tok);
        if (port > 0 && port < 65536) {
            ports[count++] = port;
        }
        tok = strtok_r(NULL, ",", &saveptr);
    }
    return count;
}

/* Helper: parse comma-separated IP list */
static int parse_ip_list(const char *value, char **ips, int max_ips)
{
    if (!value || !ips || max_ips <= 0) return 0;

    int count = 0;
    char buf[1024];
    strncpy(buf, value, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr = NULL;
    char *tok = strtok_r(buf, ",", &saveptr);
    while (tok && count < max_ips) {
        tok = trim_whitespace(tok);
        if (*tok) {
            ips[count] = malloc(strlen(tok) + 1);
            if (ips[count]) {
                strcpy(ips[count], tok);
                count++;
            }
        }
        tok = strtok_r(NULL, ",", &saveptr);
    }
    return count;
}

int net_config_load(net_config_t *cfg, const char *path)
{
    if (!cfg || !path) return -1;

    memset(cfg, 0, sizeof(*cfg));

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "[net_config] cannot open: %s\n", path);
        return -1;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';

        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\0') continue;

        /* Look for "key=value" */
        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = trim_whitespace(line);
        char *value = trim_whitespace(eq + 1);

        if (strcmp(key, "ports") == 0) {
            cfg->port_count = parse_port_list(value, cfg->ports, NET_CONFIG_MAX_PORTS);
            if (cfg->port_count > 0) {
                fprintf(stderr, "[net_config] loaded %d port(s)\n", cfg->port_count);
            }
        } else if (strcmp(key, "ips") == 0) {
            cfg->ip_count = parse_ip_list(value, cfg->ips, NET_CONFIG_MAX_IPS);
            if (cfg->ip_count > 0) {
                fprintf(stderr, "[net_config] loaded %d IP(s)\n", cfg->ip_count);
            }
        } else if (strcmp(key, "output_path") == 0) {
            cfg->output_path = malloc(strlen(value) + 1);
            if (cfg->output_path) {
                strcpy(cfg->output_path, value);
                fprintf(stderr, "[net_config] output_path: %s\n", cfg->output_path);
            }
        }
    }

    fclose(fp);

    /* Validate: at least one port should be configured */
    if (cfg->port_count == 0) {
        fprintf(stderr, "[net_config] warning: no ports configured, using default 5432\n");
        cfg->ports[0] = 5432;
        cfg->port_count = 1;
    }

    return 0;
}

char *net_config_build_port_filter(const net_config_t *cfg)
{
    if (!cfg || cfg->port_count == 0) {
        char *buf = malloc(20);
        if (buf) strcpy(buf, "tcp port 5432");
        return buf;
    }

    /* Build: "(tcp port P1 or tcp port P2 or ...)" */
    size_t needed = 512;
    char *filter = malloc(needed);
    if (!filter) return NULL;

    if (cfg->port_count == 1) {
        snprintf(filter, needed, "tcp port %d", cfg->ports[0]);
    } else {
        strcpy(filter, "(");
        for (int i = 0; i < cfg->port_count; i++) {
            if (i > 0) strcat(filter, " or ");
            char port_expr[32];
            snprintf(port_expr, sizeof(port_expr), "tcp port %d", cfg->ports[i]);
            strcat(filter, port_expr);
        }
        strcat(filter, ")");
    }

    return filter;
}

char *net_config_build_ip_port_filter(const net_config_t *cfg)
{
    char *port_filter = net_config_build_port_filter(cfg);
    if (!port_filter) return NULL;

    /* If no IPs configured, return just the port filter */
    if (cfg->ip_count == 0) {
        return port_filter;
    }

    /* Build: "(dst net IP1 or dst net IP2 or ...) and (port_filter)" */
    size_t needed = 1024;
    char *filter = malloc(needed);
    if (!filter) {
        free(port_filter);
        return NULL;
    }

    strcpy(filter, "(");
    for (int i = 0; i < cfg->ip_count; i++) {
        if (i > 0) strcat(filter, " or ");
        char ip_expr[256];
        snprintf(ip_expr, sizeof(ip_expr), "dst net %s", cfg->ips[i]);
        strcat(filter, ip_expr);
    }
    strcat(filter, ") and ");
    strcat(filter, port_filter);

    free(port_filter);
    return filter;
}

void net_config_free(net_config_t *cfg)
{
    if (!cfg) return;

    for (int i = 0; i < cfg->ip_count; i++) {
        if (cfg->ips[i]) {
            free(cfg->ips[i]);
            cfg->ips[i] = NULL;
        }
    }
    cfg->ip_count = 0;

    if (cfg->output_path) {
        free(cfg->output_path);
        cfg->output_path = NULL;
    }

    memset(cfg, 0, sizeof(*cfg));
}
