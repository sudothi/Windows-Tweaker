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

    /* Vendor ID */
    __cpuid(cpu_info, 0);
    memset(report->cpu_vendor, 0, sizeof(report->cpu_vendor));
    memcpy(report->cpu_vendor + 0, &cpu_info[1], 4);
    memcpy(report->cpu_vendor + 4, &cpu_info[3], 4);
    memcpy(report->cpu_vendor + 8, &cpu_info[2], 4);

    /* Extended leaf count for brand string */
    __cpuid(cpu_info, 0x80000000);
    unsigned int max_ext = (unsigned int)cpu_info[0];
    if (max_ext >= 0x80000004) {
        char brand[49] = {0};
        __cpuid((int *)(brand + 0),  0x80000002);
        __cpuid((int *)(brand + 16), 0x80000003);
        __cpuid((int *)(brand + 32), 0x80000004);

        /* Trim leading spaces */
        const char *p = brand;
        while (*p == ' ') p++;
        strncpy(report->cpu_brand, p, sizeof(report->cpu_brand) - 1);
    } else {
        strncpy(report->cpu_brand, "Generic x86_64 Processor", sizeof(report->cpu_brand) - 1);
    }

    /* Check AVX2 support (Leaf 7, Subleaf 0, EBX bit 5) */
    __cpuidex(cpu_info, 7, 0);
    report->avx2_supported = ((cpu_info[1] & (1 << 5)) != 0);
}

void hardware_run_audit(hardware_report_t *report) {
    memset(report, 0, sizeof(*report));

    LARGE_INTEGER freq, t_start, t_end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t_start);

    ui_spinner_tick(0, "Probing CPU registers via __cpuid...", "x86_64 Silicon");
    query_cpuid_info(report);

    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    report->logical_cores = sys_info.dwNumberOfProcessors;
    report->virt_firmware_enabled = (IsProcessorFeaturePresent(PF_VIRT_FIRMWARE_ENABLED) != 0);

    ui_spinner_tick(3, "Querying physical RAM & kernel memory...", "GlobalMemoryStatusEx");
    MEMORYSTATUSEX mem_stat;
    mem_stat.dwLength = sizeof(mem_stat);
    if (GlobalMemoryStatusEx(&mem_stat)) {
        report->ram_total_bytes = mem_stat.ullTotalPhys;
        report->ram_avail_bytes = mem_stat.ullAvailPhys;
        report->ram_used_bytes  = mem_stat.ullTotalPhys - mem_stat.ullAvailPhys;
        report->ram_load_pct    = mem_stat.dwMemoryLoad;
    }

    ui_spinner_tick(6, "Reading BIOS/UEFI & Motherboard descriptors...", "SMBIOS Registry");
    if (!hw_reg_read_str(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
                         "BaseBoardManufacturer", report->board_vendor, sizeof(report->board_vendor))) {
        strncpy(report->board_vendor, "Standard OEM", sizeof(report->board_vendor) - 1);
    }
    if (!hw_reg_read_str(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
                         "BaseBoardProduct", report->board_product, sizeof(report->board_product))) {
        strncpy(report->board_product, "System Board", sizeof(report->board_product) - 1);
    }
    if (!hw_reg_read_str(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
                         "BIOSVersion", report->bios_version, sizeof(report->bios_version))) {
        strncpy(report->bios_version, "UEFI Firmware", sizeof(report->bios_version) - 1);
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

    char summary[128];
    snprintf(summary, sizeof(summary), "%s (%u threads)", report->cpu_brand, report->logical_cores);
    ui_spinner_finish(UI_STATUS_OK, "Hardware & BIOS Probe", summary, report->probe_elapsed_ms);
}

void hardware_print_report(const hardware_report_t *report) {
    ui_section_header("🖥️", "HARDWARE & BIOS/UEFI TELEMETRY", "Silicon capabilities, Memory load & Firmware settings");

    ui_card_begin("SYSTEM & SILICON PROFILE", 70);

    char cpu_line[128];
    snprintf(cpu_line, sizeof(cpu_line), "%s (%u Logical Threads)", report->cpu_brand, report->logical_cores);
    ui_card_row("Processor (CPU)", cpu_line, ANSI_WHITE ANSI_BOLD, 70);

    char board_line[128];
    snprintf(board_line, sizeof(board_line), "%s %s (BIOS: %s)",
             report->board_vendor, report->board_product, report->bios_version);
    ui_card_row("Motherboard / BIOS", board_line, ANSI_GRAY, 70);

    char ram_used_s[32], ram_tot_s[32], ram_bar[256], ram_line[384];
    ui_format_bytes(report->ram_used_bytes, ram_used_s, sizeof(ram_used_s));
    ui_format_bytes(report->ram_total_bytes, ram_tot_s, sizeof(ram_tot_s));
    ui_usage_bar((double)report->ram_load_pct / 100.0, 18, ram_bar, sizeof(ram_bar));
    snprintf(ram_line, sizeof(ram_line), "[%s] %u%% (%s / %s)",
             ram_bar, report->ram_load_pct, ram_used_s, ram_tot_s);
    ui_card_row("Physical Memory (RAM)", ram_line, ANSI_WHITE, 70);

    uint64_t days  = report->uptime_seconds / 86400;
    uint64_t hours = (report->uptime_seconds % 86400) / 3600;
    uint64_t mins  = (report->uptime_seconds % 3600) / 60;
    char uptime_line[128];
    snprintf(uptime_line, sizeof(uptime_line), "%llud %lluh %llum %s",
             (unsigned long long)days, (unsigned long long)hours, (unsigned long long)mins,
             days >= 7 ? "(Consider clean reboot to flush kernel pools)" : "(Healthy uptime)");
    ui_card_row("Kernel Uptime", uptime_line, days >= 7 ? ANSI_AMBER : ANSI_GREEN, 70);

    ui_card_end(70);

    printf("\n");
    ui_print_status(report->virt_firmware_enabled ? UI_STATUS_OK : UI_STATUS_WARN,
                    "BIOS Virtualization",
                    report->virt_firmware_enabled
                        ? "VT-x / AMD-V Active in BIOS (Optimal for WSL2/Docker/VMs)"
                        : "DISABLED in BIOS - Enable SVM/VT-x in UEFI Settings");

    ui_print_status(report->hags_enabled ? UI_STATUS_OK : UI_STATUS_INFO,
                    "Hardware GPU Scheduling",
                    report->hags_enabled
                        ? "HAGS Enabled (HwSchMode = 2, lowers GPU scheduling latency)"
                        : "Disabled or default (Enable in Windows Graphics Settings for gaming)");

    ui_print_status(report->secure_boot_enabled ? UI_STATUS_OK : UI_STATUS_WARN,
                    "UEFI Secure Boot",
                    report->secure_boot_enabled
                        ? "Active (Required by modern anti-cheats & kernel protection)"
                        : "Inactive in BIOS");

    ui_print_status(report->avx2_supported ? UI_STATUS_OK : UI_STATUS_INFO,
                    "SIMD Vector Instruction Set",
                    report->avx2_supported ? "AVX2 256-bit SIMD Supported" : "Standard SSE2");
}
