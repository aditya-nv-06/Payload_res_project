#include "app/cli.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/cli_help.h"

void cli_options_init(cli_options_t *opts)
{
    opts->iface       = "any";
    opts->pcap_file   = NULL;
    opts->capture_pcap = NULL;
    opts->bpf_extra   = NULL;
    opts->rules_path  = "config/rules.conf";
    opts->alert_path  = "alerts.jsonl";
    opts->pcap_dump   = NULL;
    opts->model_path  = NULL;
    opts->corpus_path = NULL;
    opts->auto_pcap    = NULL;
    opts->net_config_path = NULL;
    opts->pg_connstr  = NULL;
    opts->db_connstr  = NULL;
    opts->audit_root  = ".";
    opts->audit_json_out = NULL;
    opts->capture_mode = 0;
    opts->capture_seconds = 30;
    opts->audit_mode = 0;
    opts->audit_include_docs = 0;
    opts->audit_pg_only = 0;
#ifdef WITH_LIBPQ
    opts->db_sql      = NULL;
#endif
    opts->tui_mode    = 0;
    opts->gen_test_output = 0;
    opts->gen_test_injection = 0;
    opts->gen_test_clean_count = 50;
    opts->gen_test_injection_count = 50;
    opts->anomaly_thr = -5.0;
    opts->verbose     = 0;
}

int cli_parse(int argc, char **argv, cli_options_t *opts, const char *prog)
{
    struct option long_opts[] = {
        {"help",    no_argument,       NULL, 'h'},
        {"version", no_argument,       NULL,  1 },
        {"capture", no_argument,       NULL,  6 },
        {"pcap",    required_argument,  NULL,  'r' },
        {"capture-out", required_argument, NULL,  7 },
        {"duration", required_argument, NULL,  8 },
        {"audit", no_argument, NULL, 9 },
        {"audit-root", required_argument, NULL, 10 },
        {"audit-json", required_argument, NULL, 11 },
        {"audit-include-docs", no_argument, NULL, 12 },
        {"audit-pg", no_argument, NULL, 13 },
        {"auto",    required_argument, NULL,  'A' },
        {"net-config", required_argument, NULL, 'N'},
        {"tui",     no_argument,       NULL,  2 },
        {"gen-test", no_argument,      NULL,  3 },
        {"gen-clean", required_argument, NULL, 4 },
        {"gen-inject", required_argument, NULL, 5 },
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:r:f:R:o:p:m:t:T:c:d:e:vhA:N:",
                              long_opts, NULL)) != -1) {
        switch (opt) {
        case 'i': opts->iface       = optarg; break;
        case 'r': opts->pcap_file   = optarg; break;
        case 'f': opts->bpf_extra   = optarg; break;
        case 'R': opts->rules_path  = optarg; break;
        case 'o': opts->alert_path  = optarg; break;
        case 'p': opts->pcap_dump   = optarg; break;
        case 'm': opts->model_path  = optarg; break;
        case 't': opts->corpus_path = optarg; break;
        case 'A': opts->auto_pcap    = optarg; break;
        case 'N': opts->net_config_path = optarg; break;
        case 6:   opts->capture_mode = 1; break; /* --capture */
        case 7:   opts->capture_pcap = optarg; break; /* --capture-out */
        case 8:   opts->capture_seconds = atoi(optarg); break; /* --duration */
        case 9:   opts->audit_mode = 1; break; /* --audit */
        case 10:  opts->audit_root = optarg; opts->audit_mode = 1; break; /* --audit-root */
        case 11:  opts->audit_json_out = optarg; opts->audit_mode = 1; break; /* --audit-json */
        case 12:  opts->audit_include_docs = 1; opts->audit_mode = 1; break; /* --audit-include-docs */
        case 13:  opts->audit_mode = 1; opts->audit_pg_only = 1; break; /* --audit-pg */
        case 2:   opts->tui_mode    = 1; break;          /* --tui */
        case 3:   opts->gen_test_output = 1; break;      /* --gen-test */
        case 4:   opts->gen_test_clean_count = atoi(optarg); break; /* --gen-clean */
        case 5:   opts->gen_test_injection_count = atoi(optarg); break; /* --gen-inject */
        case 'T':
            opts->anomaly_thr = atof(optarg);
            if (opts->anomaly_thr == 0.0) {
                fprintf(stderr, "[ERROR] Invalid threshold value: %s\n", optarg);
                return -1;
            }
            break;
        case 'c': opts->pg_connstr  = optarg; break;
        case 'd': opts->db_connstr  = optarg; break;
#ifdef WITH_LIBPQ
        case 'e': opts->db_sql      = optarg; break;
#endif
        case 'v': opts->verbose     = 1; break;
        case 'h':
            cli_print_usage(prog);
            return 1;
        case 1:
            cli_print_version(prog);
            return 1;
        case '?':
        default:
            fprintf(stderr, "\n[ERROR] Invalid option: -%c\n", optopt);
            fprintf(stderr, "Use '%s -h' for help.\n\n", prog);
            return -1;
        }
    }

    return 0;
}