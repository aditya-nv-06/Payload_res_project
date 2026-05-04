# Linux

Linux is the primary and best-tested platform for `pqCheck`.

## Install

```bash
sudo apt-get install -y build-essential libpcap-dev libpq-dev
```

## Build

```bash
make WITH_LIBPQ=1
```

## Run Examples

### Offline scan

```bash
pqCheck \
  -r results/sqli_classic.pcap \
  -R config/rules.conf \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

### Live capture

```bash
sudo pqCheck \
  -i eth0 \
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

### One-shot query execution

```bash
pqCheck \
  -d "host=localhost dbname=postgres user=postgres" \
  -e "SELECT now()" \
  -m baseline.model \
  -o db-alerts.jsonl \
  -v
```
