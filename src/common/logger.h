#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

typedef enum { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 } log_level_t;

/* Initialise logger. If logfile == NULL, logs go to stderr only. level sets minimum log level. */
int logger_init(const char *logfile, log_level_t level);
void logger_close(void);

/* Set runtime log level */
void logger_set_level(log_level_t level);

/* Enable/disable JSON formatted logs (0 = text, 1 = JSON) */
void logger_set_json(int enable);

/* Set a module tag included in log entries (may be NULL) */
void logger_set_module(const char *module);

/* Logging primitives */
void log_debug(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif /* LOGGER_H */
