# pgsql_ids — PostgreSQL Payload Fragmentation & SQLi Detection Sensor

A passive network intrusion-detection sensor for PostgreSQL (TCP/5432) written in C for Linux. It reassembles fragmented TCP streams, parses the PostgreSQL wire protocol, and detects SQL injection using both rule-based matching and n-gram anomaly scoring. Optionally, it correlates network-level detections with live PostgreSQL telemetry via `pg_stat_activity`.

## Architecture

`pgsql_ids` has two entry modes that share the same detection core:

- passive packet inspection from live capture or offline PCAP, and
- direct PostgreSQL session mode for executing and scoring SQL statements.

See [docs/architecture.md](docs/architecture.md) for the full Mermaid diagram and module map.
For platform-specific run instructions, see [docs/run.md](docs/run.md).
For the n-gram corpus model guide, see [docs/ngram.md](docs/ngram.md).

## Components

| Source file         | Milestone | Description |
|---------------------|-----------|-------------|
| `src/net/capture.c`     | 1         | libpcap wrapper: live interface or offline .pcap, BPF filter |
| `src/net/reassembly.c`  | 2         | TCP stream reassembly: 5-tuple flow table, seq-ordered buffering |
| `src/net/pg_parser.c`   | 3         | PostgreSQL wire-protocol parser (Simple Query + Extended Query) |
| `src/analysis/detector.c`    | 4         | Rule-based detector: keyword + POSIX regex, configurable rules |
| `src/analysis/ngram.c`       | 5         | Character trigram model: train, score, save/load |
| `src/analysis/query_eval.c`  | 6         | Shared query evaluation: rules, n-gram scoring, alert emission |
| `src/db/pg_correlate.c`| 7         | libpq connector: queries `pg_stat_activity` to enrich alerts |
| `src/db/db_session.c`  | 8         | Direct PostgreSQL session driver using libpq |
| `src/output/alert.c`       | 9         | JSON-Lines alert output + optional pcap_dump for flagged flows |
| `src/main.c`        | 1–9       | Entry point: CLI argument parsing and mode selection |

## Building

### Prerequisites

```bash
# Debian/Ubuntu
sudo apt-get install -y build-essential libpcap-dev

# With pg_stat_activity correlation:
sudo apt-get install -y libpq-dev
```

### Compile

```bash
# Without libpq (rule + anomaly detection only):
make

# With libpq (adds pg_stat_activity correlation):
make WITH_LIBPQ=1
```

### Run unit tests

```bash
make test
```

## CLI

The sensor exposes a small Unix-style CLI. The fastest way to see the current
options is:

```bash
pqCheck -h
pqCheck --version
```

### Common commands

```bash
# Offline scan of a PCAP file
pqCheck -r results/sqli_classic.pcap -R config/rules.conf -o alerts.jsonl -v

# Train an n-gram model from a corpus
pqCheck -t corpus.sql -m baseline.model

# Live capture on an interface (usually requires sudo or CAP_NET_RAW)
sudo pqCheck -i eth0 -R config/rules.conf -m baseline.model -o alerts.jsonl -v
```

### Flags

| Flag | Meaning |
|------|---------|
| `-i <iface>` | Live capture interface (default: `any`) |
| `-r <file>` | Read from an offline PCAP file |
| `-f <bpf>` | Extra BPF filter expression AND-ed with `tcp port 5432` |
| `-R <rules>` | Rules config file (default: `config/rules.conf`) |
| `-o <file>` | Alert log output file (default: `alerts.jsonl`) |
| `-p <pcap>` | Dump flagged packets to a PCAP file |
| `-m <model>` | N-gram model file used for anomaly scoring |
| `-t <corpus>` | Train an n-gram model from a SQL corpus and save it to `-m` |
| `-T <threshold>` | Anomaly threshold (default: `-5.0`) |
| `-c <connstr>` | libpq connection string for `pg_stat_activity` correlation |
| `-d <connstr>` | Open a direct PostgreSQL session, score each entered query, and execute it |
| `-e <sql>` | Execute one SQL statement in `-d` mode, then disconnect |
| `-v` | Verbose mode |
| `-h`, `--help` | Show CLI help |
| `--version` | Print the current CLI version |

### Notes

- `-t` requires `-m`; training mode exits after the model is written.
- Live capture usually needs root privileges or `CAP_NET_RAW`.
- Example traffic generators live in [tests/gen_test_traffic.py](tests/gen_test_traffic.py).

For a longer walkthrough with plain-language examples and expected behavior, see [docs/Example.md](docs/Example.md).

## Alert JSON Schema

```json
{
  "flow_id":        "a1b2c3d4e5f60718",
  "timestamp":      "2026-04-11T15:00:00Z",
  "src_ip":         "10.0.0.1",
  "src_port":       54321,
  "dst_ip":         "10.0.0.2",
  "dst_port":       5432,
  "extracted_sql":  "' UNION SELECT username, password FROM pg_shadow--",
  "rule_matches":   [
    { "rule": "UNION_SELECT", "offset": 2, "length": 12 },
    { "rule": "PG_SHADOW",    "offset": 40, "length": 9 }
  ],
  "rule_score":     7,
  "anomaly_score":  -6.84,
  "risk_level":     "CRITICAL",
  "pg_stat_activity": {
    "pid":         "12345",
    "usename":     "appuser",
    "datname":     "production",
    "state":       "active",
    "wait_event":  "Client:ClientRead",
    "query":       "' UNION SELECT ...",
    "query_start": "2026-04-11T15:00:00Z"
  }
}
```

## Configuration: `config/rules.conf`

```
# name|type|pattern|severity
UNION_SELECT|KEYWORD|union select|CRITICAL
OR_TAUTOLOGY|REGEX|or\s+[0-9]+=\s*[0-9]+|HIGH
PG_SLEEP|KEYWORD|pg_sleep|HIGH
```

Add or remove rules without recompiling — just edit the file and restart the sensor.

## Evaluation

See `tests/gen_test_traffic.py` for synthetic PCAP generation. Evaluation metrics:

| Metric | Target |
|--------|--------|
| TPR (fragmented SQLi) | ≥ 0.95 |
| FPR (clean baseline)  | ≤ 0.01 |
| Detection latency     | < 10 ms per query |
| Anomaly AUC           | ≥ 0.90 |

Results are stored in `results/`.

## References

- Springer IDS Survey: https://link.springer.com/article/10.1186/s42400-019-0038-7
- DPI Survey: https://arxiv.org/abs/0803.0037
- N-gram Payload Anomaly: https://arxiv.org/abs/1412.3664
- SQLi Defense Comparison: https://www.mdpi.com/2076-3417/15/23/12351
- PostgreSQL Wire Protocol: https://www.postgresql.org/docs/current/protocol-message-formats.html
