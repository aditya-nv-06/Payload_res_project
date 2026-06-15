# pqCheck Quickstart

This guide gets you from zero to your first SQL-injection alert in a few minutes.

## What you will do

1. Generate sample PostgreSQL traffic (clean + malicious)
2. Train an anomaly model from clean traffic
3. Scan mixed traffic and review alerts

---

## Prerequisites

- `pqCheck` built and available in your current directory or `PATH`
- `jq` (optional, for pretty-printing JSON)

If you have not built yet, see [../BUILD.md](../BUILD.md).

---

## Step 1: Generate test PCAP files

`pqCheck` can generate reproducible synthetic traffic, so you do not need a live database for learning.

```bash
# Clean traffic for training
pqCheck --gen-test --gen-clean 100 --gen-inject 0 -o baseline.pcap

# Mixed traffic for detection
pqCheck --gen-test --gen-clean 60 --gen-inject 40 -o suspect.pcap
```

---

## Step 2: Train a model from clean traffic

```bash
pqCheck -A baseline.pcap -m baseline.model -v
```

What this does:
- extracts SQL queries from `baseline.pcap`
- trains an in-memory n-gram model
- saves the model to `baseline.model`

---

## Step 3: Run detection on suspect traffic

```bash
pqCheck -r suspect.pcap \
  -R config/rules.conf \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

Detection combines:
- rule-based matching (`-R`)
- anomaly scoring (`-m`)

---

## Step 4: Read alerts

```bash
# Pretty print all alerts
jq '.' alerts.jsonl

# Show only HIGH or CRITICAL alerts
jq 'select(.risk_level=="HIGH" or .risk_level=="CRITICAL")' alerts.jsonl

# Count alerts by risk level
jq -r '.risk_level' alerts.jsonl | sort | uniq -c
```

---

## Common next steps

### Scan your own offline capture

```bash
pqCheck -r capture.pcap -R config/rules.conf -m baseline.model -o alerts.jsonl -v
```

### Start live monitoring

```bash
sudo pqCheck -i eth0 -R config/rules.conf -m baseline.model -o alerts.jsonl -v
```

If you do not have a model yet, pqCheck can automatically capture a short baseline and train in memory before entering live monitoring. See [auto-baseline.md](auto-baseline.md) for the full workflow.

### Use the interactive dashboard (TUI)

```bash
pqCheck --tui -r suspect.pcap -R config/rules.conf -m baseline.model
```

---

## Troubleshooting

| Problem | Fix |
|---|---|
| `Cannot open model file` | Train first with `pqCheck -A baseline.pcap -m baseline.model` |
| `Permission denied` on live capture | Use `sudo`, or grant `CAP_NET_RAW` |
| No alerts found | Try traffic with known injections and re-check rules path |
| TUI not available | Rebuild with `make WITH_TUI=1` |

---

## Related docs

- [README (docs index)](README.md)
- [CLI reference](cli.md)
- [Automatic baseline capture](auto-baseline.md)
- [Quick reference](QUICK-REF.md)
- [Network config](network-config.md)
- [Rules guide](rules.md)
- [N-gram guide](ngram.md)
