# PCAP Guide for pqCheck

This guide explains what PCAP files are, where to get them, and how to use them with pqCheck.

## What is a PCAP File?

A **PCAP** (Packet Capture) file has:
- Full network packets captured at the device driver level
- Timestamps accurate to microseconds
- Complete packet contents (network headers + payload)
- Standard `.pcap` file format recognized by all network tools

**PCAP files are:**
- ✅ Portable across tools (Wireshark, tcpdump, pqCheck, etc.)
- ✅ Replayable (you can analyze the same traffic multiple times)
- ✅ Shareable with security teams and analysts
- ✅ Suitable for forensics and incident response
- ❌ NOT real-time (they capture past traffic)

## Where to Get PCAPs

### Option 1: Generate Synthetic Test Traffic (Recommended for Learning)

**Pros:** No external dependencies, reproducible, includes both clean and malicious queries
**Cons:** Not real traffic

```bash
# Generate 100 clean + 50 malicious queries
python3 tests/gen_pcap_simple.py -o test_traffic.pcap -c 100 -i 50

# Verify
tcpdump -r test_traffic.pcap | head -10
```

### Option 2: Capture from Live PostgreSQL Traffic

**Pros:** Real database traffic, representative of your production environment
**Cons:** Requires network access, may need sudo

```bash
# Capture on a specific interface
sudo tcpdump -i eth0 tcp port 5432 -w postgres_traffic.pcap

# Capture with size limit (e.g., 100MB)
sudo tcpdump -i eth0 tcp port 5432 -w postgres_traffic.pcap -C 100

# Capture with time limit (e.g., 1 hour)
sudo tcpdump -i eth0 tcp port 5432 -w postgres_traffic.pcap -G 3600

# Stop with Ctrl+C
```

### Option 3: Use Pre-existing PCAP Files

The pqCheck repository includes sample PCAPs:

```bash
# See what's available
ls -lh results/

# Use them
pqCheck -r results/sqli_classic.pcap -o alerts.jsonl -R config/rules.conf
```

### Option 4: Export from Wireshark

If you've already captured traffic in Wireshark:

1. File → Export Packets → Filter to PostgreSQL (port 5432)
2. Save as PCAP format
3. Use with pqCheck

### Option 5: Replay Existing PCAP

If you have a PCAP from another tool:

```bash
# Just use it directly
pqCheck -r your_captured.pcap -o alerts.jsonl -R config/rules.conf
```

## Types of PCAPs

### Training PCAP ("Clean" Traffic)

Used to train an n-gram model of normal queries.

**Requirements:**
- Contains **only legitimate SQL** (no malicious queries)
- Represents typical application queries
- Large enough for statistical significance (100+ queries recommended)

**Example:**
```bash
# Capture traffic during normal business hours
pqCheck -A baseline_clean.pcap -m trained.model
```

### Detection PCAP ("Suspect" Traffic)

Used to scan for SQL injection and anomalies.

**Requirements:**
- Can contain suspicious or unknown queries
- Should represent the same database (or similar schema)
- Any size (single query to gigabytes)

**Example:**
```bash
# Scan for malicious activity
pqCheck -r suspect_traffic.pcap -m trained.model -o alerts.jsonl
```

### Baseline PCAP ("Reference" Traffic)

A representative sample for anomaly detection.

**Requirements:**
- Diverse: many different query types
- Large: 1000+ queries if possible
- Historical: captured over hours or days

## Working with PCAPs: Common Tasks

### Task 1: Split a PCAP

Split a large PCAP into smaller chunks (useful for analyzing specific time windows):

```bash
# Extract traffic from a specific time window
editcap -A "2024-01-01 08:00:00" -B "2024-01-01 09:00:00" \
  large_capture.pcap hourly_8am.pcap

# Or split by packet count (10,000 packets per file)
editcap -c 10000 large_capture.pcap output_
# Creates: output_00001.pcap, output_00002.pcap, etc.
```

### Task 2: Filter a PCAP by IP or Port

Extract only traffic from a specific server:

```bash
# Filter by destination IP
tcpdump -r input.pcap -w output.pcap "dst host 10.0.0.5"

# Filter by source and destination
tcpdump -r input.pcap -w output.pcap "src host 10.0.0.1 and dst host 10.0.0.5"

# Filter by port (not just 5432)
tcpdump -r input.pcap -w output.pcap "tcp port 5433 or tcp port 5434"
```

### Task 3: Merge Multiple PCAPs

Combine multiple PCAP files:

```bash
# Merge in chronological order
mergecap -w merged.pcap capture1.pcap capture2.pcap capture3.pcap

# pqCheck can then process merged.pcap
pqCheck -r merged.pcap -o alerts.jsonl -R config/rules.conf
```

### Task 4: Inspect a PCAP Without pqCheck

```bash
# View packet summary
tcpdump -r input.pcap -n | head -50

# Extract payload (shows SQL queries)
tcpdump -r input.pcap -A | grep -E "SELECT|INSERT|UPDATE|DELETE" | head -20

# Count packets
tcpdump -r input.pcap | wc -l

# Get file info
pcapinfo input.pcap  # if available on your system
```

## Workflow: From PCAP to Alerts

```
         Generate or Capture
                 |
        Training PCAP ──────> Train Model ─────> baseline.model
                                    |
                                    v
                            Anomaly Patterns
                                    |
                                    v
        Detection PCAP ────────> Score Queries ──────> Alerts
         + Rules
         + Model
```

**Step-by-step:**

```bash
# 1. Get training PCAP (clean traffic)
python3 tests/gen_pcap_simple.py -o train.pcap -c 200 -i 0

# 2. Train model
pqCheck -A train.pcap -m baseline.model

# 3. Get detection PCAP (suspicious traffic)
python3 tests/gen_pcap_simple.py -o suspect.pcap -c 100 -i 100

# 4. Run detection
pqCheck -r suspect.pcap -m baseline.model -o alerts.jsonl -R config/rules.conf

# 5. Review results
cat alerts.jsonl | jq '.[] | {timestamp, src_ip, query, risk_level}' | head -10
```

## PCAP Size and Performance

| PCAP Size | Packets | Est. Time | Notes |
|-----------|---------|-----------|-------|
| 1 MB | ~3K | <1s | Good for testing |
| 10 MB | ~30K | 5-10s | Typical capture |
| 100 MB | ~300K | 30-60s | Large capture |
| 1 GB | ~3M | 5-10m | Very large; consider splitting |

**Pro tip:** For production deployments, capture traffic in 1-hour chunks and process daily.

## Troubleshooting PCAPs

### "tcpdump: command not found"

Install tcpdump:
```bash
sudo apt-get install -y tcpdump
```

### "No packets captured"

Check if traffic is actually flowing:
```bash
# Watch live traffic for 10 seconds
sudo tcpdump -i eth0 tcp port 5432 -c 10 -v
```

### PCAP file is corrupted

Repair or validate:
```bash
# Check if file is valid
file input.pcap
# Should output: "input.pcap: tcpdump capture file (little-endian)"

# Try to repair with pcapfix (if installed)
pcapfix -o output.pcap input.pcap

# Convert with tcpdump
tcpdump -r input.pcap -w output.pcap
```

### "Permission denied" on live capture

Grant capabilities:
```bash
# Option 1: Use sudo (safest)
sudo tcpdump -i eth0 tcp port 5432 -w capture.pcap

# Option 2: Grant CAP_NET_RAW to tcpdump (not recommended)
sudo setcap cap_net_raw=+ep /usr/bin/tcpdump
```

## Production Considerations

1. **Storage:** Keep raw PCAPs for 7-30 days for incident response
2. **Privacy:** PCAPs may contain sensitive query data; encrypt at rest
3. **Replaying:** Use `-r` flag to re-process historical traffic without resaving
4. **Archiving:** Compress old PCAPs: `gzip capture.pcap`
5. **Validation:** Always test your PCAP workflow on a non-production system first

## See Also

- [Quickstart Guide](quickstart.md) – Full workflow tutorial
- [CLI Guide](cli.md) – All pqCheck command-line options
- [Examples](Example.md) – Real-world use cases
- [tcpdump man page](https://www.tcpdump.org/papers/sniffing-faq.html) – Network capture deep dive
