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
    snprintf(summary, sizeof(summary), "Hardware analisado (%s · %u threads)", report->cpu_brand, report->logical_cores);
    ui_spinner_stop(summary, true);
}

void hardware_print_report(const hardware_report_t *report) {
    char cpu_val[128];
    snprintf(cpu_val, sizeof(cpu_val), "%s (%u threads · %s)",
             report->cpu_brand, report->logical_cores, report->avx2_supported ? "AVX2 Ativo" : "SSE2");
    ui_item_block(
        true,
        "Processador (CPU)",
        cpu_val,
        "Modelo lido direto do silicio via instrucao __cpuid e suporte a vetores matematicos AVX2.",
        "Operando com todas as instrucoes vetoriais habilitadas."
    );

    char ram_used_s[32], ram_tot_s[32], ram_bar[256], ram_val[384];
    ui_format_bytes(report->ram_used_bytes, ram_used_s, sizeof(ram_used_s));
    ui_format_bytes(report->ram_total_bytes, ram_tot_s, sizeof(ram_tot_s));
    ui_bar((double)report->ram_load_pct / 100.0, 18, ram_bar, sizeof(ram_bar));
    snprintf(ram_val, sizeof(ram_val), "[%s] %u%% em uso (%s de %s)",
             ram_bar, report->ram_load_pct, ram_used_s, ram_tot_s);

    ui_item_block(
        report->ram_load_pct <= 85,
        "Memoria RAM Fisica",
        ram_val,
        "Quantidade de memoria RAM ocupada neste momento pelos programas abertos.",
        report->ram_load_pct <= 85
            ? "Folga de memoria saudavel para abrir jogos e aplicacoes pesadas."
            : "Uso alto de RAM! Desative programas que iniciam sozinhos no boot para liberar memoria."
    );

    uint64_t days  = report->uptime_seconds / 86400;
    uint64_t hours = (report->uptime_seconds % 86400) / 3600;
    uint64_t mins  = (report->uptime_seconds % 3600) / 60;
    char up_val[128];
    snprintf(up_val, sizeof(up_val), "%llud %lluh %llum sem reiniciar (Placa-mae %s · BIOS %s)",
             (unsigned long long)days, (unsigned long long)hours, (unsigned long long)mins,
             report->board_product, report->bios_version);

    ui_item_block(
        days < 7,
        "Tempo Ligado & BIOS",
        up_val,
        "Tempo continuo desde a ultima reinicializacao completa do kernel do Windows.",
        days < 7
            ? "Tempo ligado normal."
            : "Mais de 7 dias sem reiniciar. Clique em 'Reiniciar' no Windows para limpar a memoria de drivers."
    );

    ui_item_block(
        report->virt_firmware_enabled,
        "Virtualizacao na BIOS",
        report->virt_firmware_enabled ? "Ativada (AMD-V / VT-x)" : "Desativada na BIOS",
        "Permite rodar maquinas virtuais, WSL2 (Linux), Docker e emuladores de Android na velocidade maxima.",
        report->virt_firmware_enabled
            ? "Sua BIOS ja esta com a virtualizacao do processador ligada."
            : "Entre na BIOS ao ligar o PC e ative 'SVM Mode' (AMD) ou 'Intel Virtualization'."
    );

    ui_item_block(
        report->secure_boot_enabled,
        "UEFI Secure Boot",
        report->secure_boot_enabled ? "Ativado na BIOS" : "Inativo na BIOS",
        "Protecao de inicializacao da placa-mae exigida por anti-cheats modernos (Valorant, EA FC, Faceit).",
        report->secure_boot_enabled
            ? "Secure Boot ativo e pronto para jogos competitivos."
            : "Se voce joga titulos com anti-cheat de kernel, ative o 'Secure Boot' na aba Boot/Security da BIOS."
    );
}
