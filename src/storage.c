#include "storage.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static int g_spinner_step = 0;

/* Helper: calculate days elapsed between a FILETIME and current system time */
static int filetime_days_since(const FILETIME *ft) {
    FILETIME ft_now;
    GetSystemTimeAsFileTime(&ft_now);

    ULARGE_INTEGER ul_now, ul_file;
    ul_now.LowPart   = ft_now.dwLowDateTime;
    ul_now.HighPart  = ft_now.dwHighDateTime;
    ul_file.LowPart  = ft->dwLowDateTime;
    ul_file.HighPart = ft->dwHighDateTime;

    if (ul_file.QuadPart >= ul_now.QuadPart) {
        return 0;
    }

    /* 100-nanosecond intervals per day: 86400 * 10,000,000 */
    const uint64_t TICKS_PER_DAY = 864000000000ULL;
    uint64_t diff = ul_now.QuadPart - ul_file.QuadPart;
    return (int)(diff / TICKS_PER_DAY);
}

/* Recursive directory size scanner with depth guard & spinner tick */
static void scan_dir_size_recursive(
    const char *dir_path,
    int depth,
    int max_depth,
    uint64_t *acc_bytes,
    uint32_t *acc_files,
    const char *spinner_label
) {
    if (depth > max_depth) return;

    char search_pattern[MAX_PATH];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*", dir_path);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
            continue;
        }

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            continue; /* Skip symlinks/junctions to prevent cycles */
        }

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char child_path[MAX_PATH];
            snprintf(child_path, sizeof(child_path), "%s\\%s", dir_path, fd.cFileName);
            scan_dir_size_recursive(child_path, depth + 1, max_depth, acc_bytes, acc_files, spinner_label);
        } else {
            ULARGE_INTEGER fsize;
            fsize.LowPart  = fd.nFileSizeLow;
            fsize.HighPart = fd.nFileSizeHigh;
            *acc_bytes += fsize.QuadPart;
            (*acc_files)++;

            if ((*acc_files % 350) == 0 && spinner_label) {
                ui_spinner_tick(g_spinner_step++, "Scanning storage caches...", spinner_label);
            }
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
}

/* Insert a cold file into a top-N sorted array (largest files first) */
static void record_cold_file(storage_report_t *report, const char *full_path, uint64_t size_bytes, int days_unused) {
    int insert_idx = -1;

    if (report->cold_file_count < MAX_COLD_FILES) {
        insert_idx = report->cold_file_count;
        report->cold_file_count++;
    } else if (size_bytes > report->cold_files[MAX_COLD_FILES - 1].size_bytes) {
        insert_idx = MAX_COLD_FILES - 1;
    }

    if (insert_idx == -1) return;

    strncpy(report->cold_files[insert_idx].path, full_path, sizeof(report->cold_files[insert_idx].path) - 1);
    report->cold_files[insert_idx].path[sizeof(report->cold_files[insert_idx].path) - 1] = '\0';
    report->cold_files[insert_idx].size_bytes  = size_bytes;
    report->cold_files[insert_idx].days_unused = days_unused;

    /* Bubble up to maintain descending order by size */
    for (int i = insert_idx; i > 0; i--) {
        if (report->cold_files[i].size_bytes > report->cold_files[i - 1].size_bytes) {
            cold_file_t tmp = report->cold_files[i - 1];
            report->cold_files[i - 1] = report->cold_files[i];
            report->cold_files[i] = tmp;
        } else {
            break;
        }
    }
}

/* Scan user directories for cold/forgotten large files */
static void scan_cold_files_recursive(
    const char *dir_path,
    int depth,
    int max_depth,
    uint64_t min_bytes,
    int min_days,
    storage_report_t *report,
    uint32_t *inspected_count
) {
    if (depth > max_depth) return;

    char search_pattern[MAX_PATH];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*", dir_path);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            /* Skip heavy hidden system folders */
            if (fd.cFileName[0] == '.' || _stricmp(fd.cFileName, "AppData") == 0) {
                continue;
            }
            scan_cold_files_recursive(full_path, depth + 1, max_depth, min_bytes, min_days, report, inspected_count);
        } else {
            (*inspected_count)++;
            if ((*inspected_count % 250) == 0) {
                ui_spinner_tick(g_spinner_step++, "Hunting cold & unused large files...", dir_path);
            }

            ULARGE_INTEGER fsize;
            fsize.LowPart  = fd.nFileSizeLow;
            fsize.HighPart = fd.nFileSizeHigh;

            if (fsize.QuadPart >= min_bytes) {
                /* Use last write or last access time */
                int days_write  = filetime_days_since(&fd.ftLastWriteTime);
                int days_access = filetime_days_since(&fd.ftLastAccessTime);
                int days_unused = (days_write > days_access) ? days_write : days_access;

                if (days_unused >= min_days) {
                    report->total_cold_bytes += fsize.QuadPart;
                    record_cold_file(report, full_path, fsize.QuadPart, days_unused);
                }
            }
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
}

static void add_cache_candidate(storage_report_t *report, const char *name, const char *path, bool safe_to_clean) {
    if (report->cache_count >= MAX_CACHE_TARGETS || !path || path[0] == '\0') return;

    /* Avoid duplicates if %TEMP% and %LOCALAPPDATA%\Temp resolve to same path */
    for (int i = 0; i < report->cache_count; i++) {
        if (_stricmp(report->caches[i].path, path) == 0) {
            return;
        }
    }

    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return;
    }

    cache_target_t *target = &report->caches[report->cache_count++];
    strncpy(target->name, name, sizeof(target->name) - 1);
    strncpy(target->path, path, sizeof(target->path) - 1);
    target->size_bytes    = 0;
    target->file_count    = 0;
    target->exists        = true;
    target->safe_to_clean = safe_to_clean;

    ui_spinner_tick(g_spinner_step++, "Auditing cache directory...", name);
    scan_dir_size_recursive(path, 0, 5, &target->size_bytes, &target->file_count, name);

    report->total_cache_bytes += target->size_bytes;
    report->total_cache_files += target->file_count;
}

void storage_run_audit(storage_report_t *report, uint64_t min_cold_size_mb, int min_cold_days, const char *custom_dir) {
    memset(report, 0, sizeof(*report));

    LARGE_INTEGER freq, t_start, t_end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t_start);

    strncpy(report->drive_root, "C:\\", sizeof(report->drive_root) - 1);
    ULARGE_INTEGER free_avail, total_bytes, total_free;
    if (GetDiskFreeSpaceExA(report->drive_root, &free_avail, &total_bytes, &total_free)) {
        report->total_bytes = total_bytes.QuadPart;
        report->free_bytes  = total_free.QuadPart;
        report->used_bytes  = total_bytes.QuadPart - total_free.QuadPart;
    }

    char local_appdata[MAX_PATH] = {0};
    char user_profile[MAX_PATH]  = {0};
    char windir[MAX_PATH]        = {0};
    char temp_dir[MAX_PATH]      = {0};

    GetEnvironmentVariableA("LOCALAPPDATA", local_appdata, sizeof(local_appdata));
    GetEnvironmentVariableA("USERPROFILE", user_profile, sizeof(user_profile));
    GetEnvironmentVariableA("WINDIR", windir, sizeof(windir));
    GetTempPathA(sizeof(temp_dir), temp_dir);

    /* Remove trailing slash from temp_dir if present */
    size_t tlen = strlen(temp_dir);
    if (tlen > 3 && temp_dir[tlen - 1] == '\\') {
        temp_dir[tlen - 1] = '\0';
    }

    char path_buf[MAX_PATH];

    /* 1. User Temp Directory */
    if (temp_dir[0]) {
        add_cache_candidate(report, "User Temp (%TEMP%)", temp_dir, true);
    }

    /* 2. Windows System Temp */
    if (windir[0]) {
        snprintf(path_buf, sizeof(path_buf), "%s\\Temp", windir);
        add_cache_candidate(report, "System Temp (Win\\Temp)", path_buf, true);

        snprintf(path_buf, sizeof(path_buf), "%s\\Prefetch", windir);
        add_cache_candidate(report, "Windows Prefetch Cache", path_buf, false);

        snprintf(path_buf, sizeof(path_buf), "%s\\SoftwareDistribution\\Download", windir);
        add_cache_candidate(report, "Windows Update Cache", path_buf, true);
    }

    /* 3. CrashDumps, Pip, NPM, NuGet & DirectX Shader caches */
    if (local_appdata[0]) {
        snprintf(path_buf, sizeof(path_buf), "%s\\CrashDumps", local_appdata);
        add_cache_candidate(report, "Application CrashDumps", path_buf, true);

        snprintf(path_buf, sizeof(path_buf), "%s\\D3DSCache", local_appdata);
        add_cache_candidate(report, "DirectX Shader Cache", path_buf, true);

        snprintf(path_buf, sizeof(path_buf), "%s\\npm-cache", local_appdata);
        add_cache_candidate(report, "NPM Package Cache", path_buf, false);

        snprintf(path_buf, sizeof(path_buf), "%s\\pip\\Cache", local_appdata);
        add_cache_candidate(report, "Python Pip Cache", path_buf, false);

        snprintf(path_buf, sizeof(path_buf), "%s\\Google\\Chrome\\User Data\\Default\\Cache", local_appdata);
        add_cache_candidate(report, "Chrome Browser Cache", path_buf, false);
    }

    /* 4. Cold & Forgotten Large Files scan */
    uint64_t min_bytes = min_cold_size_mb * 1024ULL * 1024ULL;
    uint32_t inspected = 0;

    if (custom_dir && custom_dir[0] != '\0') {
        scan_cold_files_recursive(custom_dir, 0, 4, min_bytes, min_cold_days, report, &inspected);
    } else if (user_profile[0]) {
        const char *subfolders[] = { "Downloads", "Documents", "Videos", "Desktop" };
        for (int i = 0; i < 4; i++) {
            snprintf(path_buf, sizeof(path_buf), "%s\\%s", user_profile, subfolders[i]);
            DWORD attr = GetFileAttributesA(path_buf);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                scan_cold_files_recursive(path_buf, 0, 3, min_bytes, min_cold_days, report, &inspected);
            }
        }
    }

    QueryPerformanceCounter(&t_end);
    report->scan_elapsed_ms = (double)(t_end.QuadPart - t_start.QuadPart) * 1000.0 / (double)freq.QuadPart;

    char summary_buf[128];
    char size_buf[32];
    ui_format_bytes(report->total_cache_bytes + report->total_cold_bytes, size_buf, sizeof(size_buf));
    snprintf(summary_buf, sizeof(summary_buf), "Found %s across %u cached files & %d cold item(s)",
             size_buf, report->total_cache_files, report->cold_file_count);
    ui_spinner_finish(UI_STATUS_OK, "Storage Engine Audit", summary_buf, report->scan_elapsed_ms);
}

void storage_print_report(const storage_report_t *report) {
    ui_section_header("💾", "STORAGE & CACHE INTELLIGENCE", "Disk capacity, temporary bloat & cold files");

    /* Drive capacity visual bar */
    if (report->total_bytes > 0) {
        double ratio = (double)report->used_bytes / (double)report->total_bytes;
        char used_s[32], total_s[32], free_s[32], bar_s[256];
        ui_format_bytes(report->used_bytes, used_s, sizeof(used_s));
        ui_format_bytes(report->total_bytes, total_s, sizeof(total_s));
        ui_format_bytes(report->free_bytes, free_s, sizeof(free_s));
        ui_usage_bar(ratio, 26, bar_s, sizeof(bar_s));

        printf("  " ANSI_WHITE ANSI_BOLD "Drive %-4s" ANSI_RESET " [%s] " ANSI_BOLD "%5.1f%%" ANSI_RESET
               "  " ANSI_GRAY "%s used / %s total (" ANSI_CYAN "%s free" ANSI_GRAY ")" ANSI_RESET "\n\n",
               report->drive_root, bar_s, ratio * 100.0, used_s, total_s, free_s);
    }

    /* Cache Black Holes Table */
    printf("  " ANSI_CYAN ANSI_BOLD "%-28s %-14s %-12s %s" ANSI_RESET "\n",
           "CACHE / TEMP LOCATION", "SIZE", "FILES", "ACTIONABILITY");
    ui_divider(72);

    for (int i = 0; i < report->cache_count; i++) {
        const cache_target_t *c = &report->caches[i];
        char sz[32];
        ui_format_bytes(c->size_bytes, sz, sizeof(sz));

        const char *size_col = (c->size_bytes > 250ULL * 1024 * 1024) ? ANSI_AMBER :
                               (c->size_bytes > 20ULL * 1024 * 1024)  ? ANSI_CYAN  : ANSI_GRAY;

        const char *tag = c->safe_to_clean
            ? (ANSI_GREEN "● Auto-cleanable (`clean`)" ANSI_RESET)
            : (ANSI_DIM   "○ App-managed cache" ANSI_RESET);

        printf("  " ANSI_WHITE "%-28s" ANSI_RESET " %s%-14s" ANSI_RESET " " ANSI_GRAY "%-12u" ANSI_RESET " %s\n",
               c->name, size_col, sz, c->file_count, tag);
    }

    char total_cache_str[32];
    ui_format_bytes(report->total_cache_bytes, total_cache_str, sizeof(total_cache_str));
    ui_divider(72);
    printf("  " ANSI_BOLD "%-28s " ANSI_CYAN "%-14s" ANSI_RESET " " ANSI_GRAY "%u total files" ANSI_RESET "\n",
           "Total Cache Footprint:", total_cache_str, report->total_cache_files);

    /* Cold / Unused Large Files */
    printf("\n  " ANSI_CYAN ANSI_BOLD "❄ COLD STORAGE AUDIT (Large files unused for 30+ days)" ANSI_RESET "\n");
    ui_divider(72);
    if (report->cold_file_count == 0) {
        printf("  " ANSI_GREEN "✓" ANSI_RESET " " ANSI_GRAY "No oversized cold files found in user folders." ANSI_RESET "\n");
    } else {
        for (int i = 0; i < report->cold_file_count; i++) {
            char sz[32];
            ui_format_bytes(report->cold_files[i].size_bytes, sz, sizeof(sz));

            /* Truncate middle of path if very long */
            const char *p = report->cold_files[i].path;
            char short_path[54];
            size_t plen = strlen(p);
            if (plen > 50) {
                snprintf(short_path, sizeof(short_path), "...%s", p + (plen - 47));
            } else {
                snprintf(short_path, sizeof(short_path), "%s", p);
            }

            printf("  " ANSI_AMBER "%-11s" ANSI_RESET " " ANSI_DIM "[%3d days old]" ANSI_RESET "  " ANSI_WHITE "%s" ANSI_RESET "\n",
                   sz, report->cold_files[i].days_unused, short_path);
        }
    }
}

static void clean_dir_contents(const char *dir_path, bool dry_run, uint64_t *bytes_freed, uint32_t *files_deleted) {
    char search_pattern[MAX_PATH];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*", dir_path);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            clean_dir_contents(full_path, dry_run, bytes_freed, files_deleted);
            if (!dry_run) {
                RemoveDirectoryA(full_path); /* Succeeds if empty and unlocked */
            }
        } else {
            ULARGE_INTEGER fsize;
            fsize.LowPart  = fd.nFileSizeLow;
            fsize.HighPart = fd.nFileSizeHigh;

            if (dry_run) {
                *bytes_freed += fsize.QuadPart;
                (*files_deleted)++;
            } else {
                if (DeleteFileA(full_path)) {
                    *bytes_freed += fsize.QuadPart;
                    (*files_deleted)++;
                }
            }
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
}

void storage_clean_caches(bool dry_run, uint64_t *out_bytes_cleaned, uint32_t *out_files_cleaned) {
    *out_bytes_cleaned = 0;
    *out_files_cleaned = 0;

    char temp_dir[MAX_PATH] = {0};
    char local_appdata[MAX_PATH] = {0};
    GetTempPathA(sizeof(temp_dir), temp_dir);
    GetEnvironmentVariableA("LOCALAPPDATA", local_appdata, sizeof(local_appdata));

    size_t tlen = strlen(temp_dir);
    if (tlen > 3 && temp_dir[tlen - 1] == '\\') {
        temp_dir[tlen - 1] = '\0';
    }

    const char *mode_label = dry_run ? "Simulating Temp Cleanup (--dry-run)" : "Purging Temporary Caches";
    ui_progress_bar(1, 3, mode_label, "Scanning %TEMP%");

    if (temp_dir[0]) {
        clean_dir_contents(temp_dir, dry_run, out_bytes_cleaned, out_files_cleaned);
    }

    ui_progress_bar(2, 3, mode_label, "Scanning CrashDumps");
    if (local_appdata[0]) {
        char crash_dir[MAX_PATH];
        snprintf(crash_dir, sizeof(crash_dir), "%s\\CrashDumps", local_appdata);
        DWORD attr = GetFileAttributesA(crash_dir);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            clean_dir_contents(crash_dir, dry_run, out_bytes_cleaned, out_files_cleaned);
        }
    }

    ui_progress_bar(3, 3, mode_label, "Complete");
}
