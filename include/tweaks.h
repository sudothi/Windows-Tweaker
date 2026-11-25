#ifndef WT_TWEAKS_H
#define WT_TWEAKS_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_STARTUP_ENTRIES 24

typedef struct {
    char name[64];
    char command[260];
    char source[16]; /* "HKCU\\Run" or "HKLM\\Run" */
} startup_entry_t;

typedef struct {
    /* Startup Apps */
    startup_entry_t startup_apps[MAX_STARTUP_ENTRIES];
    int             startup_count;

    /* Power Plan */
    char            power_plan_guid[64];
    char            power_plan_name[64];
    bool            is_high_performance;

    /* Storage & Filesystem */
    bool            ntfs_trim_enabled;
    bool            ntfs_last_access_disabled;

    /* Gaming & Responsiveness */
    bool            game_dvr_enabled;
    int             menu_show_delay_ms;
    uint32_t        system_responsiveness_pct;
    bool            transparency_enabled;
    bool            explorer_ads_enabled; /* ShowSyncProviderNotifications */

    /* Contadores separados para nao confundir o usuario */
    int             pending_tweaks_count; /* Quantos ajustes automaticos ainda NAO estao otimizados */
    int             issues_detected;      /* Total geral (incluindo excesso de apps no boot) */
    double          audit_elapsed_ms;
} tweaks_report_t;

/* Audita Registro, plano de energia, TRIM do SSD e responsividade */
void tweaks_run_audit(tweaks_report_t *report);

/* Imprime relatorio de sistema no Diagnostico Completo */
void tweaks_print_report(const tweaks_report_t *report);

/* Imprime lista de programas na inicializacao (Boot) */
void tweaks_print_startup(const tweaks_report_t *report);

/* Inspeciona os ajustes de desempenho separando o que esta PENDENTE do que JA ESTA OTIMIZADO.
 * Retorna a quantidade de ajustes que ainda estao PENDENTES. */
int  tweaks_inspect_optimizations(tweaks_report_t *out_current);

/* Aplica apenas os ajustes que ainda estao pendentes (nao reexecuta o que ja esta otimizado) */
int  tweaks_apply_pending_optimizations(const tweaks_report_t *current);

/* Restaura os valores padrao originais do Windows caso o usuario queira desfazer */
void tweaks_restore_windows_defaults(void);

#endif /* WT_TWEAKS_H */
