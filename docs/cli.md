# pgsql_ids CLI Reference

This document covers the command-line interface for `pgsql_ids`, the PostgreSQL payload fragmentation and SQLi detection sensor.

## Synopsis

```bash
./pgsql_ids [options]
./pgsql_ids -h
./pgsql_ids --version
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
| `-v` | Verbose logging to stderr. |
| `-h`, `--help` | Show help. |
| `--version` | Print the CLI version. |

## Examples

### 1. Show help

```bash
./pgsql_ids -h
```

### 2. Show version

```bash
./pgsql_ids --version
```

### 3. Analyze an offline PCAP

```bash
./pgsql_ids \
  -r results/sqli_classic.pcap \
  -R config/rules.conf \
  -o alerts.jsonl \
  -v
```

### 4. Train an n-gram model

```bash
./pgsql_ids -t corpus.sql -m baseline.model
```

### 5. Live capture

```bash
sudo ./pgsql_ids \
  -i eth0 \
  -R config/rules.conf \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

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
