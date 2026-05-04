# Windows

The current codebase is Linux-oriented, so the documented Windows path is **WSL2**.

## Recommended Setup

1. Install WSL2 with Ubuntu or another Debian-based distro.
2. Install the Linux dependencies inside WSL.
3. Build and run the tool from the WSL shell.

## Install inside WSL

```bash
sudo apt-get install -y build-essential libpcap-dev libpq-dev
```

## Build inside WSL

```bash
make WITH_LIBPQ=1
```

## Run Examples inside WSL

### Offline scan

```bash
pqCheck \
  -r results/sqli_fragmented.pcap \
  -R config/rules.conf \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

### Direct database session

```bash
pqCheck \
  -d "host=localhost dbname=postgres user=postgres" \
  -m baseline.model \
  -o db-alerts.jsonl \
  -v
```

## Notes

- Native Windows builds are not documented for this codebase.
- If you need a Windows-friendly workflow, run the tool in WSL2 or a Linux container.
