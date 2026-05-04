# Examples

This guide shows the most common ways to use `pqCheck` in plain language.
Each example includes the goal, the command, and what to expect.

If you want to understand the corpus model behind the anomaly score, see [docs/ngram.md](ngram.md).

## 1. Check a saved capture for SQL injection

Use this when you already have a `.pcap` file and want to scan it for suspicious PostgreSQL traffic.

```bash
pqCheck \
  -r results/sqli_classic.pcap \
  -R config/rules.conf \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

What happens:

- packets are read from the capture file,
- PostgreSQL queries are reconstructed,
- SQLi rules are applied,
- the n-gram model scores each query, and
- alerts are written to `alerts.jsonl`.

## 2. Watch live PostgreSQL traffic

Use this when you want to inspect a running database connection as traffic flows over the network.

```bash
sudo pqCheck \
  -i eth0 \
  -R config/rules.conf \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

What happens:

- `pqCheck` captures packets from the chosen interface,
- reassembles fragmented TCP traffic,
- extracts SQL from PostgreSQL frontend messages,
- flags suspicious statements, and
- logs any alerts that pass the risk threshold.

## 3. Train the n-gram model

Use this when you want a baseline of normal SQL before scanning real traffic.

```bash
pqCheck -t corpus.sql -m baseline.model
```

What happens:

- each line in `corpus.sql` is treated as a normal SQL statement,
- the trigram model learns character patterns from that corpus, and
- the model is saved to `baseline.model`.

## 4. Use a custom BPF filter

Use this when you only want to inspect traffic from one host, subnet, or conversation.

```bash
pqCheck \
  -r results/sqli_fragmented.pcap \
  -f "host 10.0.0.2" \
  -R config/rules.conf \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

What happens:

- the built-in PostgreSQL filter is combined with your extra BPF expression,
- only matching packets are processed, and
- the rest of the detection pipeline stays the same.

## 5. Dump only flagged packets to a PCAP file

Use this when you want to keep a packet-level record of suspicious flows.

```bash
pqCheck \
  -r results/sqli_classic.pcap \
  -R config/rules.conf \
  -m baseline.model \
  -p flagged.pcap \
  -o alerts.jsonl \
  -v
```

What happens:

- alerts are still written to JSON Lines,
- packets for flagged flows are also copied into `flagged.pcap`, and
- you can review the network payload later in Wireshark or another analyzer.

## 6. Enrich alerts with `pg_stat_activity`

Use this when you want the network alert to include what PostgreSQL itself sees.

```bash
sudo pqCheck \
  -i lo \
  -R config/rules.conf \
  -c "host=localhost dbname=postgres user=postgres" \
  -o alerts.jsonl \
  -v
```

What happens:

- `pqCheck` looks up the matching backend session in `pg_stat_activity`,
- the alert includes username, database, query text, and timing data, and
- you get more context for incident response.

## 7. Connect directly to PostgreSQL and score queries

Use this when you want to type SQL statements yourself and have each one evaluated before it runs.

```bash
pqCheck \
  -d "host=localhost dbname=postgres user=postgres" \
  -m baseline.model \
  -o db-alerts.jsonl \
  -v
```

What happens:

- `pqCheck` opens a libpq connection,
- every query you enter is scored by the rule engine and n-gram model,
- the query is executed on the database, and
- you can leave the session with `\disconnect` or `Ctrl-D`.

## 8. Run one statement and exit

Use this when you only need a single query checked and executed once.

```bash
pqCheck \
  -d "host=localhost dbname=postgres user=postgres" \
  -e "SELECT now()" \
  -m baseline.model \
  -o db-alerts.jsonl \
  -v
```

What happens:

- the statement is evaluated the same way as in interactive session mode,
- it is sent to PostgreSQL once,
- the connection closes immediately after execution.

## 9. Inspect the output

After a scan, you can read the alert log with `jq`.

```bash
jq . alerts.jsonl
jq 'select(.risk_level == "HIGH" or .risk_level == "CRITICAL")' alerts.jsonl
```

What happens:

- the first command pretty-prints every alert,
- the second command shows only the higher-risk events.
