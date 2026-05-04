#include "analysis/query_eval.h"

#include <stdio.h>
#include <string.h>

void query_eval_init(query_eval_ctx_t *ctx,
                     detector_ctx_t   *detector,
                     ngram_model_t    *ngram,
                     int              ngram_loaded,
                     double           anomaly_threshold,
                     alert_ctx_t      *alert,
                     int              verbose)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->detector = detector;
    ctx->ngram = ngram;
    ctx->ngram_loaded = ngram_loaded;
    ctx->anomaly_threshold = anomaly_threshold;
    ctx->alert = alert;
    ctx->verbose = verbose;
}

void query_eval_run(query_eval_ctx_t    *ctx,
                    const flow_t        *flow,
                    const char          *sql,
                    int                  emit_all,
                    const pg_row_t      *pg_row,
                    const struct pcap_pkthdr *raw_hdr,
                    const uint8_t       *raw_pkt)
{
    if (!ctx || !ctx->detector || !ctx->alert || !sql) return;

    detect_result_t result;
    detector_check(ctx->detector, sql, &result);

    double ascore = 0.0;
    if (ctx->ngram_loaded && ctx->ngram)
        ascore = ngram_score(ctx->ngram, sql);

    int flag_rule = result.matched;
    int flag_anom = (ctx->ngram_loaded &&
                     ctx->anomaly_threshold != 0.0 &&
                     ascore < ctx->anomaly_threshold);

    if (!emit_all && !flag_rule && !flag_anom)
        return;

    alert_emit(ctx->alert,
               flow, sql, &result, ascore,
               pg_row,
               raw_hdr, raw_pkt);

    if (ctx->verbose) {
        char flow_id[17] = "0000000000000000";
        if (flow)
            reassembly_flow_id(flow, flow_id);

        fprintf(stderr,
                "[QUERY] flow=%s risk=%s rules=%d anomaly=%.3f sql=%.80s\n",
                flow_id,
                alert_risk_level(result.score, ascore, ctx->anomaly_threshold),
                result.match_count, ascore, sql);
    }
}