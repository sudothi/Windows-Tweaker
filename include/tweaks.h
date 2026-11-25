#ifndef WT_TWEAKS_H
#define WT_TWEAKS_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_STARTUP_ENTRIES 32

typedef struct {
    char name[64];
    char command[260];
    char source[16];     /* "HKCU\\Run" ou "HKLM\\Run" */
    bool is_hkcu;        /* true = HKCU, false = HKLM */
    bool enabled;        /* Lido de StartupApproved\Run (true = Habilitado, false = Desabilitado no Task Manager) */
} startup_entry_t;

typedef struct {
    /* Startup Apps */
    startup_entry_t startup_apps[MAX_STARTUP_ENTRIES];
    int             startup_count;         /* Total encontrado nas chaves Run */
    int             startup_enabled_count; /* Quantos estao realmente HABILITADOS no Gerenciador de Tarefas */
    int             startup_disabled_count;/* Quantos ja foram DESABILITADOS pelo usuario */

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

    /* Contadores precisos */
    int             pending_tweaks_count; /* Quantos ajustes automaticos ainda NAO estao otimizados */
    int             issues_detected;
    double          audit_elapsed_ms;
} tweaks_report_t;

/* Audita Registro (incluindo StartupApproved\Run), plano de energia e TRIM */
void tweaks_run_audit(tweaks_report_t *report);

/* Imprime relatorio de sistema no Diagnostico Completo */
void tweaks_print_report(const tweaks_report_t *report);

/* Imprime lista de programas na inicializacao separando HABILITADOS de DESABILITADOS */
void tweaks_print_startup(const tweaks_report_t *report);

/* Habilita ou Desabilita um programa da inicializacao gravando em StartupApproved\Run (igual ao Task Manager) */
bool tweaks_toggle_startup_entry(startup_entry_t *entry, bool new_enabled_state);

/* Ativa imediatamente o Plano de Energia 'Alto Desempenho' via powercfg */
bool tweaks_activate_high_performance_power(void);

/* Inspeciona os 4 ajustes de desempenho separando PENDENTE de JA OTIMIZADO */
int  tweaks_inspect_optimizations(tweaks_report_t *out_current);

/* Aplica apenas os ajustes que ainda estao pendentes */
int  tweaks_apply_pending_optimizations(const tweaks_report_t *current);

/* Restaura os valores padrao originais do Windows */
void tweaks_restore_windows_defaults(void);

#endif /* WT_TWEAKS_H */
