# Writing rules for config/rules.conf

This document explains the `config/rules.conf` format used by `pqCheck`, shows example rules, and provides practical advice for testing and maintaining rules.

Overview
- The rules file is a simple, line-oriented table. Each non-comment line has four pipe-separated fields:

  name|type|pattern|severity

- Lines beginning with `#` are comments and ignored. Blank lines are allowed.

Fields
- `name` — a short, unique identifier for the rule (used in alerts).
- `type` — either `KEYWORD` or `REGEX`.
  - `KEYWORD` performs a case-insensitive substring search on the query.
  - `REGEX` uses POSIX extended regular expressions compiled with `REG_EXTENDED | REG_ICASE | REG_NEWLINE`.
- `pattern` — the keyword or regex pattern to match.
- `severity` — one of `LOW`, `MEDIUM`, `HIGH`, or `CRITICAL`.

Examples
- Keyword rule (fast, portable):

  UNION_SELECT|KEYWORD|union select|CRITICAL

- Regex rule (more precise):

  STACKED_SELECT|REGEX|;[[:space:]]*select|HIGH

- Function-call detection (match name followed by an opening parenthesis):

  EXEC_DBLINK|REGEX|dblink[[:space:]]*\(|CRITICAL

Notes about regex syntax
- `pqCheck` compiles regexes using the system C library's POSIX ERE implementation. The compile flags are `REG_EXTENDED | REG_ICASE | REG_NEWLINE`:
  - `REG_EXTENDED` enables modern ERE operators such as `+`, `?`, `|`, and `()`.
  - `REG_ICASE` makes matching case-insensitive.
  - `REG_NEWLINE` changes how `^` and `$` behave with newlines.
- Use POSIX character classes where helpful: `[:space:]`, `[:digit:]`, `[:alpha:]`, etc.
- To match a literal `(` in a regex, escape it as `\(` in the rules file (the code ultimately passes the string to `regcomp` so a single backslash in the file becomes the escape for the regex engine). Example:

  lo_export[[:space:]]*\(|CRITICAL

- Avoid using complex nested grouping and alternation where possible, because subtle differences in libc regex implementations can cause portability issues. If you must use alternation, test it on the target systems.

When to prefer `KEYWORD`
- Use `KEYWORD` for short, unambiguous substrings (e.g., `pg_stat_activity`, `pg_read_file`, `%27`). It is fast and portable.
- `KEYWORD` is case-insensitive and matches anywhere in the query after lowercasing.

Testing rules locally
- Quick sanity checks using the shell's `grep -E` can detect obvious regex syntax issues (be careful — shell quoting varies):

```bash
printf "SELECT ' OR 1=1--'\n" | grep -E -e ";[[:space:]]*select" || echo "pattern rejected by grep -E"
```

- The most reliable test is to let `pqCheck` load the rules and report any `regcomp` errors:

```bash
./pqCheck -r results/sqli_classic.pcap -R config/rules.conf -v
```

If `pqCheck` prints lines like:

  [detector] regex compile error in rule 'FOO': Unmatched ( or \(

then the rule's pattern is invalid for the system regex engine; prefer rewriting as smaller regexes or using `KEYWORD`.

Best practices
- Keep rule names short, alphanumeric, and UPPER_CASE.
- Prefer `KEYWORD` when you only need a substring; switch to `REGEX` when you need word boundaries, context, or to avoid false positives.
- Add one rule per line; avoid grouping many alternatives into a single big regex — split into multiple rules (this also helps attribution in alerts).
- Use `[:space:]` instead of `\s` for POSIX compatibility.
- Test rules by running a small set of example inputs through `pqCheck` in either offline (`-r`) or DB-session (`-d`) mode.

Performance considerations
- `KEYWORD` rules are cheaper; `REGEX` rules cost more at runtime. `pqCheck` loads and compiles regexes at startup, so any regex compile-time errors will be reported early.
- If you have many regexes, measure performance on representative traffic and consider promoting frequently-matched patterns to `KEYWORD` where safe.

Troubleshooting
- If a rule doesn't appear to trigger:
  - Check that the pattern matches the actual extracted SQL (use `-v` to print extracted SQL).
  - For `KEYWORD`, ensure there are no leading/trailing whitespace differences.
  - For `REGEX`, test the pattern with `regcomp` via `pqCheck` and adjust escaping.

Common rule examples
- Detect stacked queries (separate rules per verb):

  STACKED_SELECT|REGEX|;[[:space:]]*select|HIGH
  STACKED_DROP|REGEX|;[[:space:]]*drop|HIGH

- Detect `pg_read_file` and other sensitive functions:

  PG_READ_FILE|KEYWORD|pg_read_file|CRITICAL
  EXEC_LO_EXPORT|REGEX|lo_export[[:space:]]*\(|CRITICAL

Reference
- `pqCheck` uses POSIX ERE via `regcomp(3)` and `regexec(3)`
- For more about POSIX regex syntax, see the `regex(7)` or `regcomp(3)` manual on your system.

If you'd like, I can also add a small test harness that asserts all rules compile on this machine and runs a few example inputs — would you like that added to `tests/`?
