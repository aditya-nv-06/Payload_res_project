# pqCheck CLI Reference

This document covers the command-line interface for `pqCheck`, the PostgreSQL payload fragmentation and SQLi detection sensor.

## Synopsis

```bash
pqCheck [options]
pqCheck -h
pqCheck --version
```

## What the CLI does

`pqCheck` can:

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

If you want to invoke the launcher through `sudo`, install it system-wide first with `sudo make install` so `pqCheck` is placed in `/usr/local/bin`.

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

### Auto-train from PCAP (new)

You can build an in-memory n-gram model directly from a PCAP file and immediately run detection. Use `-A <pcap>` (or `--auto`) to extract SQL queries from the PCAP, train a model in-memory, then continue into detection mode.

Examples:

Train from a PCAP and then detect on the same file:

```bash
pqCheck -A train.pcap -r train.pcap -R config/rules.conf -o alerts.jsonl -v
```

Train from a PCAP and detect on a different PCAP:

```bash
pqCheck -A train.pcap -r monitor.pcap -R config/rules.conf -o alerts.jsonl -v
```

Notes:
- If both `-A` and `-m` are provided, the disk model given by `-m` takes precedence and auto-training is skipped.
- The `-A` option currently trains the model in memory; to persist a model, use `-t` / `-m` with a corpus file.

### Network configuration (custom ports and IPs)

You can configure which PostgreSQL ports and destination IPs to monitor using a network config file. This is useful in production when PostgreSQL runs on non-standard ports or across multiple IPs.

Create a config file (e.g., `config/network.conf`) with:

```
ports=5432,5433,5434
ips=10.0.0.5,192.168.1.100
output_path=/var/log/pqcheck/alerts.jsonl
```

Then pass it to pqCheck:

```bash
pqCheck -r monitor.pcap -N config/network.conf -R config/rules.conf -v
```

Notes:
- `ports`: comma-separated list of TCP ports to monitor (required)
- `ips`: comma-separated list of destination IPs to monitor (optional; if omitted, all IPs are monitored)
- `output_path`: overrides the `-o` flag if specified
- When `-N` is provided, a custom BPF filter is generated instead of the default "tcp port 5432"

### Interactive TUI mode

Use `--tui` to launch an interactive dashboard for real-time monitoring.

```bash
pqCheck --tui -r capture.pcap -R config/rules.conf -m baseline.model
```

Features:
- **Dashboard** – Real-time statistics and status
- **Alert Log** – Scrollable list of detected SQL injection attempts
- **Configuration** – View current settings
- **Help** – Keyboard shortcuts

For detailed TUI usage, see [docs/tui.md](tui.md).

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
| `-A <pcap>` | Auto-train an in-memory n-gram model from a PCAP (extracts queries). |
| `-N <config>` | Network config file (custom ports, IPs, and output path). |
| `-T <threshold>` | Anomaly threshold. Default: `-5.0`. Lower values are stricter. |
| `-c <connstr>` | libpq connection string for `pg_stat_activity` correlation. |
| `-d <connstr>` | Open a direct PostgreSQL session, score each entered query, and execute it. |
| `-e <sql>` | Execute one SQL statement in `-d` mode, then disconnect. |
| `-v` | Verbose logging to stderr. |
| `--tui` | Enable interactive TUI mode (requires ncurses). |
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
