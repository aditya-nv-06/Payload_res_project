/*
 * tests/test_detector.c – Unit tests for the detector and ngram modules
 *
 * Compile & run:
 *   make test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "analysis/detector.h"
#include "analysis/ngram.h"
#include "output/alert.h"

/* -------------------------------------------------------------------------- */
/* Minimal test framework                                                     */
/* -------------------------------------------------------------------------- */

static int g_pass = 0, g_fail = 0;

#define ASSERT(cond, msg) do {                                     \
    if (cond) { printf("  PASS: %s\n", msg); g_pass++; }          \
    else       { printf("  FAIL: %s  (line %d)\n", msg, __LINE__); \
                 g_fail++; }                                        \
} while (0)

/* -------------------------------------------------------------------------- */
/* Helper: create a temporary rule file                                       */
/* -------------------------------------------------------------------------- */

static const char *make_rule_file(void)
{
    static char path[] = "/tmp/test_rules_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) { perror("mkstemp"); return NULL; }

    const char *rules =
        "UNION_SELECT|KEYWORD|union select|CRITICAL\n"
        "OR_TAUTOLOGY|REGEX|or\\s+[0-9]+=\\s*[0-9]+|HIGH\n"
        "INLINE_COMMENT|KEYWORD|/**/|MEDIUM\n"
        "PG_SLEEP|KEYWORD|pg_sleep|HIGH\n"
        "INFORMATION_SCHEMA|KEYWORD|information_schema|HIGH\n";

    FILE *fp = fdopen(fd, "w");
    fputs(rules, fp);
    fclose(fp);
    return path;
}

/* -------------------------------------------------------------------------- */
/* Detector tests                                                             */
/* -------------------------------------------------------------------------- */

static void test_detector(void)
{
    printf("\n=== Detector tests ===\n");

    const char *rule_path = make_rule_file();
    if (!rule_path) { printf("  SKIP (cannot create temp file)\n"); return; }

    detector_ctx_t det;
    int rc = detector_load(&det, rule_path);
    ASSERT(rc == 0, "detector_load succeeds");
    ASSERT(det.rule_count > 0, "at least one rule loaded");

    detect_result_t result;

    /* ---- Clean queries: expect no match ---- */
    detector_check(&det,
        "SELECT id, name FROM users WHERE id = $1",
        &result);
    ASSERT(!result.matched, "clean parameterised query: no match");

    detector_check(&det,
        "SELECT count(*) FROM orders",
        &result);
    ASSERT(!result.matched, "clean SELECT: no match");

    /* ---- UNION SELECT injection ---- */
    detector_check(&det,
        "' UNION SELECT username, password FROM pg_shadow--",
        &result);
    ASSERT(result.matched,          "UNION SELECT: matched");
    ASSERT(result.match_count >= 1, "UNION SELECT: at least one rule fired");

    /* ---- Boolean tautology (regex) ---- */
    detector_check(&det,
        "admin' OR 1=1--",
        &result);
    ASSERT(result.matched, "OR 1=1 tautology: matched");

    /* ---- Case-insensitive variant ---- */
    detector_check(&det,
        "' UnIoN SeLeCt 1,2,3--",
        &result);
    ASSERT(result.matched, "UNION SELECT (mixed case): matched");

    /* ---- Inline comment obfuscation ---- */
    detector_check(&det,
        "'/**/UNION/**/SELECT/**/1,2--",
        &result);
    ASSERT(result.matched, "inline comment obfuscation: matched");

    /* ---- pg_sleep time-based injection ---- */
    detector_check(&det,
        "1; SELECT pg_sleep(5)--",
        &result);
    ASSERT(result.matched, "pg_sleep time-based injection: matched");

    /* ---- Severity ordering ---- */
    detector_check(&det,
        "' UNION SELECT * FROM information_schema.tables--",
        &result);
    ASSERT(result.matched,
           "UNION + information_schema: matched");
    ASSERT(result.max_severity >= SEV_HIGH,
           "max severity is at least HIGH");

    /* ---- Severity string helper ---- */
    ASSERT(strcmp(detector_severity_str(SEV_CRITICAL), "CRITICAL") == 0,
           "severity string CRITICAL");
    ASSERT(strcmp(detector_severity_str(SEV_LOW),      "LOW")      == 0,
           "severity string LOW");

    detector_free(&det);
    remove(rule_path);
}

/* -------------------------------------------------------------------------- */
/* N-gram tests                                                               */
/* -------------------------------------------------------------------------- */

static void test_ngram(void)
{
    printf("\n=== N-gram tests ===\n");

    ngram_model_t m;
    ngram_init(&m, 3, 0.5);

    /* Train on a small legitimate corpus */
    const char *legit[] = {
        "SELECT id, name FROM users WHERE id = $1",
        "SELECT count(*) FROM orders WHERE status = 'complete'",
        "INSERT INTO events (ts, msg) VALUES (now(), $1)",
        "UPDATE users SET last_login = now() WHERE id = $1",
        "SELECT u.id, o.total FROM users u JOIN orders o ON u.id = o.user_id",
        NULL
    };
    for (int i = 0; legit[i]; i++)
        ngram_train(&m, legit[i]);

    ASSERT(m.total > 0,      "training populated model (total > 0)");
    ASSERT(m.vocab_size > 0, "training populated vocab");

    /* A legitimate query should score higher (less negative) than an injection */
    double score_legit = ngram_score(&m,
        "SELECT name FROM users WHERE id = $1");
    double score_sqli  = ngram_score(&m,
        "' UNION SELECT username,password FROM pg_shadow--");

    printf("  INFO: legit score=%.4f  sqli score=%.4f\n",
           score_legit, score_sqli);
    ASSERT(score_legit > score_sqli,
           "legitimate query scores higher than SQLi query");

    /* Save and reload model */
    const char *model_path = "/tmp/test_ngram_model.txt";
    int r = ngram_save(&m, model_path);
    ASSERT(r == 0, "ngram_save succeeds");

    ngram_model_t m2;
    ngram_init(&m2, 3, 0.5);
    r = ngram_load(&m2, model_path);
    ASSERT(r == 0, "ngram_load succeeds");

    double score_reload = ngram_score(&m2,
        "SELECT name FROM users WHERE id = $1");
    /* Scores should be very close after reload */
    ASSERT(fabs(score_reload - score_legit) < 0.01,
           "reload: same score as original model");

    ngram_free(&m);
    ngram_free(&m2);
    remove(model_path);
}

/* -------------------------------------------------------------------------- */
/* Alert risk-level helper tests                                              */
/* -------------------------------------------------------------------------- */

static void test_risk_level(void)
{
    printf("\n=== Risk-level tests ===\n");

    /* No anomaly scoring path */
    ASSERT(strcmp(alert_risk_level(SEV_CRITICAL, 0.0, 0.0), "CRITICAL") == 0,
           "CRITICAL rule score → CRITICAL risk");
    ASSERT(strcmp(alert_risk_level(SEV_HIGH,     0.0, 0.0), "HIGH")     == 0,
           "HIGH rule score → HIGH risk");
    ASSERT(strcmp(alert_risk_level(SEV_MEDIUM,   0.0, 0.0), "MEDIUM")   == 0,
           "MEDIUM rule score → MEDIUM risk");
    ASSERT(strcmp(alert_risk_level(SEV_LOW,      0.0, 0.0), "LOW")      == 0,
           "LOW rule score → LOW risk");

    /* Anomaly alone should raise level */
    ASSERT(strcmp(alert_risk_level(0, -10.0, -5.0), "HIGH") == 0,
           "Anomaly score below threshold → HIGH risk");
}

/* Expose alert_risk_level for testing without linking full alert.c symbols */

/* -------------------------------------------------------------------------- */
/* main                                                                       */
/* -------------------------------------------------------------------------- */

int main(void)
{
    printf("pqCheck unit tests\n");
    printf("====================\n");

    test_detector();
    test_ngram();
    test_risk_level();

    printf("\n====================\n");
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);
    return (g_fail > 0) ? 1 : 0;
}
