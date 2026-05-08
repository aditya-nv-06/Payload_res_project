/*
 * tui.c – Text User Interface (TUI) implementation using ncurses
 */
#include "ui/tui.h"

#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

/* Static context for signal handlers */
static tui_ctx_t *g_tui = NULL;

/* SIGINT handler to gracefully exit TUI */
static void tui_sigint_handler(int sig)
{
    (void)sig;
    if (g_tui) g_tui->running = 0;
}

/* ====================================================================== */
/* Screen rendering functions                                             */
/* ====================================================================== */

static void render_dashboard(tui_ctx_t *tui)
{
    if (!tui) return;
    
    clear();
    
    /* Title bar */
    attron(A_BOLD | A_REVERSE);
    mvprintw(0, 0, "%-*s", tui->max_cols, " pqCheck TUI – PostgreSQL SQLi Detection Dashboard");
    attroff(A_BOLD | A_REVERSE);
    
    /* Stats section */
    attron(A_BOLD);
    mvprintw(2, 2, "Status");
    attroff(A_BOLD);
    
    mvprintw(3, 4, "Capture:           %s", tui->stats.capture_active ? "ACTIVE" : "IDLE");
    mvprintw(4, 4, "Rules:             %s (%d loaded)",
             tui->stats.rules_loaded ? "YES" : "NO", 
             tui->stats.rules_loaded ? 1 : 0);
    mvprintw(5, 4, "N-gram Model:      %s", tui->stats.ngram_loaded ? "LOADED" : "DISABLED");
    
    time_t now = time(NULL);
    time_t uptime = now - tui->stats.start_time;
    mvprintw(6, 4, "Uptime:            %lds", uptime);
    
    /* Metrics section */
    attron(A_BOLD);
    mvprintw(8, 2, "Metrics");
    attroff(A_BOLD);
    
    mvprintw(9, 4, "Total Queries:     %llu", (unsigned long long)tui->stats.total_queries);
    mvprintw(10, 4, "Total Alerts:      %llu", (unsigned long long)tui->stats.total_alerts);
    mvprintw(11, 4, "Rule Detections:   %llu", (unsigned long long)tui->stats.rule_alerts);
    mvprintw(12, 4, "Anomaly Detections:%llu", (unsigned long long)tui->stats.anomaly_alerts);
    
    /* Footer */
    attron(A_REVERSE);
    mvprintw(tui->max_rows - 1, 0, "%-*s", tui->max_cols,
             " [a] Alerts  [c] Config  [h] Help  [q] Quit");
    attroff(A_REVERSE);
    
    refresh();
}

static void render_alerts(tui_ctx_t *tui)
{
    if (!tui) return;
    
    clear();
    
    /* Title bar */
    attron(A_BOLD | A_REVERSE);
    mvprintw(0, 0, "%-*s", tui->max_cols, " Alert Log");
    attroff(A_BOLD | A_REVERSE);
    
    /* Header */
    mvprintw(1, 2, "%-8s %-15s %-6s %-15s %-6s %-20s %-8s",
             "Flow", "Source IP", "Sport", "Dest IP", "Dport", "Risk", "Score");
    mvprintw(2, 0, "%.*s", tui->max_cols, "────────────────────────────────────────────────────────────────────────────────");
    
    int row = 3;
    for (int i = 0; i < tui->alert_count && row < tui->max_rows - 2; i++) {
        int idx = (tui->alert_scroll + i) % tui->alert_count;
        if (idx >= TUI_MAX_ALERTS) idx = 0;
        
        tui_alert_t *a = &tui->alerts[idx];
        
        /* Color coding by risk level */
        if (strcmp(a->risk_level, "CRITICAL") == 0) attron(A_BOLD | COLOR_PAIR(1)); /* Red */
        else if (strcmp(a->risk_level, "HIGH") == 0) attron(COLOR_PAIR(2));         /* Yellow */
        else if (strcmp(a->risk_level, "MEDIUM") == 0) attron(COLOR_PAIR(3));       /* Cyan */
        else attron(COLOR_PAIR(4));                                                  /* Green */
        
        mvprintw(row, 2, "%-8s %-15s %5u %-15s %5u %-20s %7.2f",
                 a->flow_id, a->src_ip, a->src_port, a->dst_ip, a->dst_port,
                 a->risk_level, a->anomaly_score);
        
        attroff(A_BOLD | COLOR_PAIR(1) | COLOR_PAIR(2) | COLOR_PAIR(3) | COLOR_PAIR(4));
        row++;
    }
    
    /* Footer */
    attron(A_REVERSE);
    mvprintw(tui->max_rows - 1, 0, "%-*s", tui->max_cols,
             " [↑↓] Scroll  [d] Dashboard  [c] Config  [h] Help  [q] Quit");
    attroff(A_REVERSE);
    
    refresh();
}

static void render_config(tui_ctx_t *tui)
{
    if (!tui) return;
    
    clear();
    
    /* Title bar */
    attron(A_BOLD | A_REVERSE);
    mvprintw(0, 0, "%-*s", tui->max_cols, " Configuration");
    attroff(A_BOLD | A_REVERSE);
    
    /* Display current configuration info */
    attron(A_BOLD);
    mvprintw(2, 2, "Detection Settings");
    attroff(A_BOLD);
    
    mvprintw(3, 4, "Rules File:        config/rules.conf");
    mvprintw(4, 4, "Model File:        baseline.model");
    mvprintw(5, 4, "Network Config:    config/network.conf (optional)");
    mvprintw(7, 4, "Anomaly Threshold: -5.0");
    
    attron(A_BOLD);
    mvprintw(9, 2, "Alert Output");
    attroff(A_BOLD);
    
    mvprintw(10, 4, "Alert Log:         alerts.jsonl");
    mvprintw(11, 4, "Packet Dump:       (disabled)");
    
    /* Footer */
    attron(A_REVERSE);
    mvprintw(tui->max_rows - 1, 0, "%-*s", tui->max_cols,
             " [d] Dashboard  [a] Alerts  [h] Help  [q] Quit");
    attroff(A_REVERSE);
    
    refresh();
}

static void render_help(tui_ctx_t *tui)
{
    if (!tui) return;
    
    clear();
    
    /* Title bar */
    attron(A_BOLD | A_REVERSE);
    mvprintw(0, 0, "%-*s", tui->max_cols, " Help – Keyboard Shortcuts");
    attroff(A_BOLD | A_REVERSE);
    
    int row = 2;
    
    mvprintw(row++, 4, "Navigation:");
    mvprintw(row++, 8, "[d] - Go to Dashboard");
    mvprintw(row++, 8, "[a] - Go to Alert Log");
    mvprintw(row++, 8, "[c] - Go to Configuration");
    mvprintw(row++, 8, "[h] - Show this Help screen");
    
    row++;
    mvprintw(row++, 4, "Alert Navigation:");
    mvprintw(row++, 8, "[↑] - Scroll up in alert list");
    mvprintw(row++, 8, "[↓] - Scroll down in alert list");
    
    row++;
    mvprintw(row++, 4, "General:");
    mvprintw(row++, 8, "[q] - Quit pqCheck");
    mvprintw(row++, 8, "[Ctrl+C] - Emergency exit");
    
    row += 2;
    mvprintw(row++, 4, "About:");
    mvprintw(row++, 8, "pqCheck v1.0.0 – PostgreSQL SQLi Detection Sensor");
    mvprintw(row++, 8, "Interactive TUI mode for real-time monitoring");
    
    /* Footer */
    attron(A_REVERSE);
    mvprintw(tui->max_rows - 1, 0, "%-*s", tui->max_cols,
             " Press any key to return to Dashboard");
    attroff(A_REVERSE);
    
    refresh();
}

/* ====================================================================== */
/* Public API                                                              */
/* ====================================================================== */

int tui_init(tui_ctx_t *tui)
{
    if (!tui) return -1;
    
    memset(tui, 0, sizeof(*tui));
    g_tui = tui;
    
    /* Initialize ncurses */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);  /* Non-blocking input */
    curs_set(0);            /* Hide cursor */
    
    /* Initialize colors if terminal supports them */
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_RED,     COLOR_BLACK);  /* CRITICAL */
        init_pair(2, COLOR_YELLOW,  COLOR_BLACK);  /* HIGH */
        init_pair(3, COLOR_CYAN,    COLOR_BLACK);  /* MEDIUM */
        init_pair(4, COLOR_GREEN,   COLOR_BLACK);  /* LOW */
    }
    
    getmaxyx(stdscr, tui->max_rows, tui->max_cols);
    
    tui->initialized = 1;
    tui->running = 1;
    tui->current_screen = TUI_SCREEN_DASHBOARD;
    tui->stats.start_time = time(NULL);
    
    /* Register SIGINT handler */
    signal(SIGINT, tui_sigint_handler);
    
    return 0;
}

int tui_run(tui_ctx_t *tui)
{
    if (!tui || !tui->initialized) return -1;
    
    while (tui->running) {
        /* Render current screen */
        switch (tui->current_screen) {
        case TUI_SCREEN_DASHBOARD:
            render_dashboard(tui);
            break;
        case TUI_SCREEN_ALERTS:
            render_alerts(tui);
            break;
        case TUI_SCREEN_CONFIG:
            render_config(tui);
            break;
        case TUI_SCREEN_HELP:
            render_help(tui);
            break;
        }
        
        /* Handle input */
        int ch = getch();
        switch (ch) {
        case 'd':
        case 'D':
            tui->current_screen = TUI_SCREEN_DASHBOARD;
            break;
        case 'a':
        case 'A':
            tui->current_screen = TUI_SCREEN_ALERTS;
            break;
        case 'c':
        case 'C':
            tui->current_screen = TUI_SCREEN_CONFIG;
            break;
        case 'h':
        case 'H':
            tui->current_screen = TUI_SCREEN_HELP;
            break;
        case 'q':
        case 'Q':
            tui->running = 0;
            break;
        case KEY_UP:
            if (tui->current_screen == TUI_SCREEN_ALERTS && tui->alert_scroll > 0) {
                tui->alert_scroll--;
            }
            break;
        case KEY_DOWN:
            if (tui->current_screen == TUI_SCREEN_ALERTS && 
                tui->alert_scroll + 1 < tui->alert_count) {
                tui->alert_scroll++;
            }
            break;
        }
        
        /* Small sleep to avoid busy-loop */
        usleep(100000);  /* 100ms */
    }
    
    return 0;
}

void tui_cleanup(tui_ctx_t *tui)
{
    if (!tui || !tui->initialized) return;
    
    endwin();
    tui->initialized = 0;
}

void tui_add_alert(tui_ctx_t *tui, const tui_alert_t *alert)
{
    if (!tui || !alert) return;
    
    int idx = tui->alert_count % TUI_MAX_ALERTS;
    memcpy(&tui->alerts[idx], alert, sizeof(*alert));
    
    if (tui->alert_count < TUI_MAX_ALERTS) {
        tui->alert_count++;
    } else {
        tui->alert_count = TUI_MAX_ALERTS;
    }
}

void tui_update_stats(tui_ctx_t *tui, const tui_stats_t *stats)
{
    if (!tui || !stats) return;
    memcpy(&tui->stats, stats, sizeof(*stats));
}

void tui_refresh(tui_ctx_t *tui)
{
    if (!tui || !tui->initialized) return;
    /* Refresh is handled in tui_run loop */
}
