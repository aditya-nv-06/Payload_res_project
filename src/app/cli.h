#ifndef CLI_H
#define CLI_H

typedef struct {
    const char *iface;
    const char *pcap_file;
    const char *capture_pcap;
    const char *bpf_extra;
    const char *rules_path;
    const char *alert_path;
    const char *pcap_dump;
    const char *model_path;
    const char *corpus_path;
    const char *auto_pcap;    /* Path to pcap used to auto-train model */
    const char *net_config_path; /* Network config (ports, IPs, output_path) */
    const char *pg_connstr;
    const char *db_connstr;
    const char *audit_root;
    const char *audit_json_out;
    int         capture_mode;
    int         capture_seconds;
    int         audit_mode;
    int         audit_include_docs;
    int         audit_pg_only; /* Explicit system-level Postgres audit */
#ifdef WITH_LIBPQ
    const char *db_sql;
#endif
    int         tui_mode;         /* Enable TUI mode */
    int         gen_test_output;  /* Generate test PCAP: non-zero = output file (1 for default) */
    int         gen_test_injection; /* Include SQL injection in generated PCAP */
    int         gen_test_clean_count;  /* Number of clean queries */
    int         gen_test_injection_count;  /* Number of injection queries */
    double      anomaly_thr;
    int         verbose;
} cli_options_t;

void cli_options_init(cli_options_t *opts);
int cli_parse(int argc, char **argv, cli_options_t *opts, const char *prog);

#endif /* CLI_H */