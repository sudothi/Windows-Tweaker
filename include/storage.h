#ifndef WT_STORAGE_H
#define WT_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_COLD_FILES    12
#define MAX_CACHE_TARGETS 10

typedef struct {
    char     name[64];
    char     path[260];
    uint64_t size_bytes;
    uint32_t file_count;
    bool     exists;
    bool     safe_to_clean;
} cache_target_t;

typedef struct {
    char     path[260];
    uint64_t size_bytes;
    int      days_unused;
} cold_file_t;

typedef struct {
    char           drive_root[8];
    uint64_t       total_bytes;
    uint64_t       free_bytes;
    uint64_t       used_bytes;

    cache_target_t caches[MAX_CACHE_TARGETS];
    int            cache_count;
    uint64_t       total_cache_bytes;
    uint32_t       total_cache_files;

    cold_file_t    cold_files[MAX_COLD_FILES];
    int            cold_file_count;
    uint64_t       total_cold_bytes;

    double         scan_elapsed_ms;
} storage_report_t;

/* Perform full disk, cache, and cold-file audit with live terminal spinner */
void storage_run_audit(storage_report_t *report, uint64_t min_cold_size_mb, int min_cold_days, const char *custom_dir);

/* Render formatted storage audit report to terminal */
void storage_print_report(const storage_report_t *report);

/* Clean safe temporary caches (supports dry_run preview mode) */
void storage_clean_caches(bool dry_run, uint64_t *out_bytes_Cleaned, uint32_t *out_files_cleaned);

#endif /* WT_STORAGE_H */
