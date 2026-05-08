#include "analysis/audit.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef WITH_LIBPQ
#include <libpq-fe.h>
#endif

static int has_suffix(const char *name, const char *suffix)
{
    size_t nlen = strlen(name), slen = strlen(suffix);
    if (nlen < slen) return 0;
    return strcmp(name + (nlen - slen), suffix) == 0;
}

static int should_skip_dir(const char *name)
{
    return strcmp(name, ".git") == 0 ||
           strcmp(name, "venv") == 0 ||
           strcmp(name, ".venv") == 0 ||
           strcmp(name, "node_modules") == 0 ||
           strcmp(name, "dist") == 0 ||
           strcmp(name, "build") == 0 ||
           strcmp(name, "__pycache__") == 0;
}

static int is_scan_file(const char *name, int include_docs)
{
    if (strcmp(name, "Makefile") == 0) return 1;
    if (has_suffix(name, ".c") || has_suffix(name, ".h") ||
        has_suffix(name, ".py") || has_suffix(name, ".sh") ||
        has_suffix(name, ".mk") || has_suffix(name, ".sql") ||
        has_suffix(name, ".conf")) return 1;
    if (include_docs && (has_suffix(name, ".md") || has_suffix(name, ".txt")))
        return 1;
    return 0;
}

static int should_skip_file_path(const char *path)
{
    if (!path) return 0;
    return strstr(path, "/src/analysis/audit.c") != NULL ||
           strstr(path, "/tools/pqcheck_audit.py") != NULL ||
           strstr(path, "/results/native-audit.json") != NULL;
}

static void add_finding(audit_report_t *report,
                        const char *severity,
                        const char *category,
                        const char *title,
                        const char *file,
                        int line,
                        const char *snippet,
                        const char *recommendation)
{
    if (!report) return;

    if (strcmp(severity, "HIGH") == 0) report->high_count++;
    else if (strcmp(severity, "MEDIUM") == 0) report->medium_count++;
    else report->low_count++;

    if (report->total_findings >= AUDIT_MAX_FINDINGS) {
        report->truncated = 1;
        return;
    }

    audit_finding_t *f = &report->findings[report->total_findings++];
    memset(f, 0, sizeof(*f));
    snprintf(f->severity, sizeof(f->severity), "%s", severity);
    snprintf(f->category, sizeof(f->category), "%s", category);
    snprintf(f->title, sizeof(f->title), "%s", title);
    snprintf(f->file, sizeof(f->file), "%s", file);
    f->line = line;
    snprintf(f->snippet, sizeof(f->snippet), "%s", snippet ? snippet : "");
    snprintf(f->recommendation, sizeof(f->recommendation), "%s", recommendation);
}

static int line_has_any(const char *line, const char *const *tokens, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (strstr(line, tokens[i])) return 1;
    }
    return 0;
}

static void rstrip_newline(char *line)
{
    if (!line) return;
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
        line[n - 1] = '\0';
        n--;
    }
}

static void scan_file(const char *path, audit_report_t *report)
{
    if (should_skip_file_path(path)) return;

    FILE *fp = fopen(path, "r");
    if (!fp) return;

    report->total_files_scanned++;

    char line[2048];
    int line_no = 0;
    int found_pqexec = 0;
    int found_pqexecparams = 0;
    int pqexec_line = 0;
    int prev_line_audit_ok = 0;

    const char *sql_tokens[] = {"SELECT", "INSERT", "UPDATE", "DELETE", "COPY", "ALTER", "DROP"};

    while (fgets(line, sizeof(line), fp)) {
        line_no++;
        rstrip_newline(line);

        int line_audit_ok = strstr(line, "AUDIT_OK") != NULL;

        if (strstr(line, "PQexec(") && !(line_audit_ok || prev_line_audit_ok)) {
            found_pqexec = 1;
            if (!pqexec_line) pqexec_line = line_no;
        }
        if (strstr(line, "PQexecParams") || strstr(line, "PQprepare") || strstr(line, "PQexecPrepared")) {
            found_pqexecparams = 1;
        }

        if (strstr(line, "execute(") &&
            (strstr(line, "f\"") || strstr(line, "f\'") || strstr(line, ".format("))) {
            add_finding(report,
                        "HIGH",
                        "sql-injection",
                        "Potential non-parameterized Python SQL execution",
                        path,
                        line_no,
                        line,
                        "Use execute(sql, params) and avoid f-strings/.format for SQL text.");
        }

        if ((strstr(line, "sprintf(") || strstr(line, "snprintf(") || strstr(line, "strcat(")) &&
            line_has_any(line, sql_tokens, sizeof(sql_tokens) / sizeof(sql_tokens[0]))) {
            add_finding(report,
                        "HIGH",
                        "sql-injection",
                        "Potential SQL string construction in C",
                        path,
                        line_no,
                        line,
                        "Use PQexecParams or prepared statements instead of formatting SQL strings.");
        }

        if (strstr(line, "host=") && strstr(line, "dbname=") && !strstr(line, "sslmode=")) {
            add_finding(report,
                        "MEDIUM",
                        "transport-security",
                        "Connection string missing sslmode",
                        path,
                        line_no,
                        line,
                        "Set sslmode=require (or verify-full with CA and hostname validation)." );
        }

        if (strstr(line, "COPY") && strstr(line, "PROGRAM")) {
            add_finding(report,
                        "HIGH",
                        "privilege-escalation",
                        "COPY ... PROGRAM usage detected",
                        path,
                        line_no,
                        line,
                        "Restrict this operation to trusted admin workflows only.");
        }

        if ((strstr(line, "CREATE ROLE") || strstr(line, "ALTER ROLE")) && strstr(line, "SUPERUSER")) {
            add_finding(report,
                        "HIGH",
                        "privilege-escalation",
                        "Superuser role assignment pattern detected",
                        path,
                        line_no,
                        line,
                        "Avoid superuser for application roles; use least privilege grants.");
        }

        if (strstr(line, "GRANT ALL PRIVILEGES")) {
            add_finding(report,
                        "MEDIUM",
                        "least-privilege",
                        "Broad privilege grant detected",
                        path,
                        line_no,
                        line,
                        "Grant only required privileges to application roles.");
        }

        if (strstr(line, "0.0.0.0/0") || (strstr(line, "listen_addresses") && strstr(line, "*")) ||
            strstr(line, "-p 5432:5432")) {
            add_finding(report,
                        "MEDIUM",
                        "network-exposure",
                        "Potential broad PostgreSQL network exposure",
                        path,
                        line_no,
                        line,
                        "Restrict network ACLs/firewall and bind only trusted interfaces.");
        }

        prev_line_audit_ok = line_audit_ok;
    }

    if (found_pqexec && !found_pqexecparams) {
        add_finding(report,
                    "MEDIUM",
                    "query-safety",
                    "PQexec used without parameterized APIs in file",
                    path,
                    pqexec_line,
                    "PQexec(...)",
                    "Review dynamic SQL paths and adopt PQexecParams/PQprepare/PQexecPrepared.");
    }

    fclose(fp);
}

static void scan_dir(const char *root, int include_docs, audit_report_t *report)
{
    DIR *dir = opendir(root);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        const char *name = ent->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        char path[AUDIT_PATH_MAX_LEN];
        snprintf(path, sizeof(path), "%s/%s", root, name);

        if (ent->d_type == DT_DIR) {
            if (!should_skip_dir(name)) scan_dir(path, include_docs, report);
            continue;
        }

        if (ent->d_type == DT_REG && is_scan_file(name, include_docs)) {
            scan_file(path, report);
        }
    }

    closedir(dir);
}

static void json_escape(FILE *fp, const char *s)
{
    for (; *s; s++) {
        if (*s == '\\' || *s == '"') {
            fputc('\\', fp);
            fputc(*s, fp);
        } else if (*s == '\n') {
            fputs("\\n", fp);
        } else {
            fputc(*s, fp);
        }
    }
}

static void write_json(const char *json_out, const audit_report_t *report)
{
    if (!json_out || !report) return;
    FILE *fp = fopen(json_out, "w");
    if (!fp) return;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"summary\": {\"high\": %zu, \"medium\": %zu, \"low\": %zu, \"files_scanned\": %zu, \"truncated\": %s},\n",
            report->high_count, report->medium_count, report->low_count,
            report->total_files_scanned, report->truncated ? "true" : "false");
    fprintf(fp, "  \"findings\": [\n");
    for (size_t i = 0; i < report->total_findings; i++) {
        const audit_finding_t *f = &report->findings[i];
        fprintf(fp, "    {\"severity\": \""); json_escape(fp, f->severity);
        fprintf(fp, "\", \"category\": \""); json_escape(fp, f->category);
        fprintf(fp, "\", \"title\": \""); json_escape(fp, f->title);
        fprintf(fp, "\", \"file\": \""); json_escape(fp, f->file);
        fprintf(fp, "\", \"line\": %d, \"snippet\": \"", f->line); json_escape(fp, f->snippet);
        fprintf(fp, "\", \"recommendation\": \""); json_escape(fp, f->recommendation);
        fprintf(fp, "\"}%s\n", (i + 1 < report->total_findings) ? "," : "");
    }
    fprintf(fp, "  ]\n}\n");
    fclose(fp);
}

void audit_report_init(audit_report_t *report)
{
    if (!report) return;
    memset(report, 0, sizeof(*report));
}

int audit_run(const char *root,
              int include_docs,
              const char *json_out,
              audit_report_t *report,
              FILE *out)
{
    if (!report) return 1;
    if (!out) out = stdout;

    audit_report_init(report);
    scan_dir(root ? root : ".", include_docs, report);

    fprintf(out, "== pqCheck Native Security Audit Report ==\n");
    fprintf(out, "Files scanned: %zu\n", report->total_files_scanned);
    fprintf(out, "Findings: HIGH=%zu MEDIUM=%zu LOW=%zu\n\n",
            report->high_count, report->medium_count, report->low_count);

    for (size_t i = 0; i < report->total_findings; i++) {
        const audit_finding_t *f = &report->findings[i];
        fprintf(out, "[%zu] %s | %s | %s\n", i + 1, f->severity, f->category, f->title);
        fprintf(out, "    File: %s:%d\n", f->file, f->line);
        fprintf(out, "    Snippet: %s\n", f->snippet);
        fprintf(out, "    Recommendation: %s\n\n", f->recommendation);
    }

    if (report->truncated) {
        fprintf(out, "[audit] findings were truncated at %d entries\n", AUDIT_MAX_FINDINGS);
    }

    write_json(json_out, report);
    if (json_out) {
        fprintf(out, "JSON report written to: %s\n", json_out);
    }

    return report->high_count > 0 ? 1 : 0;
}

#ifdef WITH_LIBPQ
/* PostgreSQL system audit: connect to server and run a set of checks across
 * databases/roles/settings. Populates the same `audit_report_t` structure.
 */
int audit_run_postgres(const char *pg_connstr,
                       const char *json_out,
                       audit_report_t *report,
                       FILE *out)
{
    if (!report) return 1;
    if (!out) out = stdout;

    audit_report_init(report);

    const char *conninfo = pg_connstr ? pg_connstr : "host=127.0.0.1";
    PGconn *conn = PQconnectdb(conninfo);
    if (!conn || PQstatus(conn) != CONNECTION_OK) {
        const char *err = conn ? PQerrorMessage(conn) : "unable to allocate PGconn";
        add_finding(report,
                    "HIGH",
                    "connection-failed",
                    "Failed to connect to PostgreSQL server",
                    conninfo,
                    0,
                    err,
                    "Verify that the server is reachable and credentials are correct.");
        if (conn) PQfinish(conn);
        write_json(json_out, report);
        fprintf(out, "== pqCheck Native PostgreSQL Audit Report ==\n");
        fprintf(out, "Findings: HIGH=%zu MEDIUM=%zu LOW=%zu\n\n",
                report->high_count, report->medium_count, report->low_count);
        return 1;
    }

    /* Check for superuser roles other than 'postgres' */
    PGresult *res = PQexec(conn, "SELECT rolname FROM pg_roles WHERE rolsuper = true");
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; i++) {
            const char *role = PQgetvalue(res, i, 0);
            if (role && strcmp(role, "postgres") != 0) {
                char tag[AUDIT_PATH_MAX_LEN];
                snprintf(tag, sizeof(tag), "role:%s", role);
                add_finding(report,
                            "HIGH",
                            "superuser-role",
                            "Superuser role exists",
                            tag,
                            0,
                            role,
                            "Remove superuser privilege from application roles.");
            }
        }
    }
    if (res) PQclear(res);

    /* Check SSL setting */
    res = PQexec(conn, "SHOW ssl");
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        const char *val = PQgetvalue(res, 0, 0);
        if (val && (strcmp(val, "off") == 0 || strcasecmp(val, "false") == 0)) {
            add_finding(report,
                        "MEDIUM",
                        "transport-security",
                        "PostgreSQL SSL disabled",
                        "postgres",
                        0,
                        val,
                        "Enable SSL and require sslmode in client connection strings.");
        }
    }
    if (res) PQclear(res);

    /* Check listen_addresses */
    res = PQexec(conn, "SHOW listen_addresses");
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        const char *val = PQgetvalue(res, 0, 0);
        if (val && (strstr(val, "*") || strstr(val, "0.0.0.0"))) {
            add_finding(report,
                        "MEDIUM",
                        "network-exposure",
                        "listen_addresses allows external bind",
                        "postgres",
                        0,
                        val,
                        "Restrict listen_addresses to specific interfaces.");
        }
    }
    if (res) PQclear(res);

    /* Check server version and password encryption policy */
    res = PQexec(conn, "SHOW server_version");
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        const char *ver = PQgetvalue(res, 0, 0);
        if (ver) {
            /* Recommend modern versions */
            add_finding(report,
                        "LOW",
                        "server-version",
                        "Server version reported",
                        "postgres",
                        0,
                        ver,
                        "Consider running supported PostgreSQL versions; backport/patch as needed.");
        }
    }
    if (res) PQclear(res);

    res = PQexec(conn, "SHOW password_encryption");
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        const char *enc = PQgetvalue(res, 0, 0);
        if (enc && strcmp(enc, "scram-sha-256") != 0) {
            add_finding(report,
                        "MEDIUM",
                        "auth-policy",
                        "Weak password encryption policy",
                        "postgres",
                        0,
                        enc,
                        "Prefer 'scram-sha-256' for new installations and migrate passwords where possible.");
        }
    }
    if (res) PQclear(res);

    /* Attempt to inspect pg_hba rules (if readable) for 'trust' entries */
    res = PQexec(conn, "SELECT method FROM pg_hba_file_rules");
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; i++) {
            const char *method = PQgetvalue(res, i, 0);
            if (method && strcmp(method, "trust") == 0) {
                add_finding(report,
                            "HIGH",
                            "pg_hba",
                            "pg_hba: trust authentication rule detected",
                            "pg_hba",
                            0,
                            "trust",
                            "Avoid 'trust' entries in pg_hba.conf; restrict to secure auth methods and networks.");
            }
        }
    }
    if (res) PQclear(res);

    /* Roles without password expiry (rolvaliduntil IS NULL) — flag interactive logins */
    res = PQexec(conn, "SELECT rolname FROM pg_roles WHERE rolcanlogin = true AND rolvaliduntil IS NULL");
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; i++) {
            const char *role = PQgetvalue(res, i, 0);
            if (role && strcmp(role, "postgres") != 0) {
                char tag[AUDIT_PATH_MAX_LEN];
                snprintf(tag, sizeof(tag), "role:%s", role);
                add_finding(report,
                            "MEDIUM",
                            "auth-policy",
                            "Role has no password expiry",
                            tag,
                            0,
                            "rolvaliduntil=NULL",
                            "Consider setting password expiration (rolvaliduntil) or using centralized auth/policies.");
            }
        }
    }
    if (res) PQclear(res);

    /* Check connection saturation: compare active connections vs max_connections */
    PGresult *r_conn = PQexec(conn, "SELECT count(*) FROM pg_stat_activity");
    PGresult *r_max = PQexec(conn, "SHOW max_connections");
    if (r_conn && r_max && PQresultStatus(r_conn) == PGRES_TUPLES_OK && PQresultStatus(r_max) == PGRES_TUPLES_OK) {
        int active = atoi(PQgetvalue(r_conn, 0, 0));
        int maxconn = atoi(PQgetvalue(r_max, 0, 0));
        if (maxconn > 0) {
            int pct = (active * 100) / maxconn;
            if (pct > 75) {
                char details[128];
                snprintf(details, sizeof(details), "active=%d max=%d (%d%%)", active, maxconn, pct);
                add_finding(report,
                            "MEDIUM",
                            "connections",
                            "High connection utilization",
                            "postgres",
                            0,
                            details,
                            "Investigate connection pooling/limits; consider increasing max_connections if appropriate.");
            }
        }
    }
    if (r_conn) PQclear(r_conn);
    if (r_max) PQclear(r_max);

    /* Enumerate databases and run lightweight checks per DB */
    res = PQexec(conn, "SELECT datname FROM pg_database WHERE datistemplate = false");
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        int dbcount = PQntuples(res);
        report->total_files_scanned = (size_t)dbcount;
        for (int i = 0; i < dbcount; i++) {
            const char *dbname = PQgetvalue(res, i, 0);
            if (!dbname) continue;

            /* Build a per-database connection string by appending dbname */
            char connbuf[1024];
            if (pg_connstr && pg_connstr[0])
                snprintf(connbuf, sizeof(connbuf), "%s dbname=%s", pg_connstr, dbname);
            else
                snprintf(connbuf, sizeof(connbuf), "%s dbname=%s", conninfo, dbname);

            PGconn *dbc = PQconnectdb(connbuf);
            if (!dbc || PQstatus(dbc) != CONNECTION_OK) {
                const char *err = dbc ? PQerrorMessage(dbc) : "failed to allocate PGconn";
                char tag[AUDIT_PATH_MAX_LEN];
                snprintf(tag, sizeof(tag), "db:%s", dbname);
                add_finding(report,
                            "HIGH",
                            "connection-failed",
                            "Failed to connect to database",
                            tag,
                            0,
                            err,
                            "Check credentials and permissions for the database.");
                if (dbc) PQfinish(dbc);
                continue;
            }

            /* Find tables missing primary keys (limit results to avoid blowup) */
            const char *missing_pk_sql =
                "SELECT ns.nspname, c.relname FROM pg_class c "
                "JOIN pg_namespace ns ON ns.oid=c.relnamespace "
                "WHERE c.relkind='r' AND ns.nspname NOT IN ('pg_catalog','information_schema') "
                "AND NOT EXISTS (SELECT 1 FROM pg_constraint con WHERE con.conrelid = c.oid AND con.contype='p') "
                "LIMIT 50";

            PGresult *r2 = PQexec(dbc, missing_pk_sql);
            if (r2 && PQresultStatus(r2) == PGRES_TUPLES_OK) {
                int rows2 = PQntuples(r2);
                for (int j = 0; j < rows2; j++) {
                    const char *ns = PQgetvalue(r2, j, 0);
                    const char *rel = PQgetvalue(r2, j, 1);
                    char tag[AUDIT_PATH_MAX_LEN];
                    snprintf(tag, sizeof(tag), "%s.%s", ns ? ns : "?", rel ? rel : "?");
                    add_finding(report,
                                "MEDIUM",
                                "schema-design",
                                "Table missing primary key",
                                tag,
                                0,
                                dbname,
                                "Consider adding a primary key to uniquely identify rows.");
                }
            }
            if (r2) PQclear(r2);

            /* Count idle connections */
            PGresult *r3 = PQexec(dbc, "SELECT count(*) FROM pg_stat_activity WHERE state IN ('idle','idle in transaction')");
            if (r3 && PQresultStatus(r3) == PGRES_TUPLES_OK && PQntuples(r3) > 0) {
                int cnt = atoi(PQgetvalue(r3, 0, 0));
                if (cnt > 50) {
                    char tag[AUDIT_PATH_MAX_LEN];
                    snprintf(tag, sizeof(tag), "db:%s", dbname);
                    add_finding(report,
                                "MEDIUM",
                                "connections",
                                "Many idle connections",
                                tag,
                                0,
                                PQgetvalue(r3,0,0),
                                "Investigate long-running idle connections or use connection pooling to reduce load.");
                }
            }
            if (r3) PQclear(r3);

            PQfinish(dbc);
        }
    }
    if (res) PQclear(res);

    PQfinish(conn);

    /* Print findings similar to file-based audit */
    fprintf(out, "== pqCheck Native PostgreSQL Audit Report ==\n");
    fprintf(out, "Databases scanned: %zu\n", report->total_files_scanned);
    fprintf(out, "Findings: HIGH=%zu MEDIUM=%zu LOW=%zu\n\n",
            report->high_count, report->medium_count, report->low_count);

    for (size_t i = 0; i < report->total_findings; i++) {
        const audit_finding_t *f = &report->findings[i];
        fprintf(out, "[%zu] %s | %s | %s\n", i + 1, f->severity, f->category, f->title);
        fprintf(out, "    Target: %s\n", f->file);
        fprintf(out, "    Details: %s\n", f->snippet);
        fprintf(out, "    Recommendation: %s\n\n", f->recommendation);
    }

    if (report->truncated) {
        fprintf(out, "[audit] findings were truncated at %d entries\n", AUDIT_MAX_FINDINGS);
    }

    write_json(json_out, report);
    if (json_out) fprintf(out, "JSON report written to: %s\n", json_out);

    return report->high_count > 0 ? 1 : 0;
}
#endif /* WITH_LIBPQ */
