#include "ui.h"
#include "storage.h"
#include "tweaks.h"
#include "hardware.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WT_VERSION "1.0.0"

static int compute_system_health_score(
    const hardware_report_t *hw,
    const storage_report_t  *st,
    const tweaks_report_t   *tw
) {
    int score = 100;

    /* Deduct for OS / Registry issues */
    score -= (tw->issues_detected * 6);

    /* Deduct if virtual memory / RAM load is critically high */
    if (hw->ram_load_pct > 90) {
        score -= 10;
    } else if (hw->ram_load_pct > 80) {
        score -= 5;
    }

    /* Deduct if virtualization is disabled in BIOS */
    if (!hw->virt_firmware_enabled) {
        score -= 8;
    }

    /* Deduct if C: drive is over 90% full */
    if (st->total_bytes > 0) {
        double disk_ratio = (double)st->used_bytes / (double)st->total_bytes;
        if (disk_ratio > 0.90) {
            score -= 12;
        } else if (disk_ratio > 0.80) {
            score -= 5;
        }
    }

    /* Deduct if temp caches exceed 1 GB */
    if (st->total_cache_bytes > 1024ULL * 1024 * 1024) {
        score -= 7;
    } else if (st->total_cache_bytes > 250ULL * 1024 * 1024) {
        score -= 3;
    }

    if (score < 15) score = 15;
    if (score > 100) score = 100;
    return score;
}

static void print_help(void) {
    ui_print_banner(WT_VERSION);
    ui_section_header("📖", "COMMAND LINE INTERFACE REFERENCE", "Usage: windows-tweaker <command> [flags]");

    printf("  " ANSI_CYAN ANSI_BOLD "CORE COMMANDS:" ANSI_RESET "\n");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "scan",     "Run full system diagnostic (Hardware, BIOS, Storage & OS Tweaks)");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "storage",  "Deep disk audit: cache bloat & forgotten cold large files");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "hardware", "Probe CPU (__cpuid), RAM load, BIOS virtualization & HAGS");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "startup",  "Inspect Windows Registry startup programs (HKCU & HKLM)");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "clean",    "Clean temporary caches (defaults to safe --dry-run preview)");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "optimize", "Apply safe Windows gaming & UI latency tweaks (--dry-run default)");

    printf("\n  " ANSI_CYAN ANSI_BOLD "OPTIONS & FLAGS:" ANSI_RESET "\n");
    printf("    " ANSI_AMBER "%-22s" ANSI_RESET " %s\n", "--dry-run",       "Simulate changes without modifying disk or registry (default)");
    printf("    " ANSI_GREEN "%-22s" ANSI_RESET " %s\n", "--apply",         "Execute cleanup or registry optimizations for real");
    printf("    " ANSI_CYAN  "%-22s" ANSI_RESET " %s\n", "--min-mb <MB>",   "Minimum file size in MB for cold file detection (default: 50)");
    printf("    " ANSI_CYAN  "%-22s" ANSI_RESET " %s\n", "--days <DAYS>",   "Minimum unused days for cold file detection (default: 30)");
    printf("    " ANSI_CYAN  "%-22s" ANSI_RESET " %s\n", "--path <DIR>",    "Custom folder path to scan for cold files");
    printf("    " ANSI_GRAY  "%-22s" ANSI_RESET " %s\n", "--help, -h",      "Display this help screen");
    printf("\n");
}

static void print_quick_actions_footer(void) {
    printf("\n  " ANSI_CYAN ANSI_BOLD "💡 QUICK ACTIONS:" ANSI_RESET "\n");
    printf("    " ANSI_DIM "• Preview temp cache cleanup :" ANSI_RESET "  " ANSI_WHITE "windows-tweaker clean --dry-run" ANSI_RESET "\n");
    printf("    " ANSI_DIM "• Execute temp cache cleanup :" ANSI_RESET "  " ANSI_WHITE "windows-tweaker clean --apply" ANSI_RESET "\n");
    printf("    " ANSI_DIM "• Inspect boot startup items :" ANSI_RESET "  " ANSI_WHITE "windows-tweaker startup" ANSI_RESET "\n");
    printf("    " ANSI_DIM "• Apply gaming & UI tweaks   :" ANSI_RESET "  " ANSI_WHITE "windows-tweaker optimize --apply" ANSI_RESET "\n\n");
}

int main(int argc, char *argv[]) {
    ui_init();

    const char *cmd = (argc >= 2) ? argv[1] : "scan";
    bool apply_mode = false;
    uint64_t min_cold_mb = 50;
    int min_cold_days = 30;
    const char *custom_dir = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--apply") == 0 || strcmp(argv[i], "-f") == 0) {
            apply_mode = true;
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            apply_mode = false;
        } else if (strcmp(argv[i], "--min-mb") == 0 && i + 1 < argc) {
            min_cold_mb = (uint64_t)strtoull(argv[++i], NULL, 10);
            if (min_cold_mb == 0) min_cold_mb = 1;
        } else if (strcmp(argv[i], "--days") == 0 && i + 1 < argc) {
            min_cold_days = atoi(argv[++i]);
            if (min_cold_days < 0) min_cold_days = 0;
        } else if (strcmp(argv[i], "--path") == 0 && i + 1 < argc) {
            custom_dir = argv[++i];
        }
    }

    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0 || strcmp(cmd, "help") == 0) {
        print_help();
        ui_cleanup();
        return 0;
    }

    if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0) {
        printf("windows-tweaker v%s (Win32 C11 Native)\n", WT_VERSION);
        ui_cleanup();
        return 0;
    }

    ui_print_banner(WT_VERSION);

    if (strcmp(cmd, "scan") == 0) {
        printf("\n");
        hardware_report_t hw;
        storage_report_t  st;
        tweaks_report_t   tw;

        hardware_run_audit(&hw);
        storage_run_audit(&st, min_cold_mb, min_cold_days, custom_dir);
        tweaks_run_audit(&tw);

        hardware_print_report(&hw);
        storage_print_report(&st);
        tweaks_print_report(&tw);

        int score = compute_system_health_score(&hw, &st, &tw);
        ui_render_health_score(score, st.total_cache_bytes + st.total_cold_bytes, tw.issues_detected);
        print_quick_actions_footer();
    } else if (strcmp(cmd, "storage") == 0) {
        printf("\n");
        storage_report_t st;
        storage_run_audit(&st, min_cold_mb, min_cold_days, custom_dir);
        storage_print_report(&st);
        printf("\n");
    } else if (strcmp(cmd, "hardware") == 0) {
        printf("\n");
        hardware_report_t hw;
        hardware_run_audit(&hw);
        hardware_print_report(&hw);
        printf("\n");
    } else if (strcmp(cmd, "startup") == 0) {
        printf("\n");
        tweaks_report_t tw;
        tweaks_run_audit(&tw);
        tweaks_print_startup(&tw);
        printf("\n");
    } else if (strcmp(cmd, "clean") == 0) {
        bool dry_run = !apply_mode;
        ui_section_header("🧹", dry_run ? "SAFE CACHE CLEANUP (DRY-RUN PREVIEW)" : "EXECUTING TEMPORARY CACHE CLEANUP",
                          dry_run ? "Pass --apply to permanently delete unlocked temp files" : "Purging unlocked temporary files");

        uint64_t bytes_cleaned = 0;
        uint32_t files_cleaned = 0;
        storage_clean_caches(dry_run, &bytes_cleaned, &files_cleaned);

        char sz[32];
        ui_format_bytes(bytes_cleaned, sz, sizeof(sz));
        char msg[128];
        snprintf(msg, sizeof(msg), "%s %s across %u temporary files",
                 dry_run ? "Would reclaim" : "Reclaimed", sz, files_cleaned);
        printf("\n");
        ui_print_status(UI_STATUS_OK, dry_run ? "Dry-Run Simulation" : "Cleanup Finished", msg);
        if (dry_run) {
            printf("\n  " ANSI_DIM "Tip: Run " ANSI_WHITE "windows-tweaker clean --apply" ANSI_DIM " to perform actual deletion." ANSI_RESET "\n");
        }
        printf("\n");
    } else if (strcmp(cmd, "optimize") == 0) {
        bool dry_run = !apply_mode;
        int count = tweaks_apply_safe_optimizations(dry_run);
        printf("\n  " ANSI_GREEN "✓" ANSI_RESET " %d optimization rule(s) %s.\n",
               count, dry_run ? "previewed (--dry-run)" : "applied to HKCU Registry");
        if (dry_run) {
            printf("  " ANSI_DIM "Tip: Run " ANSI_WHITE "windows-tweaker optimize --apply" ANSI_DIM " to commit these registry tweaks." ANSI_RESET "\n");
        }
        printf("\n");
    } else {
        printf("\n");
        ui_print_status(UI_STATUS_CRIT, "Unknown Command", cmd);
        printf("  " ANSI_GRAY "Run " ANSI_WHITE "windows-tweaker --help" ANSI_GRAY " to see available subcommands." ANSI_RESET "\n\n");
        ui_cleanup();
        return 1;
    }

    ui_cleanup();
    return 0;
}
