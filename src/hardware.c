#include "hardware.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <intrin.h>

static bool hw_reg_read_str(HKEY root, const char *subkey, const char *val_name, char *out_buf, DWORD buf_size) {
    HKEY hKey;
    if (RegOpenKeyExA(root, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0;
    LONG res = RegQueryValueExA(hKey, val_name, NULL, &type, (LPBYTE)out_buf, &buf_size);
    RegCloseKey(hKey);
    return (res == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ));
}

static bool hw_reg_read_dword(HKEY root, const char *subkey, const char *val_name, DWORD *out_val) {
    HKEY hKey;
    if (RegOpenKeyExA(root, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD data = 0, size = sizeof(data), type = 0;
    LONG res = RegQueryValueExA(hKey, val_name, NULL, &type, (LPBYTE)&data, &size);
    RegCloseKey(hKey);
    if (res == ERROR_SUCCESS && type == REG_DWORD) {
        *out_val = data;
        return true;
    }
    return false;
}

static void query_cpuid_info(hardware_report_t *report) {
    int cpu_info[4] = {0};

    __cpuid(cpu_info, 0);
    memset(report->cpu_vendor, 0, sizeof(report->cpu_vendor));
    memcpy(report->cpu_vendor + 0, &cpu_info[1], 4);
    memcpy(report->cpu_vendor + 4, &cpu_info[3], 4);
    memcpy(report->cpu_vendor + 8, &cpu_info[2], 4);

    __cpuid(cpu_info, 0x80000000);
    unsigned int max_ext = (unsigned int)cpu_info[0];
    if (max_ext >= 0x80000004) {
        char brand[49] = {0};
        __cpuid((int *)(brand + 0),  0x80000002);
        __cpuid((int *)(brand + 16), 0x80000003);
        __cpuid((int *)(brand + 32), 0x80000004);

        const char *p = brand;
        while (*p == ' ') p++;
        strncpy(report->cpu_brand, p, sizeof(report->cpu_brand) - 1);
        /* Remove trailing spaces */
        size_t len = strlen(report->cpu_brand);
        while (len > 0 && report->cpu_brand[len - 1] == ' ') {
            report->cpu_brand[--len] = '\0';
        }
    } else {
        strncpy(report->cpu_brand, "Generic x86_64 Processor", sizeof(report->cpu_brand) - 1);
    }

    __cpuidex(cpu_info, 7, 0);
    report->avx2_supported = ((cpu_info[1] & (1 << 5)) != 0);
}

void hardware_run_audit(hardware_report_t *report) {
    memset(report, 0, sizeof(*report));

    LARGE_INTEGER freq, t_start, t_end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t_start);

    ui_spinner_update(0, "Lendo registradores da CPU (__cpuid)...");
    query_cpuid_info(report);

    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    report->logical_cores = sys_info.dwNumberOfProcessors;
    report->virt_firmware_enabled = (IsProcessorFeaturePresent(PF_VIRT_FIRMWARE_ENABLED) != 0);

    ui_spinner_update(3, "Verificando memoria RAM e BIOS/UEFI...");
    MEMORYSTATUSEX mem_stat;
    mem_stat.dwLength = sizeof(mem_stat);
    if (GlobalMemoryStatusEx(&mem_stat)) {
        report->ram_total_bytes = mem_stat.ullTotalPhys;
        report->ram_avail_bytes = mem_stat.ullAvailPhys;
        report->ram_used_bytes  = mem_stat.ullTotalPhys - mem_stat.ullAvailPhys;
        report->ram_load_pct    = mem_stat.dwMemoryLoad;
    }

    if (!hw_reg_read_str(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
                         "BaseBoardManufacturer", report->board_vendor, sizeof(report->board_vendor))) {
        strncpy(report->board_vendor, "OEM", sizeof(report->board_vendor) - 1);
    }
    if (!hw_reg_read_str(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
                         "BaseBoardProduct", report->board_product, sizeof(report->board_product))) {
        strncpy(report->board_product, "Board", sizeof(report->board_product) - 1);
    }
    if (!hw_reg_read_str(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
                         "BIOSVersion", report->bios_version, sizeof(report->bios_version))) {
        strncpy(report->bios_version, "UEFI", sizeof(report->bios_version) - 1);
    }

    DWORD sb_val = 0;
    if (hw_reg_read_dword(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State",
                          "UEFISecureBootEnabled", &sb_val)) {
        report->secure_boot_enabled = (sb_val == 1);
    }

    DWORD hags_val = 0;
    if (hw_reg_read_dword(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
                          "HwSchMode", &hags_val)) {
        report->hags_enabled = (hags_val == 2);
    }

    report->uptime_seconds = GetTickCount64() / 1000ULL;

    QueryPerformanceCounter(&t_end);
    report->probe_elapsed_ms = (double)(t_end.QuadPart - t_start.QuadPart) * 1000.0 / (double)freq.QuadPart;

    char summary[160];
    snprintf(summary, sizeof(summary), "Hardware detectado: %s (%u threads)", report->cpu_brand, report->logical_cores);
    ui_spinner_stop(summary, true);
}

void hardware_print_report(const hardware_report_t *report) {
    ui_ok("Processador : " ANSI_BOLD "%s" ANSI_RESET " " ANSI_DIM "(%u threads · %s)" ANSI_RESET,
          report->cpu_brand, report->logical_cores, report->avx2_supported ? "AVX2 256-bit" : "SSE2");

    ui_info("Placa-mae   : %s %s " ANSI_DIM "(BIOS %s)" ANSI_RESET,
            report->board_vendor, report->board_product, report->bios_version);

    char ram_used_s[32], ram_tot_s[32], ram_bar[256];
    ui_format_bytes(report->ram_used_bytes, ram_used_s, sizeof(ram_used_s));
    ui_format_bytes(report->ram_total_bytes, ram_tot_s, sizeof(ram_tot_s));
    ui_bar((double)report->ram_load_pct / 100.0, 18, ram_bar, sizeof(ram_bar));

    if (report->ram_load_pct > 85) {
        ui_warn("Memoria RAM : [%s] " ANSI_BOLD "%u%%" ANSI_RESET " (%s usados de %s)",
                ram_bar, report->ram_load_pct, ram_used_s, ram_tot_s);
        ui_line(ANSI_DIM "Por que importa: acima de 85%% o Windows passa a usar o SSD como memoria (pagefile), causando travadinhas." ANSI_RESET);
    } else {
        ui_ok("Memoria RAM : [%s] " ANSI_BOLD "%u%%" ANSI_RESET " (%s usados de %s)",
              ram_bar, report->ram_load_pct, ram_used_s, ram_tot_s);
    }

    uint64_t days  = report->uptime_seconds / 86400;
    uint64_t hours = (report->uptime_seconds % 86400) / 3600;
    uint64_t mins  = (report->uptime_seconds % 3600) / 60;
    if (days >= 7) {
        ui_warn("Tempo ligado: " ANSI_BOLD "%llud %lluh %llum" ANSI_RESET " sem reiniciar",
                (unsigned long long)days, (unsigned long long)hours, (unsigned long long)mins);
        ui_line(ANSI_DIM "Por que importa: o 'Inicio Rapido' do Windows acumula cache de drivers. Reiniciar limpa o kernel." ANSI_RESET);
    } else {
        ui_ok("Tempo ligado: " ANSI_BOLD "%llud %lluh %llum" ANSI_RESET,
              (unsigned long long)days, (unsigned long long)hours, (unsigned long long)mins);
    }

    if (report->virt_firmware_enabled) {
        ui_ok("Virtualizacao na BIOS (AMD-V / VT-x): " ANSI_BOLD "Ativa" ANSI_RESET);
    } else {
        ui_warn("Virtualizacao na BIOS (AMD-V / VT-x): " ANSI_BOLD "Desativada" ANSI_RESET);
        ui_line(ANSI_DIM "Por que importa: sem ela, Docker, WSL2 e maquinas virtuais nao funcionam ou ficam muito lentos." ANSI_RESET);
    }

    if (report->secure_boot_enabled) {
        ui_ok("UEFI Secure Boot: " ANSI_BOLD "Ativo" ANSI_RESET);
    } else {
        ui_warn("UEFI Secure Boot: " ANSI_BOLD "Inativo na BIOS" ANSI_RESET);
        ui_line(ANSI_DIM "Por que importa: exigido por jogos com anti-cheat de kernel (Valorant/Vanguard, EA, Faceit)." ANSI_RESET);
    }
}
