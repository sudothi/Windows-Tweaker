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

    ui_spinner_tick(0, "Auditing Windows Registry & Startup keys...", "HKCU & HKLM");

    /* 1. Startup Applications */
    enumerate_startup_key(HKEY_CURRENT_USER,
                          "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          "HKCU\\Run", report);
    enumerate_startup_key(HKEY_LOCAL_MACHINE,
                          "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          "HKLM\\Run", report);

    if (report->startup_count > 6) {
        report->issues_detected++;
    }

    /* 2. Active Power Scheme */
    ui_spinner_tick(2, "Checking Windows Power Scheme...", "Control\\Power");
    if (reg_read_string(HKEY_LOCAL_MACHINE,
                        "SYSTEM\\CurrentControlSet\\Control\\Power\\User\\PowerSchemes",
                        "ActivePowerScheme",
                        report->power_plan_guid, sizeof(report->power_plan_guid))) {
        if (_stricmp(report->power_plan_guid, "8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c") == 0) {
            strncpy(report->power_plan_name, "High Performance", sizeof(report->power_plan_name) - 1);
            report->is_high_performance = true;
        } else if (_stricmp(report->power_plan_guid, "e9a42b02-d5df-448d-aa00-03f14749eb61") == 0) {
            strncpy(report->power_plan_name, "Ultimate Performance", sizeof(report->power_plan_name) - 1);
            report->is_high_performance = true;
        } else if (_stricmp(report->power_plan_guid, "a1841308-3541-4fab-bc81-f71556f20b4a") == 0) {
            strncpy(report->power_plan_name, "Power Saver (Throttled)", sizeof(report->power_plan_name) - 1);
            report->is_high_performance = false;
            report->issues_detected++;
        } else {
            strncpy(report->power_plan_name, "Balanced (Default)", sizeof(report->power_plan_name) - 1);
            report->is_high_performance = false;
            report->issues_detected++;
        }
    } else {
        strncpy(report->power_plan_name, "Balanced (Standard)", sizeof(report->power_plan_name) - 1);
        report->is_high_performance = false;
    }

    /* 3. SSD NTFS TRIM (DisableDeleteNotify == 0 means TRIM is enabled) */
    ui_spinner_tick(4, "Inspecting NTFS & SSD TRIM configuration...", "FileSystem");
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

    /* 4. GameDVR Background Recording */
    ui_spinner_tick(6, "Auditing GameDVR & Multimedia Scheduler...", "GameConfigStore");
    DWORD dvr_enabled = 0, app_capture = 0;
    bool has_dvr = reg_read_dword(HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_Enabled", &dvr_enabled);
    bool has_cap = reg_read_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", "AppCaptureEnabled", &app_capture);
    report->game_dvr_enabled = (has_dvr && dvr_enabled != 0) || (has_cap && app_capture != 0);
    if (report->game_dvr_enabled) {
        report->issues_detected++;
    }

    /* 5. MenuShowDelay (UI Responsiveness) */
    char delay_str[32] = {0};
    if (reg_read_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", delay_str, sizeof(delay_str))) {
        report->menu_show_delay_ms = atoi(delay_str);
    } else {
        report->menu_show_delay_ms = 400;
    }
    if (report->menu_show_delay_ms > 100) {
        report->issues_detected++;
    }

    /* 6. MMCSS SystemResponsiveness */
    DWORD sys_resp = 20;
    if (reg_read_dword(HKEY_LOCAL_MACHINE,
                       "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile",
                       "SystemResponsiveness", &sys_resp)) {
        report->system_responsiveness_pct = sys_resp;
    } else {
        report->system_responsiveness_pct = 20;
    }

    /* 7. Desktop Transparency Effects */
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
    snprintf(summary, sizeof(summary), "%d startup apps, %d tuning opportunity(s) found",
             report->startup_count, report->issues_detected);
    ui_spinner_finish(report->issues_detected > 0 ? UI_STATUS_WARN : UI_STATUS_OK,
                      "Windows OS & Registry Audit", summary, report->audit_elapsed_ms);
}

void tweaks_print_report(const tweaks_report_t *report) {
    ui_section_header("⚙️", "WINDOWS OS & GAMING TWEAKS AUDIT", "Registry, Power Scheme, SSD TRIM & UI Latency");

    /* 1. Power Plan */
    char detail[128];
    snprintf(detail, sizeof(detail), "Active: %s", report->power_plan_name);
    ui_print_status(report->is_high_performance ? UI_STATUS_OK : UI_STATUS_WARN,
                    "CPU Power Scheme", detail);

    /* 2. SSD TRIM */
    ui_print_status(report->ntfs_trim_enabled ? UI_STATUS_OK : UI_STATUS_CRIT,
                    "SSD NTFS TRIM Support",
                    report->ntfs_trim_enabled ? "Enabled (DisableDeleteNotify = 0)" : "DISABLED - Degrades SSD lifespan!");

    /* 3. GameDVR Background Capture */
    ui_print_status(report->game_dvr_enabled ? UI_STATUS_WARN : UI_STATUS_OK,
                    "Xbox GameDVR Capture",
                    report->game_dvr_enabled ? "Active in background (can cause FPS micro-stutter)" : "Disabled (Optimal for gaming)");

    /* 4. Desktop Menu Latency */
    snprintf(detail, sizeof(detail), "%d ms (%s)",
             report->menu_show_delay_ms,
             report->menu_show_delay_ms <= 50 ? "Snappy UI response" : "Default 400ms lag - tuneable to 20ms");
    ui_print_status(report->menu_show_delay_ms <= 50 ? UI_STATUS_OK : UI_STATUS_WARN,
                    "Desktop UI Menu Delay", detail);

    /* 5. Multimedia CPU Reservation */
    snprintf(detail, sizeof(detail), "%u%% background reservation (MMCSS SystemProfile)",
             report->system_responsiveness_pct);
    ui_print_status(report->system_responsiveness_pct <= 10 ? UI_STATUS_OK : UI_STATUS_INFO,
                    "MMCSS CPU Throttling", detail);

    /* 6. Boot Startup Load */
    snprintf(detail, sizeof(detail), "%d auto-start entries in Registry (`startup` to list)",
             report->startup_count);
    ui_print_status(report->startup_count <= 6 ? UI_STATUS_OK : UI_STATUS_WARN,
                    "Boot Startup Applications", detail);
}

void tweaks_print_startup(const tweaks_report_t *report) {
    ui_section_header("🚀", "WINDOWS STARTUP REGISTRY ENTRIES", "Programs launched automatically at boot");

    if (report->startup_count == 0) {
        printf("  " ANSI_GREEN "✓" ANSI_RESET " No registry startup entries detected.\n");
        return;
    }

    printf("  " ANSI_CYAN ANSI_BOLD "%-12s %-26s %s" ANSI_RESET "\n",
           "HIVE", "APPLICATION", "COMMAND / EXECUTABLE");
    ui_divider(72);

    for (int i = 0; i < report->startup_count; i++) {
        const startup_entry_t *e = &report->startup_apps[i];
        char short_cmd[42];
        size_t clen = strlen(e->command);
        if (clen > 38) {
            snprintf(short_cmd, sizeof(short_cmd), "%.35s...", e->command);
        } else {
            snprintf(short_cmd, sizeof(short_cmd), "%s", e->command);
        }

        const char *hive_col = (strncmp(e->source, "HKLM", 4) == 0) ? ANSI_MAGENTA : ANSI_CYAN;
        printf("  %s%-12s" ANSI_RESET " " ANSI_WHITE ANSI_BOLD "%-26.25s" ANSI_RESET " " ANSI_GRAY "%s" ANSI_RESET "\n",
               hive_col, e->source, e->name, short_cmd);
    }
    ui_divider(72);
    printf("  " ANSI_DIM "Total entries: %d (Fewer startup items reduces boot time and idle RAM usage)" ANSI_RESET "\n",
           report->startup_count);
}

int tweaks_apply_safe_optimizations(bool dry_run) {
    int applied = 0;

    ui_section_header("⚡", dry_run ? "OPTIMIZATION PREVIEW (--dry-run)" : "APPLYING SAFE WINDOWS OPTIMIZATIONS",
                      "User-level Registry & Responsiveness Tuning");

    /* 1. Tune MenuShowDelay to 20ms */
    if (dry_run) {
        ui_print_status(UI_STATUS_TWEAK, "Set MenuShowDelay -> 20ms",
                        "[DRY-RUN] Would update HKCU\\Control Panel\\Desktop\\MenuShowDelay");
        applied++;
    } else {
        if (reg_write_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", "20")) {
            ui_print_status(UI_STATUS_OK, "Set MenuShowDelay -> 20ms",
                            "Updated HKCU\\Control Panel\\Desktop\\MenuShowDelay");
            applied++;
        }
    }

    /* 2. Disable GameDVR background capture lag */
    if (dry_run) {
        ui_print_status(UI_STATUS_TWEAK, "Disable GameDVR AppCapture",
                        "[DRY-RUN] Would set HKCU\\...\\GameDVR\\AppCaptureEnabled = 0");
        applied++;
    } else {
        bool ok1 = reg_write_dword(HKEY_CURRENT_USER,
                                   "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR",
                                   "AppCaptureEnabled", 0);
        bool ok2 = reg_write_dword(HKEY_CURRENT_USER,
                                   "System\\GameConfigStore",
                                   "GameDVR_Enabled", 0);
        if (ok1 || ok2) {
            ui_print_status(UI_STATUS_OK, "Disable GameDVR AppCapture",
                            "Background recording disabled for lower frame-time latency");
            applied++;
        }
    }

    return applied;
}
