# pgsql_ids CLI Reference

This document covers the command-line interface for `pgsql_ids`, the PostgreSQL payload fragmentation and SQLi detection sensor.

## Synopsis

```bash
pqCheck [options]
pqCheck -h
pqCheck --version
```

## What the CLI does

`pgsql_ids` can:

- capture live traffic from a network interface,
- read offline `.pcap` files,
- reassemble TCP streams that carry PostgreSQL traffic,
- parse PostgreSQL frontend messages,
- apply rule-based SQLi detection,
- score queries with an n-gram anomaly model,
- optionally correlate alerts with `pg_stat_activity`, and
- write JSON-Lines alerts plus optional packet dumps.

For a full architecture diagram and module breakdown, see [docs/architecture.md](architecture.md).
For platform-specific build and run instructions, see [docs/run.md](run.md).

## Feature Guide

### Help and version

```bash
pqCheck -h
pqCheck --version
```

### Offline PCAP scan

```bash
pqCheck \
  -r results/sqli_classic.pcap \
  -R config/rules.conf \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

### Live capture

```bash
sudo pqCheck \
  -i eth0 \
  -R config/rules.conf \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

### Custom BPF filter

```bash
pqCheck \
  -r results/sqli_fragmented.pcap \
  -f "host 10.0.0.2" \
  -R config/rules.conf \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

### Train an n-gram model

```bash
pqCheck -t corpus.sql -m baseline.model
```

### Flagged packet dump

```bash
pqCheck \
  -r results/sqli_classic.pcap \
  -R config/rules.conf \
  -m baseline.model \
  -p flagged.pcap \
  -o alerts.jsonl \
  -v
```

### pg_stat_activity correlation

```bash
sudo pqCheck \
  -i lo \
  -R config/rules.conf \
  -c "host=localhost dbname=postgres user=postgres" \
  -o alerts.jsonl \
  -v
```

### Direct database session

```bash
pqCheck \
  -d "host=localhost dbname=postgres user=postgres" \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

In direct session mode, type SQL statements, then `\disconnect` or `Ctrl-D` to close the session. You can also run one statement with `-e`.

### Single-statement execution

```bash
pqCheck \
  -d "host=localhost dbname=postgres user=postgres" \
  -e "SELECT 1" \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

### Custom rules

```bash
pqCheck \
  -r results/sqli_obfuscated.pcap \
  -R config/rules.conf \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

## Options

| Flag | Description |
|------|-------------|
| `-i <iface>` | Live capture interface. Default: `any`. |
| `-r <file>` | Read from an offline PCAP file instead of live capture. |
| `-f <bpf>` | Extra BPF filter expression. It is combined with `tcp port 5432`. |
| `-R <rules>` | Rules configuration file. Default: `config/rules.conf`. |
| `-o <file>` | Alert output file. Default: `alerts.jsonl`. |
| `-p <pcap>` | Write flagged traffic to a PCAP file. |
| `-m <model>` | Load an n-gram model from disk for anomaly scoring. |
| `-t <corpus>` | Train an n-gram model from a corpus file and save it to `-m`. |
| `-T <threshold>` | Anomaly threshold. Default: `-5.0`. Lower values are stricter. |
| `-c <connstr>` | libpq connection string for `pg_stat_activity` correlation. |
| `-d <connstr>` | Open a direct PostgreSQL session, score each entered query, and execute it. |
| `-e <sql>` | Execute one SQL statement in `-d` mode, then disconnect. |
| `-v` | Verbose logging to stderr. |
| `-h`, `--help` | Show help. |
| `--version` | Print the CLI version. |

## Verbose mode output

With `-v`, the sensor prints a short configuration summary before capture starts.
That helps confirm the selected source, rules file, output path, and threshold.

Example:

```text
[CONFIG] Capture Mode: eth0
[CONFIG] Rules File: config/rules.conf
[CONFIG] Alert Output: /tmp/alerts.jsonl
[CONFIG] Anomaly Threshold: -3.00
```

## Training mode notes

- `-t` requires `-m`.
- Training mode exits after the model is written.
- The corpus should contain one SQL statement per line.

## Live capture notes

- Live capture often needs root privileges or `CAP_NET_RAW`.
- If capture fails, try `sudo` or use a PCAP file with `-r`.

## Related files

- [README.md](../README.md)
- [tests/gen_test_traffic.py](../tests/gen_test_traffic.py)
- [config/rules.conf](../config/rules.conf)
