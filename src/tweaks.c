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
    snprintf(summary, sizeof(summary), "Configuracoes do Windows analisadas (%d programas no boot, %d ajustes recomendados)",
             report->startup_count, report->issues_detected);
    ui_spinner_stop(summary, true);
}

void tweaks_print_report(const tweaks_report_t *report) {
    ui_item_block(
        report->is_high_performance,
        "Plano de Energia da CPU",
        report->power_plan_name,
        "Define se o Windows mantem o processador na velocidade maxima ou reduz o clock para economizar bateria.",
        report->is_high_performance
            ? "Processador operando sem limitacao de clock."
            : "Mude para 'Alto Desempenho' nas Opcoes de Energia do Windows quando for jogar."
    );

    ui_item_block(
        report->ntfs_trim_enabled,
        "Comando TRIM do SSD",
        report->ntfs_trim_enabled ? "Ativado (Ideal)" : "Desativado (Critico)",
        "Recurso que avisa o SSD quais blocos foram apagados para manter a velocidade de gravacao.",
        report->ntfs_trim_enabled
            ? "Seu SSD esta com a manutencao automatica de blocos ativa."
            : "Abra o terminal como Administrador e rode: fsutil behavior set DisableDeleteNotify 0"
    );

    ui_item_block(
        !report->game_dvr_enabled,
        "Gravacao Xbox GameDVR",
        report->game_dvr_enabled ? "Ativa em segundo plano (Reduz FPS)" : "Desativada (Otimizado)",
        "Recurso do Xbox Game Bar que grava a tela continuamente na memoria da placa de video.",
        report->game_dvr_enabled
            ? "Abra o menu 'Otimizar Windows e Jogos' e aperte Enter para desativar."
            : "Desativado. Sua placa de video fica 100% livre para os jogos."
    );

    char delay_val[64];
    snprintf(delay_val, sizeof(delay_val), "%d ms (%s)",
             report->menu_show_delay_ms,
             report->menu_show_delay_ms <= 50 ? "Instantaneo" : "Padrao lento do Windows");

    ui_item_block(
        report->menu_show_delay_ms <= 50,
        "Velocidade dos Menus",
        delay_val,
        "Tempo que o Windows espera antes de desenhar um menu quando voce clica ou passa o mouse.",
        report->menu_show_delay_ms <= 50
            ? "Menus configurados para resposta imediata."
            : "Abra o menu 'Otimizar Windows e Jogos' e aperte Enter para baixar de 400ms para 20ms."
    );

    char boot_val[64];
    snprintf(boot_val, sizeof(boot_val), "%d programas abrindo sozinhos", report->startup_count);

    ui_item_block(
        report->startup_count <= 6,
        "Programas no Boot",
        boot_val,
        "Aplicativos que iniciam automaticamente junto com o Windows e ficam consumindo memoria RAM.",
        report->startup_count <= 6
            ? "Quantidade enxuta de programas iniciando no boot."
            : "Abra 'Programas na Inicializacao (Boot)' no menu para ver a lista completa dos 18 apps."
    );
}

void tweaks_print_startup(const tweaks_report_t *report) {
    ui_header_box("PROGRAMAS QUE INICIAM SOZINHOS COM O SEU WINDOWS",
                  "Quanto mais programas abaixo, mais demorado fica ligar o PC e mais memoria RAM e gasta.");

    if (report->startup_count == 0) {
        ui_ok("Nenhum programa configurado para iniciar junto com o Windows.");
        return;
    }

    for (int i = 0; i < report->startup_count; i++) {
        const startup_entry_t *e = &report->startup_apps[i];
        const char *origem_explicada = (strncmp(e->source, "HKLM", 4) == 0)
            ? "Sistema (Todos os usuarios)"
            : "Seu Usuario Atual";

        printf("  " ANSI_YELLOW "%2d." ANSI_RESET " " ANSI_BOLD "%-28s" ANSI_RESET
               " " ANSI_DIM "[%s]" ANSI_RESET "\n",
               i + 1, e->name, origem_explicada);
        printf("      " ANSI_DIM "╰─ Executavel: %s" ANSI_RESET "\n\n", e->command);
    }

    ui_info("Como desativar algum deles: aperte Ctrl + Shift + Esc (Gerenciador de Tarefas) -> aba 'Aplicativos de Inicializacao'.");
}

int tweaks_apply_safe_optimizations(bool dry_run) {
    int applied = 0;

    if (dry_run) {
        tweaks_report_t current;
        tweaks_run_audit(&current);

        ui_header_box("AJUSTES DE DESEMPENHO DISPONIVEIS PARA O SEU WINDOWS",
                      "Estes ajustes alteram apenas configuracoes visuais e de jogos do seu usuario (100% seguro).");

        char menu_curr[64];
        snprintf(menu_curr, sizeof(menu_curr), "Atual: %d ms  -->  Vai ficar: 20 ms (Instantaneo)", current.menu_show_delay_ms);
        ui_item_block(
            current.menu_show_delay_ms <= 50,
            "1. Velocidade dos Menus",
            menu_curr,
            "Chave: HKEY_CURRENT_USER\\Control Panel\\Desktop\\MenuShowDelay",
            "Tira o atraso de quase meio segundo ao abrir menus do botao direito e do Explorer."
        );

        ui_item_block(
            !current.game_dvr_enabled,
            "2. Gravacao Xbox GameDVR",
            current.game_dvr_enabled ? "Atual: Ativo (1)  -->  Vai ficar: Desativado (0)" : "Atual: Ja esta Desativado (0)",
            "Chave: HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR",
            "Desliga a gravacao oculta de tela na GPU para evitar quedas de FPS e input lag em jogos."
        );

        return 2;
    }

    if (reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", "20")) {
        ui_ok("Velocidade dos Menus ajustada para " ANSI_BOLD "20 ms" ANSI_RESET " (Instantaneo)");
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
        ui_ok("Gravacao em segundo plano do " ANSI_BOLD "Xbox GameDVR desativada" ANSI_RESET);
        applied++;
    } else {
        ui_fail("Nao foi possivel atualizar as chaves do GameDVR");
    }

    return applied;
}
