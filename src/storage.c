#include "storage.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static int g_spinner_step = 0;

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

    const uint64_t TICKS_PER_DAY = 864000000000ULL;
    uint64_t diff = ul_now.QuadPart - ul_file.QuadPart;
    return (int)(diff / TICKS_PER_DAY);
}

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
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;

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
                char msg[160];
                snprintf(msg, sizeof(msg), "Analisando %s (%u arquivos)...", spinner_label, *acc_files);
                ui_spinner_update(g_spinner_step++, msg);
            }
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
}

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
    report->cold_files[insert_idx].deleted     = false;

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
            if (fd.cFileName[0] == '.' || _stricmp(fd.cFileName, "AppData") == 0) {
                continue;
            }
            scan_cold_files_recursive(full_path, depth + 1, max_depth, min_bytes, min_days, report, inspected_count);
        } else {
            (*inspected_count)++;
            if ((*inspected_count % 250) == 0) {
                char msg[160];
                snprintf(msg, sizeof(msg), "Procurando arquivos grandes sem uso em %s...", dir_path);
                ui_spinner_update(g_spinner_step++, msg);
            }

            ULARGE_INTEGER fsize;
            fsize.LowPart  = fd.nFileSizeLow;
            fsize.HighPart = fd.nFileSizeHigh;

            if (fsize.QuadPart >= min_bytes) {
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

static void add_cache_candidate(
    storage_report_t *report,
    const char *name,
    const char *path,
    const char *explanation,
    bool safe_to_clean
) {
    if (report->cache_count >= MAX_CACHE_TARGETS || !path || path[0] == '\0') return;

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
    strncpy(target->explanation, explanation, sizeof(target->explanation) - 1);
    target->size_bytes    = 0;
    target->file_count    = 0;
    target->exists        = true;
    target->safe_to_clean = safe_to_clean;

    char msg[128];
    snprintf(msg, sizeof(msg), "Verificando %s...", name);
    ui_spinner_update(g_spinner_step++, msg);
    scan_dir_size_recursive(path, 0, 5, &target->size_bytes, &target->file_count, name);

    report->total_cache_bytes += target->size_bytes;
    report->total_cache_files += target->file_count;
    if (safe_to_clean) {
        report->safe_cleanable_bytes += target->size_bytes;
    }
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

    size_t tlen = strlen(temp_dir);
    if (tlen > 3 && temp_dir[tlen - 1] == '\\') {
        temp_dir[tlen - 1] = '\0';
    }

    char path_buf[MAX_PATH];

    if (temp_dir[0]) {
        add_cache_candidate(report, "Temp do Usuario (%TEMP%)", temp_dir,
                            "Arquivos temporarios deixados por instaladores e programas", true);
    }

    if (windir[0]) {
        snprintf(path_buf, sizeof(path_buf), "%s\\Temp", windir);
        add_cache_candidate(report, "Temp do Sistema (Win\\Temp)", path_buf,
                            "Arquivos temporarios de servicos do Windows", true);

        snprintf(path_buf, sizeof(path_buf), "%s\\SoftwareDistribution\\Download", windir);
        add_cache_candidate(report, "Cache do Windows Update", path_buf,
                            "Pacotes antigos de atualizacao ja baixados pelo Windows", true);
    }

    if (local_appdata[0]) {
        snprintf(path_buf, sizeof(path_buf), "%s\\CrashDumps", local_appdata);
        add_cache_candidate(report, "Relatorios de Erro (CrashDumps)", path_buf,
                            "Despejos de memoria (.dmp) gravados quando um app trava", true);

        snprintf(path_buf, sizeof(path_buf), "%s\\D3DSCache", local_appdata);
        add_cache_candidate(report, "Cache de Shaders DirectX", path_buf,
                            "Shaders antigos compilados pela placa de video", true);

        snprintf(path_buf, sizeof(path_buf), "%s\\npm-cache", local_appdata);
        add_cache_candidate(report, "Cache do NPM (Node.js)", path_buf,
                            "Pacotes JavaScript guardados pelo gerenciador npm", false);

        snprintf(path_buf, sizeof(path_buf), "%s\\pip\\Cache", local_appdata);
        add_cache_candidate(report, "Cache do Pip (Python)", path_buf,
                            "Bibliotecas Python baixadas anteriormente pelo pip", false);

        snprintf(path_buf, sizeof(path_buf), "%s\\Google\\Chrome\\User Data\\Default\\Cache", local_appdata);
        add_cache_candidate(report, "Cache do Google Chrome", path_buf,
                            "Arquivos temporarios de sites visitados no navegador", false);
    }

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

    char summary_buf[160];
    char size_buf[32];
    ui_format_bytes(report->total_cache_bytes + report->total_cold_bytes, size_buf, sizeof(size_buf));
    snprintf(summary_buf, sizeof(summary_buf), "Armazenamento verificado: %s encontrados (%u arquivos de cache, %d arquivos grandes)",
             size_buf, report->total_cache_files, report->cold_file_count);
    ui_spinner_stop(summary_buf, true);
}

void storage_print_report(const storage_report_t *report) {
    if (report->total_bytes > 0) {
        double ratio = (double)report->used_bytes / (double)report->total_bytes;
        char used_s[32], total_s[32], free_s[32], bar_s[256];
        ui_format_bytes(report->used_bytes, used_s, sizeof(used_s));
        ui_format_bytes(report->total_bytes, total_s, sizeof(total_s));
        ui_format_bytes(report->free_bytes, free_s, sizeof(free_s));
        ui_bar(ratio, 20, bar_s, sizeof(bar_s));

        if (ratio >= 0.85) {
            ui_warn("Disco %s [%s] %.1f%% cheio (%s usados de %s, apenas %s livres)",
                    report->drive_root, bar_s, ratio * 100.0, used_s, total_s, free_s);
            ui_line(ANSI_DIM "Por que importa: SSDs acima de 85%% de uso perdem velocidade de escrita." ANSI_RESET);
        } else {
            ui_ok("Disco %s [%s] %.1f%% em uso (%s livres de %s)",
                  report->drive_root, bar_s, ratio * 100.0, free_s, total_s);
        }
    }

    for (int i = 0; i < report->cache_count; i++) {
        const cache_target_t *c = &report->caches[i];
        if (c->size_bytes == 0) continue;

        char sz[32];
        ui_format_bytes(c->size_bytes, sz, sizeof(sz));

        if (c->safe_to_clean && c->size_bytes > 50ULL * 1024 * 1024) {
            ui_warn("%-32s " ANSI_BOLD "%-10s" ANSI_RESET " " ANSI_DIM "(%u arquivos · %s)" ANSI_RESET,
                    c->name, sz, c->file_count, c->explanation);
        } else {
            ui_info("%-32s %-10s " ANSI_DIM "(%u arquivos · %s)" ANSI_RESET,
                    c->name, sz, c->file_count, c->explanation);
        }
    }

    if (report->cold_file_count > 0) {
        char cold_sz[32];
        ui_format_bytes(report->total_cold_bytes, cold_sz, sizeof(cold_sz));
        ui_warn("%d arquivo(s) grande(s) esquecido(s) somando " ANSI_BOLD "%s" ANSI_RESET " (sem uso ha +30 dias)",
                report->cold_file_count, cold_sz);
        for (int i = 0; i < report->cold_file_count && i < 5; i++) {
            char fsz[32];
            ui_format_bytes(report->cold_files[i].size_bytes, fsz, sizeof(fsz));
            ui_line(ANSI_YELLOW "%-10s" ANSI_RESET " " ANSI_DIM "(%d dias)" ANSI_RESET "  %s",
                    fsz, report->cold_files[i].days_unused, report->cold_files[i].path);
        }
    } else {
        ui_ok("Nenhum arquivo gigante esquecido encontrado nas pastas pessoais");
    }
}

static void preview_files_in_dir(const char *dir_path, int *printed, int max_to_print) {
    if (*printed >= max_to_print) return;

    char search_pattern[MAX_PATH];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*", dir_path);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (*printed >= max_to_print) break;
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            preview_files_in_dir(full_path, printed, max_to_print);
        } else {
            ULARGE_INTEGER fsize;
            fsize.LowPart  = fd.nFileSizeLow;
            fsize.HighPart = fd.nFileSizeHigh;
            char sz[32];
            ui_format_bytes(fsize.QuadPart, sz, sizeof(sz));

            ui_line("  " ANSI_DIM "%-10s  %s" ANSI_RESET, sz, full_path);
            (*printed)++;
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
}

void storage_print_removal_preview(const storage_report_t *report) {
    printf("\n  " ANSI_BOLD "1. Pastas de Cache Temporario" ANSI_RESET " " ANSI_DIM "(digite 'c' para limpar todas as pastas abaixo)" ANSI_RESET "\n\n");

    int shown_caches = 0;
    for (int i = 0; i < report->cache_count; i++) {
        const cache_target_t *c = &report->caches[i];
        if (!c->safe_to_clean || c->file_count == 0) continue;
        shown_caches++;

        char sz[32];
        ui_format_bytes(c->size_bytes, sz, sizeof(sz));
        ui_warn(ANSI_BOLD "%s" ANSI_RESET " — " ANSI_YELLOW "%s" ANSI_RESET " (%u arquivos)",
                c->name, sz, c->file_count);
        ui_line(ANSI_DIM "Pasta : %s" ANSI_RESET, c->path);
        ui_line(ANSI_DIM "O que : %s" ANSI_RESET, c->explanation);

        int printed = 0;
        preview_files_in_dir(c->path, &printed, 4);
        if ((int)c->file_count > printed) {
            ui_line("  " ANSI_DIM "... e mais %u arquivos nesta pasta" ANSI_RESET, c->file_count - (uint32_t)printed);
        }
        printf("\n");
    }

    if (shown_caches == 0) {
        ui_ok("As pastas de cache temporario ja estao limpas.");
    }

    printf("  " ANSI_BOLD "2. Arquivos Grandes nas Suas Pastas" ANSI_RESET " " ANSI_DIM "(digite o numero do arquivo, ex.: 1 ou 1,2 para excluir da pasta de origem)" ANSI_RESET "\n\n");

    if (report->cold_file_count == 0) {
        ui_ok("Nenhum arquivo grande sem uso recente encontrado.");
    } else {
        for (int i = 0; i < report->cold_file_count; i++) {
            const cold_file_t *cf = &report->cold_files[i];
            char sz[32];
            ui_format_bytes(cf->size_bytes, sz, sizeof(sz));
            if (cf->deleted) {
                printf("   " ANSI_DIM "%2d) [EXCLUIDO] %s" ANSI_RESET "\n", i + 1, cf->path);
            } else {
                printf("   " ANSI_BOLD "%2d)" ANSI_RESET " " ANSI_YELLOW "%-10s" ANSI_RESET
                       " " ANSI_DIM "(%3d dias sem uso)" ANSI_RESET "  %s\n",
                       i + 1, sz, cf->days_unused, cf->path);
            }
        }
    }
    printf("\n");
}

bool storage_delete_single_file(const char *path, uint64_t *out_freed_bytes, char *err_buf, size_t err_size) {
    if (out_freed_bytes) *out_freed_bytes = 0;
    if (err_buf && err_size > 0) err_buf[0] = '\0';

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) {
        if (err_buf) snprintf(err_buf, err_size, "Arquivo nao encontrado no disco");
        return false;
    }

    ULARGE_INTEGER fsize;
    fsize.LowPart  = fad.nFileSizeLow;
    fsize.HighPart = fad.nFileSizeHigh;

    /* Remove atributo somente-leitura ou oculto que impede DeleteFileA */
    if (fad.dwFileAttributes & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) {
        SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
    }

    if (!DeleteFileA(path)) {
        DWORD err = GetLastError();
        if (err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION) {
            if (err_buf) snprintf(err_buf, err_size, "Arquivo aberto/em uso por outro programa no momento");
        } else if (err == ERROR_ACCESS_DENIED) {
            if (err_buf) snprintf(err_buf, err_size, "Permissao negada pelo Windows (abra o terminal como Administrador)");
        } else {
            if (err_buf) snprintf(err_buf, err_size, "Codigo de erro do Windows: %lu", (unsigned long)err);
        }
        return false;
    }

    /* Confirma no disco que o arquivo realmente deixou de existir na pasta de origem */
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
        if (err_buf) snprintf(err_buf, err_size, "O Windows agendou a exclusao, mas o arquivo ainda possui handle aberto");
        return false;
    }

    if (out_freed_bytes) *out_freed_bytes = fsize.QuadPart;
    return true;
}

static void clean_dir_contents_recursive(
    const char *dir_path,
    bool dry_run,
    uint64_t *bytes_freed,
    uint32_t *files_deleted,
    uint32_t *files_locked
) {
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
            clean_dir_contents_recursive(full_path, dry_run, bytes_freed, files_deleted, files_locked);
            if (!dry_run) {
                SetFileAttributesA(full_path, FILE_ATTRIBUTE_NORMAL);
                RemoveDirectoryA(full_path);
            }
        } else {
            ULARGE_INTEGER fsize;
            fsize.LowPart  = fd.nFileSizeLow;
            fsize.HighPart = fd.nFileSizeHigh;

            if (dry_run) {
                *bytes_freed += fsize.QuadPart;
                (*files_deleted)++;
            } else {
                uint64_t freed = 0;
                if (storage_delete_single_file(full_path, &freed, NULL, 0)) {
                    *bytes_freed += freed;
                    (*files_deleted)++;
                } else {
                    if (files_locked) (*files_locked)++;
                }
            }
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
}

void storage_clean_single_cache(
    const cache_target_t *cache,
    bool dry_run,
    uint64_t *out_bytes_cleaned,
    uint32_t *out_files_cleaned,
    uint32_t *out_files_locked
) {
    uint64_t b_freed = 0;
    uint32_t f_del   = 0;
    uint32_t f_lock  = 0;

    clean_dir_contents_recursive(cache->path, dry_run, &b_freed, &f_del, &f_lock);

    if (out_bytes_cleaned) *out_bytes_cleaned += b_freed;
    if (out_files_cleaned) *out_files_cleaned += f_del;
    if (out_files_locked)  *out_files_locked  += f_lock;

    char sz[32];
    ui_format_bytes(b_freed, sz, sizeof(sz));

    if (dry_run) {
        ui_info("%s: %u arquivos (%s) prontos para limpar", cache->name, f_del, sz);
    } else if (f_del > 0) {
        if (f_lock > 0) {
            ui_ok("%s: %u arquivos excluidos (%s liberados) " ANSI_DIM "· %u ignorados por estarem em uso agora" ANSI_RESET,
                  cache->name, f_del, sz, f_lock);
        } else {
            ui_ok("%s: %u arquivos excluidos (%s liberados)", cache->name, f_del, sz);
        }
    } else if (f_lock > 0) {
        ui_warn("%s: %u arquivos estao abertos/em uso por processos em execucao (0 excluidos)",
                cache->name, f_lock);
    } else {
        ui_info("%s: pasta ja estava vazia", cache->name);
    }
}

void storage_clean_caches(
    const storage_report_t *report,
    bool dry_run,
    uint64_t *out_bytes_cleaned,
    uint32_t *out_files_cleaned,
    uint32_t *out_files_locked
) {
    *out_bytes_cleaned = 0;
    *out_files_cleaned = 0;
    if (out_files_locked) *out_files_locked = 0;

    for (int i = 0; i < report->cache_count; i++) {
        const cache_target_t *c = &report->caches[i];
        if (!c->safe_to_clean || !c->exists) continue;
        storage_clean_single_cache(c, dry_run, out_bytes_cleaned, out_files_cleaned, out_files_locked);
    }
}
