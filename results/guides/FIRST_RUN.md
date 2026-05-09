# Quick Start Guide — First Time Setup

**Objective:** Get pqCheck running in 30 minutes  
**No PostgreSQL needed!** Generate test data instead

---

## Step 1: Verify Installation (5 minutes)

### Check if pqCheck is available
```bash
# Find where it is
which pqCheck

# Show version
pqCheck --version
# Output: pqCheck v1.0.0

# Show help
pqCheck --help | head -10
```

**If not installed:** See [BUILD.md](../docs/BUILD.md) for installation

---

## Step 2: Generate Test Data (5 minutes)

### Create a sample PCAP file with both clean and malicious queries
```bash
# Generate 50 clean + 50 SQL injection attempts
pqCheck --gen-test -o /tmp/demo.pcap

# Verify it was created
ls -lh /tmp/demo.pcap
# Should show: -rw-r--r-- 1 user group  XXX /tmp/demo.pcap
```

**What this does:**
- Creates synthetic PostgreSQL network traffic
- Includes normal queries (SELECT, UPDATE, INSERT)
- Includes SQL injection payloads (UNION, OR 1=1, comments)
- Saves to `/tmp/demo.pcap` in standard PCAP format

---

## Step 3: Run Detection (5 minutes)

### Train model and detect in one command
```bash
# Auto-train model on the PCAP data, then detect against same data
pqCheck -A /tmp/demo.pcap -r /tmp/demo.pcap -o /tmp/alerts.jsonl -v

# This does:
# - Reads /tmp/demo.pcap for training (-A flag)
# - Creates in-memory anomaly model
# - Scans /tmp/demo.pcap again (-r flag)
# - Writes alerts to /tmp/alerts.jsonl (-o flag)
# - Shows verbose output (-v flag)
```

**Expected output:**
```
[INFO] Training model from PCAP...
[INFO] Loaded N queries from PCAP
[INFO] Model training complete
[INFO] Starting analysis...
[INFO] Alerts: N detected, saving to /tmp/alerts.jsonl
```

---

## Step 4: View Results (5 minutes)

### Look at the generated alerts
```bash
# Pretty-print all alerts
cat /tmp/alerts.jsonl | jq .

# Show just the queries and risk levels
cat /tmp/alerts.jsonl | jq '.[] | {query, risk_level}'

# Count alerts by risk level
cat /tmp/alerts.jsonl | jq '.risk_level' | sort | uniq -c

# Show only HIGH and CRITICAL
cat /tmp/alerts.jsonl | jq 'select(.risk_level=="HIGH" or .risk_level=="CRITICAL")'
```

---

## Step 5: Try the Interactive Dashboard (5 minutes)

### Run with TUI if it was built with TUI support
```bash
# Start interactive dashboard
pqCheck --tui -r /tmp/demo.pcap

# Navigation:
# d = dashboard (stats)
# a = alerts (detailed view)
# c = config (settings)
# h = help (shortcuts)
# q = quit (exit)
```

**Try this:**
1. Press `d` to see dashboard with stats
2. Press `a` to see alerts log
3. Press `c` to see configuration
4. Press `q` to exit

---

## You're Done! ✅

You now have successfully:
- ✅ Generated test data
- ✅ Run SQL injection detection
- ✅ Viewed alerts in JSON format
- ✅ Used the interactive dashboard

---

## What Just Happened?

### The Detection Pipeline
1. **Test Data (--gen-test)**
   - Creates synthetic PostgreSQL network traffic
   - Mixes clean and malicious queries
   - Saves as PCAP file

2. **Auto-Training (-A)**
   - Reads queries from PCAP
   - Builds n-gram frequency model
   - Learns baseline "normal" behavior

3. **Detection (-r)**
   - Scans queries against rules
   - Scores anomalies against model
   - Generates alerts

4. **Results (-o)**
   - JSONLines format (one JSON object per line)
   - Each alert has: query, risk_level, timestamp, etc.
   - Machine-parseable format

---

## Next Steps

### Option A: Continue Exploring
- Try different rules: `pqCheck -R config/rules.conf -r /tmp/demo.pcap`
- Train a model separately: `pqCheck -A /tmp/demo.pcap -m /tmp/model.ng`
- Create custom PCAP: `pqCheck --gen-test --gen-clean 100 --gen-inject 20 -o custom.pcap`

### Option B: Set Up Real Detection
1. Read [Installation Paths](../guides/INSTALLATION_PATHS.md) to choose setup method
2. Configure for your environment ([Configuration Guide](../guides/CONFIGURATION.md))
3. Deploy to production ([Production Setup](../guides/PRODUCTION_SETUP.md))

### Option C: Learn More
- Read [QUICK-REF.md](../docs/QUICK-REF.md) for command cheat sheet
- Read [docs/architecture.md](../docs/architecture.md) for system design
- Read [docs/rules.md](../docs/rules.md) to write custom rules
- Read [docs/ngram.md](../docs/ngram.md) to understand anomaly detection

---

## Handy Commands for Later

```bash
# Generate just clean data (for training)
pqCheck --gen-test --gen-clean 100 --gen-inject 0 -o clean.pcap

# Generate just attacks (for testing detection)
pqCheck --gen-test --gen-clean 0 --gen-inject 50 -o attacks.pcap

# Train a model and save it
pqCheck -A clean.pcap -m my_model.ng

# Use saved model for detection
pqCheck -r attacks.pcap -m my_model.ng -o alerts.jsonl

# Dump flagged packets (for forensics)
pqCheck -r traffic.pcap -p flagged_packets.pcap -o alerts.jsonl

# Analyze with custom rules
pqCheck -r traffic.pcap -R my_rules.conf -o alerts.jsonl

# Live capture on eth0
sudo pqCheck -i eth0 -R config/rules.conf -o alerts.jsonl

# Database session mode (execute SQL directly)
pqCheck -d "host=localhost user=postgres" 
```

---

## Troubleshooting First Run

| Problem | Solution |
|---------|----------|
| "command not found: pqCheck" | Install per [BUILD.md](../docs/BUILD.md) |
| "cannot open /tmp/demo.pcap" | Check disk space: `df -h /tmp` |
| No JSON output | Verify: `file /tmp/alerts.jsonl` should say JSON |
| "jq: command not found" | Install: `sudo apt-get install jq` (optional) |
| Permission denied on live capture | Grant capability: `sudo setcap cap_net_raw=+ep $(which pqCheck)` |
| TUI not available | Rebuild: `make clean && make WITH_TUI=1` |

---

## Get Help

- **Commands:** `pqCheck --help`
- **Full docs:** See [results/docs/DOCUMENTATION_INDEX.md](../docs/DOCUMENTATION_INDEX.md)
- **Troubleshooting:** See [builds/TROUBLESHOOTING.md](../builds/TROUBLESHOOTING.md)

---

**Congratulations!** You successfully ran pqCheck for the first time 🎉

*Quick Start Guide — v1.0*
