# pqCheck Quick Reference

Use this page as a compact command cheat sheet.

## Fastest demo (under 1 minute)

```bash
# Generate sample traffic
pqCheck --gen-test --gen-clean 50 --gen-inject 0 -o train.pcap
pqCheck --gen-test --gen-clean 25 --gen-inject 25 -o test.pcap

# Train + scan
pqCheck -A train.pcap -m demo.model
pqCheck -r test.pcap -R config/rules.conf -m demo.model -o alerts.jsonl -v

# Inspect alerts
jq '.' alerts.jsonl
```

---

## Core commands

### Generate test traffic

```bash
pqCheck --gen-test -o sample.pcap
pqCheck --gen-test --gen-clean 100 --gen-inject 0 -o clean.pcap
pqCheck --gen-test --gen-clean 0 --gen-inject 100 -o attack.pcap
```

### Train model

```bash
pqCheck -A clean.pcap -m model.ngram
pqCheck -t corpus.sql -m model.ngram
```

### Detect SQL injection

```bash
pqCheck -r capture.pcap -R config/rules.conf -m model.ngram -o alerts.jsonl -v
pqCheck -r capture.pcap -R config/rules.conf -o alerts.jsonl -v
```

### Live capture

```bash
sudo pqCheck -i eth0 -R config/rules.conf -m model.ngram -o alerts.jsonl -v
```

### Interactive TUI

```bash
pqCheck --tui -r capture.pcap -R config/rules.conf -m model.ngram
# Keys: d=dashboard, a=alerts, c=config, h=help, q=quit
```

### Custom port/IP monitoring

```bash
pqCheck -r capture.pcap -N config/network.conf -R config/rules.conf -m model.ngram -v
```

---

## High-value flags

| Flag | Purpose |
|---|---|
| `-r <pcap>` | Analyze an offline PCAP file |
| `-i <iface>` | Capture live traffic from an interface |
| `-R <rules>` | Rules file path |
| `-m <model>` | Load n-gram model |
| `-A <pcap>` | Auto-train model from PCAP |
| `-t <corpus>` | Train model from SQL corpus (requires `-m`) |
| `-N <config>` | Network config file (ports, IPs, output path) |
| `-T <threshold>` | Anomaly threshold |
| `-o <file>` | Alert output path |
| `--gen-test` | Generate synthetic test PCAP (use with `-o`) |
| `--tui` | Start interactive terminal dashboard |

---

## Quick troubleshooting

| Issue | Try |
|---|---|
| No alerts | Verify `-R` path and test with generated attack PCAP |
| Model load error | Recreate with `pqCheck -A clean.pcap -m model.ngram` |
| Permission denied on live capture | Use `sudo` or set `cap_net_raw` |
| TUI fails | Rebuild with `make WITH_TUI=1` |

---

## Where to go next

- [Quickstart walkthrough](quickstart.md)
- [CLI reference](cli.md)
- [Run guide](run.md)
- [Examples](Example.md)
