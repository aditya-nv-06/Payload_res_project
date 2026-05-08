# Getting Started with pqCheck: PCAP Files and Automated Testing

This guide walks you through the complete workflow of generating test traffic, training a detection model, and monitoring for SQL injection.

## The Easiest Way: Let pqCheck Generate Test Data

**You don't need any external tools or PostgreSQL running!** pqCheck can generate synthetic PCAP files directly:

```bash
# Generate a PCAP with 50 clean queries + 50 SQL injection attempts
pqCheck --gen-test -o test_mixed.pcap

# Generate clean training data only
pqCheck --gen-test --gen-clean 100 --gen-inject 0 -o training.pcap

# Generate malicious data for testing
pqCheck --gen-test --gen-clean 0 --gen-inject 50 -o injection_only.pcap
```

**That's it!** You now have a valid PCAP file to work with.

---

## Part 1: Quick Start in 3 Steps

### Step 1: Generate Test Data

```bash
# Generate two PCAPs: one for training, one for testing
pqCheck --gen-test --gen-clean 50 --gen-inject 0 -o training.pcap
pqCheck --gen-test --gen-clean 25 --gen-inject 25 -o suspect.pcap
```

### Step 2: Train an N-gram Model

```bash
# Auto-train from the clean traffic PCAP
pqCheck -A training.pcap -m trained.model
```

**What happens:**
- pqCheck reads `training.pcap` and extracts all SQL queries
- Builds an in-memory trigram frequency model
- Saves it to `trained.model` (optional, or just use in-memory)

### Step 3: Detect SQL Injection

```bash
# Scan the suspect traffic with your trained model
pqCheck -r suspect.pcap -m trained.model -o alerts.jsonl -v
```

**What happens:**
- Loads your trained model
- Scans each query in `suspect.pcap`
- Compares against both rules and n-gram patterns
- Writes alerts to `alerts.jsonl`

### Step 4: Review Results

```bash
# Pretty-print alerts
cat alerts.jsonl | jq .

# Show only high-risk alerts
cat alerts.jsonl | jq 'select(.risk_level=="HIGH" or .risk_level=="CRITICAL")'

# Count by type
cat alerts.jsonl | jq '.risk_level' | sort | uniq -c
```

---

## Part 2: Understanding PCAP Files

A **PCAP file** (.pcap extension) is a standard format for storing network packet data. It contains:
- Raw network packets captured from the wire
- Timestamps for each packet
- Full packet contents (headers + payload)

### Where Do PCAP Files Come From?

**Option A: Generate synthetic test traffic** (recommended for learning)
```bash
# Built-in to pqCheck - no external tools needed!
pqCheck --gen-test -o test.pcap
```

**Option B: Capture from live network** (recommended for production)
```bash
# Capture traffic on a network interface
sudo tcpdump -i eth0 tcp port 5432 -w postgres_traffic.pcap
```

**Option C: Use pre-existing PCAP files** (if you have them)
```bash
# pqCheck includes sample PCAPs in results/
ls -lh results/*.pcap
```

## Part 2: Quick Start Workflow (Automated)

The simplest approach uses pqCheck's built-in test data generation:

### Step 1: Generate Test Traffic

```bash
# Generate clean queries for training
pqCheck --gen-test --gen-clean 50 --gen-inject 0 -o baseline.pcap

# Generate mixed queries for detection testing
pqCheck --gen-test --gen-clean 50 --gen-inject 50 -o mixed.pcap
```

**What pqCheck generates:**
- Valid PostgreSQL protocol packets
- Mix of legitimate SELECT/INSERT/UPDATE/DELETE queries
- SQL injection payloads (OR '1'='1, UNION SELECT, DROP TABLE, etc.)
- Realistic timing and packet structure

### Step 2: Train an N-gram Model

Train your model on **clean, legitimate SQL traffic**:

```bash
# Auto-train from the baseline PCAP
pqCheck -A baseline.pcap -r baseline.pcap \
  -R config/rules.conf \
  -o alerts.jsonl \
  -v
```

**What happens:**
- pqCheck scans the PCAP specified with `-A`
- Extracts all SQL queries from PostgreSQL protocol messages
- Builds a trigram frequency model in memory
- Immediately uses that model to score traffic in the `-r` file
- Writes alerts to `alerts.jsonl`

### Step 3: Detect SQL Injection

Once you have a trained model or baseline, run detection on suspicious traffic:

```bash
# Scan for SQL injection
pqCheck -r mixed.pcap \
  -R config/rules.conf \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

### Step 4: Review Alerts

```bash
# View all alerts in JSON format
cat alerts.jsonl | jq .

# View only high-risk alerts
cat alerts.jsonl | jq 'select(.risk_level=="HIGH" or .risk_level=="CRITICAL")'

# Count alerts by type
cat alerts.jsonl | jq '.risk_level' | sort | uniq -c
```

## Part 3: Advanced Workflows

### Interactive TUI Monitoring

For visual, real-time monitoring:

```bash
# Launch TUI dashboard with auto-training
pqCheck --tui -A baseline.pcap -r mixed.pcap \
  -R config/rules.conf \
  -v
```

**TUI Screens:**
- `[d]` – Dashboard: shows real-time statistics
- `[a]` – Alert Log: scrollable list of detections
- `[c]` – Config: view current detection settings
- `[h]` – Help: keyboard shortcuts

### Custom Port/IP Monitoring

For non-standard PostgreSQL ports:

```bash
# Create config file
cat > config/network.conf << EOF
ports=5432,5433,5434
ips=10.0.0.5,10.0.0.6
output_path=/var/log/pqcheck/alerts.jsonl
EOF

# Run with network config
pqCheck -r capture.pcap \
  -N config/network.conf \
  -R config/rules.conf \
  -m baseline.model \
  -v
```

## Part 4: Troubleshooting

### "Cannot open model file"

If you see this error, the model file wasn't created yet:

```bash
# First, create a model by auto-training
pqCheck -A baseline.pcap -m my.model -v

# Then use it
pqCheck -r test.pcap -m my.model -o alerts.jsonl -v
```

### "No alerts detected"

The detection threshold might be too strict. Try lowering it:

```bash
# Default threshold is -5.0; increase to -3.0 to be more sensitive
pqCheck -r test.pcap -m model.ngram -T -3.0 -o alerts.jsonl -v
```

### "Permission denied" on live capture

Live capture requires special privileges:

```bash
# Option 1: Use sudo
sudo pqCheck -i eth0 -m model.ngram -v

# Option 2: Grant CAP_NET_RAW (permanent)
sudo setcap cap_net_raw=+ep $(which pqCheck)
pqCheck -i eth0 -m model.ngram -v
```

## Part 5: Complete Example Workflow

Here's a complete, copy-paste ready test:

```bash
#!/bin/bash

# Step 1: Generate baseline
echo "Generating clean training data..."
pqCheck --gen-test --gen-clean 100 --gen-inject 0 -o $HOME/baseline.pcap

# Step 2: Generate attack traffic
echo "Generating test traffic with SQL injection..."
pqCheck --gen-test --gen-clean 50 --gen-inject 50 -o $HOME/attack.pcap

# Step 3: Train model
echo "Training anomaly model..."
pqCheck -A $HOME/baseline.pcap -m $HOME/model.ngram -v

# Step 4: Run detection
echo "Scanning for SQL injection..."
pqCheck -r $HOME/attack.pcap \
  -m $HOME/model.ngram \
  -R config/rules.conf \
  -o $HOME/alerts.jsonl \
  -v

# Step 5: View results
echo ""
echo "=== ALL ALERTS ==="
jq '.' $HOME/alerts.jsonl | head -50

echo ""
echo "=== HIGH-RISK ALERTS ONLY ==="
jq 'select(.risk_level=="HIGH" or .risk_level=="CRITICAL")' $HOME/alerts.jsonl

echo ""
echo "=== SUMMARY ==="
echo "Total alerts:"
wc -l < $HOME/alerts.jsonl
```

Save this as `test_pqcheck.sh`, then run:
```bash
chmod +x test_pqcheck.sh
./test_pqcheck.sh
```

---

## Part 6: Production Deployment

### Quick checklist:

- [ ] Captured or generated baseline traffic
- [ ] Trained a model: `pqCheck -A baseline.pcap -m prod.model`
- [ ] Tested on sample PCAP: `pqCheck -r test.pcap -m prod.model`
- [ ] Configured ports/IPs via `config/network.conf` if needed
- [ ] Set up centralized alerting: pipe alerts to SIEM
- [ ] Enable TUI for interactive monitoring: `pqCheck --tui -i eth0 -m prod.model`

### Sample systemd service:

```ini
[Unit]
Description=pqCheck - PostgreSQL SQLi Detection
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/pqCheck \
  --tui \
  -i eth0 \
  -N /etc/pqcheck/network.conf \
  -m /etc/pqcheck/prod.model \
  -R /etc/pqcheck/rules.conf \
  -o /var/log/pqcheck/alerts.jsonl \
  -v
Restart=always
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

## Next Steps

- Read [docs/Example.md](Example.md) for more examples
- See [docs/ngram.md](ngram.md) for how anomaly detection works
- Check [docs/network-config.md](network-config.md) for custom port monitoring
- View [docs/tui.md](tui.md) for TUI keyboard shortcuts and features
