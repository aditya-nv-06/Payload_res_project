#!/usr/bin/env bash
set -euo pipefail

CC=${CC:-gcc}
OUT=tests/rules_compile_test

echo "Compiling rules_compile_test.c"
$CC -std=c11 -Wall -Wextra -O2 -o "$OUT" tests/rules_compile_test.c
echo "Running rules compilation test against config/rules.conf"
"$OUT" config/rules.conf
echo "Done"
