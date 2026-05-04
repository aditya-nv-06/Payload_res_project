#!/usr/bin/env python3
"""
tests/gen_test_traffic.py – Generate synthetic PCAP files for testing
the pgsql_ids sensor (Milestones 1–4, 8).

Requires: scapy  (pip install scapy)

Usage:
  python3 tests/gen_test_traffic.py [--out <dir>]

Outputs (in <dir>, default: tests/pcaps/):
  baseline.pcap        – normal PostgreSQL queries
  sqli_classic.pcap    – classic UNION / OR-based injections
  sqli_fragmented.pcap – injection split across multiple TCP segments
  sqli_obfuscated.pcap – comment/encoding-based obfuscation
"""

import argparse
import os
import struct
import sys

try:
    from scapy.all import (
        Ether, IP, TCP, Raw, wrpcap, RandShort
    )
except ImportError:
    sys.exit("scapy is required: pip install scapy")

# ---------------------------------------------------------------------------
# PostgreSQL wire-protocol helpers
# ---------------------------------------------------------------------------

def pg_startup_message(user: str = "testuser",
                       database: str = "testdb") -> bytes:
    """Build a PostgreSQL StartupMessage (protocol 3.0)."""
    body  = struct.pack(">I", 0x00030000)         # protocol 3.0
    body += b"user\x00"   + user.encode()     + b"\x00"
    body += b"database\x00" + database.encode() + b"\x00"
    body += b"\x00"
    length = struct.pack(">I", 4 + len(body))     # length includes itself
    return length + body


def pg_simple_query(sql: str) -> bytes:
    """Build a PostgreSQL Simple Query ('Q') message."""
    payload = sql.encode() + b"\x00"
    length  = struct.pack(">I", 4 + len(payload))
    return b"Q" + length + payload


def pg_parse_message(sql: str, stmt_name: str = "") -> bytes:
    """Build an Extended Query Parse ('P') message."""
    body  = stmt_name.encode() + b"\x00"
    body += sql.encode()        + b"\x00"
    body += struct.pack(">H", 0)                  # 0 parameter type OIDs
    length = struct.pack(">I", 4 + len(body))
    return b"P" + length + body


# ---------------------------------------------------------------------------
# TCP stream builder
# ---------------------------------------------------------------------------

class TCPStream:
    """Accumulate packets forming a single TCP flow."""

    SRC_IP   = "10.0.0.1"
    DST_IP   = "10.0.0.2"
    DST_PORT = 5432
    SEQ_INIT = 1000

    def __init__(self):
        self.src_port = int(RandShort())
        self.seq      = self.SEQ_INIT
        self.ack      = 0
        self.packets  = []

    def _pkt(self, flags: str, payload: bytes = b"") -> bytes:
        p = (
            Ether() /
            IP(src=self.SRC_IP, dst=self.DST_IP) /
            TCP(sport=self.src_port, dport=self.DST_PORT,
                seq=self.seq, ack=self.ack, flags=flags) /
            Raw(load=payload)
        )
        if payload:
            self.seq += len(payload)
        return p

    def handshake(self):
        # SYN
        self.packets.append(self._pkt("S"))
        self.seq += 1   # SYN consumes one sequence number
        # (SYN-ACK and ACK omitted – one-sided trace is fine for testing)

    def send(self, data: bytes):
        """Send data in one segment."""
        p = self._pkt("PA", data)
        self.packets.append(p)

    def send_fragmented(self, data: bytes, chunk_size: int = 10):
        """Send data split into small segments of chunk_size bytes each."""
        for i in range(0, len(data), chunk_size):
            chunk = data[i:i + chunk_size]
            p = self._pkt("PA", chunk)
            self.packets.append(p)

    def teardown(self):
        self.packets.append(self._pkt("FA"))
        self.seq += 1


def make_stream(queries, fragmented: bool = False, chunk_size: int = 10):
    s = TCPStream()
    s.handshake()
    s.send(pg_startup_message())
    for sql in queries:
        msg = pg_simple_query(sql)
        if fragmented:
            s.send_fragmented(msg, chunk_size)
        else:
            s.send(msg)
    s.teardown()
    return s.packets


# ---------------------------------------------------------------------------
# Query corpora
# ---------------------------------------------------------------------------

BASELINE_QUERIES = [
    "SELECT id, name, email FROM users WHERE id = $1",
    "SELECT count(*) FROM orders WHERE status = 'complete'",
    "INSERT INTO events (ts, msg) VALUES (now(), 'user_login')",
    "UPDATE users SET last_login = now() WHERE id = 42",
    "SELECT u.id, o.total FROM users u JOIN orders o ON u.id = o.user_id",
    "SELECT version()",
    "BEGIN",
    "COMMIT",
    "SELECT pg_catalog.set_config('search_path', '', false)",
]

SQLI_CLASSIC = [
    "' UNION SELECT username, password FROM pg_shadow--",
    "admin' OR 1=1--",
    "1; DROP TABLE users--",
    "' OR '1'='1",
    "1 UNION ALL SELECT NULL, table_name FROM information_schema.tables--",
    "'; SELECT pg_sleep(5)--",
    "1' AND 1=2 UNION SELECT usename,passwd FROM pg_shadow--",
]

SQLI_FRAGMENTED = [
    # Injection spread across packets (reassembly test)
    "' UNION SELECT username, password FROM pg_shadow--",
    "1 UNION ALL SELECT NULL,table_name FROM information_schema.tables--",
]

SQLI_OBFUSCATED = [
    "'/**/UNION/**/SELECT/**/username,password/**/FROM/**/pg_shadow--",
    "%27%20OR%201%3D1--",                  # URL-encoded
    "' OR 0x313d31--",                     # hex-encoded tautology
    "' /*!UNION*/ /*!SELECT*/ 1,2--",      # MySQL version comments (testing)
    "char(39)||' OR '||char(49)||'='||char(49)",
]

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default="tests/pcaps",
                    help="Output directory (default: tests/pcaps)")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)

    configs = [
        ("baseline.pcap",        BASELINE_QUERIES,  False, 40),
        ("sqli_classic.pcap",    SQLI_CLASSIC,      False, 40),
        ("sqli_fragmented.pcap", SQLI_FRAGMENTED,   True,  8),
        ("sqli_obfuscated.pcap", SQLI_OBFUSCATED,   False, 40),
    ]

    for fname, queries, frag, chunk in configs:
        pkts = make_stream(queries, fragmented=frag, chunk_size=chunk)
        out  = os.path.join(args.out, fname)
        wrpcap(out, pkts)
        print(f"  wrote {len(pkts):3d} packets → {out}")

    print(f"\nGenerated {len(configs)} PCAP files in {args.out}/")
    print("Run the sensor against each with:")
    print(f"  pqCheck -r {args.out}/sqli_classic.pcap -R config/rules.conf -v")


if __name__ == "__main__":
    main()
