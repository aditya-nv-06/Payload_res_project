# Network Configuration Guide

The network config file allows you to customize which PostgreSQL ports and destination IPs to monitor without modifying CLI arguments. This is especially useful in production environments with non-standard port configurations or multiple database servers.

## Configuration File Format

Location: `config/network.conf` (or specify with `-N <config>`)

Format:
```
# Comments start with #
key=value
```

Supported keys:
- `ports` – comma-separated list of TCP ports to monitor (required)
- `ips` – comma-separated list of destination IPs to monitor (optional)
- `output_path` – override the default alert output location (optional)

## Examples

### Single Port, All IPs
```
ports=5432
```

### Multiple Ports, All IPs
```
ports=5432,5433,5434
```

### Multiple Ports and Specific IPs
```
ports=5432,5433
ips=10.0.0.5,10.0.0.6,192.168.1.100
```

### With Custom Output Path
```
ports=5432,5433
ips=10.0.0.5,10.0.0.6
output_path=/var/log/pqcheck/alerts.jsonl
```

## Usage

```bash
# Use default network.conf in config/
pqCheck -r capture.pcap -N config/network.conf -R config/rules.conf -v

# Use a custom config path
pqCheck -r capture.pcap -N /etc/pqcheck/prod-network.conf -R config/rules.conf -v
```

## How It Works

When `-N` is provided, pqCheck:
1. Parses the network config file
2. Generates a custom BPF (Berkeley Packet Filter) expression based on the configured ports and IPs
3. Uses that filter instead of the default hardcoded "tcp port 5432"
4. If `output_path` is specified, overrides the `-o` flag

### Generated BPF Examples

**Input:**
```
ports=5432
```
**Output BPF:** `tcp port 5432`

**Input:**
```
ports=5432,5433,5434
```
**Output BPF:** `(tcp port 5432 or tcp port 5433 or tcp port 5434)`

**Input:**
```
ports=5432,5433
ips=10.0.0.5,10.0.0.6
```
**Output BPF:** `(dst net 10.0.0.5 or dst net 10.0.0.6) and (tcp port 5432 or tcp port 5433)`

## Production Deployment

Recommended directory structure:
```
/etc/pqcheck/
  ├── network.conf          # Main config
  ├── network.conf.primary  # Primary DB server config
  ├── network.conf.replicas # Read replica config
  ├── rules.conf
  └── baseline.model
```

Example systemd service integration:
```
[Service]
ExecStart=/usr/local/bin/pqCheck -i eth0 -N /etc/pqcheck/network.conf \
  -R /etc/pqcheck/rules.conf \
  -m /etc/pqcheck/baseline.model \
  -o /var/log/pqcheck/alerts.jsonl \
  -v
```

## Troubleshooting

**Invalid BPF filter error:**
- Ensure port numbers are between 1 and 65535
- Verify IPs are valid IPv4 addresses (no CIDR notation)

**Output path not being used:**
- Check that the directory exists and is writable
- Verify the config file is being loaded (check stderr with `-v`)

**No packets captured:**
- Verify the ports and IPs in the config match your actual PostgreSQL deployment
- Test with a single, known port first
