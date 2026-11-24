#ifndef WT_UI_H
#define WT_UI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * WINDOWS TWEAKER - TERMINAL UI (TURTLE STYLE + 100% SETAS & ENTER)
 * ============================================================================ */

#define ANSI_RESET   "\x1b[0m"
#define ANSI_BOLD    "\x1b[1m"
#define ANSI_DIM     "\x1b[2m"
#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_MAGENTA "\x1b[35m"

/* Console init & restore */
bool ui_init(void);
void ui_cleanup(void);
void ui_clear(void);

/* Banner com o trevo de 3 folhas em degrade monocromatico verde esmeralda */
void ui_banner(void);

/* Secoes e blocos estruturados faceis de ler */
void ui_step(int n, int total, const char *title);
void ui_header_box(const char *title, const char *subtitle);
void ui_item_block(
    bool is_ok,
    const char *name,
    const char *current_value,
    const char *what_it_is,
    const char *action_hint
);

/* Primitivas de linha */
void ui_ok(const char *fmt, ...);
void ui_warn(const char *fmt, ...);
void ui_fail(const char *fmt, ...);
void ui_info(const char *fmt, ...);
void ui_line(const char *fmt, ...);

/* Spinner de uma linha */
void ui_spinner_update(int frame, const char *text);
void ui_spinner_stop(const char *text, bool good);

/* Caixa com borda dinamica igual ao box(lines, title) do Turtle */
void ui_box(const char **lines, int line_count, const char *title);

/* Barra de uso limpa */
void ui_bar(double ratio, int width, char *out_buf, size_t buf_size);

/* Seletores 100% controlados por Setas (CIMA/BAIXO) e ENTER */
int  ui_select(const char *title, const char **labels, const char **explanations, int count);
bool ui_confirm_arrows(const char *question, const char *detail_what_will_happen);
void ui_pause_enter(const char *msg);

/* Formatador de bytes (KB, MB, GB, TB) */
void ui_format_bytes(uint64_t bytes, char *out_buf, size_t buf_size);

#endif /* WT_UI_H */
