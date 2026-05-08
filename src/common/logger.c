#include "common/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

static FILE *log_fp = NULL;
static log_level_t current_level = LOG_INFO;
static int json_mode = 0;
static char module_tag[128] = "";

static const char *level_to_str(log_level_t l)
{
    switch (l) {
    case LOG_DEBUG: return "DEBUG";
    case LOG_INFO:  return "INFO";
    case LOG_WARN:  return "WARN";
    case LOG_ERROR: return "ERROR";
    default: return "UNK";
    }
}

int logger_init(const char *logfile, log_level_t level)
{
    current_level = level;
    if (logfile && logfile[0]) {
        log_fp = fopen(logfile, "a");
        if (!log_fp) return -1;
        setvbuf(log_fp, NULL, _IOLBF, 0);
    }
    return 0;
}

void logger_close(void)
{
    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }
}

void logger_set_level(log_level_t level)
{
    current_level = level;
}

void logger_set_json(int enable)
{
    json_mode = enable ? 1 : 0;
}

void logger_set_module(const char *module)
{
    if (!module) {
        module_tag[0] = '\0';
        return;
    }
    strncpy(module_tag, module, sizeof(module_tag)-1);
    module_tag[sizeof(module_tag)-1] = '\0';
}

static void logger_log(log_level_t level, const char *fmt, va_list ap)
{
    if (level < current_level) return;

    char timestr[64];
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm);

    char msg[2048];
    vsnprintf(msg, sizeof(msg), fmt, ap);

    if (json_mode) {
        /* JSON formatted log */
        if (module_tag[0]) {
            if (log_fp) fprintf(log_fp, "{\"ts\":\"%s\",\"lvl\":\"%s\",\"module\":\"%s\",\"msg\":\"%s\"}\n", timestr, level_to_str(level), module_tag, msg);
            fprintf(stderr, "{\"ts\":\"%s\",\"lvl\":\"%s\",\"module\":\"%s\",\"msg\":\"%s\"}\n", timestr, level_to_str(level), module_tag, msg);
        } else {
            if (log_fp) fprintf(log_fp, "{\"ts\":\"%s\",\"lvl\":\"%s\",\"msg\":\"%s\"}\n", timestr, level_to_str(level), msg);
            fprintf(stderr, "{\"ts\":\"%s\",\"lvl\":\"%s\",\"msg\":\"%s\"}\n", timestr, level_to_str(level), msg);
        }
    } else {
        if (module_tag[0])
            fprintf(stderr, "%s [%s] %s: %s\n", timestr, level_to_str(level), module_tag, msg);
        else
            fprintf(stderr, "%s [%s] %s\n", timestr, level_to_str(level), msg);
        if (log_fp) {
            if (module_tag[0])
                fprintf(log_fp, "%s [%s] %s: %s\n", timestr, level_to_str(level), module_tag, msg);
            else
                fprintf(log_fp, "%s [%s] %s\n", timestr, level_to_str(level), msg);
        }
    }
}

void log_debug(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); logger_log(LOG_DEBUG, fmt, ap); va_end(ap);
}
void log_info(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); logger_log(LOG_INFO, fmt, ap); va_end(ap);
}
void log_warn(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); logger_log(LOG_WARN, fmt, ap); va_end(ap);
}
void log_error(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); logger_log(LOG_ERROR, fmt, ap); va_end(ap);
}
