# macOS

macOS can use the same CLI workflow, but you need Homebrew dependencies and a local build environment.

## Install Dependencies

```bash
brew install libpcap libpq
```

## Build

```bash
make WITH_LIBPQ=1
```

## Run Examples

### Offline scan

```bash
pqCheck \
  -r results/sqli_obfuscated.pcap \
  -R config/rules.conf \
  -m baseline.model \
  -o alerts.jsonl \
  -v
```

### Live capture

```bash
sudo pqCheck \
  -i en0 \
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

- If your local compiler or libpcap/libpq headers differ, use the Linux build in a container or VM.
- Use your active interface name instead of `en0` if it differs on your machine.
