#!/usr/bin/env python3
"""
pqcheck_audit.py

Static and optional live PostgreSQL security audit CLI for mixed C/Python/Shell repos.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import socket
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, List, Optional


TEXT_EXTENSIONS = {
    ".c",
    ".h",
    ".py",
    ".sh",
    ".mk",
    ".sql",
    ".conf",
}

SKIP_DIRS = {
    ".git",
    "venv",
    ".venv",
    "node_modules",
    "build",
    "dist",
    "__pycache__",
}


@dataclass
class Finding:
    severity: str
    category: str
    title: str
    file: str
    line: int
    snippet: str
    recommendation: str
    reference: str


@dataclass
class RuntimeCheck:
    name: str
    status: str
    details: str
    recommendation: str


UNSAFE_PY_EXECUTE_RE = re.compile(
    r"execute\s*\(\s*(f[\"']|[\"'][^\"']*\{[^\"']*\}[\"']|[^\n]*\.format\s*\()",
    re.IGNORECASE,
)
UNSAFE_C_SQL_BUILD_RE = re.compile(
    r"\b(snprintf|sprintf|strcat)\s*\([^\n;]*\"[^\"]*(SELECT|INSERT|UPDATE|DELETE|COPY|ALTER|DROP)",
    re.IGNORECASE,
)
PQEXEC_RE = re.compile(r"\bPQexec\s*\(", re.IGNORECASE)
PQEXEC_PARAMS_RE = re.compile(r"\b(PQexecParams|PQprepare|PQexecPrepared)\b", re.IGNORECASE)
CONNSTR_RE = re.compile(r"(host=[^\"'\n]+dbname=[^\"'\n]+)", re.IGNORECASE)
DANGEROUS_SQL_RE = re.compile(
    r"\b(COPY\s+.*\s+PROGRAM|CREATE\s+EXTENSION\s+(file_fdw|dblink)|ALTER\s+ROLE\s+\w+\s+SUPERUSER|CREATE\s+ROLE\s+\w+\s+SUPERUSER)\b",
    re.IGNORECASE,
)
LEAST_PRIV_RE = re.compile(r"\bGRANT\s+ALL\s+PRIVILEGES\b", re.IGNORECASE)
NETWORK_EXPOSE_RE = re.compile(
    r"(0\.0\.0\.0/0|listen_addresses\s*=\s*'?\*'?|-p\s*5432:5432)",
    re.IGNORECASE,
)


def is_text_candidate(path: Path, include_docs: bool = False) -> bool:
    if path.name == "Makefile":
        return True
    if include_docs and path.suffix.lower() in {".md", ".txt"}:
        return True
    return path.suffix.lower() in TEXT_EXTENSIONS


def read_text(path: Path) -> Optional[str]:
    try:
        return path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return None


def iter_files(root: Path, include_docs: bool = False) -> Iterable[Path]:
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for filename in filenames:
            p = Path(dirpath) / filename
            if is_text_candidate(p, include_docs=include_docs):
                yield p


def line_number_at(text: str, index: int) -> int:
    return text.count("\n", 0, index) + 1


def line_snippet(text: str, line_no: int) -> str:
    lines = text.splitlines()
    if line_no <= 0 or line_no > len(lines):
        return ""
    return lines[line_no - 1].strip()[:220]


def add_finding(
    findings: List[Finding],
    severity: str,
    category: str,
    title: str,
    path: Path,
    line: int,
    snippet: str,
    recommendation: str,
    reference: str,
) -> None:
    findings.append(
        Finding(
            severity=severity,
            category=category,
            title=title,
            file=str(path),
            line=line,
            snippet=snippet,
            recommendation=recommendation,
            reference=reference,
        )
    )


def scan_file(path: Path) -> List[Finding]:
    findings: List[Finding] = []
    text = read_text(path)
    if not text:
        return findings

    for m in UNSAFE_PY_EXECUTE_RE.finditer(text):
        line = line_number_at(text, m.start())
        add_finding(
            findings,
            "HIGH",
            "sql-injection",
            "Potential non-parameterized Python SQL execution",
            path,
            line,
            line_snippet(text, line),
            "Use parameterized queries (execute(sql, params)) and avoid f-strings/.format for SQL.",
            "https://www.postgresql.org/docs/current/libpq-exec.html",
        )

    for m in UNSAFE_C_SQL_BUILD_RE.finditer(text):
        line = line_number_at(text, m.start())
        add_finding(
            findings,
            "HIGH",
            "sql-injection",
            "Potential SQL string construction in C",
            path,
            line,
            line_snippet(text, line),
            "Avoid string-building SQL with sprintf/snprintf; prefer PQexecParams or prepared statements.",
            "https://www.postgresql.org/docs/current/libpq-exec.html",
        )

    if PQEXEC_RE.search(text) and not PQEXEC_PARAMS_RE.search(text):
        m = PQEXEC_RE.search(text)
        if m:
            line = line_number_at(text, m.start())
            add_finding(
                findings,
                "MEDIUM",
                "query-safety",
                "PQexec used without parameterized APIs in file",
                path,
                line,
                line_snippet(text, line),
                "Review this file for dynamic SQL; use PQexecParams/PQprepare/PQexecPrepared where inputs are external.",
                "https://www.postgresql.org/docs/current/libpq-exec.html",
            )

    for m in CONNSTR_RE.finditer(text):
        cs = m.group(1)
        if "sslmode=" not in cs.lower():
            line = line_number_at(text, m.start())
            add_finding(
                findings,
                "MEDIUM",
                "transport-security",
                "Connection string missing sslmode",
                path,
                line,
                line_snippet(text, line),
                "Set sslmode=require (or verify-full in production with certificates).",
                "https://www.postgresql.org/docs/current/libpq-ssl.html",
            )

    for m in DANGEROUS_SQL_RE.finditer(text):
        line = line_number_at(text, m.start())
        add_finding(
            findings,
            "HIGH",
            "privilege-escalation",
            "Potentially dangerous PostgreSQL command",
            path,
            line,
            line_snippet(text, line),
            "Restrict superuser/extension/program-copy usage and gate with admin-only workflows.",
            "https://www.postgresql.org/docs/current/sql-copy.html",
        )

    for m in LEAST_PRIV_RE.finditer(text):
        line = line_number_at(text, m.start())
        add_finding(
            findings,
            "MEDIUM",
            "least-privilege",
            "Broad privilege grant detected",
            path,
            line,
            line_snippet(text, line),
            "Prefer role-specific privileges and minimum grants required by application operations.",
            "https://www.postgresql.org/docs/current/sql-grant.html",
        )

    for m in NETWORK_EXPOSE_RE.finditer(text):
        line = line_number_at(text, m.start())
        add_finding(
            findings,
            "MEDIUM",
            "network-exposure",
            "Potential broad PostgreSQL network exposure",
            path,
            line,
            line_snippet(text, line),
            "Restrict listen addresses/CIDR ranges and firewall PostgreSQL to trusted sources only.",
            "https://www.postgresql.org/docs/current/runtime-config-connection.html",
        )

    return findings


def run_psql_query(connstr: str, query: str) -> RuntimeCheck:
    cmd = ["psql", connstr, "-At", "-c", query]
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=10,
            check=False,
        )
    except FileNotFoundError:
        return RuntimeCheck(
            name=f"psql query: {query}",
            status="SKIP",
            details="psql not found in PATH",
            recommendation="Install PostgreSQL client tools to enable live DB checks.",
        )
    except subprocess.TimeoutExpired:
        return RuntimeCheck(
            name=f"psql query: {query}",
            status="FAIL",
            details="Timed out while executing query",
            recommendation="Check connectivity/firewall and retry.",
        )

    if result.returncode != 0:
        err = (result.stderr or "unknown error").strip()
        return RuntimeCheck(
            name=f"psql query: {query}",
            status="FAIL",
            details=err,
            recommendation="Verify credentials, DB reachability, and SSL settings.",
        )

    return RuntimeCheck(
        name=f"psql query: {query}",
        status="PASS",
        details=(result.stdout or "").strip(),
        recommendation="No action.",
    )


def check_port_exposure(host: str, port: int, timeout: float = 2.0) -> RuntimeCheck:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect((host, port))
        status = "WARN"
        details = f"TCP {host}:{port} is reachable"
        recommendation = "Confirm this endpoint is firewall-restricted and not exposed to untrusted networks."
    except OSError as exc:
        status = "PASS"
        details = f"TCP {host}:{port} not reachable ({exc})"
        recommendation = "No action if this host is expected to be closed from current network."
    finally:
        sock.close()

    return RuntimeCheck(
        name=f"network port reachability ({host}:{port})",
        status=status,
        details=details,
        recommendation=recommendation,
    )


def summarize(findings: List[Finding]) -> dict:
    counts = {"HIGH": 0, "MEDIUM": 0, "LOW": 0}
    for f in findings:
        counts[f.severity] = counts.get(f.severity, 0) + 1
    return counts


def print_report(findings: List[Finding], checks: List[RuntimeCheck]) -> None:
    counts = summarize(findings)
    print("== pqCheck Security Audit Report ==")
    print(f"Findings: HIGH={counts.get('HIGH', 0)} MEDIUM={counts.get('MEDIUM', 0)} LOW={counts.get('LOW', 0)}")
    print("")

    if findings:
        for i, f in enumerate(findings, start=1):
            print(f"[{i}] {f.severity} | {f.category} | {f.title}")
            print(f"    File: {f.file}:{f.line}")
            print(f"    Snippet: {f.snippet}")
            print(f"    Recommendation: {f.recommendation}")
            print(f"    Reference: {f.reference}")
            print("")
    else:
        print("No static findings detected.")
        print("")

    if checks:
        print("Runtime checks:")
        for c in checks:
            print(f"- {c.name}: {c.status}")
            print(f"  details: {c.details}")
            print(f"  recommendation: {c.recommendation}")
        print("")


def main() -> int:
    parser = argparse.ArgumentParser(description="Security audit CLI for PostgreSQL-related code and configuration.")
    parser.add_argument("--root", default=".", help="Repository root to scan (default: current directory)")
    parser.add_argument("--connstr", default=None, help="Optional libpq connection string for live checks")
    parser.add_argument("--check-port", default=None, help="Optional host:port to test network reachability (e.g. 127.0.0.1:5432)")
    parser.add_argument("--json-out", default=None, help="Write full report as JSON to this file")
    parser.add_argument("--include-docs", action="store_true", help="Also scan markdown/text docs (may increase noise)")

    args = parser.parse_args()
    root = Path(args.root).resolve()

    findings: List[Finding] = []
    for p in iter_files(root, include_docs=args.include_docs):
        findings.extend(scan_file(p))

    checks: List[RuntimeCheck] = []

    if args.connstr:
        checks.append(run_psql_query(args.connstr, "SHOW server_version;"))
        checks.append(run_psql_query(args.connstr, "SHOW ssl;"))
        checks.append(run_psql_query(args.connstr, "SELECT current_user;"))
        checks.append(
            run_psql_query(
                args.connstr,
                "SELECT usesuper FROM pg_user WHERE usename = current_user;",
            )
        )

    if args.check_port:
        try:
            host, port_text = args.check_port.rsplit(":", 1)
            checks.append(check_port_exposure(host, int(port_text)))
        except ValueError:
            print("Invalid --check-port value. Expected host:port", file=sys.stderr)
            return 2

    print_report(findings, checks)

    if args.json_out:
        payload = {
            "summary": summarize(findings),
            "findings": [asdict(f) for f in findings],
            "runtime_checks": [asdict(c) for c in checks],
        }
        out = Path(args.json_out)
        out.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        print(f"JSON report written to: {out}")

    return 1 if any(f.severity == "HIGH" for f in findings) else 0


if __name__ == "__main__":
    raise SystemExit(main())
