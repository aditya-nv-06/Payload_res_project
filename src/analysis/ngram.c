/*
 * ngram.c – N-gram anomaly detection (Milestone 5)
 */
#include "analysis/ngram.h"
#include "common/util.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static uint32_t ngram_hash(const char *key)
{
    return util_hash_djb2((const uint8_t *)key, strlen(key))
           & (NGRAM_TABLE_SZ - 1);
}

static ngram_entry_t *ngram_find(const ngram_model_t *m, const char *key)
{
    uint32_t h = ngram_hash(key);
    for (ngram_entry_t *e = m->table[h]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return e;
    return NULL;
}

static void ngram_increment(ngram_model_t *m, const char *key)
{
    uint32_t h = ngram_hash(key);
    for (ngram_entry_t *e = m->table[h]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            e->count++;
            m->total++;
            return;
        }
    }
    /* New entry */
    ngram_entry_t *e = calloc(1, sizeof(*e));
    if (!e) return;
    memcpy(e->key, key, (size_t)m->n);
    e->key[m->n] = '\0';
    e->count    = 1;
    e->next     = m->table[h];
    m->table[h] = e;
    m->total++;
    m->vocab_size++;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

void ngram_init(ngram_model_t *m, int n, double smoothing)
{
    memset(m, 0, sizeof(*m));
    m->n         = (n >= 1 && n <= 7) ? n : NGRAM_N;
    m->smoothing = smoothing > 0.0 ? smoothing : NGRAM_SMOOTHING;
}

void ngram_train(ngram_model_t *m, const char *text)
{
    if (!text) return;
    size_t len = strlen(text);
    if (len < (size_t)m->n) return;

    char gram[8];
    gram[m->n] = '\0';

    for (size_t i = 0; i + m->n <= len; i++) {
        memcpy(gram, text + i, (size_t)m->n);
        ngram_increment(m, gram);
    }
}

double ngram_score(const ngram_model_t *m, const char *text)
{
    if (!text) return 0.0;
    size_t len = strlen(text);
    if (len < (size_t)m->n || m->total == 0) return 0.0;

    char     gram[8];
    gram[m->n] = '\0';

    double sum   = 0.0;
    long   count = 0;

    /*
     * Log-probability with Laplace smoothing:
     *   log P(gram) = log( (c(gram) + k) / (total + k * V) )
     * where k = smoothing, V = vocab_size (estimated as 95^n for char-level).
     */
    double vocab = (m->vocab_size > 0)
                   ? (double)m->vocab_size
                   : pow(95.0, (double)m->n);   /* printable ASCII space */
    double denom = (double)m->total + m->smoothing * vocab;

    for (size_t i = 0; i + (size_t)m->n <= len; i++) {
        memcpy(gram, text + i, (size_t)m->n);

        ngram_entry_t *e    = ngram_find(m, gram);
        double         freq = e ? (double)e->count : 0.0;
        double         prob = (freq + m->smoothing) / denom;
        sum += log(prob);
        count++;
    }

    return (count > 0) ? sum / (double)count : 0.0;
}

int ngram_save(const ngram_model_t *m, const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) { perror(path); return -1; }

    fprintf(fp, "# ngram_n=%d smoothing=%.6f total=%llu vocab=%llu\n",
            m->n, m->smoothing,
            (unsigned long long)m->total,
            (unsigned long long)m->vocab_size);

    for (int i = 0; i < NGRAM_TABLE_SZ; i++) {
        for (ngram_entry_t *e = m->table[i]; e; e = e->next) {
            /* Hex-encode the key to handle non-printable characters safely */
            char hex[16];
            util_hexencode((const uint8_t *)e->key, (size_t)m->n, hex);
            fprintf(fp, "%s\t%llu\n", hex, (unsigned long long)e->count);
        }
    }
    fclose(fp);
    return 0;
}

int ngram_load(ngram_model_t *m, const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "[ngram] cannot open model file: %s\n", path);
        return -1;
    }

    /* Reset */
    ngram_free(m);

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '#') {
            /* Parse header */
            int    n       = NGRAM_N;
            double smooth  = NGRAM_SMOOTHING;
            unsigned long long tot = 0, voc = 0;
            sscanf(line, "# ngram_n=%d smoothing=%lf total=%llu vocab=%llu",
                   &n, &smooth, &tot, &voc);
            if (m->n == 0) {
                m->n         = n;
                m->smoothing = smooth;
            }
            m->total      = tot;
            m->vocab_size = voc;
            continue;
        }

        char hex[16]; unsigned long long cnt;
        if (sscanf(line, "%15s\t%llu", hex, &cnt) != 2) continue;

        /* Decode hex key */
        int klen = (int)strlen(hex) / 2;
        if (klen < 1 || klen > 7) continue;
        char key[8] = {0};
        for (int i = 0; i < klen; i++) {
            unsigned int byte;
            if (sscanf(hex + 2 * i, "%02x", &byte) == 1)
                key[i] = (char)(unsigned char)byte;
        }

        uint32_t h = ngram_hash(key);
        ngram_entry_t *e = calloc(1, sizeof(*e));
        if (!e) { fclose(fp); return -1; }
        memcpy(e->key, key, (size_t)klen);
        e->count    = (uint64_t)cnt;
        e->next     = m->table[h];
        m->table[h] = e;
    }
    fclose(fp);
    return 0;
}

int ngram_train_file(ngram_model_t *m, const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr, "[ngram] cannot open: %s\n", path); return -1; }

    char line[16384];
    int  count = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;
        ngram_train(m, line);
        count++;
    }
    fclose(fp);
    fprintf(stderr, "[ngram] trained on %d queries from %s "
            "(total=%llu, vocab=%llu)\n",
            count, path,
            (unsigned long long)m->total,
            (unsigned long long)m->vocab_size);
    return count;
}

void ngram_free(ngram_model_t *m)
{
    for (int i = 0; i < NGRAM_TABLE_SZ; i++) {
        ngram_entry_t *e = m->table[i];
        while (e) {
            ngram_entry_t *next = e->next;
            free(e);
            e = next;
        }
        m->table[i] = NULL;
    }
    m->total = m->vocab_size = 0;
}
