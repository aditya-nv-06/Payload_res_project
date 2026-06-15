# Automatic Baseline Capture

This guide explains the first-time live monitoring workflow for `pqCheck` when no saved n-gram model is available.

## What it does

When you start `pqCheck` in live capture mode without `-m <model>` and automatic baseline capture is enabled, the sensor can:

1. open the selected network interface,
2. capture a short window of live PostgreSQL traffic,
3. save that traffic to a temporary PCAP,
4. train an in-memory n-gram model from the captured SQL queries,
5. continue into normal live monitoring with the newly trained model,
6. emit alerts as JSON-Lines like any other live run.

The current implementation uses pqCheck's internal libpcap capture path to collect the baseline PCAP. It does not require a separate `tcpdump` process.

## Why this exists

This workflow is designed for first-time users who do not yet have a baseline model or a captured training PCAP. It removes the need to:

- generate synthetic traffic first,
- manually capture a training file,
- train a model in a separate step,
- or decide which command comes first.

The tradeoff is that the first capture window is assumed to contain mostly legitimate traffic. If malicious activity is already present during that window, the resulting baseline can be contaminated.

## End-to-end flow

### 1. User starts live monitoring

Example:

```bash
sudo pqCheck -i eth0 -R config/rules.conf -o alerts.jsonl -v
```

If no `-m <model>` is supplied, pqCheck checks whether automatic baseline capture is enabled.

### 2. pqCheck decides whether to auto-train

Automatic baseline capture is enabled by default in live mode.

You can control it with:

- `--auto-baseline` to force-enable it
- `--no-auto-baseline` to disable it
- `--auto-baseline-duration <sec>` to change the capture window

### 3. pqCheck captures baseline traffic

pqCheck records live traffic from the selected interface for the configured duration. During this phase it behaves like a temporary training capture rather than a monitoring run.

Default duration: 30 seconds.

Recommended starting points:

- small lab or test network: 15-30 seconds
- busier production-like environment: 30-60 seconds

Choose a window long enough to observe ordinary PostgreSQL activity, but short enough to reduce the chance of collecting suspicious traffic.

### 4. pqCheck trains the model in memory

Once the baseline PCAP is written, pqCheck extracts PostgreSQL SQL statements from it and trains the n-gram model in memory.

The model is not written to disk unless you explicitly train one with `-t <corpus>` and `-m <model>`.

### 5. pqCheck continues with live detection

After training completes, the same process proceeds into the normal detection pipeline:

- rules from `-R` are loaded,
- the trained model is enabled,
- alerts are emitted as JSON-Lines,
- optional packet dumps still work if `-p <dump.pcap>` is supplied.

## Temporary files

The baseline PCAP is created in `/tmp` as a temporary file and removed automatically after auto-training finishes.

If cleanup fails, pqCheck prints a warning and keeps running.

## Operational guidance

### When to use it

Use automatic baseline capture when:

- you are running pqCheck for the first time,
- you do not already have a trusted model,
- you want the model to reflect real traffic on that network,
- and you are comfortable treating the initial window as clean baseline traffic.

### When not to use it

Disable it when:

- you already have a vetted model,
- you need a deterministic, pre-trained deployment,
- the network may already contain suspicious activity,
- or you prefer to train from a curated corpus or offline PCAP.

### Interface selection

Automatic baseline capture follows the same live interface selection as normal capture. If you want to monitor a specific NIC, provide `-i <iface>`. If you do not specify one, `pqCheck` uses its default interface setting.

### Progress output

With `-v`, pqCheck prints progress messages that show the transition through the workflow:

1. collecting baseline traffic,
2. training the model,
3. switching into live monitoring.

That makes it easier to tell whether the process is still capturing, training, or actively alerting.

## Configuration summary

| Option | Meaning |
|---|---|
| `-m <model>` | Load a saved model from disk. If present, automatic baseline capture is skipped. |
| `--auto-baseline` | Enable automatic baseline capture in live mode. |
| `--no-auto-baseline` | Disable automatic baseline capture in live mode. |
| `--auto-baseline-duration <sec>` | Set the baseline capture window in seconds. |

## Example

```bash
sudo pqCheck \
  -i eth0 \
  -R config/rules.conf \
  --auto-baseline-duration 45 \
  -o alerts.jsonl \
  -v
```

This run will:

1. capture 45 seconds of baseline traffic from `eth0`,
2. train an in-memory model from the captured SQL,
3. continue monitoring the same interface,
4. write alerts to `alerts.jsonl`.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Baseline capture never starts | Live capture permissions are missing | Run with `sudo` or grant `CAP_NET_RAW`. |
| Auto-training finishes but no anomaly alerts appear | Traffic may be clean, or the model is too permissive | Shorten the baseline window, review rules, or lower the anomaly threshold. |
| Baseline capture fails immediately | Interface unavailable or capture permissions missing | Verify `-i <iface>` and network privileges. |
| You want a persisted model | Automatic baseline only trains in memory | Train a saved model with `-t <corpus> -m <model>`. |

## Related docs

- [CLI reference](cli.md)
- [Quickstart](quickstart.md)
- [Run guide](run.md)
- [PCAP workflows](pcap-guide.md)