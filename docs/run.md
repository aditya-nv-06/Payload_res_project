# Run Guide

This guide is the entry point for running `pqCheck` on each supported platform.

## Platform Pages

- [Linux](platforms/linux.md)
- [Windows](platforms/windows.md)
- [macOS](platforms/macos.md)

## What changes by platform

- Linux is the primary target and is fully supported by the current codebase.
- Windows is documented through WSL2, which gives you a Linux environment on Windows.
- macOS is documented with Homebrew-based dependencies and a native build path that may need local verification.

## Quick Feature Map

- Offline scan: read a PCAP and detect SQLi.
- Live capture: inspect PostgreSQL traffic as it moves across the wire.
- Direct session mode: connect to PostgreSQL and score each SQL statement before execution.
- Correlation mode: enrich passive alerts with `pg_stat_activity`.
- Training mode: build an n-gram model from a corpus.

The `pqCheck` executable is the compiled sensor binary.
If you want to invoke it without a path prefix, place the repository root on your `PATH` or install the launcher somewhere on `PATH`.

## Production usage with PCAPs

In production pipelines you can provide offline PCAP files instead of live capture using `-r <file>`. This is suitable for batch analysis, replaying traffic, or running detection on staged captures. Use `-A <file>` to build an in-memory n-gram model from a PCAP and immediately proceed to detection (for example, when bootstrapping a model from recent traffic).

For a detailed first-time live monitoring flow that automatically captures a short baseline and trains a model in memory before detection, see [auto-baseline.md](auto-baseline.md).

## Custom Network Configuration

For production deployments where PostgreSQL runs on non-standard ports or multiple IPs, see [docs/network-config.md](network-config.md) for configuring port and IP monitoring via a config file. You can specify multiple ports and destination IPs without modifying CLI flags.
