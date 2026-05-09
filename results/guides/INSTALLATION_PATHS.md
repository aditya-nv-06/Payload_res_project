# Installation Paths — Choose Your Setup

**Choose ONE path based on your needs. Each path is fully documented with step-by-step instructions.**

---

## Quick Comparison

| Path | Goal | Time | Sudo | Binary Size |
|------|------|------|------|--------|
| **A** | Try it now | 5 min | No | ~170KB |
| **B** | Local dev | 10 min | No | ~170KB |
| **C** | System install | 15 min | Yes | ~170KB |
| **D** | Production service | 20 min | Yes | ~170KB |
| **E** | Enterprise deploy | 30 min | Yes | .deb/.rpm |

---

## Path A: Quick Trial (No Installation)

**Time:** 5 minutes  
**Goal:** Try pqCheck without any system changes  
**When:** Learning, testing, demos, CI/CD

### Instructions
```bash
# 1. Build with features
cd /home/aditya/res_cli/Payload_res_project
make clean
make WITH_TUI=1 WITH_LIBPQ=1

# 2. Run from current directory
./pqCheck --gen-test -o test.pcap
./pqCheck -A test.pcap -r test.pcap -o alerts.jsonl

# 3. View results
cat alerts.jsonl | jq '.[0]'

# 4. Try dashboard (if available)
./pqCheck --tui -r test.pcap   # Press q to quit

# 5. Cleanup (optional)
make clean
```

**Result:** ✅ pqCheck working and validated

---

## Path B: Local User Install (Recommended)

**Time:** 10 minutes  
**Goal:** Install for personal use, no system access needed  
**When:** Development, personal workstation, testing

### Step 1: Build
```bash
cd /home/aditya/res_cli/Payload_res_project
make clean
make WITH_TUI=1 WITH_LIBPQ=1
```

### Step 2: Install Locally
```bash
# Install to ~/.local (no sudo required)
make install

# Verify
which pqCheck                    # Should show ~/.local/bin/pqCheck
pqCheck --version              # Should show v1.0.0
```

### Step 3: Update PATH (if needed)
```bash
# Check if ~/.local/bin already in PATH
echo $PATH | grep '.local/bin'

# If not, add it
echo 'export PATH=$HOME/.local/bin:$PATH' >> ~/.bashrc
source ~/.bashrc

# Verify
pqCheck --version
```

### Step 4: Verify Installation
```bash
# Quick test
pqCheck --gen-test -o /tmp/test.pcap
pqCheck -A /tmp/test.pcap -r /tmp/test.pcap -o /tmp/test.jsonl
echo "Alerts: $(cat /tmp/test.jsonl | jq 'length')"
```

**Configuration:** `~/.local/etc/pqcheck/`  
**Executables:** `~/.local/bin/pqCheck`  
**Docs:** `~/.local/share/doc/pqcheck/`

**Result:** ✅ pqCheck ready for local use

---

## Path C: System-Wide Installation

**Time:** 15 minutes  
**Goal:** Install for all users, production-ready  
**When:** System administrator, multi-user systems

### Step 1: Build
```bash
cd /home/aditya/res_cli/Payload_res_project
make clean
make WITH_TUI=1 WITH_LIBPQ=1
```

### Step 2: Install System-Wide
```bash
# Install to /usr/local (requires sudo)
sudo make install PREFIX=/usr/local
```

### Step 3: Grant Network Permissions
```bash
# For live capturing (-i eth0)
sudo setcap cap_net_raw=+ep /usr/local/bin/pqCheck

# Verify
getcap /usr/local/bin/pqCheck
# Should show: cap_net_raw+ep
```

### Step 4: Verify Installation
```bash
#Check binary location
which pqCheck                    # Should show /usr/local/bin/pqCheck
pqCheck --version              # Should show v1.0.0

# Quick test
pqCheck --gen-test -o /tmp/test.pcap
pqCheck -A /tmp/test.pcap -r /tmp/test.pcap
```

**Configuration:** `/usr/local/etc/pqcheck/`  
**Executables:** `/usr/local/bin/pqCheck`  
**Docs:** `/usr/local/share/doc/pqcheck/`

**Result:** ✅ pqCheck available system-wide

---

## Path D: Systemd Daemon (Production)

**Time:** 20 minutes  
**Goal:** Background service with auto-restart  
**When:** Production deployment, continuous monitoring

### Step 1-3: Follow Path C above
_(Complete system-wide installation first)_

### Step 4: Install Systemd Service
```bash
# Copy service file
sudo cp packaging/pqcheck.service /etc/systemd/system/

# Reload systemd
sudo systemctl daemon-reload

# Enable service (auto-start on boot)
sudo systemctl enable pqcheck
```

### Step 5: Configure Service
```bash
# Edit configuration files
sudo nano /usr/local/etc/pqcheck/network.conf
sudo nano /usr/local/etc/pqcheck/rules.conf

# Test configuration syntax
pqCheck --help    # Should work without errors
```

### Step 6: Start Service
```bash
# Start the service
sudo systemctl start pqcheck

# Check status
sudo systemctl status pqcheck

# View live logs
sudo journalctl -u pqcheck -f
# Press Ctrl+C to stop following logs

# Get recent logs
sudo journalctl -u pqcheck -n 50
```

### Step 7: Verify Service
```bash
# Check if running
sudo systemctl is-active pqcheck

# Check if enabled at boot
sudo systemctl is-enabled pqcheck

# Test manual execution
sudo /usr/local/bin/pqCheck --gen-test -o /tmp/test.pcap
```

**Service File:** `/etc/systemd/system/pqcheck.service`  
**Logs:** `journalctl -u pqcheck` or `/var/log/pqcheck/alerts.jsonl`  
**Status:** `systemctl status pqcheck`

**Useful Commands:**
```bash
sudo systemctl start pqcheck      # Start service
sudo systemctl stop pqcheck       # Stop service
sudo systemctl restart pqcheck    # Restart service
sudo systemctl status pqcheck     # Show status
journalctl -u pqcheck -f          # Follow logs
systemctl --type=service --failed # Find failed services
```

**Result:** ✅ pqCheck running as background service

---

## Path E: Enterprise Package Distribution

**Time:** 30 minutes  
**Goal:** Create installable package for team deployment  
**When:** Enterprise, package management, distribution

### Prerequisites
```bash
# Install fpm (Ruby packaging tool)
sudo apt-get install ruby-dev
sudo gem install fpm

# Verify
fpm --version
```

### For DEB (Debian/Ubuntu)

**Build:**
```bash
cd /home/aditya/res_cli/Payload_res_project
chmod +x packaging/build-deb.sh
bash packaging/build-deb.sh

# Verify
ls -lh pqcheck_1.0.0_amd64.deb
```

**Install:**
```bash
# Single user
sudo apt-get install ./pqcheck_1.0.0_amd64.deb

# Or distribute the .deb file
scp pqcheck_1.0.0_amd64.deb user@server:/tmp/
# Then on target:
sudo apt-get install /tmp/pqcheck_1.0.0_amd64.deb
```

### For RPM (RHEL/CentOS/Fedora)

**Build:**
```bash
cd /home/aditya/res_cli/Payload_res_project
sudo dnf install ruby-devel
sudo gem install fpm

chmod +x packaging/build-rpm.sh
bash packaging/build-rpm.sh

# Verify
ls -lh pqcheck-1.0.0-1.x86_64.rpm
```

**Install:**
```bash
# Single user
sudo dnf install pqcheck-1.0.0-1.x86_64.rpm

# Or distribute the .rpm file
scp pqcheck-1.0.0-1.x86_64.rpm user@server:/tmp/
# Then on target:
sudo dnf install /tmp/pqcheck-1.0.0-1.x86_64.rpm
```

### Deploy Package

**Method 1: Manual**
```bash
# Copy package
cp pqcheck_1.0.0_amd64.deb /shared/packages/

# On destination:
sudo apt-get install /shared/packages/pqcheck_1.0.0_amd64.deb
```

**Method 2: Local Package Repository**
```bash
# Setup repository
mkdir -p /var/www/html/pqcheck-repo/
cp pqcheck_1.0.0_amd64.deb /var/www/html/pqcheck-repo/

# Add to systems
echo 'deb http://repo.example.com/pqcheck-repo /' | sudo tee /etc/apt/sources.list.d/pqcheck.list
sudo apt-get update
sudo apt-get install pqcheck
```

**Result:** ✅ Package ready for team deployment

---

## Decision Tree

```
START
  ↓
Want to try it now?
  ├─ YES → Path A (5 min)
  └─ NO
      ↓
Want to use daily?
  ├─ YES (personal) → Path B (10 min)
  ├─ YES (team) → Path C or D
  └─ NO
      ↓
Want background service?
  ├─ YES → Path D (20 min)
  ├─ NO → Path C (15 min)
  └─ N/A
      ↓
Want to distribute?
  └─ YES → Path E (30 min)
```

---

## Verification for All Paths

After installation, run this to verify:

```bash
# 1. Binary found
which pqCheck

# 2. Version correct
pqCheck --version

# 3. Help works
pqCheck --help | head -5

# 4. Generate test data
pqCheck --gen-test -o /tmp/verify.pcap

# 5. Run detection
pqCheck -A /tmp/verify.pcap -r /tmp/verify.pcap -o /tmp/verify.json

# 6. Check results
tail /tmp/verify.json

# ✅ All passed = Installation successful
```

---

## Uninstall Instructions

### Path A (Quick Trial)
```bash
cd /home/aditya/res_cli/Payload_res_project
make clean
# Done - no system changes
```

### Path B (Local Install)
```bash
# Uninstall
make uninstall

# Cleanup
rm -rf ~/.local/etc/pqcheck ~/.local/share/pqcheck
```

### Path C (System Install)
```bash
# Uninstall
sudo make uninstall PREFIX=/usr/local

# Cleanup
sudo rm -rf /usr/local/etc/pqcheck /usr/local/share/pqcheck
```

### Path D (Systemd)
```bash
# Stop and disable
sudo systemctl stop pqcheck
sudo systemctl disable pqcheck

# Remove systemd file
sudo rm /etc/systemd/system/pqcheck.service
sudo systemctl daemon-reload

# Then follow Path C uninstall
```

### Path E (Package)
```bash
# DEB
sudo apt-get remove pqcheck

# RPM
sudo dnf remove pqcheck
```

---

## Common Issues During Installation

| Issue | Solution |
|-------|----------|
| "make: command not found" | Install: `sudo apt-get install build-essential` |
| "gcc: command not found" | Install: `sudo apt-get install gcc` |
| "libpcap-dev not found" | Install: `sudo apt-get install libpcap-dev` |
| "Permission denied" | Add `sudo` to commands for system paths |
| "Command not found" after install | Add ~/.local/bin to PATH (Path B) |
| "Already installed" error | Run `make clean` first |

---

## Next Steps

After successful installation, see:
- **[FIRST_RUN.md](FIRST_RUN.md)** — Get started immediately
- **[CONFIGURATION.md](CONFIGURATION.md)** — Customize for your environment
- **[PRODUCTION_SETUP.md](PRODUCTION_SETUP.md)** — Deploy to production
- **[QUICK-REF.md](../docs/QUICK-REF.md)** — Command cheat sheet

---

**Ready to start? Pick a path above and follow the steps!**

*Installation Guide — v1.0*
