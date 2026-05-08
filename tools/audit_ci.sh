#!/usr/bin/env bash
# Simple CI helper: run pqCheck system audit and fail if HIGH or MEDIUM findings > 0
set -euo pipefail

PG_CONNSTR=${PG_CONNSTR:-}
THRESH_MEDIUM=${THRESH_MEDIUM:-0}
THRESH_HIGH=${THRESH_HIGH:-0}

if [ -z "$PG_CONNSTR" ]; then
  echo "[audit-ci] PG_CONNSTR not set; skipping system-level audit"
  exit 0
fi

OUT=$(mktemp /tmp/pqcheck_audit.XXXXXX)
trap 'rm -f "$OUT"' EXIT

echo "[audit-ci] Running pqCheck system audit..."
./pqCheck --audit -c "$PG_CONNSTR" --audit-json "$OUT" > /tmp/pqcheck_audit.stdout 2>&1 || true

# Extract counts from stdout (Findings: HIGH=x MEDIUM=y LOW=z)
LINE=$(sed -n '1,120p' /tmp/pqcheck_audit.stdout | grep -E "Findings: HIGH=" || true)
if [ -z "$LINE" ]; then
  echo "[audit-ci] No findings summary produced. See /tmp/pqcheck_audit.stdout"
  cat /tmp/pqcheck_audit.stdout
  exit 1
fi

# Parse numbers
HIGH=$(echo "$LINE" | sed -n 's/.*HIGH=\([0-9]*\).*/\1/p')
MEDIUM=$(echo "$LINE" | sed -n 's/.*MEDIUM=\([0-9]*\).*/\1/p')
LOW=$(echo "$LINE" | sed -n 's/.*LOW=\([0-9]*\).*/\1/p')

echo "[audit-ci] Findings summary: HIGH=$HIGH MEDIUM=$MEDIUM LOW=$LOW"

if [ "$HIGH" -gt "$THRESH_HIGH" ] || [ "$MEDIUM" -gt "$THRESH_MEDIUM" ]; then
  echo "[audit-ci] Audit failed: HIGH>$THRESH_HIGH or MEDIUM>$THRESH_MEDIUM"
  exit 2
fi

echo "[audit-ci] Audit OK"
exit 0
