#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/query_eval.h"
#include "analysis/detector.h"
#include "analysis/ngram.h"
#include "db/db_session.h"

int main(void)
{
    detector_ctx_t det;
    if (detector_load(&det, "tests/rules_test.conf") < 0) {
        fprintf(stderr, "failed to load rules_test.conf\n");
        return 2;
    }

    ngram_model_t m;
    memset(&m, 0, sizeof(m));
    ngram_init(&m, NGRAM_N, NGRAM_SMOOTHING);
    /* No ngram model loaded for this test */

    query_eval_ctx_t eval;
    query_eval_init(&eval, &det, &m, 0, -5.0, NULL, 0);

    const char *benign = "SELECT id, name FROM users WHERE id = 1";
    const char *malicious = "SELECT username FROM users UNION SELECT password FROM secrets";
    const char *dropqry = "DROP TABLE users";

    detect_result_t dres;
    double score;

    int f1 = db_validate_query(&eval, benign, &dres, &score);
    int f2 = db_validate_query(&eval, malicious, &dres, &score);
    int f3 = db_validate_query(&eval, dropqry, &dres, &score);

    printf("benign flagged=%d\n", f1);
    printf("malicious flagged=%d\n", f2);
    printf("drop flagged=%d\n", f3);

    /* Expect benign=0, malicious=1, drop=1 */
    if (f1 == 0 && f2 == 1 && f3 == 1) {
        printf("db validation: PASS\n");
        return 0;
    }
    printf("db validation: FAIL\n");
    return 1;
}
