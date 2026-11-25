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
                if ((buf[0] & 0x01) != 0) {
                    return false;
                }
                return true;
            }
        }
    }
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
        data[0] = 0x02;
    } else {
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
    int rc = system("powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c >nul 2>nul");
    if (rc != 0) {
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

bool tweaks_flush_dns_cache(void) {
    int rc = system("ipconfig /flushdns >nul 2>nul");
    return (rc == 0);
}

static void add_rule(
    tweaks_report_t *report,
    tweak_id_t id,
    const char *category,
    const char *title,
    bool is_opt,
    const char *curr_state,
    const char *target_state,
    const char *tech_loc,
    const char *explanation,
    int weight
) {
    if (report->rule_count >= MAX_TWEAK_RULES) return;
    tweak_rule_t *r = &report->rules[report->rule_count++];
    r->id           = id;
    r->is_optimized = is_opt;
    r->score_weight = weight;
    strncpy(r->category,      category,     sizeof(r->category) - 1);
    strncpy(r->title,         title,        sizeof(r->title) - 1);
    strncpy(r->current_state, curr_state,   sizeof(r->current_state) - 1);
    strncpy(r->target_state,  target_state, sizeof(r->target_state) - 1);
    strncpy(r->tech_location, tech_loc,     sizeof(r->tech_location) - 1);
    strncpy(r->explanation,   explanation,  sizeof(r->explanation) - 1);

    if (is_opt) {
        report->optimized_tweaks_count++;
    } else {
        report->pending_tweaks_count++;
        report->issues_detected++;
    }
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

    if (report->startup_enabled_count > 6) {
        report->issues_detected++;
    }

    /* Check NTFS TRIM & VBS */
    DWORD disable_trim = 0;
    if (reg_read_dword(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\FileSystem", "DisableDeleteNotify", &disable_trim)) {
        report->ntfs_trim_enabled = (disable_trim == 0);
    } else {
        report->ntfs_trim_enabled = true;
    }

    DWORD vbs_val = 0;
    if (reg_read_dword(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\DeviceGuard", "EnableVirtualizationBasedSecurity", &vbs_val)) {
        report->vbs_enabled = (vbs_val != 0);
    }

    ui_spinner_update(2, "Auditando 12 regras de Energia, Jogos, Rede, RAM e Privacidade...");

    /* ========================================================================
     * REGRA 1: PLANO DE ENERGIA DA CPU (ALTO DESEMPENHO)
     * ======================================================================== */
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
        } else {
            strncpy(report->power_plan_name, "Equilibrado (Padrao)", sizeof(report->power_plan_name) - 1);
            report->is_high_performance = false;
        }
    } else {
        strncpy(report->power_plan_name, "Equilibrado (Padrao)", sizeof(report->power_plan_name) - 1);
        report->is_high_performance = false;
    }

    add_rule(report, TWEAK_PWR_HIGH_PERF, "ENERGIA & CPU",
             "1. Plano de Energia da CPU",
             report->is_high_performance,
             report->power_plan_name, "Alto Desempenho (Clock estavel)",
             "powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c",
             "Impede o Windows de derrubar o clock do processador continuamente, evitando quedas de FPS.", 6);

    /* ========================================================================
     * REGRA 2: CLOCK MINIMO DA CPU NA TOMADA (100% AC P-STATE SEM QUEDA)
     * Chave: PowerSchemes\<GUID>\54533251-82be-4824-96c1-47b60b740d00\893dee8e-2bef-41e0-89c6-b55d0929964c
     * ======================================================================== */
    char min_cpu_key[256];
    snprintf(min_cpu_key, sizeof(min_cpu_key),
             "SYSTEM\\CurrentControlSet\\Control\\Power\\User\\PowerSchemes\\%s\\54533251-82be-4824-96c1-47b60b740d00\\893dee8e-2bef-41e0-89c6-b55d0929964c",
             report->power_plan_guid[0] ? report->power_plan_guid : "8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c");
    DWORD ac_min_cpu = 5;
    bool has_ac_min = reg_read_dword(HKEY_LOCAL_MACHINE, min_cpu_key, "ACSettingIndex", &ac_min_cpu);
    bool cpu_min_opt = report->is_high_performance && (!has_ac_min || ac_min_cpu == 100);

    char cpu_min_curr[64];
    snprintf(cpu_min_curr, sizeof(cpu_min_curr), "Minimo na tomada: %lu%%",
             (unsigned long)(has_ac_min ? ac_min_cpu : (report->is_high_performance ? 100 : 5)));

    add_rule(report, TWEAK_PWR_HIBER_REDUCED, "ENERGIA & CPU",
             "2. Clock Minimo da CPU na Tomada (P-State)",
             cpu_min_opt,
             cpu_min_opt ? "100% na tomada (5% na bateria)" : cpu_min_curr,
             "100% na tomada (Zero queda de clock)",
             "powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMIN 100",
             "Mantem o processador em 100% quando conectado na tomada, sem afetar a economia na bateria.", 4);

    /* ========================================================================
     * REGRA 3: GRAVACAO EM SEGUNDO PLANO XBOX GAMEDVR
     * ======================================================================== */
    DWORD dvr_enabled = 0, app_capture = 0;
    bool has_dvr = reg_read_dword(HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_Enabled", &dvr_enabled);
    bool has_cap = reg_read_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", "AppCaptureEnabled", &app_capture);
    bool dvr_active = (has_dvr && dvr_enabled != 0) || (has_cap && app_capture != 0);

    add_rule(report, TWEAK_GAME_DVR_OFF, "JOGOS & INPUT",
             "3. Gravacao Oculta Xbox GameDVR",
             !dvr_active,
             !dvr_active ? "Desativada (0)" : "Ativa em 2o plano (1)",
             "Desativada (0)",
             "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR\\AppCaptureEnabled = 0",
             "Desliga a captura continua de video na memoria da placa de video, melhorando 1% low FPS.", 5);

    /* ========================================================================
     * REGRA 4: MODO DE JOGO AUTOMATICO (WINDOWS GAME MODE)
     * ======================================================================== */
    DWORD gm_auto = 1;
    bool has_gm = reg_read_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\GameBar", "AutoGameModeEnabled", &gm_auto);
    bool gm_opt = (!has_gm || gm_auto == 1);

    add_rule(report, TWEAK_GAME_MODE_ON, "JOGOS & INPUT",
             "4. Modo de Jogo do Windows (Game Mode)",
             gm_opt,
             gm_opt ? "Ativado (Prioriza jogos)" : "Desativado (0)",
             "Ativado (1)",
             "HKCU\\Software\\Microsoft\\GameBar\\AutoGameModeEnabled = 1",
             "Suspende indexacao e atualizacoes em segundo plano enquanto um jogo estiver aberto.", 3);

    /* ========================================================================
     * REGRA 5: MOUSE 1:1 RAW INPUT (SEM ACELERACAO DO PONTEIRO)
     * ======================================================================== */
    char m_speed[16] = "1", m_t1[16] = "6", m_t2[16] = "10";
    reg_read_string(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseSpeed", m_speed, sizeof(m_speed));
    reg_read_string(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseThreshold1", m_t1, sizeof(m_t1));
    reg_read_string(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseThreshold2", m_t2, sizeof(m_t2));
    bool mouse_raw = (atoi(m_speed) == 0 && atoi(m_t1) == 0 && atoi(m_t2) == 0);

    add_rule(report, TWEAK_GAME_MOUSE_RAW, "JOGOS & INPUT",
             "5. Precisao 1:1 do Mouse (Sem Aceleracao)",
             mouse_raw,
             mouse_raw ? "Raw Input 1:1 (Sem aceleracao)" : "Aceleracao Ativa (MouseSpeed = 1)",
             "Raw Input 1:1 (MouseSpeed = 0)",
             "HKCU\\Control Panel\\Mouse (MouseSpeed=0, MouseThreshold1=0, MouseThreshold2=0)",
             "Remove a curva de aceleracao artificial do Windows para que a mira responda 1:1 a sua mao.", 4);

    /* ========================================================================
     * REGRA 6: PRIORIDADE MULTIMIDIA DE GPU/CPU PARA JOGOS (MMCSS)
     * ======================================================================== */
    DWORD gpu_prio = 0, cpu_prio = 0;
    bool has_gp = reg_read_dword(HKEY_LOCAL_MACHINE,
                                 "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\Tasks\\Games",
                                 "GPU Priority", &gpu_prio);
    bool has_cp = reg_read_dword(HKEY_LOCAL_MACHINE,
                                 "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\Tasks\\Games",
                                 "Priority", &cpu_prio);
    DWORD user_gpu_pref = 0;
    reg_read_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\DirectX\\UserGpuPreferences", "DirectXUserGlobalSettings", &user_gpu_pref);
    bool mmcss_opt = (has_gp && gpu_prio == 8 && has_cp && cpu_prio == 6) || (user_gpu_pref == 1);

    add_rule(report, TWEAK_GAME_MMCSS_PRIORITY, "JOGOS & INPUT",
             "6. Prioridade de Escalonamento de Jogos",
             mmcss_opt,
             mmcss_opt ? "Alta Prioridade (GPU=8, CPU=6)" : "Prioridade Padrao (GPU=2, CPU=2)",
             "Alta Prioridade (GPU=8, CPU=6)",
             "HKLM\\...\\Multimedia\\SystemProfile\\Tasks\\Games (GPU Priority=8, Priority=6)",
             "Configura o agendador multimidia do Windows para dar prioridade maxima de tempo de GPU aos jogos.", 4);

    /* ========================================================================
     * REGRA 7: BLOQUEAR UPLOAD P2P DO WINDOWS UPDATE (DELIVERY OPTIMIZATION)
     * ======================================================================== */
    DWORD do_mode = 1, do_pol = 1;
    bool has_do1 = reg_read_dword(HKEY_CURRENT_USER,
                                  "Software\\Microsoft\\Windows\\CurrentVersion\\DeliveryOptimization",
                                  "SystemSettingsDownloadMode", &do_mode);
    bool has_do2 = reg_read_dword(HKEY_CURRENT_USER,
                                  "Software\\Policies\\Microsoft\\Windows\\DeliveryOptimization",
                                  "DODownloadMode", &do_pol);
    bool p2p_off = (has_do1 && do_mode == 0) || (has_do2 && do_pol == 0);

    add_rule(report, TWEAK_NET_WU_P2P_OFF, "REDE & PING",
             "7. Upload P2P Oculto do Windows Update",
             p2p_off,
             p2p_off ? "Bloqueado (Nao gasta seu upload)" : "Ativo (Compartilhando updates via P2P)",
             "Bloqueado (DODownloadMode = 0)",
             "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\DeliveryOptimization",
             "Impede que o Windows use seu upload de internet para enviar atualizacoes a outros PCs durante jogos.", 4);

    /* ========================================================================
     * REGRA 8: BLOQUEAR APPS DA MICROSOFT STORE RODANDO EM 2o PLANO
     * ======================================================================== */
    DWORD bg_disabled = 0, bg_toggle = 1;
    bool has_bg1 = reg_read_dword(HKEY_CURRENT_USER,
                                  "Software\\Microsoft\\Windows\\CurrentVersion\\BackgroundAccessApplications",
                                  "GlobalUserDisabled", &bg_disabled);
    bool has_bg2 = reg_read_dword(HKEY_CURRENT_USER,
                                  "Software\\Microsoft\\Windows\\CurrentVersion\\Search",
                                  "BackgroundAppGlobalToggle", &bg_toggle);
    bool bg_apps_off = (has_bg1 && bg_disabled == 1) && (has_bg2 && bg_toggle == 0);

    add_rule(report, TWEAK_RAM_BG_APPS_OFF, "RAM, PRIVACIDADE & UI",
             "8. Apps da Microsoft Store em 2o Plano",
             bg_apps_off,
             bg_apps_off ? "Bloqueados (Economizando RAM)" : "Permitidos (Apps UWP abertos escondidos)",
             "Bloqueados (GlobalUserDisabled = 1)",
             "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\BackgroundAccessApplications",
             "Impede que dezenas de apps da Store (Clima, Noticias, Fotos, Xbox, Cortana) fiquem abertos gastando RAM.", 6);

    /* ========================================================================
     * REGRA 9: DESATIVAR BUSCA WEB DO BING NO MENU INICIAR
     * ======================================================================== */
    DWORD no_bing_pol = 0, bing_enabled = 1;
    bool has_b1 = reg_read_dword(HKEY_CURRENT_USER,
                                 "Software\\Policies\\Microsoft\\Windows\\Explorer",
                                 "DisableSearchBoxSuggestions", &no_bing_pol);
    bool has_b2 = reg_read_dword(HKEY_CURRENT_USER,
                                 "Software\\Microsoft\\Windows\\CurrentVersion\\Search",
                                 "BingSearchEnabled", &bing_enabled);
    bool bing_off = (has_b1 && no_bing_pol == 1) || (has_b2 && bing_enabled == 0);

    add_rule(report, TWEAK_PRIV_BING_SEARCH_OFF, "RAM, PRIVACIDADE & UI",
             "9. Pesquisa Web do Bing no Menu Iniciar",
             bing_off,
             bing_off ? "Desativada (Busca 100% local e rapida)" : "Ativa (Pesquisa no Bing a cada tecla)",
             "Desativada (Busca apenas local)",
             "HKCU\\Software\\Policies\\Microsoft\\Windows\\Explorer\\DisableSearchBoxSuggestions = 1",
             "Deixa a pesquisa do Menu Iniciar instantanea buscando apenas seus arquivos e programas locais.", 4);

    /* ========================================================================
     * REGRA 10: BLOQUEAR INSTALACAO SILENCIOSA DE BLOATWARE & ID DE ANUNCIOS
     * ======================================================================== */
    DWORD silent_apps = 1, ad_id = 1;
    bool has_s1 = reg_read_dword(HKEY_CURRENT_USER,
                                 "Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
                                 "SilentInstalledAppsEnabled", &silent_apps);
    bool has_a1 = reg_read_dword(HKEY_CURRENT_USER,
                                 "Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo",
                                 "Enabled", &ad_id);
    bool bloat_off = (has_s1 && silent_apps == 0) && (has_a1 && ad_id == 0);

    add_rule(report, TWEAK_PRIV_BLOATWARE_ADS_OFF, "RAM, PRIVACIDADE & UI",
             "10. Bloatware Automatico e ID de Anuncios",
             bloat_off,
             bloat_off ? "Bloqueados (Sem instalacoes ocultas)" : "Ativos (Permite apps patrocinados e tracking)",
             "Bloqueados (SilentInstalledApps = 0)",
             "HKCU\\...\\ContentDeliveryManager\\SilentInstalledAppsEnabled = 0 & AdvertisingInfo = 0",
             "Impede o Windows de baixar sozinho apps patrocinados (TikTok, Candy Crush) e desliga rastreamento.", 4);

    /* ========================================================================
     * REGRA 11: MENUS INSTANTANEOS (20ms) E SEM ANUNCIOS NO EXPLORER
     * ======================================================================== */
    char delay_str[32] = "400";
    reg_read_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", delay_str, sizeof(delay_str));
    int menu_ms = atoi(delay_str);

    DWORD sync_ads = 1;
    bool has_sync = reg_read_dword(HKEY_CURRENT_USER,
                                   "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                                   "ShowSyncProviderNotifications", &sync_ads);
    bool ui_fast_opt = (menu_ms <= 50) && (has_sync && sync_ads == 0);

    char ui_curr[80];
    snprintf(ui_curr, sizeof(ui_curr), "Menus: %d ms · Anuncios Explorer: %s",
             menu_ms, (has_sync && sync_ads == 0) ? "Desativados" : "Ativos");

    add_rule(report, TWEAK_UI_FAST_MENUS_EXPLORER, "RAM, PRIVACIDADE & UI",
             "11. Menus Instantaneos e Explorer Limpo",
             ui_fast_opt,
             ui_curr,
             "Menus: 20 ms · Anuncios Explorer: Desativados",
             "HKCU\\Control Panel\\Desktop\\MenuShowDelay=20 & ShowSyncProviderNotifications=0",
             "Tira o atraso de 400ms dos menus do botao direito e remove propagandas dentro das pastas.", 4);

    /* ========================================================================
     * REGRA 12: FECHAMENTO RAPIDO DE APPS TRAVADOS (2s EM VEZ DE 20s)
     * ======================================================================== */
    char hung_str[32] = "5000", wait_kill_str[32] = "20000", auto_end_str[16] = "0";
    reg_read_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "HungAppTimeout", hung_str, sizeof(hung_str));
    reg_read_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "WaitToKillAppTimeout", wait_kill_str, sizeof(wait_kill_str));
    reg_read_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "AutoEndTasks", auto_end_str, sizeof(auto_end_str));

    bool fast_kill_opt = (atoi(hung_str) <= 2000 && atoi(wait_kill_str) <= 2000 && atoi(auto_end_str) == 1);
    char kill_curr[80];
    snprintf(kill_curr, sizeof(kill_curr), "Espera atual: %d ms (%s)",
             atoi(wait_kill_str), fast_kill_opt ? "Fechamento rapido 2s" : "Padrao lento 20s");

    add_rule(report, TWEAK_UI_FAST_KILL_HUNG_APPS, "RAM, PRIVACIDADE & UI",
             "12. Fechar Apps Travados e Desligar Rapido",
             fast_kill_opt,
             kill_curr,
             "Encerrar em 2000 ms (2s) com AutoEndTasks=1",
             "HKCU\\Control Panel\\Desktop (HungAppTimeout=2000, WaitToKillAppTimeout=2000, AutoEndTasks=1)",
             "Reduz de 20s para 2s o tempo que o Windows espera para fechar um jogo ou programa que travou.", 4);

    QueryPerformanceCounter(&t_end);
    report->audit_elapsed_ms = (double)(t_end.QuadPart - t_start.QuadPart) * 1000.0 / (double)freq.QuadPart;

    char summary[160];
    snprintf(summary, sizeof(summary),
             "Windows auditado: %d/%d otimizacoes ja ativas (%d pendentes) · Boot: %d habilitados (%d desabilitados)",
             report->optimized_tweaks_count, report->rule_count, report->pending_tweaks_count,
             report->startup_enabled_count, report->startup_disabled_count);
    ui_spinner_stop(summary, true);
}

bool tweaks_apply_single_rule(tweak_id_t id) {
    switch (id) {
        case TWEAK_PWR_HIGH_PERF:
            if (tweaks_activate_high_performance_power()) {
                ui_ok("1. Plano de Energia alterado para " ANSI_BOLD "Alto Desempenho" ANSI_RESET);
                return true;
            }
            ui_warn("Nao foi possivel ativar o plano Alto Desempenho.");
            return false;

        case TWEAK_PWR_HIBER_REDUCED: {
            tweaks_activate_high_performance_power();
            int rc = system("powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMIN 100 >nul 2>nul");
            system("powercfg /setactive SCHEME_CURRENT >nul 2>nul");
            if (rc == 0) {
                ui_ok("2. Clock Minimo da CPU na tomada ajustado para " ANSI_BOLD "100%% (Zero Throttling)" ANSI_RESET);
                return true;
            }
            ui_warn("2. Nao foi possivel ajustar o estado minimo do processador.");
            return false;
        }

        case TWEAK_GAME_DVR_OFF: {
            bool ok1 = reg_write_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", "AppCaptureEnabled", 0);
            bool ok2 = reg_write_dword(HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_Enabled", 0);
            if (ok1 || ok2) {
                ui_ok("3. Gravacao oculta em 2o plano do " ANSI_BOLD "Xbox GameDVR desativada" ANSI_RESET);
                return true;
            }
            return false;
        }

        case TWEAK_GAME_MODE_ON: {
            bool ok1 = reg_write_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\GameBar", "AllowAutoGameMode", 1);
            bool ok2 = reg_write_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\GameBar", "AutoGameModeEnabled", 1);
            if (ok1 || ok2) {
                ui_ok("4. " ANSI_BOLD "Modo de Jogo Automatico (Game Mode)" ANSI_RESET " ativado");
                return true;
            }
            return false;
        }

        case TWEAK_GAME_MOUSE_RAW: {
            bool ok1 = reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseSpeed", "0");
            bool ok2 = reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseThreshold1", "0");
            bool ok3 = reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseThreshold2", "0");
            if (ok1 && ok2 && ok3) {
                /* Atualiza parametros em tempo real via SystemParametersInfoA sem precisar reiniciar! */
                int mouse_params[3] = {0, 0, 0};
                SystemParametersInfoA(SPI_SETMOUSE, 0, mouse_params, SPIF_SENDCHANGE);
                ui_ok("5. " ANSI_BOLD "Mouse 1:1 Raw Input" ANSI_RESET " ativado em tempo real (aceleracao removida)");
                return true;
            }
            return false;
        }

        case TWEAK_GAME_MMCSS_PRIORITY: {
            bool ok1 = reg_write_dword(HKEY_LOCAL_MACHINE,
                                       "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\Tasks\\Games",
                                       "GPU Priority", 8);
            bool ok2 = reg_write_dword(HKEY_LOCAL_MACHINE,
                                       "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\Tasks\\Games",
                                       "Priority", 6);
            reg_write_string(HKEY_LOCAL_MACHINE,
                             "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\Tasks\\Games",
                             "Scheduling Category", "High");
            bool ok_user = reg_write_dword(HKEY_CURRENT_USER,
                                           "Software\\Microsoft\\DirectX\\UserGpuPreferences",
                                           "DirectXUserGlobalSettings", 1);
            if ((ok1 && ok2) || ok_user) {
                ui_ok("6. " ANSI_BOLD "Prioridade Maxima de Escalonamento para Jogos" ANSI_RESET " aplicada");
                return true;
            }
            return false;
        }

        case TWEAK_NET_WU_P2P_OFF: {
            bool ok1 = reg_write_dword(HKEY_CURRENT_USER,
                                       "Software\\Microsoft\\Windows\\CurrentVersion\\DeliveryOptimization",
                                       "SystemSettingsDownloadMode", 0);
            bool ok2 = reg_write_dword(HKEY_CURRENT_USER,
                                       "Software\\Policies\\Microsoft\\Windows\\DeliveryOptimization",
                                       "DODownloadMode", 0);
            if (ok1 || ok2) {
                ui_ok("7. " ANSI_BOLD "Upload P2P oculto do Windows Update" ANSI_RESET " bloqueado");
                return true;
            }
            return false;
        }

        case TWEAK_RAM_BG_APPS_OFF: {
            bool ok1 = reg_write_dword(HKEY_CURRENT_USER,
                                       "Software\\Microsoft\\Windows\\CurrentVersion\\BackgroundAccessApplications",
                                       "GlobalUserDisabled", 1);
            bool ok2 = reg_write_dword(HKEY_CURRENT_USER,
                                       "Software\\Microsoft\\Windows\\CurrentVersion\\Search",
                                       "BackgroundAppGlobalToggle", 0);
            if (ok1 && ok2) {
                ui_ok("8. " ANSI_BOLD "Apps da Microsoft Store em 2o plano" ANSI_RESET " bloqueados (economia de RAM)");
                return true;
            }
            return false;
        }

        case TWEAK_PRIV_BING_SEARCH_OFF: {
            bool ok1 = reg_write_dword(HKEY_CURRENT_USER,
                                       "Software\\Policies\\Microsoft\\Windows\\Explorer",
                                       "DisableSearchBoxSuggestions", 1);
            bool ok2 = reg_write_dword(HKEY_CURRENT_USER,
                                       "Software\\Microsoft\\Windows\\CurrentVersion\\Search",
                                       "BingSearchEnabled", 0);
            if (ok1 || ok2) {
                ui_ok("9. " ANSI_BOLD "Pesquisa Web do Bing no Menu Iniciar" ANSI_RESET " desativada (busca 100%% local)");
                return true;
            }
            return false;
        }

        case TWEAK_PRIV_BLOATWARE_ADS_OFF: {
            bool ok1 = reg_write_dword(HKEY_CURRENT_USER,
                                       "Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
                                       "SilentInstalledAppsEnabled", 0);
            reg_write_dword(HKEY_CURRENT_USER,
                            "Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
                            "SubscribedContent-338388Enabled", 0);
            bool ok2 = reg_write_dword(HKEY_CURRENT_USER,
                                       "Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo",
                                       "Enabled", 0);
            if (ok1 && ok2) {
                ui_ok("10. " ANSI_BOLD "Instalacao silenciosa de Bloatware e ID de Anuncios" ANSI_RESET " bloqueados");
                return true;
            }
            return false;
        }

        case TWEAK_UI_FAST_MENUS_EXPLORER: {
            bool ok1 = reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", "20");
            bool ok2 = reg_write_dword(HKEY_CURRENT_USER,
                                       "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                                       "ShowSyncProviderNotifications", 0);
            if (ok1 && ok2) {
                ui_ok("11. " ANSI_BOLD "Menus Instantaneos (20ms) e Explorer sem anuncios" ANSI_RESET " ativados");
                return true;
            }
            return false;
        }

        case TWEAK_UI_FAST_KILL_HUNG_APPS: {
            bool ok1 = reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "HungAppTimeout", "2000");
            bool ok2 = reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "WaitToKillAppTimeout", "2000");
            bool ok3 = reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "AutoEndTasks", "1");
            if (ok1 && ok2 && ok3) {
                ui_ok("12. " ANSI_BOLD "Fechamento rapido de programas travados (2s)" ANSI_RESET " ativado");
                return true;
            }
            return false;
        }
    }
    return false;
}

void tweaks_print_report(const tweaks_report_t *report) {
    for (int i = 0; i < report->rule_count; i++) {
        const tweak_rule_t *r = &report->rules[i];
        char val_line[180];
        if (r->is_optimized) {
            snprintf(val_line, sizeof(val_line), "JA OTIMIZADO (%s)", r->current_state);
        } else {
            snprintf(val_line, sizeof(val_line), "PENDENTE (%s  -->  %s)", r->current_state, r->target_state);
        }

        ui_item_block(
            r->is_optimized,
            r->title,
            val_line,
            r->explanation,
            r->is_optimized
                ? "Ja esta otimizado no seu Windows."
                : "Pode ser aplicado com 1 Enter na tela '3. Otimizar Windows, Rede, RAM e Jogos'."
        );
    }

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
            : "Abra '4. Programas na Inicializacao' para desabilitar os que nao precisa."
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

    ui_header_box("PAINEL COMPLETO DE OTIMIZACAO DO WINDOWS (12 REGRAS EM 4 CATEGORIAS)",
                  "Itens 'JA OTIMIZADO' sao ignorados. Voce pode aplicar todos os 'PENDENTE' ou escolher 1 por 1.");

    const char *last_cat = "";
    for (int i = 0; i < out_current->rule_count; i++) {
        const tweak_rule_t *r = &out_current->rules[i];
        if (strcmp(last_cat, r->category) != 0) {
            last_cat = r->category;
            printf("  " ANSI_CYAN ANSI_BOLD ":: CATEGORIA: %s" ANSI_RESET "\n", last_cat);
            printf("  " ANSI_DIM "──────────────────────────────────────────────────────────────────────────" ANSI_RESET "\n");
        }

        char state_line[180];
        if (r->is_optimized) {
            snprintf(state_line, sizeof(state_line), "JA OTIMIZADO (%s)", r->current_state);
        } else {
            snprintf(state_line, sizeof(state_line), "PENDENTE: %s  -->  %s", r->current_state, r->target_state);
        }

        ui_item_block(
            r->is_optimized,
            r->title,
            state_line,
            r->tech_location,
            r->is_optimized ? "Ja esta aplicado no seu PC. Nao precisa rodar novamente." : r->explanation
        );
    }

    return out_current->pending_tweaks_count;
}

int tweaks_apply_pending_optimizations(const tweaks_report_t *current) {
    int applied = 0;
    for (int i = 0; i < current->rule_count; i++) {
        const tweak_rule_t *r = &current->rules[i];
        if (r->is_optimized) continue;
        if (tweaks_apply_single_rule(r->id)) {
            applied++;
        }
    }
    return applied;
}

void tweaks_restore_windows_defaults(void) {
    system("powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e >nul 2>nul");
    system("powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMIN 5 >nul 2>nul");
    system("powercfg /setactive SCHEME_CURRENT >nul 2>nul");
    reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", "400");
    reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "HungAppTimeout", "5000");
    reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "WaitToKillAppTimeout", "20000");
    reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "AutoEndTasks", "0");
    reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseSpeed", "1");
    reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseThreshold1", "6");
    reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseThreshold2", "10");
    int mouse_params[3] = {6, 10, 1};
    SystemParametersInfoA(SPI_SETMOUSE, 0, mouse_params, SPIF_SENDCHANGE);

    reg_write_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", "AppCaptureEnabled", 1);
    reg_write_dword(HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_Enabled", 1);
    reg_write_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", "ShowSyncProviderNotifications", 1);
    reg_write_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\DeliveryOptimization", "SystemSettingsDownloadMode", 1);
    reg_write_dword(HKEY_CURRENT_USER, "Software\\Policies\\Microsoft\\Windows\\DeliveryOptimization", "DODownloadMode", 1);
    reg_write_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\BackgroundAccessApplications", "GlobalUserDisabled", 0);
    reg_write_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Search", "BackgroundAppGlobalToggle", 1);
    reg_write_dword(HKEY_CURRENT_USER, "Software\\Policies\\Microsoft\\Windows\\Explorer", "DisableSearchBoxSuggestions", 0);
    reg_write_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Search", "BingSearchEnabled", 1);
    reg_write_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager", "SilentInstalledAppsEnabled", 1);
    reg_write_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo", "Enabled", 1);
    ui_info("Configuracoes restauradas para os valores padrao de fabrica do Windows.");
}
