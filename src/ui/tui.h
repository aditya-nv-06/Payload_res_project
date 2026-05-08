/*
 * tui.h – Text User Interface (TUI) for pqCheck
 *
 * Provides an interactive ncurses-based dashboard for monitoring SQL injection
 * detection in real time. Screens include:
 *   - Dashboard: capture status, query count, alert rate
 *   - Alerts: scrollable list of detected SQLi with flow info
 *   - Config: current settings and thresholds
 *   - Help: keyboard shortcuts
 */

#ifndef TUI_H
#define TUI_H

#include <stdint.h>
#include <time.h>

/* Data passed to TUI for display */
typedef struct {
    uint64_t total_queries;
    uint64_t total_alerts;
    uint64_t rule_alerts;
    uint64_t anomaly_alerts;
    
    int      capture_active;
    int      rules_loaded;
    int      ngram_loaded;
    
    time_t   start_time;
} tui_stats_t;

/* Alert entry for the TUI alert list */
typedef struct {
    char  flow_id[32];         /* 5-tuple identifier */
    char  timestamp[32];       /* ISO8601 timestamp */
    char  src_ip[16];
    uint16_t src_port;
    char  dst_ip[16];
    uint16_t dst_port;
    
    char  extracted_sql[256];  /* First 255 chars of SQL */
    int   rule_score;
    double anomaly_score;
    char  risk_level[16];      /* CRITICAL, HIGH, MEDIUM, LOW */
} tui_alert_t;

/* TUI context */
typedef struct {
    /* ncurses state */
    int initialized;
    int running;
    
    /* Screen dimensions */
    int max_rows;
    int max_cols;
    
    /* Alert history (circular buffer) */
#define TUI_MAX_ALERTS 256
    tui_alert_t alerts[TUI_MAX_ALERTS];
    int alert_count;
    int alert_scroll;
    
    /* Current screen */
#define TUI_SCREEN_DASHBOARD 0
#define TUI_SCREEN_ALERTS    1
#define TUI_SCREEN_CONFIG    2
#define TUI_SCREEN_HELP      3
    int current_screen;
    
    /* Statistics */
    tui_stats_t stats;
} tui_ctx_t;

/* ====================================================================== */

/* Initialize the TUI; must be called early */
int tui_init(tui_ctx_t *tui);

/* Start the interactive TUI loop (blocking) */
int tui_run(tui_ctx_t *tui);

/* Clean up ncurses and resources */
void tui_cleanup(tui_ctx_t *tui);

/* Add an alert to the TUI alert list */
void tui_add_alert(tui_ctx_t *tui, const tui_alert_t *alert);

/* Update statistics display */
void tui_update_stats(tui_ctx_t *tui, const tui_stats_t *stats);

/* Refresh the display (call periodically) */
void tui_refresh(tui_ctx_t *tui);

#endif /* TUI_H */
