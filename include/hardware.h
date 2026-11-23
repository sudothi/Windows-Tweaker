#ifndef WT_HARDWARE_H
#define WT_HARDWARE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    /* CPU Info (via __cpuid & Win32) */
    char     cpu_brand[64];
    char     cpu_vendor[20];
    uint32_t logical_cores;
    bool     virt_firmware_enabled; /* VT-x / AMD-V in BIOS */
    bool     avx2_supported;

    /* Memory (RAM) */
    uint64_t ram_total_bytes;
    uint64_t ram_avail_bytes;
    uint64_t ram_used_bytes;
    uint32_t ram_load_pct;

    /* Motherboard & BIOS */
    char     board_vendor[64];
    char     board_product[64];
    char     bios_version[64];
    bool     secure_boot_enabled;
    bool     hags_enabled; /* Hardware-Accelerated GPU Scheduling */

    /* Uptime */
    uint64_t uptime_seconds;

    double   probe_elapsed_ms;
} hardware_report_t;

/* Query CPU registers, RAM status, BIOS SMBIOS registry & GPU scheduling */
void hardware_run_audit(hardware_report_t *report);

/* Render formatted hardware & firmware diagnostic cards */
void hardware_print_report(const hardware_report_t *report);

#endif /* WT_HARDWARE_H */
