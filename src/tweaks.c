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

    ui_spinner_tick(0, "Lendo chaves de Registro do Windows...", "HKCU & HKLM");

    enumerate_startup_key(HKEY_CURRENT_USER,
                          "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          "HKCU\\Run", report);
    enumerate_startup_key(HKEY_LOCAL_MACHINE,
                          "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          "HKLM\\Run", report);

    if (report->startup_count > 6) {
        report->issues_detected++;
    }

    ui_spinner_tick(2, "Verificando Plano de Energia da CPU...", "Control\\Power");
    if (reg_read_string(HKEY_LOCAL_MACHINE,
                        "SYSTEM\\CurrentControlSet\\Control\\Power\\User\\PowerSchemes",
                        "ActivePowerScheme",
                        report->power_plan_guid, sizeof(report->power_plan_guid))) {
        if (_stricmp(report->power_plan_guid, "8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c") == 0) {
            strncpy(report->power_plan_name, "Alto Desempenho (High Performance)", sizeof(report->power_plan_name) - 1);
            report->is_high_performance = true;
        } else if (_stricmp(report->power_plan_guid, "e9a42b02-d5df-448d-aa00-03f14749eb61") == 0) {
            strncpy(report->power_plan_name, "Desempenho Maximo (Ultimate Performance)", sizeof(report->power_plan_name) - 1);
            report->is_high_performance = true;
        } else if (_stricmp(report->power_plan_guid, "a1841308-3541-4fab-bc81-f71556f20b4a") == 0) {
            strncpy(report->power_plan_name, "Economia de Energia (CPU Limitada)", sizeof(report->power_plan_name) - 1);
            report->is_high_performance = false;
            report->issues_detected++;
        } else {
            strncpy(report->power_plan_name, "Equilibrado (Padrao do Windows)", sizeof(report->power_plan_name) - 1);
            report->is_high_performance = false;
            report->issues_detected++;
        }
    } else {
        strncpy(report->power_plan_name, "Equilibrado (Padrao do Windows)", sizeof(report->power_plan_name) - 1);
        report->is_high_performance = false;
    }

    ui_spinner_tick(4, "Inspecionando suporte a TRIM no SSD...", "FileSystem");
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

    ui_spinner_tick(6, "Auditando GameDVR e agendador multimidia...", "GameConfigStore");
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

    char summary[128];
    snprintf(summary, sizeof(summary), "%d programas no boot, %d ajuste(s) recomendado(s)",
             report->startup_count, report->issues_detected);
    ui_spinner_finish(report->issues_detected > 0 ? UI_STATUS_WARN : UI_STATUS_OK,
                      "Auditoria de Sistema e Registro", summary, report->audit_elapsed_ms);
}

void tweaks_print_report(const tweaks_report_t *report) {
    ui_section_header("SISTEMA", "DIAGNOSTICO DE OTIMIZACAO DO WINDOWS E JOGOS",
                      "Explicacao detalhada do que esta mal otimizado e como corrigir");

    char state_buf[128];

    /* 1. Power Plan */
    snprintf(state_buf, sizeof(state_buf), "Plano Ativo: %s", report->power_plan_name);
    ui_print_diagnostic_item(
        report->is_high_performance ? UI_STATUS_OK : UI_STATUS_WARN,
        "Plano de Energia da CPU",
        state_buf,
        report->is_high_performance
            ? "O processador opera sem reducoes bruscas de clock (parking/throttling)."
            : "O modo Equilibrado reduz a frequencia da CPU continuamente para poupar energia, gerando oscilacao de FPS e latencia.",
        report->is_high_performance
            ? "Nenhuma alteracao necessaria."
            : "Ative o plano 'Alto Desempenho' nas Opcoes de Energia do Painel de Controle quando estiver conectado na tomada."
    );

    /* 2. SSD TRIM */
    ui_print_diagnostic_item(
        report->ntfs_trim_enabled ? UI_STATUS_OK : UI_STATUS_CRIT,
        "Comando TRIM do SSD (NTFS)",
        report->ntfs_trim_enabled ? "Ativado (DisableDeleteNotify = 0)" : "DESATIVADO (DisableDeleteNotify = 1)",
        "O comando TRIM avisa o controlador do SSD quais blocos de memoria foram apagados para que ele limpe as celulas antecipadamente.",
        report->ntfs_trim_enabled
            ? "O SSD esta configurado corretamente para manter velocidade maxima e vida util."
            : "Rode 'fsutil behavior set DisableDeleteNotify 0' como Administrador para reativar o TRIM imediatamente."
    );

    /* 3. Xbox GameDVR Capture */
    ui_print_diagnostic_item(
        report->game_dvr_enabled ? UI_STATUS_WARN : UI_STATUS_OK,
        "Gravacao Xbox GameDVR",
        report->game_dvr_enabled ? "Ativa em segundo plano (AppCaptureEnabled = 1)" : "Desativada (Otimizado para jogos)",
        report->game_dvr_enabled
            ? "O Windows mantem um buffer de captura de video na GPU mesmo quando voce nao esta gravando, causando micro-stuttering e perda de FPS."
            : "A placa de video dedica 100% do seu encoder e banda de VRAM diretamente para o jogo ou aplicacao.",
        report->game_dvr_enabled
            ? "Use a opcao [3] 'Inspecionar e Aplicar Otimizacoes' para desativar o GameDVR no Registro com 1 clique."
            : "Configuracao ideal ja aplicada."
    );

    /* 4. Desktop UI Menu Delay */
    snprintf(state_buf, sizeof(state_buf), "%d ms de atraso", report->menu_show_delay_ms);
    ui_print_diagnostic_item(
        report->menu_show_delay_ms <= 50 ? UI_STATUS_OK : UI_STATUS_WARN,
        "Latencia de Menus (UI Delay)",
        state_buf,
        report->menu_show_delay_ms <= 50
            ? "Menus de contexto e janelas do Windows respondem quase instantaneamente."
            : "Por padrao o Windows espera 400 milissegundos antes de abrir menus ao passar o mouse, dando sensacao de sistema pesado.",
        report->menu_show_delay_ms <= 50
            ? "Configuracao ideal ja aplicada."
            : "Use a opcao [3] 'Inspecionar e Aplicar Otimizacoes' para reduzir a chave MenuShowDelay de 400ms para 20ms."
    );

    /* 5. Multimedia CPU Reservation */
    snprintf(state_buf, sizeof(state_buf), "%u%% reservado para segundo plano (MMCSS)", report->system_responsiveness_pct);
    ui_print_diagnostic_item(
        report->system_responsiveness_pct <= 10 ? UI_STATUS_OK : UI_STATUS_INFO,
        "Reserva de CPU Multimidia",
        state_buf,
        "O Multimedia Class Scheduler (MMCSS) reserva ate 20% de tempo de CPU para tarefas em segundo plano durante jogos.",
        "Manter entre 10% e 20% garante estabilidade de audio e rede sem comprometer threads principais."
    );

    /* 6. Boot Startup Load */
    snprintf(state_buf, sizeof(state_buf), "%d programas iniciando junto com o Windows", report->startup_count);
    ui_print_diagnostic_item(
        report->startup_count <= 6 ? UI_STATUS_OK : UI_STATUS_WARN,
        "Sobrecarga de Inicializacao",
        state_buf,
        report->startup_count <= 6
            ? "Poucos aplicativos disputam disco e RAM durante o boot do sistema."
            : "Ter muitos programas no boot (Steam, Epic, Spotify, iCloud, atualizadores) atrasa a inicializacao e consome gigabytes de RAM ociosamente.",
        report->startup_count <= 6
            ? "Inicializacao enxuta e saudavel."
            : "Abra a opcao [4] 'Gerenciador de Inicializacao' para ver a lista completa dos 18 programas e o impacto de cada um."
    );
}

void tweaks_print_startup(const tweaks_report_t *report) {
    ui_section_header("BOOT", "PROGRAMAS CONFIGURADOS NA INICIALIZACAO DO WINDOWS",
                      "Chaves HKCU\\...\\Run e HKLM\\...\\Run que iniciam sozinhas ao ligar o PC");

    if (report->startup_count == 0) {
        printf("  " ANSI_GREEN "[OK]" ANSI_RESET " Nenhum programa listado nas chaves Run do Registro.\n");
        return;
    }

    printf("  " ANSI_DIM "Explicacao: Cada item abaixo e disparado automaticamente pelo Windows logo apos o login.\n"
           "  Desativar launchers de jogos e sincronizadores que voce nao usa todo dia economiza RAM e acelera o boot." ANSI_RESET "\n\n");

    printf("  " ANSI_CYAN ANSI_BOLD "%-4s %-12s %-26s %s" ANSI_RESET "\n",
           "#", "ORIGEM", "NOME DO PROGRAMA", "EXECUTAVEL / COMANDO NO REGISTRO");
    ui_divider(84);

    for (int i = 0; i < report->startup_count; i++) {
        const startup_entry_t *e = &report->startup_apps[i];
        char short_cmd[48];
        size_t clen = strlen(e->command);
        if (clen > 44) {
            snprintf(short_cmd, sizeof(short_cmd), "%.41s...", e->command);
        } else {
            snprintf(short_cmd, sizeof(short_cmd), "%s", e->command);
        }

        const char *hive_col = (strncmp(e->source, "HKLM", 4) == 0) ? ANSI_MAGENTA : ANSI_CYAN;
        printf("  " ANSI_DIM "[%2d]" ANSI_RESET " %s%-12s" ANSI_RESET " " ANSI_WHITE ANSI_BOLD "%-26.25s" ANSI_RESET " " ANSI_GRAY "%s" ANSI_RESET "\n",
               i + 1, hive_col, e->source, e->name, short_cmd);
    }
    ui_divider(84);
    printf("  " ANSI_AMBER "Total: %d programas" ANSI_RESET ANSI_DIM " (Dica: Voce pode desabilitar os itens desnecessarios na aba 'Inicializar' do Gerenciador de Tarefas)" ANSI_RESET "\n",
           report->startup_count);
}

int tweaks_apply_safe_optimizations(bool dry_run) {
    int applied = 0;

    ui_section_header("AJUSTES", dry_run ? "INSPECAO DO QUE SERA ALTERADO NO REGISTRO (--dry-run)"
                                         : "APLICANDO OTIMIZACOES SEGURAS NO REGISTRO DO WINDOWS",
                      "Alteracoes seguras no escopo do usuario (HKEY_CURRENT_USER)");

    /* 1. Tune MenuShowDelay to 20ms */
    printf("  " BG_MAGENTA " REGISTRO 1 " ANSI_RESET " " ANSI_WHITE ANSI_BOLD "Reduzir Atraso de Renderizacao de Menus (MenuShowDelay)" ANSI_RESET "\n");
    printf("    " ANSI_DIM "├─ Chave Alvo  : HKEY_CURRENT_USER\\Control Panel\\Desktop\\MenuShowDelay" ANSI_RESET "\n");
    printf("    " ANSI_DIM "├─ Valor Padrao: \"400\" (400ms)  -->  Novo Valor: \"20\" (20ms)" ANSI_RESET "\n");
    printf("    " ANSI_DIM "╰─ Efeito Real : Elimina a espera artificial ao abrir menus de contexto e janelas do Explorer." ANSI_RESET "\n");

    if (dry_run) {
        ui_print_status(UI_STATUS_TWEAK, "Simulacao MenuShowDelay", "Pronto para aplicar (20ms)");
        applied++;
    } else {
        if (reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", "20")) {
            ui_print_status(UI_STATUS_OK, "MenuShowDelay Atualizado", "Gravado valor \"20\" com sucesso");
            applied++;
        }
    }
    printf("\n");

    /* 2. Disable GameDVR background capture lag */
    printf("  " BG_MAGENTA " REGISTRO 2 " ANSI_RESET " " ANSI_WHITE ANSI_BOLD "Desativar Captura em Segundo Plano do Xbox GameDVR" ANSI_RESET "\n");
    printf("    " ANSI_DIM "├─ Chave Alvo 1: HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR\\AppCaptureEnabled" ANSI_RESET "\n");
    printf("    " ANSI_DIM "├─ Chave Alvo 2: HKEY_CURRENT_USER\\System\\GameConfigStore\\GameDVR_Enabled" ANSI_RESET "\n");
    printf("    " ANSI_DIM "├─ Valor Padrao: 1 (Ativo)      -->  Novo Valor: 0 (Desativado)" ANSI_RESET "\n");
    printf("    " ANSI_DIM "╰─ Efeito Real : Impede que o Windows grave buffers de video ocultos na GPU, estabilizando o frame-time em jogos." ANSI_RESET "\n");

    if (dry_run) {
        ui_print_status(UI_STATUS_TWEAK, "Simulacao Xbox GameDVR", "Pronto para desativar captura de fundo");
        applied++;
    } else {
        bool ok1 = reg_write_dword(HKEY_CURRENT_USER,
                                   "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR",
                                   "AppCaptureEnabled", 0);
        bool ok2 = reg_write_dword(HKEY_CURRENT_USER,
                                   "System\\GameConfigStore",
                                   "GameDVR_Enabled", 0);
        if (ok1 || ok2) {
            ui_print_status(UI_STATUS_OK, "Xbox GameDVR Desativado", "Captura em segundo plano desligada com sucesso");
            applied++;
        }
    }

    return applied;
}
