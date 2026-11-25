#ifndef WT_TWEAKS_H
#define WT_TWEAKS_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_STARTUP_ENTRIES 32
#define MAX_TWEAK_RULES     12

typedef enum {
    TWEAK_PWR_HIGH_PERF = 0,     /* 1. Plano de Energia Alto Desempenho */
    TWEAK_PWR_HIBER_REDUCED,     /* 2. Hibernacao Compacta (reduz hiberfil.sys pela metade) */
    TWEAK_GAME_DVR_OFF,          /* 3. Desativar Captura em 2o Plano do Xbox GameDVR */
    TWEAK_GAME_MODE_ON,          /* 4. Ativar Modo de Jogo Automatico (AutoGameMode) */
    TWEAK_GAME_MOUSE_RAW,        /* 5. Mouse 1:1 Raw Input (Desativar aceleracao do ponteiro) */
    TWEAK_GAME_MMCSS_PRIORITY,   /* 6. Prioridade Maxima de GPU/CPU para Jogos (MMCSS) */
    TWEAK_NET_WU_P2P_OFF,        /* 7. Bloquear Upload P2P do Windows Update (DeliveryOptimization) */
    TWEAK_RAM_BG_APPS_OFF,       /* 8. Bloquear Apps da Microsoft Store rodando em 2o plano */
    TWEAK_PRIV_BING_SEARCH_OFF,  /* 9. Desativar Busca Web do Bing no Menu Iniciar */
    TWEAK_PRIV_BLOATWARE_ADS_OFF,/* 10. Bloquear Instalacao Silenciosa de Bloatware e ID de Anuncios */
    TWEAK_UI_FAST_MENUS_EXPLORER,/* 11. Menus Instantaneos (20ms) e Sem Anuncios no Explorer */
    TWEAK_UI_FAST_KILL_HUNG_APPS /* 12. Fechar Programas Travados em 2s (em vez de 20s) */
} tweak_id_t;

typedef struct {
    tweak_id_t id;
    char       category[32];     /* "ENERGIA & CPU", "JOGOS & INPUT", "REDE & PING", "RAM, PRIVACIDADE & UI" */
    char       title[64];
    char       current_state[80];
    char       target_state[80];
    char       tech_location[120];
    char       explanation[160];
    bool       is_optimized;
    int        score_weight;     /* Pontos que vale na nota de 0 a 100 */
} tweak_rule_t;

typedef struct {
    char name[64];
    char command[260];
    char source[16];     /* "HKCU\\Run" ou "HKLM\\Run" */
    bool is_hkcu;
    bool enabled;        /* Lido de StartupApproved\Run */
} startup_entry_t;

typedef struct {
    /* Startup Apps */
    startup_entry_t startup_apps[MAX_STARTUP_ENTRIES];
    int             startup_count;
    int             startup_enabled_count;
    int             startup_disabled_count;

    /* 12 Regras de Otimizacao Reais */
    tweak_rule_t    rules[MAX_TWEAK_RULES];
    int             rule_count;
    int             pending_tweaks_count;
    int             optimized_tweaks_count;

    /* Campos rapidos de status */
    char            power_plan_guid[64];
    char            power_plan_name[64];
    bool            is_high_performance;
    bool            ntfs_trim_enabled;
    bool            vbs_enabled; /* Virtualization-Based Security (Isolamento de Nucleo) */

    int             issues_detected;
    double          audit_elapsed_ms;
} tweaks_report_t;

/* Audita todas as 12 regras de otimizacao, StartupApproved\Run, TRIM e VBS */
void tweaks_run_audit(tweaks_report_t *report);

/* Imprime resumo das categorias no Diagnostico Completo */
void tweaks_print_report(const tweaks_report_t *report);

/* Imprime lista de programas na inicializacao separando HABILITADOS de DESABILITADOS */
void tweaks_print_startup(const tweaks_report_t *report);

/* Habilita ou Desabilita um programa da inicializacao gravando em StartupApproved\Run */
bool tweaks_toggle_startup_entry(startup_entry_t *entry, bool new_enabled_state);

/* Ativa imediatamente o Plano de Energia 'Alto Desempenho' via powercfg */
bool tweaks_activate_high_performance_power(void);

/* Limpa o cache de resolucao DNS do Windows (ipconfig /flushdns) */
bool tweaks_flush_dns_cache(void);

/* Inspeciona todas as 12 regras separando PENDENTE de JA OTIMIZADO e retorna quantas estao pendentes */
int  tweaks_inspect_optimizations(tweaks_report_t *out_current);

/* Aplica uma unica regra especifica pelo seu ID (retorna true se aplicada com sucesso) */
bool tweaks_apply_single_rule(tweak_id_t id);

/* Aplica todas as regras que estiverem PENDENTES no relatorio */
int  tweaks_apply_pending_optimizations(const tweaks_report_t *current);

/* Restaura os valores padrao originais de fabrica do Windows */
void tweaks_restore_windows_defaults(void);

#endif /* WT_TWEAKS_H */
