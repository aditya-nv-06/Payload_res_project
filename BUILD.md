# Build & Installation Guide

Choose your path below and follow the numbered steps in order.

---

## 🎯 Which Path Should I Take?

| Path | Use Case | Effort | When Ready |
|------|----------|--------|-----------|
| **A: Quick Test** | Try pqCheck locally, demos, CI/CD | ⭐ | 5 min |
| **B: Local Install** | Personal workstation, no sudo | ⭐⭐ | 10 min |
| **C: System Install** | Production server, system-wide | ⭐⭐⭐ | 15 min |
| **D: Systemd Daemon** | Background service, auto-restart | ⭐⭐⭐⭐ | 20 min |
| **E: Packaged (DEB/RPM)** | Enterprise distribution | ⭐⭐⭐⭐⭐ | 30 min |

---

## Step 1: Install Dependencies

Select your operating system:

### Debian/Ubuntu
```bash
sudo apt-get update && sudo apt-get install -y \
    build-essential \
    libpcap-dev \
    libncurses-dev \
    libpq-dev
```

### RHEL / CentOS / Fedora
```bash
sudo dnf install -y \
    gcc \
    libpcap-devel \
    ncurses-devel \
    libpq-devel
```

### Alpine Linux
```bash
apk add --no-cache \
    build-base \
    libpcap-dev \
    ncurses-dev \
    postgresql-client-dev
```

### macOS (Homebrew)
```bash
brew install libpcap ncurses libpq
```

### Windows (WSL2 Recommended)
Use WSL2 with Ubuntu 22.04 and follow Debian/Ubuntu instructions above.

---

## Step 2: Choose Your Installation Path

### 🚀 Path A: Quick Test (No Installation)

```bash
# Build locally without installing
make clean
make WITH_TUI=1 WITH_LIBPQ=1

# Run directly from source tree
./pqCheck --gen-test -o test.pcap
./pqCheck -A test.pcap -r test.pcap -o alerts.jsonl
./pqCheck --tui -r test.pcap

# When done, clean up
make clean
```

**✅ Pro:** No system changes, reversible  
**❌ Con:** Must run from source directory

---

### 👤 Path B: Local User Install (Recommended)

```bash
# Step 1: Build with all features
make clean
make WITH_TUI=1 WITH_LIBPQ=1

# Step 2: Install to ~/.local (no sudo required)
make install

# Step 3: Verify installation
ls -lh ~/.local/bin/pqCheck
pqCheck --version

# Step 4: Add to PATH (if not already)
echo 'export PATH=$HOME/.local/bin:$PATH' >> ~/.bashrc
source ~/.bashrc

# Step 5: Test the installation
pqCheck --gen-test -o /tmp/test.pcap
pqCheck -A /tmp/test.pcap -r /tmp/test.pcap -o /tmp/alerts.jsonl
cat /tmp/alerts.jsonl | jq .
```

**Configuration files installed to:** `~/.local/etc/pqcheck/`  
**Documentation installed to:** `~/.local/share/doc/pqcheck/`

**✅ Pro:** Personal workspace, no system involvement  
**❌ Con:** Only available to current user

---

### 🖥️ Path C: System-Wide Installation

```bash
# Step 1: Build with all features
make clean
make WITH_TUI=1 WITH_LIBPQ=1

# Step 2: Install system-wide
sudo make install PREFIX=/usr/local

# Step 3: Verify installation
ls -lh /usr/local/bin/pqCheck
pqCheck --version

# Step 4: Configure (optional)
sudo nano /usr/local/etc/pqcheck/rules.conf
sudo nano /usr/local/etc/pqcheck/network.conf

# Step 5: Test
pqCheck --gen-test -o /tmp/test.pcap
pqCheck -A /tmp/test.pcap -r /tmp/test.pcap -o /tmp/alerts.jsonl
```

**Installed to:** `/usr/local/bin/pqCheck`  
**Configuration:** `/usr/local/etc/pqcheck/`  
**Documentation:** `/usr/local/share/doc/pqcheck/`

**✅ Pro:** Available to all users  
**❌ Con:** Requires sudo

---

### 🔧 Path D: Systemd Daemon (Production)

```bash
# Step 1-3: Follow Path C (System-Wide) above

# Step 4: Install systemd service file
sudo cp packaging/pqcheck.service /etc/systemd/system/

# Step 5: Configure for your environment
sudo nano /usr/local/etc/pqcheck/config.conf

# Step 6: Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable pqcheck
sudo systemctl start pqcheck

# Step 7: Verify daemon
sudo systemctl status pqcheck
sudo journalctl -u pqcheck -f    # Follow logs

# Step 8: Monitor alerts
sudo tail -f /var/log/pqcheck/alerts.jsonl
```

**Runs as:** System service (respects capabilities)  
**Auto-restarts:** On failure or reboot  
**Logs to:** `/var/log/pqcheck/alerts.jsonl` and `journalctl -u pqcheck`

**✅ Pro:** Production-ready, auto-restart, centralized logging  
**❌ Con:** Requires sudo, more complex setup

---

### 📦 Path E: Packaged Distribution (DEB or RPM)

#### For Debian/Ubuntu (DEB)

```bash
# Step 1: Install fpm (packaging tool)
sudo apt-get install -y ruby-dev
sudo gem install fpm

# Step 2: Build the DEB package
chmod +x packaging/build-deb.sh
bash packaging/build-deb.sh

# Step 3: Verify package was created
ls -lh pqcheck_1.0.0_amd64.deb

# Step 4: Install the package
sudo apt-get install ./pqcheck_1.0.0_amd64.deb

# Step 5: Verify
pqCheck --version
```

#### For RHEL / CentOS / Fedora (RPM)

```bash
# Step 1: Install fpm
sudo dnf install -y ruby-devel
sudo gem install fpm

# Step 2: Build the RPM package
chmod +x packaging/build-rpm.sh
bash packaging/build-rpm.sh

# Step 3: Verify package was created
ls -lh pqcheck-1.0.0-1.x86_64.rpm

# Step 4: Install the package
sudo dnf install pqcheck-1.0.0-1.x86_64.rpm

# Step 5: Verify
pqCheck --version
```

**✅ Pro:** Distributable, managed by package manager, easy to remove  
**❌ Con:** Requires fpm, Ruby available

---

## Step 3: Verify Your Installation

Run these tests in order:

```bash
# 1. Show version
pqCheck --version

# 2. Display help
pqCheck --help | head -20

# 3. Generate test PCAP (no network access needed)
pqCheck --gen-test -o /tmp/demo.pcap

# 4. Train and detect
pqCheck -A /tmp/demo.pcap -r /tmp/demo.pcap -o /tmp/test_alerts.jsonl

# 5. View results
cat /tmp/test_alerts.jsonl | jq '.[] | {query, risk_level}'

# 6. Launch interactive dashboard (if TUI enabled)
pqCheck --tui -r /tmp/demo.pcap
# Navigate: [d]=dashboard, [a]=alerts, [c]=config, [q]=quit
```

All tests pass? ✅ Ready to use pqCheck!

---

## Step 4: Grant Network Capabilities (For Live Capture)

If you plan to use live capture (`-i eth0`), grant the binary network capabilities:

```bash
# For user install
sudo setcap cap_net_raw=+ep ~/.local/bin/pqCheck

# For system install
sudo setcap cap_net_raw=+ep /usr/local/bin/pqCheck

# Verify it worked
getcap ~/.local/bin/pqCheck
# Should output: cap_net_raw+ep
```

Then test live capture:
```bash
pqCheck -i eth0 -R config/rules.conf -o /tmp/live.jsonl -v
# Press Ctrl+C after a few seconds to stop
```

---

## Directory Layout After Installation

### User Install (`~/.local`)
```
~/.local/
├── bin/pqCheck              # The binary
├── etc/pqcheck/
│   ├── rules.conf           # Detection rules
│   └── network.conf         # Network configuration
└── share/
    ├── doc/pqcheck/         # Documentation
    └── pqcheck/
        ├── rules.conf.example
        └── network.conf.example
```

### System Install (`/usr/local`)
```
/usr/local/
├── bin/pqCheck
├── etc/pqcheck/
│   ├── rules.conf
│   └── network.conf
└── share/
    ├── doc/pqcheck/
    └── pqcheck/
        ├── rules.conf.example
        └── network.conf.example
```

### Systemd Service
```
/etc/systemd/system/pqcheck.service     # Service definition
/var/log/pqcheck/alerts.jsonl           # Alert output  
```

---

## Build Options Reference

### Minimal Build (Rules + Anomaly Only)
```bash
make clean && make
# Size: ~80KB, Dependencies: libpcap only
```

### Production Build (All Features)
```bash
make clean && make WITH_TUI=1 WITH_LIBPQ=1
# Size: ~170KB, Dependencies: libpcap, ncurses, libpq
```

### Custom Configuration
```bash
# Set compiler flags
make CFLAGS="-O3 -march=native" WITH_TUI=1

# Set installation prefix
make install PREFIX=/opt/pqcheck

# Set custom directories
make install BINDIR=/custom/bin CONFDIR=/custom/etc
```

---

## Troubleshooting

### Build Error: "pcap.h: No such file"
**Solution:** Install libpcap development package (see Step 1)

### Build Error: "ncurses.h: No such file"
**Solution:** Install ncurses development package (see Step 1)

### Build Error: "libpq-fe.h: No such file"
**Solution:** Either skip WITH_LIBPQ or install PostgreSQL client dev package

### Build Error: Undefined symbol in pg_correlate
**Solution:**
```bash
make clean
make WITH_LIBPQ=1  # Must enable this explicitly
```

### Permission Denied on Live Capture
**Solution:** Run with sudo or grant capabilities (see Step 4)

### systemctl: Failed to start pqcheck
**Solution:**
```bash
# Check what went wrong
sudo journalctl -u pqcheck -n 20

# Verify binary exists
which pqCheck && pqCheck --version

# Manually start service to debug
pqCheck --audit --audit-json /tmp/test.json
```

### Package Contains Wrong Paths
**Solution:** Check `PREFIX` environment variable before building:
```bash
echo $PREFIX   # Should be unset or /usr/local
make clean
make install PREFIX=/usr/local
```

---

## Uninstallation

### User Install
```bash
make uninstall
rm -rf ~/.local/etc/pqcheck ~/.local/share/pqcheck
```

### System Install
```bash
sudo make uninstall PREFIX=/usr/local
sudo rm -rf /usr/local/etc/pqcheck /usr/local/share/pqcheck
```

### Systemd Service
```bash
sudo systemctl stop pqcheck
sudo systemctl disable pqcheck
sudo rm /etc/systemd/system/pqcheck.service
sudo systemctl daemon-reload
```

### Package Manager
```bash
# DEB
sudo apt-get remove pqcheck

# RPM
sudo dnf remove pqcheck
```

---

## Next Steps

After installation, see:
- **Quick Start:** [docs/QUICK-REF.md](docs/QUICK-REF.md) (5 min)
- **Full Walkthrough:** [docs/quickstart.md](docs/quickstart.md) (15 min)
- **Interactive Dashboard:** [docs/tui.md](docs/tui.md) (10 min)
- **Production Setup:** [docs/run.md](docs/run.md) (20 min)

```
~/.local/    (user install)
├── bin/
│   └── pqCheck
├── etc/pqcheck/
│   ├── rules.conf
│   └── network.conf

/usr/local/  (system install with sudo)
├── bin/
│   └── pqCheck
├── etc/pqcheck/
│   ├── rules.conf
│   └── network.conf
├── share/doc/pqcheck/
│   ├── quickstart.md
│   ├── cli.md
│   └── ...
└── share/pqcheck/
    ├── rules.conf.example
    └── network.conf.example

/etc/systemd/system/  (if deployed)
└── pqcheck.service
```

---

## Running After Installation

### Interactive Testing
```bash
pqCheck --gen-test -o test.pcap
pqCheck -A test.pcap -r test.pcap -o alerts.jsonl
pqCheck --tui -r test.pcap
```

### Live Capture (Requires CAP_NET_RAW)
```bash
# Option 1: Use sudo
sudo pqCheck -i eth0 -m model.ngram -o alerts.jsonl

# Option 2: Grant capability (permanent)
sudo setcap cap_net_raw=+ep $(which pqCheck)
pqCheck -i eth0 -m model.ngram -o alerts.jsonl
```

### Systemd Daemon Mode
```bash
# Enable and start
sudo systemctl enable pqcheck
sudo systemctl start pqcheck

# Monitor logs
sudo journalctl -u pqcheck -f

# Stop
sudo systemctl stop pqcheck
```

---

## Docker Support (Optional)

Create a `Dockerfile`:
```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential libpcap-dev libncurses-dev libpq-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN make WITH_TUI=1 WITH_LIBPQ=1 && \
    make install PREFIX=/usr/local

ENTRYPOINT ["pqCheck"]
CMD ["--gen-test", "-o", "/tmp/demo.pcap"]
```

Build and run:
```bash
docker build -t pqcheck:latest .
docker run pqcheck:latest --version
```

---

## Troubleshooting

### Build fails: "pcap.h: No such file"
```bash
# Debian/Ubuntu
sudo apt-get install libpcap-dev

# RHEL/CentOS
sudo dnf install libpcap-devel
```

### Build fails: "ncurses.h: No such file"
```bash
# Debian/Ubuntu
sudo apt-get install libncurses-dev

# RHEL/CentOS
sudo dnf install ncurses-devel
```

### Build fails: "libpq-fe.h: No such file"
```bash
# Debian/Ubuntu
sudo apt-get install libpq-dev

# RHEL/CentOS
sudo dnf install libpq-devel

# Optional: specify pg_config path
PG_CONFIG=/usr/lib/postgresql/14/bin/pg_config make WITH_LIBPQ=1
```

### Permission denied on live capture
```bash
# Grant network capabilities
sudo setcap cap_net_raw=+ep /usr/local/bin/pqCheck

# Verify
getcap /usr/local/bin/pqCheck
# Should show: /usr/local/bin/pqCheck = cap_net_raw+ep
```

### Systemd service won't start
```bash
# Check logs
sudo journalctl -u pqcheck --no-pager | tail -20

# Restart service
sudo systemctl restart pqcheck

# View current status
sudo systemctl status pqcheck
```

---

## Uninstallation

### Local install
```bash
make uninstall
```

### System install
```bash
sudo make uninstall PREFIX=/usr/local
```

### DEB package
```bash
sudo apt-get remove pqcheck
sudo systemctl stop pqcheck  # if running as service
```

### RPM package
```bash
sudo dnf remove pqcheck
sudo systemctl stop pqcheck  # if running as service
```

---

## Development Build

For contributing or local testing:

```bash
# Clone and setup
git clone <repo>
cd Payload_res_project

# Build with debugging symbols
make CFLAGS="-std=c11 -Wall -Wextra -Wpedantic -O0 -g -D_GNU_SOURCE -Isrc" WITH_TUI=1

# Run tests
make test

# Run with debug output
./pqCheck --gen-test -o /tmp/test.pcap
./pqCheck -A /tmp/test.pcap -r /tmp/test.pcap -v 2>&1
```

---

## Building from Git

```bash
git clone https://github.com/aditya-resql/pqcheck.git
cd pqcheck/Payload_res_project

# Install build deps
sudo apt-get install build-essential libpcap-dev libncurses-dev libpq-dev

# Build and install
./install.sh
```

---

## CI/CD Integration

GitHub Actions example:
```yaml
name: Build & Test

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install dependencies
        run: sudo apt-get install -y libpcap-dev libncurses-dev libpq-dev
      - name: Build
        run: cd Payload_res_project && make WITH_TUI=1 WITH_LIBPQ=1
      - name: Test
        run: cd Payload_res_project && make test
      - name: Build packages
        run: |
          cd Payload_res_project
          sudo apt-get install -y ruby-dev
          sudo gem install fpm
          bash packaging/build-deb.sh
```

---

## Support

For build issues:
- Check `make info` for build configuration
- Review Makefile for supported variables
- See docs/QUICK-REF.md for quick command reference
- Run `pqCheck -h` for CLI help
