#ifndef WT_UI_H
#define WT_UI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * WINDOWS TWEAKER - TERMINAL UI ENGINE (TRUECOLOR RGB & INTERACTIVE TUI)
 * Zero-Emoji Pure C11 ANSI Virtual Terminal Renderer
 * ============================================================================ */

#define ANSI_RESET       "\033[0m"
#define ANSI_BOLD        "\033[1m"
#define ANSI_DIM         "\033[38;5;242m"

#define ANSI_CYAN        "\033[38;5;51m"
#define ANSI_GREEN       "\033[38;5;48m"
#define ANSI_AMBER       "\033[38;5;214m"
#define ANSI_RED         "\033[38;5;196m"
#define ANSI_MAGENTA     "\033[38;5;141m"
#define ANSI_BLUE        "\033[38;5;39m"
#define ANSI_WHITE       "\033[97m"
#define ANSI_GRAY        "\033[38;5;248m"

/* High-Contrast Status Pill Backgrounds (Zero Emojis) */
#define BG_GREEN         "\033[48;5;22m\033[38;5;48m\033[1m"
#define BG_AMBER         "\033[48;5;58m\033[38;5;214m\033[1m"
#define BG_RED           "\033[48;5;52m\033[38;5;196m\033[1m"
#define BG_CYAN          "\033[48;5;23m\033[38;5;51m\033[1m"
#define BG_MAGENTA       "\033[48;5;53m\033[38;5;141m\033[1m"
#define BG_SELECT        "\033[48;5;24m\033[97m\033[1m"

/* Cursor & Screen Controls */
#define ANSI_CLEAR_LINE  "\033[2K\r"
#define ANSI_CLEAR_SCREEN "\033[2J\033[H"
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
void ui_clear_screen(void);

/* TrueColor RGB Gradient Text & ASCII Logo */
void ui_print_gradient_line(const char *text, int r1, int g1, int b1, int r2, int g2, int b2);
void ui_print_banner(const char *version);
void ui_section_header(const char *tag, const char *title, const char *subtitle);
void ui_divider(int width);

/* Badges & Detailed Diagnostic Blocks */
const char *ui_badge_str(ui_status_t status);
void ui_print_status(ui_status_t status, const char *title, const char *detail);
void ui_print_diagnostic_item(
    ui_status_t status,
    const char *title,
    const char *current_state,
    const char *why_it_matters,
    const char *recommendation
);

/* Animated Spinners & Progress Bars */
void ui_spinner_tick(int frame, const char *action, const char *target);
void ui_spinner_finish(ui_status_t status, const char *action, const char *summary, double elapsed_ms);
void ui_progress_bar(int current, int total, const char *label, const char *sublabel);
void ui_usage_bar(double ratio, int bar_width, char *out_buf, size_t buf_size);

/* Box / Card Containers */
void ui_card_begin(const char *title, int width);
void ui_card_row(const char *label, const char *value, const char *accent_color, int width);
void ui_card_end(int width);

/* Interactive Keyboard Input Helpers */
int  ui_menu_select(const char *prompt_title, const char **items, const char **descriptions, int item_count);
bool ui_confirm_prompt(const char *question);
void ui_wait_for_key(const char *prompt_msg);

/* Utilities */
void ui_format_bytes(uint64_t bytes, char *out_buf, size_t buf_size);

#endif /* WT_UI_H */
