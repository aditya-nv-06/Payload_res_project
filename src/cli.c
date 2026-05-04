#include "cli.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#define PGSQL_IDS_VERSION "1.0.0"

void cli_options_init(cli_options_t *opts)
{
    opts->iface       = "any";
    opts->pcap_file   = NULL;
    opts->bpf_extra   = NULL;
    opts->rules_path  = "config/rules.conf";
    opts->alert_path  = "alerts.jsonl";
    opts->pcap_dump   = NULL;
    opts->model_path  = NULL;
    opts->corpus_path = NULL;
    opts->pg_connstr  = NULL;
    opts->db_connstr  = NULL;
#ifdef WITH_LIBPQ
    opts->db_sql      = NULL;
#endif
    opts->anomaly_thr = -5.0;
    opts->verbose     = 0;
}

static void print_version(const char *prog)
{
    printf("%s v%s\n"
           "PostgreSQL Payload Fragmentation & SQLi Detection Sensor\n",
           prog,
           PGSQL_IDS_VERSION);
}

static void print_usage(const char *prog)
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
        "DB SESSION OPTIONS:\n"
        "  -d <connstr>   Connect to PostgreSQL and execute SQL strings\n",
        PGSQL_IDS_VERSION, prog);

#ifdef WITH_LIBPQ
    fprintf(stderr,
        "  -e <sql>       Execute one SQL statement in -d mode, then exit\n");
#endif

    fprintf(stderr,
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
        "\n"
        "  # Connect to a database and evaluate each entered SQL statement\n"
        "  %s -d \"host=localhost dbname=postgres user=postgres\" -v\n"
        "\n"
        "  # Execute a single SQL statement in database session mode\n"
        "  %s -d \"host=localhost dbname=postgres user=postgres\" -e \"SELECT 1\"\n"
        "\n",
        prog, prog, prog, prog, prog, prog);
}

int cli_parse(int argc, char **argv, cli_options_t *opts, const char *prog)
{
    struct option long_opts[] = {
        {"help",    no_argument,       NULL, 'h'},
        {"version", no_argument,       NULL,  1 },
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:r:f:R:o:p:m:t:T:c:d:e:vh",
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
            print_usage(prog);
            return 1;
        case 1:
            print_version(prog);
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