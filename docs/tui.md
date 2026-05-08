# TUI Mode Guide

The Text User Interface (TUI) mode provides an interactive dashboard for real-time monitoring of SQL injection detection. This is useful for live packet analysis and interactive incident response.

## Building with TUI Support

TUI mode requires ncurses. Build with:

```bash
# Install ncurses dev library (Debian/Ubuntu)
sudo apt-get install -y libncurses-dev

# Build pqCheck with TUI support
make clean && make WITH_TUI=1
```

## Starting TUI Mode

```bash
# Monitor a live interface
sudo pqCheck --tui -i eth0 -R config/rules.conf -m baseline.model

# Analyze a PCAP file interactively
pqCheck --tui -r capture.pcap -R config/rules.conf -m baseline.model

# TUI with custom network config
pqCheck --tui -r capture.pcap -N config/network.conf -R config/rules.conf
```

## TUI Screens

### Dashboard (default)

Displays real-time statistics:
- **Status** – Capture active/idle, rules loaded, n-gram model status
- **Metrics** – Total queries, total alerts, rule detections, anomaly detections
- **Uptime** – How long the monitoring session has been running

Press `[d]` to return to the dashboard from any screen.

### Alert Log

Scrollable list of detected SQL injection attempts with:
- **Flow ID** – 8-character flow identifier (5-tuple hash)
- **Source IP:Port** – Client address
- **Dest IP:Port** – PostgreSQL server address
- **Risk Level** – CRITICAL (red), HIGH (yellow), MEDIUM (cyan), LOW (green)
- **Anomaly Score** – N-gram scoring result

**Navigation:**
- `[↑]` / `[↓]` – Scroll through alerts
- `[a]` – Switch to Alert Log

### Configuration

Displays the current settings:
- Rules file path
- Model file path
- Network config path
- Anomaly threshold
- Alert output path

Press `[c]` to view the current configuration.

### Help

Shows keyboard shortcuts and general information.

Press `[h]` to view the help screen.

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `[d]` | Go to Dashboard |
| `[a]` | Go to Alert Log |
| `[c]` | Go to Configuration |
| `[h]` | Show Help |
| `[↑]` | Scroll up (in Alert Log) |
| `[↓]` | Scroll down (in Alert Log) |
| `[q]` | Quit pqCheck |
| `[Ctrl+C]` | Emergency exit |

## Color Coding

Alert risk levels are color-coded for quick visual identification:

- **RED** (bold) – CRITICAL risk
- **YELLOW** – HIGH risk
- **CYAN** – MEDIUM risk
- **GREEN** – LOW risk

## Tips

### Maximizing Terminal Window

TUI adapts to your terminal size. For best experience, use a terminal that is at least **80 columns wide** and **24 rows tall**.

```bash
# Check terminal size
stty size

# Use fullscreen terminal for better readability
```

### Real-Time Monitoring

TUI is designed for live capture (`-i <iface>`). You can also analyze PCAPs with `--tui -r <file>`, but for production monitoring, combine with:

```bash
sudo pqCheck --tui -i eth0 -N config/network.conf -m baseline.model -o /var/log/pqcheck/alerts.jsonl
```

### Combining with Logging

TUI mode still writes alerts to the JSON Lines file specified by `-o`. Monitor logs independently:

```bash
# In another terminal
tail -f alerts.jsonl | jq .
```

## Troubleshooting

**"TUI mode requires ncurses support"**

When you see this error, rebuild with ncurses:
```bash
make clean && make WITH_TUI=1
```

**Terminal display issues**

- Resize your terminal to be larger
- Check that your terminal supports color (`echo $TERM`)
- Try a different terminal emulator (xterm, gnome-terminal, etc.)

## Limitations

- TUI mode is currently **read-only** – you cannot modify settings from the UI
- Real-time updates are refreshed every 100ms – rapid alert generation may be cached briefly
- The alert history buffer holds the last 256 alerts
