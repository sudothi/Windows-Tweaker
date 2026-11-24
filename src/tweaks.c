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

static void enumerate_startup_key(HKEY root, const char *subkey, const char *source_label, tweaks_report_t *report) {
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
        }
    }

    RegCloseKey(hKey);
}

void tweaks_run_audit(tweaks_report_t *report) {
    memset(report, 0, sizeof(*report));

    LARGE_INTEGER freq, t_start, t_end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t_start);

    ui_spinner_update(0, "Lendo chaves de inicializacao e Registro do Windows...");

    enumerate_startup_key(HKEY_CURRENT_USER,
                          "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          "HKCU\\Run", report);
    enumerate_startup_key(HKEY_LOCAL_MACHINE,
                          "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          "HKLM\\Run", report);

    if (report->startup_count > 6) {
        report->issues_detected++;
    }

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
            report->issues_detected++;
        } else {
            strncpy(report->power_plan_name, "Equilibrado (Padrao)", sizeof(report->power_plan_name) - 1);
            report->is_high_performance = false;
            report->issues_detected++;
        }
    } else {
        strncpy(report->power_plan_name, "Equilibrado (Padrao)", sizeof(report->power_plan_name) - 1);
        report->is_high_performance = false;
    }

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

    DWORD disable_last_access = 0;
    if (reg_read_dword(HKEY_LOCAL_MACHINE,
                       "SYSTEM\\CurrentControlSet\\Control\\FileSystem",
                       "NtfsDisableLastAccessUpdate", &disable_last_access)) {
        report->ntfs_last_access_disabled = (disable_last_access != 0);
    }

    DWORD dvr_enabled = 0, app_capture = 0;
    bool has_dvr = reg_read_dword(HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_Enabled", &dvr_enabled);
    bool has_cap = reg_read_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", "AppCaptureEnabled", &app_capture);
    report->game_dvr_enabled = (has_dvr && dvr_enabled != 0) || (has_cap && app_capture != 0);
    if (report->game_dvr_enabled) {
        report->issues_detected++;
    }

    char delay_str[32] = {0};
    if (reg_read_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", delay_str, sizeof(delay_str))) {
        report->menu_show_delay_ms = atoi(delay_str);
    } else {
        report->menu_show_delay_ms = 400;
    }
    if (report->menu_show_delay_ms > 100) {
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

    DWORD transp = 1;
    if (reg_read_dword(HKEY_CURRENT_USER,
                       "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                       "EnableTransparency", &transp)) {
        report->transparency_enabled = (transp != 0);
    } else {
        report->transparency_enabled = true;
    }

    QueryPerformanceCounter(&t_end);
    report->audit_elapsed_ms = (double)(t_end.QuadPart - t_start.QuadPart) * 1000.0 / (double)freq.QuadPart;

    char summary[160];
    snprintf(summary, sizeof(summary), "Configuracoes do Windows verificadas (%d programas no boot, %d ajustes disponiveis)",
             report->startup_count, report->issues_detected);
    ui_spinner_stop(summary, true);
}

void tweaks_print_report(const tweaks_report_t *report) {
    /* 1. Power Plan */
    if (report->is_high_performance) {
        ui_ok("Plano de Energia: " ANSI_BOLD "%s" ANSI_RESET " " ANSI_DIM "(CPU operando sem limitacao de clock)" ANSI_RESET,
              report->power_plan_name);
    } else {
        ui_warn("Plano de Energia: " ANSI_BOLD "%s" ANSI_RESET, report->power_plan_name);
        ui_line(ANSI_DIM "Por que importa: reduz o clock do processador continuamente, causando queda de FPS." ANSI_RESET);
        ui_line(ANSI_CYAN "Como resolver  : mude para 'Alto Desempenho' nas Opcoes de Energia do Windows." ANSI_RESET);
    }

    /* 2. SSD TRIM */
    if (report->ntfs_trim_enabled) {
        ui_ok("SSD NTFS TRIM: " ANSI_BOLD "Ativado" ANSI_RESET " " ANSI_DIM "(mantem a velocidade de escrita e vida util do SSD)" ANSI_RESET);
    } else {
        ui_fail("SSD NTFS TRIM: " ANSI_BOLD "Desativado" ANSI_RESET);
        ui_line(ANSI_DIM "Por que importa: sem TRIM o SSD nao limpa blocos apagados antecipadamente e fica lento." ANSI_RESET);
        ui_line(ANSI_CYAN "Como resolver  : rode 'fsutil behavior set DisableDeleteNotify 0' como Administrador." ANSI_RESET);
    }

    /* 3. Xbox GameDVR Capture */
    if (!report->game_dvr_enabled) {
        ui_ok("Xbox GameDVR: " ANSI_BOLD "Desativado" ANSI_RESET " " ANSI_DIM "(sem gravacao oculta consumindo GPU)" ANSI_RESET);
    } else {
        ui_warn("Xbox GameDVR: " ANSI_BOLD "Ativo em segundo plano" ANSI_RESET);
        ui_line(ANSI_DIM "Por que importa: grava buffer de video na placa de video mesmo sem voce pedir, gerando micro-stutter." ANSI_RESET);
        ui_line(ANSI_CYAN "Como resolver  : escolha a opcao '3) Aplicar otimizacoes do Windows' no menu." ANSI_RESET);
    }

    /* 4. Desktop UI Menu Delay */
    if (report->menu_show_delay_ms <= 50) {
        ui_ok("Resposta de Menus (MenuShowDelay): " ANSI_BOLD "%d ms" ANSI_RESET " " ANSI_DIM "(resposta visual imediata)" ANSI_RESET,
              report->menu_show_delay_ms);
    } else {
        ui_warn("Resposta de Menus (MenuShowDelay): " ANSI_BOLD "%d ms" ANSI_RESET " " ANSI_DIM "(padrao lento do Windows)" ANSI_RESET,
                report->menu_show_delay_ms);
        ui_line(ANSI_DIM "Por que importa: o Windows espera 400ms antes de desenhar menus ao passar o mouse." ANSI_RESET);
        ui_line(ANSI_CYAN "Como resolver  : escolha a opcao '3) Aplicar otimizacoes do Windows' para baixar para 20ms." ANSI_RESET);
    }

    /* 5. Boot Startup Load */
    if (report->startup_count <= 6) {
        ui_ok("Inicializacao (Boot): " ANSI_BOLD "%d programas" ANSI_RESET " " ANSI_DIM "(inicializacao enxuta)" ANSI_RESET,
              report->startup_count);
    } else {
        ui_warn("Inicializacao (Boot): " ANSI_BOLD "%d programas iniciando sozinhos com o Windows" ANSI_RESET,
                report->startup_count);
        ui_line(ANSI_DIM "Por que importa: cada app na chave Run disputa leitura de disco e consome RAM desde o login." ANSI_RESET);
        ui_line(ANSI_CYAN "Como resolver  : escolha a opcao '4) Programas na inicializacao' para ver quais sao." ANSI_RESET);
    }
}

void tweaks_print_startup(const tweaks_report_t *report) {
    if (report->startup_count == 0) {
        ui_ok("Nenhum programa configurado para iniciar junto com o Windows.");
        return;
    }

    ui_info("Estes %d programas estao registrados em HKCU\\Run e HKLM\\Run e abrem sozinhos ao ligar o PC:\n",
            report->startup_count);

    for (int i = 0; i < report->startup_count; i++) {
        const startup_entry_t *e = &report->startup_apps[i];
        printf("   " ANSI_BOLD "%2d)" ANSI_RESET " %-26.25s " ANSI_DIM "[%s]" ANSI_RESET "\n",
               i + 1, e->name, e->source);
        ui_line("   " ANSI_DIM "%s" ANSI_RESET, e->command);
    }

    printf("\n");
    ui_info("Dica: desative os que voce nao usa todo dia na aba 'Inicializar' do Gerenciador de Tarefas (Ctrl+Shift+Esc).");
}

int tweaks_apply_safe_optimizations(bool dry_run) {
    int applied = 0;

    if (dry_run) {
        ui_info("Otimizacoes que serao aplicadas no Registro do seu usuario (HKEY_CURRENT_USER):\n");
        ui_warn(ANSI_BOLD "1. MenuShowDelay: 400ms -> 20ms" ANSI_RESET);
        ui_line(ANSI_DIM "Chave : HKCU\\Control Panel\\Desktop\\MenuShowDelay" ANSI_RESET);
        ui_line(ANSI_DIM "Efeito: menus e janelas do Windows abrem instantaneamente sem atraso artificial." ANSI_RESET);

        ui_warn(ANSI_BOLD "2. Xbox GameDVR: Ativo (1) -> Desativado (0)" ANSI_RESET);
        ui_line(ANSI_DIM "Chave : HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR\\AppCaptureEnabled" ANSI_RESET);
        ui_line(ANSI_DIM "Efeito: desliga a gravacao oculta de video em segundo plano na placa de video." ANSI_RESET);
        return 2;
    }

    if (reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", "20")) {
        ui_ok("MenuShowDelay ajustado para " ANSI_BOLD "20ms" ANSI_RESET " (HKCU\\Control Panel\\Desktop)");
        applied++;
    } else {
        ui_fail("Nao foi possivel gravar MenuShowDelay");
    }

    bool ok1 = reg_write_dword(HKEY_CURRENT_USER,
                               "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR",
                               "AppCaptureEnabled", 0);
    bool ok2 = reg_write_dword(HKEY_CURRENT_USER,
                               "System\\GameConfigStore",
                               "GameDVR_Enabled", 0);
    if (ok1 || ok2) {
        ui_ok("Captura em segundo plano do " ANSI_BOLD "Xbox GameDVR desativada" ANSI_RESET);
        applied++;
    } else {
        ui_fail("Nao foi possivel atualizar as chaves do GameDVR");
    }

    return applied;
}
