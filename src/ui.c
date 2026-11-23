#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

static DWORD g_original_out_mode = 0;
static UINT  g_original_cp = 0;
static bool  g_console_initialized = false;

static const char *SPINNER_FRAMES[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
};
#define SPINNER_FRAME_COUNT 10

bool ui_init(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return false;
    }

    g_original_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        g_original_out_mode = dwMode;
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
        SetConsoleMode(hOut, dwMode);
    }

    g_console_initialized = true;
    return true;
}

void ui_cleanup(void) {
    printf(ANSI_SHOW_CURSOR ANSI_RESET);
    fflush(stdout);
    if (g_console_initialized) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE && g_original_out_mode != 0) {
            SetConsoleMode(hOut, g_original_out_mode);
        }
        if (g_original_cp != 0) {
            SetConsoleOutputCP(g_original_cp);
        }
    }
}

void ui_print_banner(const char *version) {
    printf("\n");
    printf(ANSI_CYAN ANSI_BOLD "  ╭──────────────────────────────────────────────────────────────────────╮\n" ANSI_RESET);
    printf(ANSI_CYAN ANSI_BOLD "  │" ANSI_RESET "  " BG_CYAN " ⚡ WINDOWS TWEAKER " ANSI_RESET " " ANSI_WHITE ANSI_BOLD "System Diagnostic, Storage & OS Tuning Engine" ANSI_RESET "  " ANSI_CYAN ANSI_BOLD "│\n" ANSI_RESET);
    printf(ANSI_CYAN ANSI_BOLD "  │" ANSI_RESET "  " ANSI_DIM "Pure C11 Win32 Native CLI  •  v%-8s  •  Zero-Bloat Telemetry" ANSI_RESET "    " ANSI_CYAN ANSI_BOLD "│\n" ANSI_RESET, version);
    printf(ANSI_CYAN ANSI_BOLD "  ╰──────────────────────────────────────────────────────────────────────╯\n" ANSI_RESET);
}

void ui_section_header(const char *icon, const char *title, const char *subtitle) {
    printf("\n  " ANSI_CYAN ANSI_BOLD "%s %s" ANSI_RESET, icon, title);
    if (subtitle && subtitle[0] != '\0') {
        printf(" " ANSI_DIM "─ %s" ANSI_RESET, subtitle);
    }
    printf("\n  " ANSI_DIM "────────────────────────────────────────────────────────────────────────" ANSI_RESET "\n");
}

void ui_divider(int width) {
    printf("  " ANSI_DIM);
    for (int i = 0; i < width; i++) {
        printf("─");
    }
    printf(ANSI_RESET "\n");
}

const char *ui_badge_str(ui_status_t status) {
    switch (status) {
        case UI_STATUS_OK:    return BG_GREEN   "  OK   " ANSI_RESET;
        case UI_STATUS_WARN:  return BG_AMBER   " WARN  " ANSI_RESET;
        case UI_STATUS_CRIT:  return BG_RED     " ALERT " ANSI_RESET;
        case UI_STATUS_INFO:  return BG_CYAN    " INFO  " ANSI_RESET;
        case UI_STATUS_SCAN:  return BG_CYAN    " SCAN  " ANSI_RESET;
        case UI_STATUS_TWEAK: return BG_MAGENTA " TWEAK " ANSI_RESET;
        default:              return BG_CYAN    " INFO  " ANSI_RESET;
    }
}

void ui_print_status(ui_status_t status, const char *title, const char *detail) {
    printf("  %s " ANSI_WHITE ANSI_BOLD "%-28s" ANSI_RESET, ui_badge_str(status), title);
    if (detail && detail[0] != '\0') {
        printf(" " ANSI_GRAY "%s" ANSI_RESET, detail);
    }
    printf("\n");
}

void ui_spinner_tick(int frame, const char *action, const char *target) {
    const char *glyph = SPINNER_FRAMES[frame % SPINNER_FRAME_COUNT];
    printf(ANSI_CLEAR_LINE ANSI_HIDE_CURSOR "  " ANSI_CYAN ANSI_BOLD "%s" ANSI_RESET " " ANSI_WHITE "%s" ANSI_RESET " " ANSI_DIM "%s" ANSI_RESET,
           glyph, action, target ? target : "");
    fflush(stdout);
}

void ui_spinner_finish(ui_status_t status, const char *action, const char *summary, double elapsed_ms) {
    printf(ANSI_CLEAR_LINE ANSI_SHOW_CURSOR "  %s " ANSI_WHITE "%-28s" ANSI_RESET " " ANSI_GRAY "%s" ANSI_RESET " " ANSI_DIM "(%.1f ms)" ANSI_RESET "\n",
           ui_badge_str(status), action, summary ? summary : "", elapsed_ms);
    fflush(stdout);
}

void ui_progress_bar(int current, int total, const char *label, const char *sublabel) {
    if (total <= 0) total = 1;
    if (current > total) current = total;

    int bar_width = 24;
    double ratio = (double)current / (double)total;
    int filled = (int)(ratio * bar_width);
    int pct = (int)(ratio * 100.0);

    printf(ANSI_CLEAR_LINE ANSI_HIDE_CURSOR "  " ANSI_CYAN "▸" ANSI_RESET " " ANSI_WHITE "%-22s" ANSI_RESET " [", label);
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            printf(ANSI_CYAN "█" ANSI_RESET);
        } else {
            printf(ANSI_DIM "░" ANSI_RESET);
        }
    }
    printf("] " ANSI_BOLD "%3d%%" ANSI_RESET, pct);
    if (sublabel && sublabel[0] != '\0') {
        printf(" " ANSI_DIM "%s" ANSI_RESET, sublabel);
    }
    if (current == total) {
        printf(ANSI_SHOW_CURSOR "\n");
    }
    fflush(stdout);
}

void ui_usage_bar(double ratio, int bar_width, char *out_buf, size_t buf_size) {
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;

    int filled = (int)(ratio * bar_width);
    const char *color = ANSI_GREEN;
    if (ratio >= 0.85) {
        color = ANSI_RED;
    } else if (ratio >= 0.65) {
        color = ANSI_AMBER;
    } else {
        color = ANSI_CYAN;
    }

    size_t pos = 0;
    pos += (size_t)snprintf(out_buf + pos, buf_size - pos, "%s", color);
    for (int i = 0; i < filled && pos + 8 < buf_size; i++) {
        pos += (size_t)snprintf(out_buf + pos, buf_size - pos, "█");
    }
    pos += (size_t)snprintf(out_buf + pos, buf_size - pos, "%s", ANSI_DIM);
    for (int i = filled; i < bar_width && pos + 8 < buf_size; i++) {
        pos += (size_t)snprintf(out_buf + pos, buf_size - pos, "░");
    }
    snprintf(out_buf + pos, buf_size - pos, ANSI_RESET);
}

void ui_card_begin(const char *title, int width) {
    int title_len = (int)strlen(title);
    int dashes = width - title_len - 5;
    if (dashes < 2) dashes = 2;

    printf("  " ANSI_DIM "╭─ " ANSI_RESET ANSI_CYAN ANSI_BOLD "%s" ANSI_RESET " " ANSI_DIM, title);
    for (int i = 0; i < dashes; i++) printf("─");
    printf("╮" ANSI_RESET "\n");
}

void ui_card_row(const char *label, const char *value, const char *accent_color, int width) {
    (void)width;
    const char *col = (accent_color && accent_color[0] != '\0') ? accent_color : ANSI_WHITE;
    printf("  " ANSI_DIM "│" ANSI_RESET "  " ANSI_GRAY "%-24s" ANSI_RESET " %s%s" ANSI_RESET "\n",
           label, col, value);
}

void ui_card_end(int width) {
    printf("  " ANSI_DIM "╰");
    for (int i = 0; i < width; i++) printf("─");
    printf("╯" ANSI_RESET "\n");
}

void ui_format_bytes(uint64_t bytes, char *out_buf, size_t buf_size) {
    const double KB = 1024.0;
    const double MB = KB * 1024.0;
    const double GB = MB * 1024.0;
    const double TB = GB * 1024.0;

    if (bytes >= (uint64_t)TB) {
        snprintf(out_buf, buf_size, "%.2f TB", (double)bytes / TB);
    } else if (bytes >= (uint64_t)GB) {
        snprintf(out_buf, buf_size, "%.2f GB", (double)bytes / GB);
    } else if (bytes >= (uint64_t)MB) {
        snprintf(out_buf, buf_size, "%.1f MB", (double)bytes / MB);
    } else if (bytes >= (uint64_t)KB) {
        snprintf(out_buf, buf_size, "%.1f KB", (double)bytes / KB);
    } else {
        snprintf(out_buf, buf_size, "%llu B", (unsigned long long)bytes);
    }
}

void ui_render_health_score(int score, uint64_t reclaimable_bytes, int issues_found) {
    if (score < 0) score = 0;
    if (score > 100) score = 100;

    const char *score_color = ANSI_GREEN;
    const char *grade_text  = "EXCELLENT (OPTIMIZED)";
    if (score < 60) {
        score_color = ANSI_RED;
        grade_text  = "NEEDS ATTENTION (UNOPTIMIZED)";
    } else if (score < 85) {
        score_color = ANSI_AMBER;
        grade_text  = "MODERATE (TUNING RECOMMENDED)";
    }

    char reclaim_str[64];
    ui_format_bytes(reclaimable_bytes, reclaim_str, sizeof(reclaim_str));

    char bar[256];
    ui_usage_bar((double)score / 100.0, 28, bar, sizeof(bar));

    printf("\n");
    printf("  " ANSI_DIM "╭─ " ANSI_RESET ANSI_MAGENTA ANSI_BOLD "SYSTEM OPTIMIZATION SUMMARY" ANSI_RESET ANSI_DIM " ──────────────────────────────────────────╮" ANSI_RESET "\n");
    printf("  " ANSI_DIM "│" ANSI_RESET "  Health Score       : %s" ANSI_BOLD "%3d / 100" ANSI_RESET "  [%s]  %s%s" ANSI_RESET "\n",
           score_color, score, bar, score_color, grade_text);
    printf("  " ANSI_DIM "│" ANSI_RESET "  Reclaimable Space  : " ANSI_CYAN ANSI_BOLD "%-14s" ANSI_RESET ANSI_DIM "(Temp caches, logs & cold artifacts)" ANSI_RESET "\n",
           reclaim_str);
    printf("  " ANSI_DIM "│" ANSI_RESET "  Tuning Suggestions : " ANSI_AMBER ANSI_BOLD "%d actionable optimization(s) detected" ANSI_RESET "\n",
           issues_found);
    printf("  " ANSI_DIM "╰──────────────────────────────────────────────────────────────────────╯" ANSI_RESET "\n");
}
