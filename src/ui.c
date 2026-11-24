#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <windows.h>
#include <conio.h>

static DWORD g_original_out_mode = 0;
static UINT  g_original_cp = 0;
static bool  g_console_initialized = false;

static const char *FRAMES[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
};
#define FRAME_COUNT 10

/* Logo 1 do Windows Tweaker calibrada para a proporcao real da fonte (24 colunas x 9 linhas) */
static const char *BANNER_ART[] = {
    "      ▄████▄▄████▄",
    "     ▀████████████▀",
    "      ▀██████████▀",
    "▄██████████████████████▄",
    "████████████████████████",
    " ▀████████████████████▀",
    "  ████████▀██▀████████",
    "   ▀▀█▀▀  ▄██   ▀▀█▀▀",
    "         ███"
};
#define BANNER_ROWS 9

static const char *BANNER_TEXT[] = {
    "",
    "Windows Tweaker",
    "o seu PC, limpo e otimizado",
    "",
    "diagnostico, limpeza de disco e",
    "ajustes do Windows, direto em C",
    "",
    "",
    ""
};
#define BANNER_TEXT_ROWS 9

/* Degrade monocromatico do trevo: 3 tons de Verde Esmeralda (Profundo -> Vivo -> Menta Claro) */
static const int FIRE[3][3] = {
    {5,   150, 105}, /* #059669 Esmeralda Profundo */
    {16,  185, 129}, /* #10B981 Esmeralda Vivo     */
    {110, 231, 183}  /* #6EE7B7 Menta Claro        */
};

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
    printf("\x1b[?25h" ANSI_RESET);
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

void ui_clear(void) {
    printf("\x1b[2J\x1b[H");
    fflush(stdout);
}

static int visible_utf8_len(const char *s) {
    int count = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        if (*p == 0x1B && *(p + 1) == '[') {
            p += 2;
            while (*p && *p != 'm') p++;
            if (*p == 'm') p++;
            continue;
        }
        if ((*p & 0xC0) != 0x80) {
            count++;
        }
        p++;
    }
    return count;
}

static int print_fire_row(const char *line, int row) {
    const unsigned char *p = (const unsigned char *)line;
    int col = 0;

    while (*p) {
        int char_bytes = 1;
        if ((*p & 0xE0) == 0xC0) char_bytes = 2;
        else if ((*p & 0xF0) == 0xE0) char_bytes = 3;
        else if ((*p & 0xF8) == 0xF0) char_bytes = 4;

        if (*p == ' ') {
            putchar(' ');
        } else {
            double t = (((double)col / 24.0) + ((double)row / (double)(BANNER_ROWS - 1))) / 2.0 * 1.15;
            if (t > 1.0) t = 1.0;
            if (t < 0.0) t = 0.0;

            double k = t * 2.0;
            int idx = (int)k;
            if (idx > 1) idx = 1;
            double f = k - (double)idx;

            int r  = (int)(FIRE[idx][0] + (FIRE[idx + 1][0] - FIRE[idx][0]) * f + 0.5);
            int g  = (int)(FIRE[idx][1] + (FIRE[idx + 1][1] - FIRE[idx][1]) * f + 0.5);
            int bl = (int)(FIRE[idx][2] + (FIRE[idx + 1][2] - FIRE[idx][2]) * f + 0.5);

            printf("\x1b[38;2;%d;%d;%dm", r, g, bl);
            for (int b = 0; b < char_bytes && p[b]; b++) {
                putchar(p[b]);
            }
        }

        p += char_bytes;
        col++;
    }
    printf(ANSI_RESET);
    return col;
}

void ui_banner(void) {
    printf("\n");
    int off = (BANNER_ROWS - BANNER_TEXT_ROWS) / 2;
    if (off < 0) off = 0;

    for (int i = 0; i < BANNER_ROWS; i++) {
        printf("  ");
        int visual_cols = print_fire_row(BANNER_ART[i], i);

        int text_idx = i - off;
        if (text_idx >= 0 && text_idx < BANNER_TEXT_ROWS && BANNER_TEXT[text_idx][0] != '\0') {
            int pad = 24 - visual_cols + 5;
            if (pad < 2) pad = 2;
            for (int s = 0; s < pad; s++) putchar(' ');

            if (text_idx == 1) {
                printf(ANSI_BOLD "%s" ANSI_RESET, BANNER_TEXT[text_idx]);
            } else {
                printf(ANSI_DIM "%s" ANSI_RESET, BANNER_TEXT[text_idx]);
            }
        }
        printf("\n");
    }
    printf("\n");
}

void ui_step(int n, int total, const char *title) {
    printf("\n" ANSI_GREEN "◆" ANSI_RESET " " ANSI_DIM "ETAPA %d/%d" ANSI_RESET "  " ANSI_BOLD "%s" ANSI_RESET "\n",
           n, total, title);
    printf("  " ANSI_DIM "──────────────────────────────────────────────────────────────────────────" ANSI_RESET "\n");
}

void ui_header_box(const char *title, const char *subtitle) {
    printf("\n" ANSI_GREEN "◆" ANSI_RESET " " ANSI_BOLD "%s" ANSI_RESET "\n", title);
    if (subtitle && subtitle[0] != '\0') {
        printf("  " ANSI_DIM "%s" ANSI_RESET "\n", subtitle);
    }
    printf("  " ANSI_DIM "──────────────────────────────────────────────────────────────────────────" ANSI_RESET "\n");
}

void ui_item_block(
    bool is_ok,
    const char *name,
    const char *current_value,
    const char *what_it_is,
    const char *action_hint
) {
    if (is_ok) {
        printf("  " ANSI_GREEN "✓" ANSI_RESET " " ANSI_BOLD "%-28s" ANSI_RESET " " ANSI_GREEN "%s" ANSI_RESET "\n",
               name, current_value);
    } else {
        printf("  " ANSI_YELLOW "!" ANSI_RESET " " ANSI_BOLD "%-28s" ANSI_RESET " " ANSI_YELLOW "%s" ANSI_RESET "\n",
               name, current_value);
    }

    if (what_it_is && what_it_is[0] != '\0') {
        printf("    " ANSI_DIM "│ O que e     : %s" ANSI_RESET "\n", what_it_is);
    }
    if (action_hint && action_hint[0] != '\0') {
        if (is_ok) {
            printf("    " ANSI_DIM "╰ Status      : %s" ANSI_RESET "\n", action_hint);
        } else {
            printf("    " ANSI_DIM "╰ " ANSI_RESET ANSI_CYAN "Como melhorar: %s" ANSI_RESET "\n", action_hint);
        }
    }
    printf("\n");
}

void ui_ok(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("  " ANSI_GREEN "✓" ANSI_RESET " ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void ui_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("  " ANSI_YELLOW "!" ANSI_RESET " ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void ui_fail(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("  " ANSI_RED "✗" ANSI_RESET " ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void ui_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("  " ANSI_DIM "·" ANSI_RESET " ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void ui_line(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("  " ANSI_DIM "│" ANSI_RESET " ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void ui_spinner_update(int frame, const char *text) {
    printf("\r  " ANSI_GREEN "%s" ANSI_RESET " %s\x1b[K", FRAMES[frame % FRAME_COUNT], text);
    fflush(stdout);
}

void ui_spinner_stop(const char *text, bool good) {
    printf("\r\x1b[K");
    if (good) {
        ui_ok("%s", text);
    } else {
        ui_fail("%s", text);
    }
    fflush(stdout);
}

void ui_box(const char **lines, int line_count, const char *title) {
    int title_len = title ? visible_utf8_len(title) : 0;
    int w = (title_len > 0) ? (title_len + 2) : 0;

    for (int i = 0; i < line_count; i++) {
        int len = visible_utf8_len(lines[i]);
        if (len > w) w = len;
    }

    printf("\n");
    if (title && title[0] != '\0') {
        printf("  ╭─ " ANSI_BOLD "%s" ANSI_RESET " ", title);
        int dashes = w - title_len - 1;
        if (dashes < 0) dashes = 0;
        for (int i = 0; i < dashes; i++) printf("─");
        printf("╮\n");
    } else {
        printf("  ╭");
        for (int i = 0; i < w + 2; i++) printf("─");
        printf("╮\n");
    }

    for (int i = 0; i < line_count; i++) {
        int len = visible_utf8_len(lines[i]);
        int pad = w - len;
        if (pad < 0) pad = 0;
        printf("  │ %s", lines[i]);
        for (int s = 0; s < pad; s++) putchar(' ');
        printf(" │\n");
    }

    printf("  ╰");
    for (int i = 0; i < w + 2; i++) printf("─");
    printf("╯\n");
}

void ui_bar(double ratio, int width, char *out_buf, size_t buf_size) {
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;

    int filled = (int)(ratio * width + 0.5);
    const char *col = (ratio >= 0.85) ? ANSI_RED :
                      (ratio >= 0.65) ? ANSI_YELLOW : ANSI_GREEN;

    size_t pos = 0;
    pos += (size_t)snprintf(out_buf + pos, buf_size - pos, "%s", col);
    for (int i = 0; i < filled && pos + 8 < buf_size; i++) {
        pos += (size_t)snprintf(out_buf + pos, buf_size - pos, "█");
    }
    pos += (size_t)snprintf(out_buf + pos, buf_size - pos, "%s", ANSI_DIM);
    for (int i = filled; i < width && pos + 8 < buf_size; i++) {
        pos += (size_t)snprintf(out_buf + pos, buf_size - pos, "░");
    }
    snprintf(out_buf + pos, buf_size - pos, "%s", ANSI_RESET);
}

int ui_select(const char *title, const char **labels, const char **explanations, int count) {
    int selected = 0;

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD in_mode = 0;
    if (!GetConsoleMode(hIn, &in_mode)) {
        return count - 1;
    }

    printf("\n" ANSI_GREEN "◆" ANSI_RESET " " ANSI_BOLD "%s" ANSI_RESET
           "  " ANSI_DIM "(Use as setas ↑ / ↓ para mover e ENTER para confirmar)" ANSI_RESET "\n",
           title);
    printf("  " ANSI_DIM "──────────────────────────────────────────────────────────────────────────" ANSI_RESET "\n");

    printf("\x1b[?25l");
    /* Reserva 2 linhas por item para redesenho in-place */
    for (int i = 0; i < count; i++) {
        printf("\n\n");
    }

    for (;;) {
        printf("\x1b[%dA", count * 2);

        for (int i = 0; i < count; i++) {
            const char *desc = (explanations && explanations[i]) ? explanations[i] : "";
            printf("\x1b[2K\r");
            if (i == selected) {
                printf("  " ANSI_GREEN ANSI_BOLD "❯ %s" ANSI_RESET "\n", labels[i]);
                printf("\x1b[2K\r    " ANSI_CYAN "╰─ O que faz: %s" ANSI_RESET "\n", desc);
            } else {
                printf("    " ANSI_DIM "%s" ANSI_RESET "\n", labels[i]);
                printf("\x1b[2K\r    " ANSI_DIM "   %s" ANSI_RESET "\n", "");
            }
        }
        fflush(stdout);

        int ch = _getch();
        if (ch == 0 || ch == 224) {
            int ext = _getch();
            if (ext == 72) { /* Seta CIMA */
                selected = (selected - 1 + count) % count;
            } else if (ext == 80) { /* Seta BAIXO */
                selected = (selected + 1) % count;
            }
        } else if (ch == 13) { /* ENTER */
            printf("\x1b[?25h");
            return selected;
        } else if (ch == 27) { /* ESC -> ultima opcao (Voltar/Sair) */
            printf("\x1b[?25h");
            return count - 1;
        } else if (ch == 'w' || ch == 'W' || ch == 'k' || ch == 'K') {
            selected = (selected - 1 + count) % count;
        } else if (ch == 's' || ch == 'S' || ch == 'j' || ch == 'J') {
            selected = (selected + 1) % count;
        } else if (ch >= '1' && ch <= '9') {
            int idx = ch - '1';
            if (idx < count) {
                printf("\x1b[?25h");
                return idx;
            }
        }
    }
}

bool ui_confirm_arrows(const char *question, const char *detail_what_will_happen) {
    printf("\n  " ANSI_YELLOW "!" ANSI_RESET " " ANSI_BOLD "%s" ANSI_RESET "\n", question);
    if (detail_what_will_happen && detail_what_will_happen[0] != '\0') {
        printf("    " ANSI_DIM "│ %s" ANSI_RESET "\n", detail_what_will_happen);
    }

    const char *opts[] = {
        "Sim, confirmar e executar agora",
        "Nao, cancelar e voltar"
    };
    const char *descs[] = {
        detail_what_will_happen ? detail_what_will_happen : "Executa a operacao selecionada",
        "Cancela a operacao sem alterar nem apagar nada"
    };

    int choice = ui_select("CONFIRMAR OPERACAO", opts, descs, 2);
    return (choice == 0);
}

void ui_pause_enter(const char *msg) {
    printf("\n  " ANSI_GREEN "❯" ANSI_RESET " " ANSI_BOLD "%s" ANSI_RESET
           " " ANSI_DIM "(Pressione ENTER)" ANSI_RESET,
           msg ? msg : "Voltar ao menu");
    fflush(stdout);
    for (;;) {
        int ch = _getch();
        if (ch == 13 || ch == 27 || ch == ' ') break;
    }
    printf("\n");
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
