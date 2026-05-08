# Examples

This guide shows the most common ways to use `pqCheck` in plain language.
Each example includes the goal, the command, and what to expect.

**New in v1.0.0:** Auto-train n-gram models directly from PCAPs (Example 9), and configure custom PostgreSQL ports/IPs via config file (Example 10).

If you want to understand the corpus model behind the anomaly score, see [docs/ngram.md](ngram.md).
For detailed network config options, see [docs/network-config.md](network-config.md).
For a complete walkthrough of PCAP generation and workflow, see **[docs/quickstart.md](quickstart.md)** (recommended for new users).
For in-depth PCAP management and troubleshooting, see [docs/pcap-guide.md](pcap-guide.md).

## 0. Generate test traffic (for learning, no PostgreSQL needed)

Use this when you want to try `pqCheck` but don't have a PostgreSQL database running or existing PCAP files.

```bash
# Generate synthetic test traffic with clean queries and SQL injection attempts
pqCheck --gen-test -o test_traffic.pcap

# Or customize the mix (20 clean + 30 injection):
pqCheck --gen-test --gen-clean 20 --gen-inject 30 -o test_traffic.pcap

# Verify the file was created
ls -lh test_traffic.pcap
file test_traffic.pcap
```

What happens:

- pqCheck generates synthetic PostgreSQL protocol packets directly,
- the output includes 50 clean SQL queries and 50 SQL injection attempts by default (customizable),
- the PCAP file is valid and suitable for training and testing `pqCheck`, and
- you can use it immediately with Examples 1, 9, and 11.

Optional flags:

```bash
pqCheck --gen-test --gen-clean 100 --gen-inject 0 -o clean_only.pcap
#                   ^^^^^^^^^^^           ^^^^^^ customize counts
```

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

Automation: A small demo script that runs example queries and writes
alerts to `db-alerts.jsonl` is provided at [tests/db_session_demo.sh](tests/db_session_demo.sh).
Run it with:

```bash
bash tests/db_session_demo.sh
```

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

## 9. Auto-train from PCAP and detect immediately

Use this when you want to bootstrap an n-gram model from clean traffic in one PCAP, then run detection on the same or another PCAP without saving the model to disk.

```bash
pqCheck \
  -A results/baseline.pcap \
  --pcap results/sqli_obfuscated.pcap \
  -R config/rules.conf \
  -o alerts.jsonl \
  -v
```

What happens:

- `pqCheck` reads `results/baseline.pcap` and extracts all SQL queries,
- an in-memory n-gram model is trained on those queries,
- the model is immediately used to score traffic in `results/sqli_obfuscated.pcap`,
- no model file is written to disk, and
- alerts are logged normally.

This is useful for batch jobs or when you want a fast anomaly baseline without pre-training.

## 10. Monitor custom PostgreSQL ports and IPs using a config file

Use this when PostgreSQL runs on non-standard ports or multiple database servers.

Create a config file (e.g., `config/network.conf`):

```
ports=5432,5433,5434
ips=10.0.0.5,10.0.0.6
output_path=/var/log/pqcheck/alerts.jsonl
```

Then use it:

```bash
pqCheck \
  -r capture.pcap \
  -N config/network.conf \
  -R config/rules.conf \
  -m baseline.model \
  -v
```

What happens:

- the network config file is parsed,
- a custom BPF filter is generated for the specified ports and destination IPs,
- only traffic matching that filter is processed,
- the output path is overridden from the config (if specified),
- all other detection logic proceeds normally.

This is ideal for production deployments across multiple database servers or custom port assignments.

## 11. Inspect the output

After a scan, you can read the alert log with `jq`.

```bash
jq . alerts.jsonl
jq 'select(.risk_level == "HIGH" or .risk_level == "CRITICAL")' alerts.jsonl
```

What happens:

- the first command pretty-prints every alert,
- the second command shows only the higher-risk events.
