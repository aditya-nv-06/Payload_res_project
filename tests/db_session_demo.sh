#!/usr/bin/env bash

set -euo pipefail

# Demo: run a few example queries through pqCheck's libpq-backed session
# Writes alerts to `db-alerts.jsonl` in the repo root.

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PQ="${SCRIPT_DIR}/../pqCheck"

echo "Running DB-session demo using ${PQ}"

printf "SELECT ' UNION SELECT username, password FROM pg_shadow--';\nSELECT ' OR 1=1--';\nSELECT '1; DROP TABLE users--';\n\\disconnect\n" \
  | "$PQ" -d "host=localhost dbname=postgres user=postgres" -m baseline.model -o db-alerts.jsonl -v

echo "Wrote alerts to db-alerts.jsonl"
