# pqCheck Quick Reference Guide

## 🚀 30-Second Start (Easiest Path)

```bash
# 1. Generate test data (one command, no tools needed)
pqCheck --gen-test -o training.pcap
pqCheck --gen-test --gen-clean 20 --gen-inject 20 -o test.pcap

# 2. Auto-train + detect
pqCheck -A training.pcap -r test.pcap -o alerts.jsonl

# 3. View results
cat alerts.jsonl | jq .
```

**That's it!** You now have working SQL injection detection running.

---

## 📋 CLI Cheat Sheet

### Generate Test PCAPs
```bash
pqCheck --gen-test -o output.pcap                    # 50 clean + 50 injection (default)
pqCheck --gen-test --gen-clean 100 --gen-inject 0 -o clean.pcap        # Clean training data
pqCheck --gen-test --gen-clean 0 --gen-inject 100 -o attack.pcap       # Pure attacks
pqCheck --gen-test --gen-clean 30 --gen-inject 70 -o mixed.pcap        # Custom mix
```

### Train Models
```bash
pqCheck -A training.pcap -m model.ngram                # Auto-train from PCAP
pqCheck -t corpus.sql -m model.ngram                  # Train from SQL corpus (one query per line)
```

### Scan for SQL Injection
```bash
pqCheck -r suspect.pcap -m model.ngram -o alerts.jsonl -v              # Offline analysis
pqCheck -r suspect.pcap -R config/rules.conf -o alerts.jsonl -v        # Rules only (no model)
sudo pqCheck -i eth0 -m model.ngram -o alerts.jsonl -v                 # Live capture
```

### Interactive TUI Dashboard
```bash
pqCheck --tui -A training.pcap -r test.pcap -m model.ngram             # Interactive monitoring
# Keyboard: [d]=dashboard, [a]=alerts, [c]=config, [h]=help, [q]=quit
```

### Monitor Custom Ports/IPs
```bash
# Create config file
cat > config/network.conf << EOF
ports=5432,5433,5434
ips=10.0.0.5,10.0.0.6
output_path=/var/log/pqcheck/alerts.jsonl
EOF

# Use it
pqCheck -r capture.pcap -N config/network.conf -m model.ngram -o alerts.jsonl
```

---

## 📊 Common Workflows

### Scenario 1: Quick Testing (No Dependencies)
```bash
# Perfect for: Trying pqCheck, demos, CI/CD pipelines
pqCheck --gen-test -o test.pcap
pqCheck -A test.pcap -r test.pcap -o alerts.jsonl
cat alerts.jsonl | jq '.[] | {query, risk_level}'
```

### Scenario 2: Production Baseline Training
```bash
# Perfect for: Setting up detection in production

# 1. Capture 1 hour of legitimate traffic
sudo tcpdump -i eth0 tcp port 5432 -w baseline.pcap -G 3600

# 2. Train model
pqCheck -A baseline.pcap -m prod.model -v

# 3. Monitor with that baseline
sudo pqCheck -i eth0 -m prod.model -o /var/log/alerts.jsonl --tui
```

### Scenario 3: Offline Forensics
```bash
# Perfect for: Incident analysis, PCAP replay

# 1. Analyze a captured attack
pqCheck -r compromised.pcap -R config/rules.conf -p attacks.pcap -o forensics.jsonl -v

# 2. Extract flagged packets for Wireshark
wireshark attacks.pcap

# 3. Review detailed alerts
jq '.[0]' forensics.jsonl | less
```

---

## 🔍 Alert Output Format

Each alert in `alerts.jsonl` contains:

```json
{
  "timestamp": "2026-05-08T12:22:15Z",
  "flow_id": "10.0.0.5:55432→10.0.0.10:5432",
  "src_ip": "10.0.0.5",
  "src_port": 55432,
  "dst_ip": "10.0.0.10",
  "dst_port": 5432,
  "query": "SELECT * FROM users WHERE id=1' OR '1'='1",
  "rule_matched": "sql_injection_or_pattern",
  "anomaly_score": -2.5,
  "risk_level": "CRITICAL"
}
```

**Risk Levels:**
- `CRITICAL` – Both rule match + anomaly
- `HIGH` – Strong anomaly or high-confidence rule
- `MEDIUM` – Suspicious pattern detected
- `LOW` – Minor risk indicator

---

## 🛠️ Troubleshooting

| Problem | Solution |
|---------|----------|
| "No alerts detected" | Try stricter threshold: `pqCheck ... -T -3.0` |
| "Cannot open model" | Train first: `pqCheck -A baseline.pcap -m model.ngram` |
| "No captures on live" | Check interface: `sudo tcpdump -i eth0 tcp port 5432 -c 5` |
| "Permission denied" | Use sudo or `sudo setcap cap_net_raw=+ep $(which pqCheck)` |
| "PCAP file is invalid" | Check format: `file input.pcap` should show "pcap capture file" |

---

## 📚 Full Documentation

- **[Quickstart Guide](quickstart.md)** – Detailed walkthrough for beginners
- **[PCAP Guide](pcap-guide.md)** – Deep dive into PCAP files and capture methods
- **[Examples](Example.md)** – 11 real-world use cases
- **[CLI Reference](cli.md)** – All command-line options
- **[TUI Guide](tui.md)** – Dashboard navigation and keyboard shortcuts
- **[Rules](rules.md)** – Writing custom detection rules
- **[N-gram Model](ngram.md)** – How anomaly detection works

---

## ⚡ Pro Tips

1. **Generate reproducible test data:**
   ```bash
   pqCheck --gen-test --gen-clean 100 --gen-inject 50 -o test.pcap
   # Same command = same output every run (for CI/CD)
   ```

2. **Filter alerts by severity:**
   ```bash
   cat alerts.jsonl | jq 'select(.risk_level=="CRITICAL")'
   ```

3. **Monitor specific PostgreSQL instances:**
   ```bash
   pqCheck -r capture.pcap -f "dst host 10.0.0.5" -m model.ngram -o alerts.jsonl
   ```

4. **Save alerts to multiple formats:**
   ```bash
   jq -r '.[] | "\(.timestamp) \(.src_ip) \(.risk_level): \(.query)"' alerts.jsonl > alerts.txt
   ```

5. **Chain with other tools:**
   ```bash
   pqCheck -r capture.pcap -m model.ngram -o - | jq '.[] | select(.risk_level=="CRITICAL")' | wc -l
   ```

---

## 📞 Quick Help

```bash
pqCheck -h              # Show all available options
pqCheck --version       # Show version information
pqCheck --tui -h        # TUI-specific help
```

---

**Last Updated:** May 8, 2026 | pqCheck v1.0.0
