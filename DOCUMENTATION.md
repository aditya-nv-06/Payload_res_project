# 📚 Complete Documentation Index

This document provides a structured guide to all documentation in the pqCheck project. Follow the paths below based on your goals.

---

## 🎯 Quick Navigation by Role

### 👤 I'm a New User — First Time Here
**Time commitment:** 30 minutes from zero to running detection

1. **[QUICK-REF.md](docs/QUICK-REF.md)** (5 min)
   - 30-second demo that works without setup
   - Command cheat sheet for common operations
   - Three real-world scenarios

2. **[docs/quickstart.md](docs/quickstart.md)** (15 min)
   - Complete hands-on walkthrough
   - Generate test PCAPs
   - Train and detect SQL injection
   - Review results

3. **[docs/QUICK-REF.md](docs/QUICK-REF.md#common-workflows)** (10 min)
   - Review "Common Workflows" section
   - Pick one scenario that matches your use case

✅ **Next:** Move to your role below

---

### 🏗️ I'm Setting Up Installation
**Time commitment:** 20-30 minutes (build to running)

1. **[BUILD.md](BUILD.md)** (20 min) — Follow these steps in order:
   - Step 1: Install dependencies (pick your OS)
   - Step 2: Choose your installation path:
     - 🚀 Path A: Quick test (no installation)
     - 👤 Path B: Local user install
     - 🖥️ Path C: System-wide install
     - 🔧 Path D: Systemd daemon
     - 📦 Path E: Packaged distribution (DEB/RPM)
   - Step 3: Verify installation
   - Step 4: Grant network capabilities (if live capture)

2. **[docs/run.md](docs/run.md)** (10 min)
   - Integration with your platform (Linux/macOS/Windows)
   - Starting the service
   - Configuration management
   - Troubleshooting platform-specific issues

✅ **Next:** Move to your use case below

---

### 🔍 I'm Running pqCheck — Operations & Usage
**Time commitment:** Variable by use case

#### Beginner: Testing & Learning
- **[docs/QUICK-REF.md](docs/QUICK-REF.md)** — Command cheat sheet
- **[docs/quickstart.md](docs/quickstart.md)** — Hands-on examples
- **[docs/Example.md](docs/Example.md)** — Real-world scenarios

#### Intermediate: Custom Detection
- **[docs/rules.md](docs/rules.md)** (15 min) — How to write and tune detection rules
- **[docs/network-config.md](docs/network-config.md)** (10 min) — Custom ports, IPs, output paths
- **[docs/ngram.md](docs/ngram.md)** (15 min) — Anomaly detection model: training, tuning, evaluation

#### Advanced: Live Monitoring
- **[docs/tui.md](docs/tui.md)** (10 min) — Interactive dashboard layout and keyboard controls
- **[docs/pcap-guide.md](docs/pcap-guide.md)** (15 min) — Capture workflows, storage, forensics
- **[docs/cli.md](docs/cli.md)** (10 min) — Full CLI reference with all options

✅ **Next:** Check "I'm Troubleshooting" if you hit issues

---

### 🏭 I'm Setting Up Production Deployment
**Time commitment:** 1-2 hours for complete production setup

1. **[BUILD.md](BUILD.md) — Path D or E** (30 min)
   - System-wide or packaged installation
   - Systemd daemon configuration

2. **[docs/run.md](docs/run.md)** (20 min)
   - Platform integration
   - Security hardening
   - Monitoring setup

3. **[docs/network-config.md](docs/network-config.md)** (15 min)
   - Configure ports, IPs, alert outputs
   - Integrate with SIEM or logging system

4. **[docs/rules.md](docs/rules.md)** (20 min)
   - Tune detection rules for your environment
   - Create custom rules for your PostgreSQL usage patterns

5. **[docs/ngram.md](docs/ngram.md)** (20 min)
   - Train baseline model from your production traffic
   - Evaluate model performance
   - Adjust sensitivity/threshold

6. **[TESTING.md](TESTING.md)** (15 min)
   - Automated test suite verification
   - Load testing procedures
   - Performance benchmarks

✅ **Next:** Review "I'm Troubleshooting" section

---

### 🏗️ I'm a Developer — Understanding the Architecture
**Time commitment:** 1 hour for full understanding

1. **[docs/architecture.md](docs/architecture.md)** (20 min)
   - System design with Mermaid flowchart
   - Module responsibilities and dependencies
   - Data flow through the detection pipeline

2. **[docs/cli.md](docs/cli.md)** (10 min)
   - All entry points and mode selection
   - Argument parsing logic

3. **[src/README.md](src/README.md)** (15 min) — *if available*
   - Module deep-dives
   - Build instructions for developers
   - Contribution guidelines

4. **[CONTRIBUTING.md](CONTRIBUTING.md)** (15 min) — *if available*
   - Code style and conventions
   - Testing requirements
   - Pull request process

✅ **Next:** Explore the code in `src/` with README guide

---

### 🆘 I'm Troubleshooting — Something's Not Working
**Time commitment:** Depends on issue

1. **Check your issue type:**
   - **Build won't compile?** → [BUILD.md — Troubleshooting](BUILD.md#troubleshooting)
   - **Live capture fails?** → [docs/run.md — Permissions](docs/run.md)
   - **Detection not working?** → [docs/rules.md — Tuning](docs/rules.md)
   - **Wrong results?** → [docs/ngram.md — Evaluation](docs/ngram.md)
   - **Dashboard issues?** → [docs/tui.md — Troubleshooting](docs/tui.md)
   - **Config problems?** → [docs/network-config.md](docs/network-config.md)

2. **Still stuck?**
   - Check [TESTING.md](TESTING.md) for known issues
   - Review [docs/Example.md](docs/Example.md) for similar scenarios
   - Run: `pqCheck --help` for immediate command reference

✅ **Next:** Create an issue or ask for support

---

## 📖 Complete Documentation Map

### Foundation Documents (Start here)
| Document | Purpose | Read Time | Audience |
|----------|---------|-----------|----------|
| [README.md](README.md) | Project overview and doc navigation | 5 min | Everyone |
| [DOCUMENTATION.md](DOCUMENTATION.md) | This file — structured learning paths | 10 min | Everyone |

### Getting Started (Beginner)
| Document | Purpose | Read Time | Audience |
|----------|---------|-----------|----------|
| [docs/QUICK-REF.md](docs/QUICK-REF.md) | 30-second start + cheat sheet | 5 min | Everyone |
| [docs/quickstart.md](docs/quickstart.md) | Hands-on walkthrough | 15 min | Beginners |

### Building & Installation
| Document | Purpose | Read Time | Audience |
|----------|---------|-----------|----------|
| [BUILD.md](BUILD.md) | Build from source, all installation paths | 20-30 min | Everyone |
| [docs/run.md](docs/run.md) | Platform integration, security, monitoring | 15 min | Operators |

### Core Operations
| Document | Purpose | Read Time | Audience |
|----------|---------|-----------|----------|
| [docs/cli.md](docs/cli.md) | Full CLI reference | 10 min | Users |
| [docs/quickstart.md](docs/quickstart.md) | Workflow examples | 15 min | Beginners |
| [docs/Example.md](docs/Example.md) | Real-world scenarios | 20 min | Intermediate |

### Configuration & Tuning
| Document | Purpose | Read Time | Audience |
|----------|---------|-----------|----------|
| [docs/network-config.md](docs/network-config.md) | Custom ports, IPs, outputs | 10 min | Operators |
| [docs/rules.md](docs/rules.md) | Writing and tuning rules | 15 min | Advanced |
| [docs/ngram.md](docs/ngram.md) | Anomaly detection tuning | 15 min | Advanced |

### Advanced Features
| Document | Purpose | Read Time | Audience |
|----------|---------|-----------|----------|
| [docs/tui.md](docs/tui.md) | Interactive dashboard | 10 min | Operators |
| [docs/pcap-guide.md](docs/pcap-guide.md) | Capture, storage, forensics | 15 min | Advanced |

### Architecture & Development
| Document | Purpose | Read Time | Audience |
|----------|---------|-----------|----------|
| [docs/architecture.md](docs/architecture.md) | System design with diagrams | 20 min | Developers |
| [TESTING.md](TESTING.md) | Test suite and validation | 15 min | Developers |

---

## 🔄 Learning Paths by Goal

### Path 1: "I Just Want To Try It"
```
QUICK-REF.md (skim first section)
    ↓
Run the 30-second demo
    ✅ Done! Did it work?
```

### Path 2: "I Want To Understand It"
```
README.md (skim)
    ↓
docs/QUICK-REF.md (read all)
    ↓
docs/quickstart.md (careful read)
    ↓
docs/architecture.md (understand flow)
    ✅ Done! You now understand pqCheck
```

### Path 3: "I Want To Install It Locally"
```
B BUILD.md—Path B (follow step by step)
    ↓
Run Step 3: Verify your installation
    ↓
docs/QUICK-REF.md—Scenario 1 (run the same test)
    ✅ Done! Installation complete
```

### Path 4: "I Want To Deploy To Production"
```
BUILD.md—Path D or E (30 min)
    ↓
docs/run.md (platform + security)
    ↓
docs/network-config.md (customize your setup)
    ↓
docs/rules.md (tune for your environment)
    ↓
docs/ngram.md (train baseline model)
    ↓
TESTING.md (validate your setup)
    ✅ Done! Ready for production
```

### Path 5: "I Want To Understand The Code"
```
docs/architecture.md (read entire doc)
    ↓
src/main.c (skim entry point)
    ↓
docs/architecture.md—Modules (match each module to source in src/)
    ↓
TESTING.md (understand test coverage)
    ✅ Done! Ready to contribute
```

---

## 🏃 Quick Commands Reference

**Stuck?** These one-liners work without reading docs:

```bash
# Show 30-second demo (no setup needed)
pqCheck --gen-test -o test.pcap
pqCheck -A test.pcap -r test.pcap -o alerts.jsonl | jq .

# Get help
pqCheck --help

# Check version
pqCheck --version

# Open this documentation in browser (if you have a viewer)
cat README.md
```

---

## 📊 Documentation Statistics

- **Total documents:** 12+
- **Total reading time:** 2-3 hours (all docs)
- **Time to first success:** 5 minutes (QUICK-REF.md)
- **Time to production:** 2-4 hours (depends on environment)

---

## 🎯 Common Questions → Which Doc?

| Question | Document | Section |
|----------|----------|---------|
| How do I install pqCheck? | BUILD.md | Step 2 (choose path) |
| How do I run a quick test? | docs/QUICK-REF.md | "30-Second Start" |
| How do I use it on my system? | docs/run.md | Platform-specific section |
| How do I write custom rules? | docs/rules.md | Full document |
| What's the anomaly detection? | docs/ngram.md | "Overview" section |
| How do I monitor live traffic? | docs/tui.md | Full document |
| What commands are available? | docs/cli.md | Full reference |
| How does it work internally? | docs/architecture.md | Full document |
| Can I run it in production? | BUILD.md Path D or E + docs/run.md | Daemon setup |
| What are the test cases? | TESTING.md | Full document |
| I'm getting errors | BUILD.md | "Troubleshooting" |
| I want to contribute | CONTRIBUTING.md or ask | Issues/PRs |

---

## ✅ You're Ready!

Pick your role from "Quick Navigation by Role" above and start with the first document. Each document links to the next logical step.

**🚀 Fastest path (5 min):** docs/QUICK-REF.md → Run demo → Done!

**📚 Full understanding (2 hours):** Follow any "Learning Path" above

**🆘 Something's wrong?** Jump to "I'm Troubleshooting" section→
