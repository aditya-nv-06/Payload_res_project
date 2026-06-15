#include "app/cli_help.h"

#include <stdio.h>
#include <string.h>

#define PQCHECK_VERSION "1.0.0"

void cli_print_version(const char *prog)
{
    printf("%s v%s\n"
           "PostgreSQL Payload Fragmentation & SQLi Detection Sensor\n",
           prog,
           PQCHECK_VERSION);
}

void cli_print_usage(const char *prog)
{
    const int inner_width = 60;
    char title[128];
    snprintf(title, sizeof(title), "PostgreSQL IDS - SQLi Detection Sensor v%s", PQCHECK_VERSION);
    int tlen = (int)strlen(title);
    int left = 0, right = 0;
    if (tlen < inner_width) {
        left = (inner_width - tlen) / 2;
        right = inner_width - tlen - left;
    }

    fprintf(stderr, "\n  ╔");
    for (int i = 0; i < inner_width; i++) fputs("═", stderr);
    fprintf(stderr, "╗\n  ║");
    for (int i = 0; i < left; i++) fputc(' ', stderr);
    fprintf(stderr, "%s", title);
    for (int i = 0; i < right; i++) fputc(' ', stderr);
    fprintf(stderr, "║\n  ╚");
    for (int i = 0; i < inner_width; i++) fputs("═", stderr);
    fprintf(stderr, "╝\n\n");

    fprintf(stderr, "Usage: %s [options]\n\n", prog);

    fprintf(stderr,
        "  --capture      Live-capture packets to a pcap, auto-train, then detect\n"
        "  --pcap <file>  Alias for -r <file> (explicit offline pcap input)\n"
        "  --capture-out <file>  Write live capture to this pcap path\n"
        "  --duration N   Capture duration in seconds when using --capture\n"
        "  --auto-baseline          Auto-capture baseline when live mode has no model (default: on)\n"
        "  --no-auto-baseline       Disable automatic baseline capture in live mode\n"
        "  --auto-baseline-duration N  Baseline capture seconds in live mode (default: 30)\n"
        "  --audit        Run native security audit mode and exit (file-system scan)\n"
        "  --audit-pg     Run system-level PostgreSQL audit using libpq (server checks)\n"
        "  --audit-root <path>   Root directory to scan (default: .)\n"
        "  --audit-json <file>   Save audit report as JSON\n"
        "  --audit-include-docs  Include .md/.txt in audit scan\n"
        "\n"
        "  -i <iface>     Live capture interface (default: any)\n"
        "  -r <file>      Read from offline PCAP file\n"
        "  -f <bpf>       Extra BPF filter (AND-ed with tcp port 5432)\n"
        "\n"
        "ANALYSIS OPTIONS:\n"
        "  -R <rules>     Rules config file (default: config/rules.conf)\n"
        "  -m <model>     N-gram model file (enables anomaly scoring)\n"
        "  -A <pcap>      Extract queries from PCAP and auto-train model\n"
        "  -T <thresh>    Anomaly threshold (default: -5.0)\n"
        "\n"
        "NETWORK CONFIG (alternative to CLI flags):\n"
        "  -N <config>    Network config file (ports, IPs, output_path)\n"
        "\n"
        "DB SESSION OPTIONS:\n"
        "  -d <connstr>   Connect to PostgreSQL and execute SQL strings\n");

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
        "  --gen-test     Generate synthetic test PCAP (use with -o to save)\n"
        "  --gen-clean N  Number of clean queries (default: 50)\n"
        "  --gen-inject N Number of injection queries (default: 50)\n"
        "\n"
        "DATABASE OPTIONS:\n"
        "  -c <connstr>   libpq connection string for pg_stat_activity\n"
        "\n"
        "OTHER:\n"
        "  -v             Verbose output to stderr\n"
        "  --tui          Enable interactive TUI mode (requires ncurses)\n"
        "  --version      Show version information\n"
        "  -h, --help     Show this help message\n"
        "\n"
        "EXAMPLES:\n"
        "  # Run native security audit mode\n"
        "  %s --audit --audit-root . --audit-json results/audit.json\n"
        "\n"
        "  # Run audit summary in TUI\n"
        "  %s --audit --tui\n"
        "\n"
        "  # Capture live traffic, save it, auto-train, then detect\n"
        "  %s --capture --capture-out /tmp/live.pcap --duration 30\n"
        "\n"
        "  # Use an explicit pcap input file\n"
        "  %s --pcap capture.pcap -R config/rules.conf -o alerts.jsonl\n"
        "\n"
        "  # Live capture on eth0 with verbose output\n"
        "  %s -i eth0 -R config/rules.conf -v\n"
        "\n"
        "  # First-time live run: auto-capture baseline for 45s, then monitor\n"
        "  %s -i eth0 --auto-baseline-duration 45 -R config/rules.conf -o alerts.jsonl -v\n"
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
        "  # Monitor custom PostgreSQL ports/IPs via network config\n"
        "  %s -r capture.pcap -N config/network.conf -o alerts.jsonl\n"
        "\n"
        "  # Connect to a database and evaluate each entered SQL statement\n"
        "  %s -d \"host=localhost dbname=postgres user=postgres sslmode=require\" -v\n"
        "\n"
        "  # Execute a single SQL statement in database session mode\n"
        "  %s -d \"host=localhost dbname=postgres user=postgres sslmode=require\" -e \"SELECT 1\"\n"
        "\n",
        prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}