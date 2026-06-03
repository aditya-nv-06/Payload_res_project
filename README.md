# pqCheck — PostgreSQL Payload Fragmentation & SQLi Detection Sensor

A passive network intrusion-detection sensor for PostgreSQL (TCP/5432) written in C for Linux. It reassembles fragmented TCP streams, parses the PostgreSQL wire protocol, and detects SQL injection using both rule-based matching and n-gram anomaly scoring. Optionally, it correlates network-level detections with live PostgreSQL telemetry via `pg_stat_activity`.

---

## 📖 **[→ Full Documentation Index](DOCUMENTATION.md)** ← Start Here for Structured Learning

Choose your path by role and goal. All learning paths are ordered and structured for easy follow-up.

---

## 🚀 Quick Navigation Guide

Use this table for the fastest entry point, then follow [DOCUMENTATION.md](DOCUMENTATION.md) for full paths.

| Goal | Start here | Next |
|---|---|---|
| First quick run | [docs/QUICK-REF.md](docs/QUICK-REF.md) | [docs/quickstart.md](docs/quickstart.md) |
| Install and run | [BUILD.md](BUILD.md) | [docs/run.md](docs/run.md) |
| Production setup | [docs/network-config.md](docs/network-config.md) | [docs/rules.md](docs/rules.md), [docs/ngram.md](docs/ngram.md) |
| Understand internals | [docs/architecture.md](docs/architecture.md) | [docs/cli.md](docs/cli.md), [docs/Example.md](docs/Example.md) |

---

## Entry Modes

`pqCheck` has two entry paths that share the same detection core:

- **Passive packet inspection** from live capture or offline PCAP files
- **Direct PostgreSQL session mode** for executing and scoring SQL statements

Optional interactive **TUI (Text User Interface)** mode for real-time monitoring with dashboard, alerts, and config display.

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

# With interactive TUI mode:
sudo apt-get install -y libncurses-dev
```

# System PostgreSQL audit (native C)

If you build with `WITH_LIBPQ=1`, `pqCheck` provides a native system-level
PostgreSQL audit mode implemented in C that connects to the server (via
libpq) and runs a set of operational and security checks across databases,
roles and configuration settings. Use `-c` to pass a libpq connection string
or rely on `PGHOST`/`PGUSER` environment variables.

Example:

```bash
make WITH_LIBPQ=1
./pqCheck --audit -c "host=127.0.0.1 user=postgres" --audit-json results/pg-audit.json
```

There is also a Makefile helper that reads `PG_CONNSTR`:

```bash
make audit-pg PG_CONNSTR="host=127.0.0.1 user=postgres"
```

### Compile

```bash
# Without libpq or ncurses (rule + anomaly detection only):
make

# With libpq (adds pg_stat_activity correlation):
make WITH_LIBPQ=1

# With ncurses TUI (interactive dashboard):
make WITH_TUI=1

# With both:
make WITH_LIBPQ=1 WITH_TUI=1
```

### Run unit tests

```bash
make test
```

### Install the `pqCheck` command

```bash
make install
```

This places `pqCheck` (the compiled executable) in `~/.local/bin` for a normal user install.
If you want `sudo pqCheck` to work, run `sudo make install` so the launcher is installed into `/usr/local/bin`, which is on sudo's search path on most Linux systems.

### Advanced Installation Options

For more installation options, including system-wide installation, package managers, and Docker:

```bash
# Interactive installation menu (recommended for most users):
./install.sh

# System-wide installation (requires sudo):
sudo make install PREFIX=/usr/local

# Build and install distribution packages (requires fpm):
bash packaging/build-deb.sh    # Debian/Ubuntu .deb
bash packaging/build-rpm.sh    # RHEL/CentOS/Fedora .rpm
```

See [BUILD.md](BUILD.md) for complete installation guide, package management, Docker setup, and troubleshooting.
See [TESTING.md](TESTING.md) for infrastructure validation and deployment notes.

## CLI

The sensor exposes a small Unix-style CLI. The fastest way to see the current
options is:

```bash
pqCheck -h
pqCheck --version
```

### Common commands

```bash
# FIRST TIME? Generate test traffic directly (no external tools needed!)
pqCheck --gen-test -o test_traffic.pcap

# Auto-train model from test traffic + run detection
pqCheck -A test_traffic.pcap -r test_traffic.pcap \
  -R config/rules.conf -o alerts.jsonl -v

# View results
cat alerts.jsonl | jq .
```

### Security audit CLI (Python)

Run the repository/database security audit scanner:

```bash
# Static scan of repo code/config/scripts
python3 tools/pqcheck_audit.py --root .

# Include docs too (higher noise, broader coverage)
python3 tools/pqcheck_audit.py --root . --include-docs

# Or via Makefile
make audit

# Optional live checks using a libpq connection string
python3 tools/pqcheck_audit.py \
  --root . \
  --connstr "host=localhost port=5432 dbname=postgres user=postgres sslmode=require" \
  --check-port 127.0.0.1:5432 \
  --json-out results/security-audit.json
```

What it checks:

- parameterized vs raw SQL usage patterns in C/Python
- connection-string transport security (`sslmode` usage)
- dangerous PostgreSQL commands (e.g., superuser/escalation patterns)
- broad privilege grants
- possible network exposure patterns (`0.0.0.0/0`, `listen_addresses='*'`, port publishing)
- optional live DB checks (`SHOW server_version`, `SHOW ssl`, current user/superuser)

**For production environments:**

```bash
# Offline scan of a PCAP file
pqCheck -r results/sqli_classic.pcap -R config/rules.conf -o alerts.jsonl -v

# Interactive TUI dashboard (requires ncurses)
pqCheck --tui -r capture.pcap -R config/rules.conf -m baseline.model

# Train an n-gram model from a SQL corpus
pqCheck -t corpus.sql -m baseline.model

# Live capture to a pcap, then auto-train and detect in one step
sudo pqCheck --capture --capture-out results/live_capture.pcap --duration 30

# Live capture on an interface (usually requires sudo or CAP_NET_RAW)
sudo pqCheck -i eth0 -R config/rules.conf -m baseline.model -o alerts.jsonl -v
```

### Auto-train from PCAP (quick)

You can auto-train an in-memory n-gram model from a PCAP and then run detection. Example:

```bash
pqCheck -A train.pcap --pcap monitor.pcap -R config/rules.conf -o alerts.jsonl -v
```

Production note: offline PCAP files may be used in production workflows — supply them with `-r <file>` for detection, or use `-A <file>` to train a temporary model from historical traffic before running detection.

### Monitor custom PostgreSQL ports and IPs

For production environments where PostgreSQL runs on non-standard ports or multiple destinations, use a network config file:

```bash
# Edit config/network.conf:
# ports=5432,5433,5434
# ips=10.0.0.5,192.168.1.100

pqCheck -r monitor.pcap -N config/network.conf -R config/rules.conf -o alerts.jsonl -v
```

The network config file supports:
- `ports` – comma-separated TCP ports (e.g., `5432,5433`)
- `ips` – comma-separated destination IPs (optional)
- `output_path` – override default alert output location (optional)

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
| `-A <pcap>` | Auto-train in-memory n-gram model from a PCAP file |
| `-N <config>` | Network config file (custom ports, IPs, output path) |
| `-T <threshold>` | Anomaly threshold (default: `-5.0`) |
| `-c <connstr>` | libpq connection string for `pg_stat_activity` correlation |
| `-d <connstr>` | Open a direct PostgreSQL session, score each entered query, and execute it |
| `-e <sql>` | Execute one SQL statement in `-d` mode, then disconnect |
| `-v` | Verbose mode |
| `-h`, `--help` | Show CLI help |
| `--version` | Print the current CLI version |

### Notes

- `-t` requires `-m`; training mode exits after the model is written.
- `-N` allows you to configure custom PostgreSQL ports and destination IPs via a config file.
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
