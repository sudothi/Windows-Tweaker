#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <conio.h>

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

void ui_clear_screen(void) {
    printf(ANSI_CLEAR_SCREEN);
    fflush(stdout);
}

void ui_print_gradient_line(const char *text, int r1, int g1, int b1, int r2, int g2, int b2) {
    int len = (int)strlen(text);
    if (len <= 0) {
        printf("\n");
        return;
    }

    printf(ANSI_BOLD);
    for (int i = 0; i < len; i++) {
        double t = (len > 1) ? ((double)i / (double)(len - 1)) : 0.0;
        int r = (int)(r1 + t * (r2 - r1));
        int g = (int)(g1 + t * (g2 - g1));
        int b = (int)(b1 + t * (b2 - b1));
        printf("\033[38;2;%d;%d;%dm%c", r, g, b, text[i]);
    }
    printf(ANSI_RESET "\n");
}

void ui_print_banner(const char *version) {
    static const char *LOGO_LINES[] = {
        "  ██╗    ██╗██╗███╗   ██╗    ████████╗██╗    ██╗███████╗ █████╗ ██╗  ██╗███████╗██████╗ ",
        "  ██║    ██║██║████╗  ██║    ╚══██╔══╝██║    ██║██╔════╝██╔══██╗██║ ██╔╝██╔════╝██╔══██╗",
        "  ██║ █╗ ██║██║██╔██╗ ██║       ██║   ██║ █╗ ██║█████╗  ███████║█████╔╝ █████╗  ██████╔╝",
        "  ██║███╗██║██║██║╚██╗██║       ██║   ██║███╗██║██╔══╝  ██╔══██║██╔═██╗ ██╔══╝  ██╔══██╗",
        "  ╚███╔███╔╝██║██║ ╚████║       ██║   ╚███╔███╔╝███████╗██║  ██║██║  ██╗███████╗██║  ██║",
        "   ╚══╝╚══╝ ╚═╝╚═╝  ╚═══╝       ╚═╝    ╚══╝╚══╝ ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝"
    };

    /* TrueColor vertical + horizontal RGB gradient (Electric Cyan -> Neon Violet) */
    printf("\n");
    for (int row = 0; row < 6; row++) {
        int r_start = 0   + (row * 22);
        int g_start = 240 - (row * 25);
        int b_start = 255;

        int r_end   = 155 + (row * 16);
        int g_end   = 80  - (row * 10);
        int b_end   = 255;

        ui_print_gradient_line(LOGO_LINES[row], r_start, g_start, b_start, r_end, g_end, b_end);
    }

    printf("  " ANSI_DIM "╭────────────────────────────────────────────────────────────────────────────────────╮" ANSI_RESET "\n");
    printf("  " ANSI_DIM "│" ANSI_RESET "  " BG_CYAN " CORE ENGINE " ANSI_RESET "  " ANSI_WHITE ANSI_BOLD "Sistema de Diagnostico, Limpeza de Disco e Otimizacao Windows" ANSI_RESET "       " ANSI_DIM "│" ANSI_RESET "\n");
    printf("  " ANSI_DIM "│" ANSI_RESET "  " ANSI_DIM "Arquitetura C11 Nativa (Win32 API + __cpuid)  |  Versao: %-8s  |  Modo Seguro" ANSI_RESET "   " ANSI_DIM "│" ANSI_RESET "\n", version);
    printf("  " ANSI_DIM "╰────────────────────────────────────────────────────────────────────────────────────╯" ANSI_RESET "\n");
}

void ui_section_header(const char *tag, const char *title, const char *subtitle) {
    printf("\n  " BG_CYAN " %s " ANSI_RESET " " ANSI_CYAN ANSI_BOLD "%s" ANSI_RESET, tag, title);
    if (subtitle && subtitle[0] != '\0') {
        printf(" " ANSI_DIM "// %s" ANSI_RESET, subtitle);
    }
    printf("\n  " ANSI_DIM "────────────────────────────────────────────────────────────────────────────────────" ANSI_RESET "\n");
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
        case UI_STATUS_OK:    return BG_GREEN   "   OK   " ANSI_RESET;
        case UI_STATUS_WARN:  return BG_AMBER   " ATENCAO" ANSI_RESET;
        case UI_STATUS_CRIT:  return BG_RED     " CRITICO" ANSI_RESET;
        case UI_STATUS_INFO:  return BG_CYAN    "  INFO  " ANSI_RESET;
        case UI_STATUS_SCAN:  return BG_CYAN    "  SCAN  " ANSI_RESET;
        case UI_STATUS_TWEAK: return BG_MAGENTA " AJUSTE " ANSI_RESET;
        default:              return BG_CYAN    "  INFO  " ANSI_RESET;
    }
}

void ui_print_status(ui_status_t status, const char *title, const char *detail) {
    printf("  %s " ANSI_WHITE ANSI_BOLD "%-28s" ANSI_RESET, ui_badge_str(status), title);
    if (detail && detail[0] != '\0') {
        printf(" " ANSI_GRAY "%s" ANSI_RESET, detail);
    }
    printf("\n");
}

void ui_print_diagnostic_item(
    ui_status_t status,
    const char *title,
    const char *current_state,
    const char *why_it_matters,
    const char *recommendation
) {
    const char *state_col = (status == UI_STATUS_OK)   ? ANSI_GREEN :
                            (status == UI_STATUS_WARN) ? ANSI_AMBER :
                            (status == UI_STATUS_CRIT) ? ANSI_RED   : ANSI_CYAN;

    printf("  %s " ANSI_WHITE ANSI_BOLD "%-28s" ANSI_RESET " %s%s" ANSI_RESET "\n",
           ui_badge_str(status), title, state_col, current_state);

    if (why_it_matters && why_it_matters[0] != '\0') {
        printf("           " ANSI_DIM "├─ Impacto     :" ANSI_RESET " " ANSI_GRAY "%s" ANSI_RESET "\n", why_it_matters);
    }
    if (recommendation && recommendation[0] != '\0') {
        const char *rec_col = (status == UI_STATUS_OK) ? ANSI_DIM : ANSI_CYAN;
        printf("           " ANSI_DIM "╰─ Recomendacao:" ANSI_RESET " %s%s" ANSI_RESET "\n", rec_col, recommendation);
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

    int bar_width = 26;
    double ratio = (double)current / (double)total;
    int filled = (int)(ratio * bar_width);
    int pct = (int)(ratio * 100.0);

    printf(ANSI_CLEAR_LINE ANSI_HIDE_CURSOR "  " ANSI_CYAN ">>" ANSI_RESET " " ANSI_WHITE "%-24s" ANSI_RESET " [", label);
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

int ui_menu_select(const char *prompt_title, const char **items, const char **descriptions, int item_count) {
    int selected = 0;

    /* Check if stdin is an interactive console */
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD in_mode = 0;
    if (!GetConsoleMode(hIn, &in_mode)) {
        return item_count - 1;
    }

    printf("\n  " ANSI_CYAN ANSI_BOLD ":: %s" ANSI_RESET " " ANSI_DIM "(Use Setas CIMA/BAIXO, Numeros 1-%d ou ENTER)" ANSI_RESET "\n",
           prompt_title, item_count);
    ui_divider(84);

    printf(ANSI_HIDE_CURSOR);
    for (int i = 0; i < item_count; i++) {
        printf("\n\n");
    }

    while (1) {
        /* Move cursor up by item_count * 2 lines to redraw menu in-place */
        printf("\033[%dA", item_count * 2);

        for (int i = 0; i < item_count; i++) {
            printf(ANSI_CLEAR_LINE);
            if (i == selected) {
                printf("  " ANSI_CYAN ANSI_BOLD ">>" ANSI_RESET " " BG_SELECT " [%d] %-36s " ANSI_RESET "\n",
                       i + 1, items[i]);
                printf(ANSI_CLEAR_LINE "        " ANSI_CYAN "╰─ %s" ANSI_RESET "\n",
                       descriptions ? descriptions[i] : "");
            } else {
                printf("     " ANSI_GRAY "[%d] %-36s" ANSI_RESET "\n",
                       i + 1, items[i]);
                printf(ANSI_CLEAR_LINE "        " ANSI_DIM "   %s" ANSI_RESET "\n",
                       descriptions ? descriptions[i] : "");
            }
        }
        fflush(stdout);

        int ch = _getch();
        if (ch == 0 || ch == 224) {
            int ext = _getch();
            if (ext == 72) { /* Up arrow */
                selected = (selected - 1 + item_count) % item_count;
            } else if (ext == 80) { /* Down arrow */
                selected = (selected + 1) % item_count;
            }
        } else if (ch == 13) { /* Enter */
            printf(ANSI_SHOW_CURSOR);
            return selected;
        } else if (ch == 27 || ch == 'q' || ch == 'Q') { /* ESC or Q */
            printf(ANSI_SHOW_CURSOR);
            return item_count - 1;
        } else if (ch == 'w' || ch == 'W' || ch == 'k' || ch == 'K') {
            selected = (selected - 1 + item_count) % item_count;
        } else if (ch == 's' || ch == 'S' || ch == 'j' || ch == 'J') {
            selected = (selected + 1) % item_count;
        } else if (ch >= '1' && ch <= '9') {
            int idx = ch - '1';
            if (idx < item_count) {
                printf(ANSI_SHOW_CURSOR);
                return idx;
            }
        }
    }
}

bool ui_confirm_prompt(const char *question) {
    printf("\n  " BG_AMBER " CONFIRMACAO " ANSI_RESET " " ANSI_WHITE ANSI_BOLD "%s" ANSI_RESET " " ANSI_CYAN "[S/N]: " ANSI_RESET, question);
    fflush(stdout);

    int ch = _getch();
    printf("%c\n", (ch >= 32 && ch <= 126) ? ch : 'N');
    return (ch == 's' || ch == 'S' || ch == 'y' || ch == 'Y');
}

void ui_wait_for_key(const char *prompt_msg) {
    printf("\n  " ANSI_DIM ">> %s" ANSI_RESET,
           prompt_msg ? prompt_msg : "Pressione qualquer tecla para voltar ao Menu Principal...");
    fflush(stdout);
    _getch();
    printf("\n");
}
