#ifndef QUERY_EVAL_H
#define QUERY_EVAL_H

#include <pcap.h>

#include "alert.h"
#include "detector.h"
#include "ngram.h"
#include "pg_correlate.h"
#include "reassembly.h"

typedef struct {
    detector_ctx_t *detector;
    ngram_model_t  *ngram;
    int             ngram_loaded;
    double          anomaly_threshold;
    alert_ctx_t    *alert;
    int             verbose;
} query_eval_ctx_t;

void query_eval_init(query_eval_ctx_t *ctx,
                     detector_ctx_t   *detector,
                     ngram_model_t    *ngram,
                     int              ngram_loaded,
                     double           anomaly_threshold,
                     alert_ctx_t      *alert,
                     int              verbose);

void query_eval_run(query_eval_ctx_t    *ctx,
                    const flow_t        *flow,
                    const char          *sql,
                    int                  emit_all,
                    const pg_row_t      *pg_row,
                    const struct pcap_pkthdr *raw_hdr,
                    const uint8_t       *raw_pkt);

#endif /* QUERY_EVAL_H */