# Pre-Deployment Checklist

**Purpose:** Ensure pqCheck is ready for production deployment  
**Time Required:** 30-45 minutes  
**Prerequisites:** Completed BUILD.md Path C or D

---

## Phase 1: Preparation (5 minutes)

- [ ] PostgreSQL server identified and accessible
- [ ] Network interface for capture identified (eth0, etc.)
- [ ] Firewall rules allowing access documented
- [ ] Incident response procedures in place
- [ ] Alert destination/SIEM configured

---

## Phase 2: Installation Verification (10 minutes)

### Binary Installation
- [ ] `which pqCheck` returns correct path
- [ ] `pqCheck --version` shows 1.0.0
- [ ] `pqCheck --help` displays all options
- [ ] Binary is executable (`ls -l pqCheck | grep x`)

### File Locations
- [ ] Config files in correct location (`~/.local/etc/pqcheck` OR `/usr/local/etc/pqcheck`)
- [ ] Documentation accessible
- [ ] Rules file exists and readable
- [ ] Network configuration file present (or create from example)

### Dependencies
- [ ] `ldd pqCheck` shows all libraries found
- [ ] `libpcap.so.1` available
- [ ] `libpq.so.5` available (if WITH_LIBPQ=1)
- [ ] `libncurses.so.6` available (if WITH_TUI=1)

---

## Phase 3: Configuration Review (10 minutes)

### Rules Configuration
- [ ] `~/.local/etc/pqcheck/rules.conf` (or `/usr/local/etc/pqcheck/rules.conf`) exists
- [ ] Rules file has syntax errors checked: `pqCheck -R rules.conf --help` runs
- [ ] Custom rules added if needed
- [ ] Rule severity levels appropriate for environment

### Network Configuration
- [ ] `network.conf` created or customized
- [ ] Correct PostgreSQL port configured (5432 default)
- [ ] Correct IP ranges configured
- [ ] Alert output path configured
- [ ] Output path has write permissions

### Database Connectivity (if WITH_LIBPQ=1)
- [ ] PostgreSQL server is reachable (ping/telnet test)
- [ ] Libpq connection string documented
- [ ] Test connection: `psql "default_connection_string" -c "SELECT 1"`
- [ ] audit mode tested: `pqCheck --audit -c "connection_string"`

---

## Phase 4: Testing (10 minutes)

### Functionality Test
```bash
# 1. Generate test data
pqCheck --gen-test -o /tmp/demo.pcap

# 2. Run offline analysis
pqCheck -r /tmp/demo.pcap -R config/rules.conf -o /tmp/test.jsonl

# Verify:
cat /tmp/test.jsonl | jq 'length'  # Should show > 0
```
- [ ] Test PCAP generated successfully
- [ ] Offline analysis completed without errors
- [ ] Results file contains valid JSON-Lines
- [ ] At least one alert triggered

### Live Capture Test (if not using systemd)
```bash
# This tests capability setup
timeout 5 pqCheck -i lo -R config/rules.conf -v || true
```
- [ ] Live capture starts without permission error
- [ ] Can see packet processing in verbose output
- [ ] No "Operation not permitted" errors

### Dashboard Test (if WITH_TUI=1)
```bash
pqCheck --tui -r /tmp/demo.pcap
# Press: d (dashboard), a (alerts), c (config), q (quit)
```
- [ ] Dashboard displays without errors
- [ ] Can navigate between screens
- [ ] Alerts displayed correctly
- [ ] Configuration visible

---

## Phase 5: Security Setup (10 minutes)

### Permissions
- [ ] Binary owned by root or specific user: `ls -l pqCheck`
- [ ] Config files readable: `stat config/rules.conf`
- [ ] Output directory writable: `touch /var/log/pqcheck/test.txt`
- [ ] Archive directory permissions appropriate

### Capabilities (if live capture needed)
```bash
sudo setcap cap_net_raw=+ep $(which pqCheck)
getcap $(which pqCheck)  # Should show: cap_net_raw+ep
```
- [ ] Capabilities set correctly
- [ ] Verified with `getcap` command
- [ ] Can run as non-root user without sudo

### Systemd Service (if Path D)
- [ ] Service file in `/etc/systemd/system/pqcheck.service`
- [ ] Service file syntax valid: `systemd-analyze verify pqcheck.service`
- [ ] Service starts: `sudo systemctl start pqcheck`
- [ ] Service status okay: `sudo systemctl status pqcheck`
- [ ] Logs appear in: `sudo journalctl -u pqcheck`

---

## Phase 6: Integration Setup (5 minutes)

### Logging
- [ ] Alert output file path configured
- [ ] Log rotation configured (if file-based)
- [ ] Directory exists with correct permissions
- [ ] Disk space adequate (predicted growth)

#### SIEM Integration (if applicable)
- [ ] Syslog facility configured
- [ ] SIEM receiving alerts
- [ ] Alert format matches SIEM parser
- [ ] Test alert sent and received

#### Monitoring/Alerting
- [ ] Health check script ready (if needed)
- [ ] Monitoring agent configured
- [ ] Thresholds set for alerting
- [ ] High alert rate threshold configured

---

## Phase 7: Documentation & Runbooks (5 minutes)

### Documentation
- [ ] Installation procedure documented
- [ ] Configuration documented
- [ ] Alert response procedure documented
- [ ] Troubleshooting guide prepared
- [ ] Contact information for support

### Runbooks
- [ ] How to restart service
- [ ] How to review alerts
- [ ] How to update rules
- [ ] How to update model
- [ ] Emergency stop procedure

---

## Phase 8: Final Validation (5 minutes)

### Complete test flow
```bash
# Run full workflow
pqCheck --gen-test -o /tmp/final_test.pcap
pqCheck -A /tmp/final_test.pcap -r /tmp/final_test.pcap -o /tmp/final_alerts.jsonl -v
echo "Alerts generated: $(cat /tmp/final_alerts.jsonl | jq 'length')"
```
- [ ] Complete workflow runs end-to-end
- [ ] No errors in verbose output
- [ ] Results can be parsed successfully
- [ ] Performance acceptable

### System Integration
- [ ] Systemd service passes validation
- [ ] logs appear in correct location
- [ ] SIEM receiving alerts (if configured)
- [ ] Monitoring detects system running

---

## Sign-Off

- [ ] **Prepared by:** _____________ **Date:** _______
- [ ] **Reviewed by:** _____________ **Date:** _______
- [ ] **Approved by:** _____________ **Date:** _______

### Notes
```
[Space for deployment notes]


```

---

## Troubleshooting During Checklist

| Issue | Solution |
|-------|----------|
| Binary not executable | `chmod +x pqCheck` |
| Missing libpcap | Install: `sudo apt-get install libpcap` |
| Permission denied on capture | Run checklist item 6 (capabilities) |
| service won't start | Check logs: `journalctl -u pqcheck -n 50` |
| No alerts generated | Check rules file: `cat config/rules.conf` |
| SIEM not receiving | Verify syslog facility configured |

---

**Status: Ready for Production ✅** once all items checked

*Pre-Deployment Checklist — v1.0*
