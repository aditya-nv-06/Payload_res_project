# N-gram Corpus Model

This project uses an n-gram model to score SQL statements by how much they look like the training data.
In simple terms, it learns the character patterns from a corpus of normal SQL and then checks whether new queries look familiar or unusual.

## What an n-gram is

An n-gram is a short sequence of characters.

- 1-gram: one character at a time
- 2-gram: two characters at a time
- 3-gram: three characters at a time

`pgsql_ids` uses trigram scoring by default, so it looks at three-character patterns inside each SQL query.

## What the corpus model means here

The corpus is the training data for the model.
For this project, the corpus should contain SQL statements that represent normal or expected database activity.

Each line in the corpus file is treated as one training example.

Example corpus:

```text
SELECT id, name FROM users WHERE id = $1
SELECT count(*) FROM orders WHERE status = 'complete'
INSERT INTO events (ts, msg) VALUES (now(), $1)
UPDATE users SET last_login = now() WHERE id = $1
```

## What the model learns

During training, the model counts how often each trigram appears in the corpus.
Queries that use similar patterns get a higher score.
Queries with unusual patterns, obfuscation, or injection-like text get a lower score.

## How this project uses the model

The n-gram model is used as an anomaly detector.

- A normal-looking query should score higher.
- A suspicious or unusual query should score lower.
- The program compares the score against the anomaly threshold you set with `-T`.

If the score is below the threshold, the query is treated as suspicious.

## Training workflow

Use `-t` to train a model from a corpus, and `-m` to save the trained model.

```bash
pqCheck -t corpus.sql -m baseline.model
```

What happens:

- the corpus file is read line by line,
- trigram counts are collected,
- the learned model is written to `baseline.model`.

## Scoring workflow

Use `-m` to load a trained model while scanning traffic or running direct session mode.

```bash
pqCheck -r results/sqli_classic.pcap -m baseline.model -R config/rules.conf -o alerts.jsonl -v
```

What happens:

- each extracted SQL statement is scored against the trained model,
- the score contributes to the final risk level,
- low scores indicate queries that look unlike the training corpus.

## Choosing a good corpus

A useful corpus should:

- contain queries that are normal for your application,
- include common statements like `SELECT`, `INSERT`, `UPDATE`, and `DELETE`,
- reflect the way your application formats SQL, and
- avoid mixing in attacks or test payloads.

If the corpus is too small or too narrow, the model may flag normal queries as suspicious.

## Practical tips

- Keep the corpus close to real production traffic.
- Use parameterized SQL where possible in the corpus.
- Retrain if the database workload changes significantly.
- Combine the model with rules, not instead of rules.

## Related files

- [docs/Example.md](Example.md)
- [README.md](../README.md)
- [src/analysis/ngram.c](../src/analysis/ngram.c)
