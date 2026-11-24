#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <windows.h>

static DWORD g_original_out_mode = 0;
static UINT  g_original_cp = 0;
static bool  g_console_initialized = false;

static const char *FRAMES[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
};
#define FRAME_COUNT 10

/* Arte em meio-bloco (▄, ▀, █) de 35 colunas de largura visual, igual ao BANNER_ART do Turtle */
static const char *BANNER_ART[] = {
    "            ▄▄▄▄▄▄           ▄▄████",
    "         ███████████▄       ▄██████",
    "       ▄████████▀ ▄▄▄▄▄▄▄▄  ███████",
    "            ▀▀   ██████████  ▀███▀",
    "            ▄███ █████████▀▄▄▄",
    "          ▄██████ █████▀ ▄█████",
    "          ███████▄▄▄▄▄  ▄██████",
    "          ▀▀▀▀▀████████ ███████ ▄▄",
    "     ▄▄  ████▄ ████████ ▀██████ ██▄",
    "  ▄█████ █████ ███████▀▄▄▄▀▀▀▀ ▄███",
    " ▄██████ █████ ▀▀███▀▄██████▄  ████",
    " ███▀▀▀  ████▀▄▄███▄ ███████▀ █████",
    " ▀▀       ▀▀▄████████ ████▀    ███▀",
    "            ▀███████▀  ▀       ██▀",
    "               ▄▄▄▄▄          ▀▀",
    "              ▄█████",
    "              █████",
    "             ███▀▀"
};
#define BANNER_ROWS 18

static const char *BANNER_TEXT[] = {
    "",
    "",
    "",
    "",
    "",
    "Windows Tweaker",
    "o seu PC, limpo e otimizado",
    "",
    "diagnostico, limpeza de disco e",
    "ajustes do Windows, direto em C"
};
#define BANNER_TEXT_ROWS 10

/* Paleta do degrade diagonal identica ao FIRE do Turtle: vermelho -> laranja -> amarelo */
static const int FIRE[3][3] = {
    {226, 38,  38},
    {249, 115, 22},
    {250, 204, 21}
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
    printf(ANSI_RESET);
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

/* Conta quantas colunas visuais (codepoints UTF-8) existem ignorando codigos ANSI */
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
        /* Em UTF-8, bytes de continuacao tem os bits 10xxxxxx (0x80..0xBF) */
        if ((*p & 0xC0) != 0x80) {
            count++;
        }
        p++;
    }
    return count;
}

/* Imprime uma linha do BANNER_ART aplicando o degrade diagonal FIRE por codepoint UTF-8 */
static int print_fire_row(const char *line, int row) {
    const unsigned char *p = (const unsigned char *)line;
    int col = 0;

    while (*p) {
        /* Determina quantos bytes tem este caractere UTF-8 */
        int char_bytes = 1;
        if ((*p & 0xE0) == 0xC0) char_bytes = 2;
        else if ((*p & 0xF0) == 0xE0) char_bytes = 3;
        else if ((*p & 0xF8) == 0xF0) char_bytes = 4;

        if (*p == ' ') {
            putchar(' ');
        } else {
            double t = (((double)col / 35.0) + ((double)row / (double)(BANNER_ROWS - 1))) / 2.0 * 1.15;
            if (t > 1.0) t = 1.0;
            if (t < 0.0) t = 0.0;

            double k = t * 2.0; /* FIRE.length - 1 == 2 */
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
            int pad = 35 - visual_cols + 4;
            if (pad < 2) pad = 2;
            for (int s = 0; s < pad; s++) putchar(' ');

            if (text_idx == 5) {
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
    printf("\n" ANSI_MAGENTA "◆" ANSI_RESET " " ANSI_DIM "%d/%d" ANSI_RESET " " ANSI_BOLD "%s" ANSI_RESET "\n",
           n, total, title);
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
    printf("\r  " ANSI_CYAN "%s" ANSI_RESET " %s\x1b[K", FRAMES[frame % FRAME_COUNT], text);
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
        printf("╭─ " ANSI_BOLD "%s" ANSI_RESET " ", title);
        int dashes = w - title_len - 1;
        if (dashes < 0) dashes = 0;
        for (int i = 0; i < dashes; i++) printf("─");
        printf("╮\n");
    } else {
        printf("╭");
        for (int i = 0; i < w + 2; i++) printf("─");
        printf("╮\n");
    }

    for (int i = 0; i < line_count; i++) {
        int len = visible_utf8_len(lines[i]);
        int pad = w - len;
        if (pad < 0) pad = 0;
        printf("│ %s", lines[i]);
        for (int s = 0; s < pad; s++) putchar(' ');
        printf(" │\n");
    }

    printf("╰");
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

void ui_ask(const char *question, const char *def_val, char *out_buf, size_t buf_size) {
    if (def_val && def_val[0] != '\0') {
        printf("  %s " ANSI_DIM "[%s]" ANSI_RESET " ", question, def_val);
    } else {
        printf("  %s ", question);
    }
    fflush(stdout);

    if (!fgets(out_buf, (int)buf_size, stdin)) {
        printf(ANSI_DIM "\n  Cancelado.\n" ANSI_RESET);
        out_buf[0] = '\0';
        return;
    }

    /* Remove \r e \n do final */
    size_t len = strlen(out_buf);
    while (len > 0 && (out_buf[len - 1] == '\n' || out_buf[len - 1] == '\r' || out_buf[len - 1] == ' ')) {
        out_buf[--len] = '\0';
    }

    if (out_buf[0] == '\0' && def_val) {
        strncpy(out_buf, def_val, buf_size - 1);
        out_buf[buf_size - 1] = '\0';
    }
}

bool ui_confirm(const char *question, bool def_val) {
    char ans[32] = {0};
    printf("  %s " ANSI_DIM "%s" ANSI_RESET " ", question, def_val ? "[S/n]" : "[s/N]");
    fflush(stdout);

    if (!fgets(ans, sizeof(ans), stdin)) {
        return false;
    }

    size_t len = strlen(ans);
    while (len > 0 && (ans[len - 1] == '\n' || ans[len - 1] == '\r' || ans[len - 1] == ' ')) {
        ans[--len] = '\0';
    }

    if (ans[0] == '\0') return def_val;
    return (_stricmp(ans, "s") == 0 || _stricmp(ans, "sim") == 0 || _stricmp(ans, "y") == 0);
}

void ui_pause(void) {
    char dummy[16];
    printf("\n  " ANSI_DIM "Pressione Enter para continuar." ANSI_RESET " ");
    fflush(stdout);
    fgets(dummy, sizeof(dummy), stdin);
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
