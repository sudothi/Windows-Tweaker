#ifndef WT_TWEAKS_H
#define WT_TWEAKS_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_STARTUP_ENTRIES 24

typedef struct {
    char name[64];
    char command[260];
    char source[16]; /* "HKCU\\Run" or "HKLM\\Run" */
} startup_entry_t;

typedef struct {
    /* Startup Apps */
    startup_entry_t startup_apps[MAX_STARTUP_ENTRIES];
    int             startup_count;

    /* Power Plan */
    char            power_plan_guid[64];
    char            power_plan_name[64];
    bool            is_high_performance;

    /* Storage & Filesystem */
    bool            ntfs_trim_enabled;
    bool            ntfs_last_access_disabled;

    /* Gaming & Responsiveness */
    bool            game_dvr_enabled;
    int             menu_show_delay_ms;
    uint32_t        system_responsiveness_pct;
    bool            transparency_enabled;

    /* Computed actionable issues */
    int             issues_detected;
    double          audit_elapsed_ms;
} tweaks_report_t;

/* Audit Windows Registry, power scheme, SSD TRIM, and gaming responsiveness */
void tweaks_run_audit(tweaks_report_t *report);

/* Print formatted OS & Registry diagnostic report */
void tweaks_print_report(const tweaks_report_t *report);

/* Print detailed startup applications table */
void tweaks_print_startup(const tweaks_report_t *report);

/* Apply safe user-level responsiveness & gaming optimizations (supports --dry-run) */
int tweaks_apply_safe_optimizations(bool dry_run);

#endif /* WT_TWEAKS_H */
