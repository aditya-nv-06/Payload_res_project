#ifndef CLI_H
#define CLI_H

typedef struct {
    const char *iface;
    const char *pcap_file;
    const char *bpf_extra;
    const char *rules_path;
    const char *alert_path;
    const char *pcap_dump;
    const char *model_path;
    const char *corpus_path;
    const char *pg_connstr;
    const char *db_connstr;
#ifdef WITH_LIBPQ
    const char *db_sql;
#endif
    double      anomaly_thr;
    int         verbose;
} cli_options_t;

void cli_options_init(cli_options_t *opts);
int cli_parse(int argc, char **argv, cli_options_t *opts, const char *prog);

#endif /* CLI_H */