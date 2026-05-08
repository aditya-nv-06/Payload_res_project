#!/usr/bin/env python3
"""
Simple PCAP generator for pqCheck testing.
Generates synthetic PostgreSQL traffic with optional SQL injection payloads.
"""

import socket
import struct
import time
from datetime import datetime

# PostgreSQL protocol packets
POSTGRES_STARTUP = b'\x00\x00\x00\x00\x03\x00\x00\x00user\x00postgres\x00database\x00postgres\x00\x00'
POSTGRES_AUTH = b'R\x00\x00\x00\x08\x00\x00\x00\x00'
POSTGRES_READY = b'Z\x00\x00\x00\x05I'

class SimplePcapWriter:
    """Minimal PCAP writer for testing."""
    
    PCAP_MAGIC = 0xa1b2c3d4
    PCAP_VERSION_MAJOR = 2
    PCAP_VERSION_MINOR = 4
    
    def __init__(self, filename):
        self.f = open(filename, 'wb')
        self.packet_count = 0
        self.write_global_header()
    
    def write_global_header(self):
        """Write PCAP global header."""
        header = struct.pack('<IHHIIII',
            self.PCAP_MAGIC,
            self.PCAP_VERSION_MAJOR,
            self.PCAP_VERSION_MINOR,
            0,  # timezone
            0,  # timestamp accuracy
            65535,  # snaplen
            1   # Ethernet
        )
        self.f.write(header)
    
    def write_packet(self, payload, timestamp=None):
        """Write a packet to PCAP."""
        if timestamp is None:
            timestamp = time.time()
        
        ts_sec = int(timestamp)
        ts_usec = int((timestamp - ts_sec) * 1000000)
        
        # Simple Ethernet + IPv4 + TCP wrapper (minimal)
        # For testing, just the payload is what matters
        packet_data = payload
        
        # Packet header
        header = struct.pack('<IIII',
            ts_sec,
            ts_usec,
            len(packet_data),  # included length
            len(packet_data)   # original length
        )
        
        self.f.write(header + packet_data)
        self.packet_count += 1
    
    def close(self):
        self.f.close()

def generate_sql_query(is_injection=False):
    """Generate a SQL query (clean or malicious)."""
    
    if is_injection:
        # SQL injection payloads
        payloads = [
            "' OR '1'='1",
            "'; DROP TABLE users; --",
            "1' UNION SELECT * FROM users --",
            "admin' --",
            "' OR 1=1 --",
            "1' OR '1'='1",
        ]
        injection = __import__('random').choice(payloads)
        return f"SELECT * FROM users WHERE id = {injection}"
    else:
        # Clean queries
        queries = [
            "SELECT id, username, email FROM users WHERE id = 1",
            "INSERT INTO logs (timestamp, message) VALUES (NOW(), 'login')",
            "UPDATE user_profiles SET last_seen = NOW() WHERE user_id = 42",
            "SELECT COUNT(*) FROM transactions WHERE status = 'completed'",
            "DELETE FROM temp_cache WHERE created_at < NOW() - INTERVAL 1 DAY",
            "SELECT * FROM orders WHERE customer_id = 123 LIMIT 10",
        ]
        return __import__('random').choice(queries)

def generate_postgres_query_packet(query):
    """Generate a PostgreSQL protocol query packet."""
    # PostgreSQL message: 'Q' (simple query) + length + null-terminated query
    query_bytes = query.encode('utf-8') + b'\x00'
    length = struct.pack('>I', len(query_bytes) + 4)  # +4 for length itself
    return b'Q' + length + query_bytes

def main():
    import argparse
    import sys
    
    parser = argparse.ArgumentParser(
        description='Generate test PCAP files for pqCheck'
    )
    parser.add_argument(
        '-o', '--output',
        default='test_traffic.pcap',
        help='Output PCAP filename (default: test_traffic.pcap)'
    )
    parser.add_argument(
        '-c', '--clean',
        type=int,
        default=50,
        help='Number of clean queries (default: 50)'
    )
    parser.add_argument(
        '-i', '--injection',
        type=int,
        default=50,
        help='Number of SQL injection queries (default: 50)'
    )
    parser.add_argument(
        '-s', '--seed',
        type=int,
        help='Random seed for reproducibility'
    )
    
    args = parser.parse_args()
    
    if args.seed is not None:
        __import__('random').seed(args.seed)
    
    print(f"[*] Generating {args.clean} clean queries + {args.injection} SQL injection queries")
    print(f"[*] Output: {args.output}")
    
    writer = SimplePcapWriter(args.output)
    
    start_time = time.time()
    packet_count = 0
    
    # Write clean queries
    for i in range(args.clean):
        query = generate_sql_query(is_injection=False)
        packet = generate_postgres_query_packet(query)
        writer.write_packet(packet, start_time + (i * 0.1))
        packet_count += 1
        if (i + 1) % 10 == 0:
            print(f"  [+] Generated {i+1}/{args.clean} clean queries")
    
    # Write injection queries
    for i in range(args.injection):
        query = generate_sql_query(is_injection=True)
        packet = generate_postgres_query_packet(query)
        writer.write_packet(packet, start_time + (args.clean * 0.1) + (i * 0.1))
        packet_count += 1
        if (i + 1) % 10 == 0:
            print(f"  [+] Generated {i+1}/{args.injection} injection queries")
    
    writer.close()
    
    # Print summary
    import os
    file_size = os.path.getsize(args.output)
    print(f"\n[✓] Success!")
    print(f"    Packets: {packet_count}")
    print(f"    File size: {file_size} bytes")
    print(f"    Path: {args.output}")
    print(f"\nNext steps:")
    print(f"  1. Train model: pqCheck -A {args.output} -m test.model")
    print(f"  2. Run detection: pqCheck -r {args.output} -m test.model -o alerts.jsonl")
    print(f"  3. View TUI: pqCheck --tui -A {args.output} -m test.model")

if __name__ == '__main__':
    main()
