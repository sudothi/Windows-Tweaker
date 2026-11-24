#ifndef WT_UI_H
#define WT_UI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * WINDOWS TWEAKER - TERMINAL UI (BASEADO EM SUDOTHI/TURTLE apps/cli/src/ui.mjs)
 * Sem blocos de fundo azul. Apenas cores limpas, icone half-block em degrade
 * diagonal, ◆ passos, ✓/!/✗/·/│, spinner de uma linha e box() dinamico.
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

/* Banner no estilo exato do Turtle (icone half-block em degrade diagonal + texto ao lado) */
void ui_banner(void);

/* Primitivas identicas ao apps/cli/src/ui.mjs do Turtle */
void ui_step(int n, int total, const char *title);
void ui_ok(const char *fmt, ...);
void ui_warn(const char *fmt, ...);
void ui_fail(const char *fmt, ...);
void ui_info(const char *fmt, ...);
void ui_line(const char *fmt, ...);

/* Spinner de uma linha (\r  ⠋ texto\x1b[K) -> fecha com ui_ok ou ui_fail */
void ui_spinner_update(int frame, const char *text);
void ui_spinner_stop(const char *text, bool good);

/* Caixa com borda dinamica igual ao box(lines, title) do Turtle */
void ui_box(const char **lines, int line_count, const char *title);

/* Barra de uso limpa sem fundo colorido */
void ui_bar(double ratio, int width, char *out_buf, size_t buf_size);

/* Prompts de terminal identicos ao prompter() do Turtle */
void ui_ask(const char *question, const char *def_val, char *out_buf, size_t buf_size);
bool ui_confirm(const char *question, bool def_val);
void ui_pause(void);

/* Formatador de bytes (KB, MB, GB, TB) */
void ui_format_bytes(uint64_t bytes, char *out_buf, size_t buf_size);

#endif /* WT_UI_H */
