/*
 * ngram.h – N-gram anomaly detection (Milestone 5)
 *
 * Builds a character-level n-gram frequency model from a corpus of
 * legitimate SQL queries (training), then scores new queries by computing
 * their average log-probability under the model (scoring).
 *
 * A high anomaly score (low average log-prob) indicates the query looks
 * unlike legitimate traffic — potentially obfuscated or injected SQL.
 *
 * Reference: https://arxiv.org/abs/1412.3664
 */
 
#ifndef NGRAM_H
#define NGRAM_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */

#define NGRAM_N          3           /* trigrams by default                   */
#define NGRAM_TABLE_SZ   65536       /* hash table buckets (power of 2)       */
#define NGRAM_SMOOTHING  0.5         /* Laplace (add-k) smoothing constant    */

typedef struct ngram_entry {
    char     key[8];                 /* n-gram string (up to 7 chars + NUL)   */
    uint64_t count;
    struct ngram_entry *next;        /* hash-chain                            */
} ngram_entry_t;

typedef struct {
    ngram_entry_t *table[NGRAM_TABLE_SZ];
    uint64_t       total;            /* total n-gram occurrences seen         */
    uint64_t       vocab_size;       /* number of distinct n-grams            */
    int            n;                /* n (gram size)                         */
    double         smoothing;        /* add-k smoothing constant              */
} ngram_model_t;

/* -------------------------------------------------------------------------- */

/* Initialise an empty model.  n must be 1..7. */
void ngram_init(ngram_model_t *m, int n, double smoothing);

/* Train: update counts with every n-gram in 'text'. */
void ngram_train(ngram_model_t *m, const char *text);

/*
 * Score a query string.
 * Returns the average log-probability of its n-grams under the model.
 * Lower values (more negative) == higher anomaly.
 * Returns 0.0 if the query is too short to extract any n-gram.
 */
double ngram_score(const ngram_model_t *m, const char *text);

/*
 * Save the model to a text file (one line per n-gram: "<ngram>\t<count>").
 * Returns 0 on success.
 */
int ngram_save(const ngram_model_t *m, const char *path);

/*
 * Load a model saved by ngram_save().
 * Returns 0 on success, -1 on failure.
 */
int ngram_load(ngram_model_t *m, const char *path);

/*
 * Train the model from a file containing one SQL query per line.
 * Returns the number of lines processed.
 */
int ngram_train_file(ngram_model_t *m, const char *path);

/* Free all heap memory owned by the model. */
void ngram_free(ngram_model_t *m);

#endif /* NGRAM_H */
