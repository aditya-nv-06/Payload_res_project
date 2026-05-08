# Build & Installation Guide

This guide covers building pqCheck from source and packaging it for distribution.

## Prerequisites

### Debian/Ubuntu
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    libpcap-dev \
    libncurses-dev \
    libpq-dev
```

### RHEL/CentOS/Fedora
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

---

## Quick Build & Install

### Option 1: Interactive Installation (Recommended)
```bash
chmod +x install.sh
./install.sh
# Follow prompts for local/system/package installation
```

### Option 2: Local User Install (No Sudo)
```bash
make WITH_TUI=1
make install
# Installs to ~/.local/bin
```

### Option 3: System-wide Installation
```bash
make WITH_TUI=1 WITH_LIBPQ=1
sudo make install PREFIX=/usr/local
```

### Option 4: Full-featured Installation
```bash
# Build with all features
make WITH_TUI=1 WITH_LIBPQ=1

# Install to system
sudo make install PREFIX=/usr/local

# Deploy systemd service (optional)
sudo cp packaging/pqcheck.service /etc/systemd/system/
sudo systemctl enable pqcheck
sudo systemctl start pqcheck
```

---

## Building Packages

### DEB Package (Debian/Ubuntu)

**Prerequisites:**
```bash
sudo apt-get install -y ruby-dev
sudo gem install fpm
```

**Build:**
```bash
chmod +x packaging/build-deb.sh
bash packaging/build-deb.sh
# Creates: pqcheck_1.0.0_amd64.deb
```

**Install:**
```bash
sudo dpkg -i pqcheck_1.0.0_amd64.deb
sudo systemctl start pqcheck
```

### RPM Package (RHEL/CentOS/Fedora)

**Prerequisites:**
```bash
sudo dnf install -y ruby-devel
sudo gem install fpm
```

**Build:**
```bash
chmod +x packaging/build-rpm.sh
bash packaging/build-rpm.sh
# Creates: pqcheck-1.0.0-1.x86_64.rpm
```

**Install:**
```bash
sudo dnf install pqcheck-1.0.0-1.x86_64.rpm
sudo systemctl start pqcheck
```

---

## Build Options

### Basic Build (No Optional Features)
```bash
make
# Output: pqCheck binary with rule-based + anomaly detection
```

### With PostgreSQL Correlation
```bash
make WITH_LIBPQ=1
# Adds libpq support for pg_stat_activity enrichment
```

### With Interactive TUI Dashboard
```bash
make WITH_TUI=1
# Adds ncurses-based TUI with 4 screens
```

### All Features Enabled
```bash
make WITH_TUI=1 WITH_LIBPQ=1
# Full-featured build for production
```

---

## Installation Paths

Default installation follows Linux FHS (Filesystem Hierarchy Standard):

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
