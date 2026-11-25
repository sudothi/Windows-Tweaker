#include "tweaks.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static bool reg_read_dword(HKEY root, const char *subkey, const char *value_name, DWORD *out_val) {
    HKEY hKey;
    if (RegOpenKeyExA(root, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD data = 0;
    DWORD data_size = sizeof(data);
    DWORD type = 0;
    LONG res = RegQueryValueExA(hKey, value_name, NULL, &type, (LPBYTE)&data, &data_size);
    RegCloseKey(hKey);
    if (res == ERROR_SUCCESS && type == REG_DWORD) {
        *out_val = data;
        return true;
    }
    return false;
}

static bool reg_read_string(HKEY root, const char *subkey, const char *value_name, char *out_buf, DWORD buf_size) {
    HKEY hKey;
    if (RegOpenKeyExA(root, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0;
    LONG res = RegQueryValueExA(hKey, value_name, NULL, &type, (LPBYTE)out_buf, &buf_size);
    RegCloseKey(hKey);
    return (res == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ));
}

static bool reg_write_dword(HKEY root, const char *subkey, const char *value_name, DWORD val) {
    HKEY hKey;
    if (RegCreateKeyExA(root, subkey, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }
    LONG res = RegSetValueExA(hKey, value_name, 0, REG_DWORD, (const BYTE *)&val, sizeof(val));
    RegCloseKey(hKey);
    return (res == ERROR_SUCCESS);
}

static bool reg_write_string(HKEY root, const char *subkey, const char *value_name, const char *str_val) {
    HKEY hKey;
    if (RegCreateKeyExA(root, subkey, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }
    DWORD len = (DWORD)strlen(str_val) + 1;
    LONG res = RegSetValueExA(hKey, value_name, 0, REG_SZ, (const BYTE *)str_val, len);
    RegCloseKey(hKey);
    return (res == ERROR_SUCCESS);
}

/* Verifica na chave StartupApproved\Run (usada pelo Gerenciador de Tarefas do Windows)
 * se um item de inicializacao esta HABILITADO (true) ou DESABILITADO (false).
 * No Windows 10/11:
 *   byte[0] == 0x02 (ou par) -> Habilitado
 *   byte[0] == 0x03 (ou impar: bit 0 == 1) -> Desabilitado pelo usuario no Task Manager
 */
static bool is_startup_entry_enabled(HKEY root, const char *value_name) {
    const char *approved_keys[] = {
        "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",
        "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run32"
    };

    for (int k = 0; k < 2; k++) {
        HKEY hKey;
        if (RegOpenKeyExA(root, approved_keys[k], 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            BYTE buf[32] = {0};
            DWORD buf_size = sizeof(buf);
            DWORD type = 0;
            LONG res = RegQueryValueExA(hKey, value_name, NULL, &type, buf, &buf_size);
            RegCloseKey(hKey);

            if (res == ERROR_SUCCESS && type == REG_BINARY && buf_size >= 4) {
                /* Se o bit 0 do primeiro byte estiver ligado (ex: 0x03, 0x01, 0x07), esta DESABILITADO */
                if ((buf[0] & 0x01) != 0) {
                    return false;
                }
                return true;
            }
        }
    }

    /* Se nao houver registro em StartupApproved, por padrao o Windows considera Habilitado */
    return true;
}

static void enumerate_startup_key(HKEY root, const char *subkey, const char *source_label, bool is_hkcu, tweaks_report_t *report) {
    HKEY hKey;
    if (RegOpenKeyExA(root, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return;
    }

    DWORD index = 0;
    char value_name[128];
    BYTE value_data[260];
    DWORD name_len, data_len, type;

    while (report->startup_count < MAX_STARTUP_ENTRIES) {
        name_len = sizeof(value_name);
        data_len = sizeof(value_data) - 1;
        LONG res = RegEnumValueA(hKey, index++, value_name, &name_len, NULL, &type, value_data, &data_len);
        if (res != ERROR_SUCCESS) {
            break;
        }
        if (type == REG_SZ || type == REG_EXPAND_SZ) {
            value_data[data_len] = '\0';
            startup_entry_t *entry = &report->startup_apps[report->startup_count++];
            strncpy(entry->name, value_name, sizeof(entry->name) - 1);
            strncpy(entry->command, (const char *)value_data, sizeof(entry->command) - 1);
            strncpy(entry->source, source_label, sizeof(entry->source) - 1);
            entry->is_hkcu = is_hkcu;
            entry->enabled = is_startup_entry_enabled(root, value_name);

            if (entry->enabled) {
                report->startup_enabled_count++;
            } else {
                report->startup_disabled_count++;
            }
        }
    }

    RegCloseKey(hKey);
}

bool tweaks_toggle_startup_entry(startup_entry_t *entry, bool new_enabled_state) {
    HKEY root = entry->is_hkcu ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    const char *subkey = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";

    HKEY hKey;
    if (RegCreateKeyExA(root, subkey, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }

    BYTE data[12];
    memset(data, 0, sizeof(data));

    if (new_enabled_state) {
        /* 02 00 00 00 00 00 00 00 00 00 00 00 = Habilitado no Task Manager */
        data[0] = 0x02;
    } else {
        /* 03 00 00 00 + FILETIME atual de 8 bytes = Desabilitado no Task Manager */
        data[0] = 0x03;
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        memcpy(data + 4, &ft, sizeof(FILETIME));
    }

    LONG res = RegSetValueExA(hKey, entry->name, 0, REG_BINARY, data, sizeof(data));
    RegCloseKey(hKey);

    if (res == ERROR_SUCCESS) {
        entry->enabled = new_enabled_state;
        return true;
    }
    return false;
}

bool tweaks_activate_high_performance_power(void) {
    /* Tenta ativar o esquema Alto Desempenho padrao (8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c) */
    int rc = system("powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c >nul 2>nul");
    if (rc != 0) {
        /* Em alguns notebooks Modern Standby, duplica o template oficial primeiro e depois ativa */
        system("powercfg /duplicatescheme 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c >nul 2>nul");
        rc = system("powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c >nul 2>nul");
    }

    char active_guid[64] = {0};
    if (reg_read_string(HKEY_LOCAL_MACHINE,
                        "SYSTEM\\CurrentControlSet\\Control\\Power\\User\\PowerSchemes",
                        "ActivePowerScheme",
                        active_guid, sizeof(active_guid))) {
        if (_stricmp(active_guid, "8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c") == 0 ||
            _stricmp(active_guid, "e9a42b02-d5df-448d-aa00-03f14749eb61") == 0) {
            return true;
        }
    }
    return (rc == 0);
}

void tweaks_run_audit(tweaks_report_t *report) {
    memset(report, 0, sizeof(*report));

    LARGE_INTEGER freq, t_start, t_end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t_start);

    ui_spinner_update(0, "Lendo status real de inicializacao (StartupApproved\\Run)...");

    enumerate_startup_key(HKEY_CURRENT_USER,
                          "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          "HKCU\\Run", true, report);
    enumerate_startup_key(HKEY_LOCAL_MACHINE,
                          "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          "HKLM\\Run", false, report);

    /* So considera problema se houver mais de 6 programas REALMENTE HABILITADOS */
    if (report->startup_enabled_count > 6) {
        report->issues_detected++;
    }

    /* 1. Power Plan */
    ui_spinner_update(2, "Conferindo plano de energia da CPU...");
    if (reg_read_string(HKEY_LOCAL_MACHINE,
                        "SYSTEM\\CurrentControlSet\\Control\\Power\\User\\PowerSchemes",
                        "ActivePowerScheme",
                        report->power_plan_guid, sizeof(report->power_plan_guid))) {
        if (_stricmp(report->power_plan_guid, "8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c") == 0) {
            strncpy(report->power_plan_name, "Alto Desempenho", sizeof(report->power_plan_name) - 1);
            report->is_high_performance = true;
        } else if (_stricmp(report->power_plan_guid, "e9a42b02-d5df-448d-aa00-03f14749eb61") == 0) {
            strncpy(report->power_plan_name, "Desempenho Maximo", sizeof(report->power_plan_name) - 1);
            report->is_high_performance = true;
        } else if (_stricmp(report->power_plan_guid, "a1841308-3541-4fab-bc81-f71556f20b4a") == 0) {
            strncpy(report->power_plan_name, "Economia de Energia", sizeof(report->power_plan_name) - 1);
            report->is_high_performance = false;
            report->pending_tweaks_count++;
            report->issues_detected++;
        } else {
            strncpy(report->power_plan_name, "Equilibrado (Padrao)", sizeof(report->power_plan_name) - 1);
            report->is_high_performance = false;
            report->pending_tweaks_count++;
            report->issues_detected++;
        }
    } else {
        strncpy(report->power_plan_name, "Equilibrado (Padrao)", sizeof(report->power_plan_name) - 1);
        report->is_high_performance = false;
        report->pending_tweaks_count++;
    }

    /* 2. SSD TRIM */
    ui_spinner_update(4, "Verificando TRIM do SSD e agendador de jogos...");
    DWORD disable_trim = 0;
    if (reg_read_dword(HKEY_LOCAL_MACHINE,
                       "SYSTEM\\CurrentControlSet\\Control\\FileSystem",
                       "DisableDeleteNotify", &disable_trim)) {
        report->ntfs_trim_enabled = (disable_trim == 0);
    } else {
        report->ntfs_trim_enabled = true;
    }
    if (!report->ntfs_trim_enabled) {
        report->issues_detected++;
    }

    /* 3. Xbox GameDVR */
    DWORD dvr_enabled = 0, app_capture = 0;
    bool has_dvr = reg_read_dword(HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_Enabled", &dvr_enabled);
    bool has_cap = reg_read_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", "AppCaptureEnabled", &app_capture);
    report->game_dvr_enabled = (has_dvr && dvr_enabled != 0) || (has_cap && app_capture != 0);
    if (report->game_dvr_enabled) {
        report->pending_tweaks_count++;
        report->issues_detected++;
    }

    /* 4. MenuShowDelay */
    char delay_str[32] = {0};
    if (reg_read_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", delay_str, sizeof(delay_str))) {
        report->menu_show_delay_ms = atoi(delay_str);
    } else {
        report->menu_show_delay_ms = 400;
    }
    if (report->menu_show_delay_ms > 50) {
        report->pending_tweaks_count++;
        report->issues_detected++;
    }

    /* 5. Sugestoes/Anuncios no Windows Explorer */
    DWORD sync_ads = 1;
    if (reg_read_dword(HKEY_CURRENT_USER,
                       "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                       "ShowSyncProviderNotifications", &sync_ads)) {
        report->explorer_ads_enabled = (sync_ads != 0);
    } else {
        report->explorer_ads_enabled = true;
    }
    if (report->explorer_ads_enabled) {
        report->pending_tweaks_count++;
        report->issues_detected++;
    }

    DWORD sys_resp = 20;
    if (reg_read_dword(HKEY_LOCAL_MACHINE,
                       "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile",
                       "SystemResponsiveness", &sys_resp)) {
        report->system_responsiveness_pct = sys_resp;
    } else {
        report->system_responsiveness_pct = 20;
    }

    QueryPerformanceCounter(&t_end);
    report->audit_elapsed_ms = (double)(t_end.QuadPart - t_start.QuadPart) * 1000.0 / (double)freq.QuadPart;

    char summary[160];
    snprintf(summary, sizeof(summary),
             "Windows verificado: %d ajuste(s) pendente(s) · Boot: %d habilitados (%d ja desabilitados por voce)",
             report->pending_tweaks_count, report->startup_enabled_count, report->startup_disabled_count);
    ui_spinner_stop(summary, true);
}

void tweaks_print_report(const tweaks_report_t *report) {
    ui_item_block(
        report->is_high_performance,
        "Plano de Energia da CPU",
        report->is_high_performance ? "JA OTIMIZADO (Alto Desempenho)" : "PENDENTE (Modo Equilibrado)",
        "Define se o Windows mantem o processador na velocidade maxima ou reduz o clock.",
        report->is_high_performance
            ? "Processador operando na frequencia maxima sem oscilacoes."
            : "Escolha 'Mudar Plano de Energia para Alto Desempenho' no menu abaixo para corrigir agora."
    );

    ui_item_block(
        report->ntfs_trim_enabled,
        "Comando TRIM do SSD",
        report->ntfs_trim_enabled ? "JA OTIMIZADO (Ativado)" : "PENDENTE (Desativado)",
        "Recurso que avisa o SSD quais blocos foram apagados para manter a velocidade de gravacao.",
        report->ntfs_trim_enabled
            ? "Seu SSD esta com a manutencao automatica de blocos ativa."
            : "Abra o terminal como Administrador e rode: fsutil behavior set DisableDeleteNotify 0"
    );

    ui_item_block(
        !report->game_dvr_enabled,
        "Gravacao Xbox GameDVR",
        !report->game_dvr_enabled ? "JA OTIMIZADO (Desativado)" : "PENDENTE (Ativo em 2o plano)",
        "Recurso do Xbox Game Bar que grava a tela continuamente na memoria da placa de video.",
        !report->game_dvr_enabled
            ? "Ja esta desativado. Sua placa de video fica 100% livre para os jogos."
            : "Pode ser desativado na tela '3. Otimizar Windows e Jogos'."
    );

    char delay_val[64];
    if (report->menu_show_delay_ms <= 50) {
        snprintf(delay_val, sizeof(delay_val), "JA OTIMIZADO (%d ms - Instantaneo)", report->menu_show_delay_ms);
    } else {
        snprintf(delay_val, sizeof(delay_val), "PENDENTE (%d ms - Padrao lento)", report->menu_show_delay_ms);
    }

    ui_item_block(
        report->menu_show_delay_ms <= 50,
        "Velocidade dos Menus",
        delay_val,
        "Tempo que o Windows espera antes de desenhar um menu quando voce clica ou passa o mouse.",
        report->menu_show_delay_ms <= 50
            ? "Ja esta aplicado no seu PC (resposta imediata)."
            : "Pode ser reduzido para 20ms na tela '3. Otimizar Windows e Jogos'."
    );

    char boot_val[96];
    snprintf(boot_val, sizeof(boot_val), "%d Habilitados abrindo no boot (%d ja Desabilitados por voce)",
             report->startup_enabled_count, report->startup_disabled_count);

    ui_item_block(
        report->startup_enabled_count <= 6,
        "Programas no Boot",
        boot_val,
        "Lido de StartupApproved\\Run (mesma tabela da aba Inicializar do Gerenciador de Tarefas).",
        report->startup_enabled_count <= 6
            ? "Quantidade enxuta! A maioria dos programas ja foi desabilitada por voce."
            : "Abra 'Programas na Inicializacao (Boot)' para desabilitar os que nao precisa."
    );
}

void tweaks_print_startup(const tweaks_report_t *report) {
    ui_header_box("APLICATIVOS DE INICIALIZACAO (SINCRONIZADO COM O GERENCIADOR DE TAREFAS)",
                  "Mostra exatamente quais programas estao HABILITADOS e quais voce ja deixou DESABILITADOS.");

    if (report->startup_count == 0) {
        ui_ok("Nenhum programa configurado na inicializacao do Windows.");
        return;
    }

    printf("  " ANSI_GREEN "·" ANSI_RESET " Resumo real: " ANSI_YELLOW ANSI_BOLD "%d Habilitados" ANSI_RESET
           " (abrindo com o PC)  |  " ANSI_DIM "%d Desabilitados (nao abrem com o PC)" ANSI_RESET "\n\n",
           report->startup_enabled_count, report->startup_disabled_count);

    /* Primeiro lista os HABILITADOS (que realmente abrem com o PC) */
    printf("  " ANSI_YELLOW ANSI_BOLD "1. REALMENTE HABILITADOS (Abrem sozinhos quando voce liga o PC):" ANSI_RESET "\n");
    int shown_en = 0;
    for (int i = 0; i < report->startup_count; i++) {
        const startup_entry_t *e = &report->startup_apps[i];
        if (!e->enabled) continue;
        shown_en++;
        printf("    " ANSI_GREEN "●" ANSI_RESET " " ANSI_BOLD "%-28s" ANSI_RESET
               " " ANSI_GREEN "[HABILITADO]" ANSI_RESET " " ANSI_DIM "(%s)" ANSI_RESET "\n",
               e->name, e->is_hkcu ? "Seu Usuario" : "Sistema HKLM");
        printf("      " ANSI_DIM "╰─ %s" ANSI_RESET "\n", e->command);
    }
    if (shown_en == 0) {
        printf("    " ANSI_DIM "Nenhum programa habilitado." ANSI_RESET "\n");
    }

    /* Depois lista os que ja estao DESABILITADOS no Gerenciador de Tarefas */
    printf("\n  " ANSI_DIM "2. JA DESABILITADOS POR VOCE (Nao abrem junto com o PC):" ANSI_RESET "\n");
    int shown_dis = 0;
    for (int i = 0; i < report->startup_count; i++) {
        const startup_entry_t *e = &report->startup_apps[i];
        if (e->enabled) continue;
        shown_dis++;
        printf("    " ANSI_DIM "○ %-28s [DESABILITADO] (%s)" ANSI_RESET "\n",
               e->name, e->is_hkcu ? "Seu Usuario" : "Sistema HKLM");
    }
    if (shown_dis == 0) {
        printf("    " ANSI_DIM "Nenhum programa desabilitado." ANSI_RESET "\n");
    }
    printf("\n");
}

int tweaks_inspect_optimizations(tweaks_report_t *out_current) {
    tweaks_run_audit(out_current);

    ui_header_box("ANALISE DOS 4 AJUSTES DE DESEMPENHO DO WINDOWS",
                  "Itens marcados como 'JA OTIMIZADO' sao ignorados. Apenas os 'PENDENTE' precisam ser aplicados.");

    /* 1. Plano de Energia */
    ui_item_block(
        out_current->is_high_performance,
        "1. Plano de Energia da CPU",
        out_current->is_high_performance
            ? "JA OTIMIZADO (Alto Desempenho ativo)"
            : "PENDENTE: Atual 'Equilibrado'  -->  Mudar para 'Alto Desempenho'",
        "Comando nativo: powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c",
        out_current->is_high_performance
            ? "Ja esta otimizado. Nao precisa rodar novamente."
            : "Evita que o Windows derrube o clock do processador durante jogos e tarefas pesadas."
    );

    /* 2. Velocidade dos Menus */
    char menu_state[96];
    if (out_current->menu_show_delay_ms <= 50) {
        snprintf(menu_state, sizeof(menu_state), "JA OTIMIZADO (Atual: %d ms - Instantaneo)", out_current->menu_show_delay_ms);
    } else {
        snprintf(menu_state, sizeof(menu_state), "PENDENTE: Atual %d ms  -->  Mudar para 20 ms", out_current->menu_show_delay_ms);
    }
    ui_item_block(
        out_current->menu_show_delay_ms <= 50,
        "2. Velocidade dos Menus",
        menu_state,
        "Chave: HKEY_CURRENT_USER\\Control Panel\\Desktop\\MenuShowDelay",
        out_current->menu_show_delay_ms <= 50
            ? "Ja esta otimizado no seu PC. Nao precisa rodar novamente."
            : "Tira o atraso de quase meio segundo ao abrir menus do botao direito e do Explorer."
    );

    /* 3. Gravacao Xbox GameDVR */
    ui_item_block(
        !out_current->game_dvr_enabled,
        "3. Gravacao Xbox GameDVR",
        !out_current->game_dvr_enabled
            ? "JA OTIMIZADO (Gravacao de fundo Desativada)"
            : "PENDENTE: Atual Ativo (1)  -->  Mudar para Desativado (0)",
        "Chave: HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR",
        !out_current->game_dvr_enabled
            ? "Ja esta otimizado no seu PC. Nao precisa rodar novamente."
            : "Desliga a gravacao oculta de tela na GPU para evitar quedas de FPS e input lag."
    );

    /* 4. Anuncios / Sugestoes de Sincronizacao no Explorer */
    ui_item_block(
        !out_current->explorer_ads_enabled,
        "4. Anuncios no Explorer",
        !out_current->explorer_ads_enabled
            ? "JA OTIMIZADO (Propagandas do Explorer Desativadas)"
            : "PENDENTE: Atual Ativo (1)  -->  Mudar para Desativado (0)",
        "Chave: HKCU\\...\\Explorer\\Advanced\\ShowSyncProviderNotifications",
        !out_current->explorer_ads_enabled
            ? "Ja esta otimizado no seu PC. Nao precisa rodar novamente."
            : "Remove banners promocionais e avisos de sincronizacao dentro das pastas do Windows Explorer."
    );

    return out_current->pending_tweaks_count;
}

int tweaks_apply_pending_optimizations(const tweaks_report_t *current) {
    int applied = 0;

    if (!current->is_high_performance) {
        if (tweaks_activate_high_performance_power()) {
            ui_ok("Plano de Energia alterado para " ANSI_BOLD "Alto Desempenho" ANSI_RESET);
            applied++;
        } else {
            ui_warn("Nao foi possivel ativar o plano Alto Desempenho via powercfg.");
        }
    }

    if (current->menu_show_delay_ms > 50) {
        if (reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", "20")) {
            ui_ok("Velocidade dos Menus ajustada para " ANSI_BOLD "20 ms" ANSI_RESET);
            applied++;
        }
    }

    if (current->game_dvr_enabled) {
        bool ok1 = reg_write_dword(HKEY_CURRENT_USER,
                                   "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR",
                                   "AppCaptureEnabled", 0);
        bool ok2 = reg_write_dword(HKEY_CURRENT_USER,
                                   "System\\GameConfigStore",
                                   "GameDVR_Enabled", 0);
        if (ok1 || ok2) {
            ui_ok("Gravacao em segundo plano do " ANSI_BOLD "Xbox GameDVR desativada" ANSI_RESET);
            applied++;
        }
    }

    if (current->explorer_ads_enabled) {
        if (reg_write_dword(HKEY_CURRENT_USER,
                            "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                            "ShowSyncProviderNotifications", 0)) {
            ui_ok("Anuncios e notificacoes promocionais do " ANSI_BOLD "Windows Explorer desativados" ANSI_RESET);
            applied++;
        }
    }

    return applied;
}

void tweaks_restore_windows_defaults(void) {
    system("powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e >nul 2>nul");
    reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", "400");
    reg_write_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", "AppCaptureEnabled", 1);
    reg_write_dword(HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_Enabled", 1);
    reg_write_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", "ShowSyncProviderNotifications", 1);
    ui_info("Configuracoes restauradas para os valores padrao de fabrica do Windows.");
}
