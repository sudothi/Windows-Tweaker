#ifndef WT_UI_H
#define WT_UI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * WINDOWS TWEAKER - TERMINAL UI ENGINE (ANTIGRAVITY CLI STYLE)
 * Pure C ANSI Virtual Terminal Sequence & UTF-8 Console Renderer
 * ============================================================================ */

/* ANSI Escape Codes - Modern High-Contrast Palette */
#define ANSI_RESET       "\033[0m"
#define ANSI_BOLD        "\033[1m"
#define ANSI_DIM         "\033[38;5;242m"
#define ANSI_ITALIC      "\033[3m"

#define ANSI_CYAN        "\033[38;5;51m"
#define ANSI_GREEN       "\033[38;5;48m"
#define ANSI_AMBER       "\033[38;5;214m"
#define ANSI_RED         "\033[38;5;196m"
#define ANSI_MAGENTA     "\033[38;5;141m"
#define ANSI_BLUE        "\033[38;5;39m"
#define ANSI_WHITE       "\033[97m"
#define ANSI_GRAY        "\033[38;5;246m"

/* Background Pill Accents */
#define BG_GREEN         "\033[48;5;22m\033[38;5;48m\033[1m"
#define BG_AMBER         "\033[48;5;58m\033[38;5;214m\033[1m"
#define BG_RED           "\033[48;5;52m\033[38;5;196m\033[1m"
#define BG_CYAN          "\033[48;5;23m\033[38;5;51m\033[1m"
#define BG_MAGENTA       "\033[48;5;53m\033[38;5;141m\033[1m"

/* Cursor & Line Controls */
#define ANSI_CLEAR_LINE  "\033[2K\r"
#define ANSI_HIDE_CURSOR "\033[?25l"
#define ANSI_SHOW_CURSOR "\033[?25h"

typedef enum {
    UI_STATUS_OK = 0,
    UI_STATUS_WARN,
    UI_STATUS_CRIT,
    UI_STATUS_INFO,
    UI_STATUS_SCAN,
    UI_STATUS_TWEAK
} ui_status_t;

/* Console Initialization & Restoration */
bool ui_init(void);
void ui_cleanup(void);

/* Header & Section Styling */
void ui_print_banner(const char *version);
void ui_section_header(const char *icon, const char *title, const char *subtitle);
void ui_divider(int width);

/* Badges & Status Lines */
void ui_print_status(ui_status_t status, const char *title, const char *detail);
const char *ui_badge_str(ui_status_t status);

/* Animated Spinners & Progress Bars */
void ui_spinner_tick(int frame, const char *action, const char *target);
void ui_spinner_finish(ui_status_t status, const char *action, const char *summary, double elapsed_ms);
void ui_progress_bar(int current, int total, const char *label, const char *sublabel);
void ui_usage_bar(double ratio, int bar_width, char *out_buf, size_t buf_size);

/* Box / Card Containers */
void ui_card_begin(const char *title, int width);
void ui_card_row(const char *label, const char *value, const char *accent_color, int width);
void ui_card_end(int width);

/* Health Score Widget */
void ui_render_health_score(int score, uint64_t reclaimable_bytes, int issues_found);

/* Utilities */
void ui_format_bytes(uint64_t bytes, char *out_buf, size_t buf_size);

#endif /* WT_UI_H */
