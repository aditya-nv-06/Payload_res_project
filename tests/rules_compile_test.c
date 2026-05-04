#define _POSIX_C_SOURCE 200809L

/*
 * rules_compile_test.c
 * Load config/rules.conf and attempt to compile every REGEX pattern
 * using POSIX ERE with the same flags pqCheck uses.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <regex.h>

#define MAXLINE 4096

int main(int argc, char **argv)
{
    const char *path = "config/rules.conf";
    if (argc > 1) path = argv[1];

    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("fopen");
        return 2;
    }

    char line[MAXLINE];
    int lineno = 0;
    int failures = 0;

    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;

        char *save = NULL;
        char *tok = strtok_r(line, "|", &save);
        if (!tok) continue; /* name */
        /* type */
        tok = strtok_r(NULL, "|", &save);
        if (!tok) continue;
        char type[64];
        strncpy(type, tok, sizeof(type)-1); type[sizeof(type)-1] = '\0';

        /* pattern */
        tok = strtok_r(NULL, "|", &save);
        if (!tok) continue;
        char pattern[MAXLINE];
        strncpy(pattern, tok, sizeof(pattern)-1); pattern[sizeof(pattern)-1] = '\0';

        if (strcasecmp(type, "REGEX") != 0) continue;

        regex_t r;
        int ret = regcomp(&r, pattern, REG_EXTENDED | REG_ICASE | REG_NEWLINE);
        if (ret != 0) {
            char errbuf[256];
            regerror(ret, &r, errbuf, sizeof(errbuf));
            fprintf(stderr, "[FAIL] %s:%d regex compile error: %s -> %s\n",
                    path, lineno, pattern, errbuf);
            failures++;
        } else {
            regfree(&r);
        }
    }

    fclose(fp);

    if (failures) {
        fprintf(stderr, "rules_compile_test: %d regex(es) failed to compile\n", failures);
        return 1;
    }

    printf("rules_compile_test: all REGEX rules compiled successfully\n");
    return 0;
}
