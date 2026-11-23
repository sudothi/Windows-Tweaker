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
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
            continue;
        }
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            continue;
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
                ui_spinner_tick(g_spinner_step++, "Mapeando caches de armazenamento...", spinner_label);
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
                ui_spinner_tick(g_spinner_step++, "Buscando arquivos grandes sem uso recente...", dir_path);
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

    ui_spinner_tick(g_spinner_step++, "Auditando diretorio de cache...", name);
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
        add_cache_candidate(report, "User Temp (%TEMP%)", temp_dir,
                            "Arquivos temporarios de instaladores, descompactadores e sessoes antigas", true);
    }

    if (windir[0]) {
        snprintf(path_buf, sizeof(path_buf), "%s\\Temp", windir);
        add_cache_candidate(report, "System Temp (Win\\Temp)", path_buf,
                            "Logs e arquivos temporarios criados por servicos internos do Windows", true);

        snprintf(path_buf, sizeof(path_buf), "%s\\Prefetch", windir);
        add_cache_candidate(report, "Windows Prefetch Cache", path_buf,
                            "Indices de pre-carregamento de executaveis gerenciados pelo kernel", false);

        snprintf(path_buf, sizeof(path_buf), "%s\\SoftwareDistribution\\Download", windir);
        add_cache_candidate(report, "Windows Update Cache", path_buf,
                            "Instaladores de atualizacoes do Windows ja baixadas anteriormente", true);
    }

    if (local_appdata[0]) {
        snprintf(path_buf, sizeof(path_buf), "%s\\CrashDumps", local_appdata);
        add_cache_candidate(report, "Application CrashDumps", path_buf,
                            "Despejos de memoria (.dmp) gravados quando programas travam ou fecham", true);

        snprintf(path_buf, sizeof(path_buf), "%s\\D3DSCache", local_appdata);
        add_cache_candidate(report, "DirectX Shader Cache", path_buf,
                            "Shaders compilados de jogos e placas de video (recriados sob demanda)", true);

        snprintf(path_buf, sizeof(path_buf), "%s\\npm-cache", local_appdata);
        add_cache_candidate(report, "NPM Package Cache", path_buf,
                            "Cache local de bibliotecas Node.js baixadas pelo gerenciador npm", false);

        snprintf(path_buf, sizeof(path_buf), "%s\\pip\\Cache", local_appdata);
        add_cache_candidate(report, "Python Pip Cache", path_buf,
                            "Rodas (.whl) e pacotes Python armazenados em cache pelo pip", false);

        snprintf(path_buf, sizeof(path_buf), "%s\\Google\\Chrome\\User Data\\Default\\Cache", local_appdata);
        add_cache_candidate(report, "Chrome Browser Cache", path_buf,
                            "Imagens, scripts e ativos web salvos pelo navegador Google Chrome", false);
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

    char summary_buf[128];
    char size_buf[32];
    ui_format_bytes(report->total_cache_bytes + report->total_cold_bytes, size_buf, sizeof(size_buf));
    snprintf(summary_buf, sizeof(summary_buf), "Mapeados %s em %u arquivos de cache e %d arquivo(s) frio(s)",
             size_buf, report->total_cache_files, report->cold_file_count);
    ui_spinner_finish(UI_STATUS_OK, "Auditoria de Armazenamento", summary_buf, report->scan_elapsed_ms);
}

void storage_print_report(const storage_report_t *report) {
    ui_section_header("DISCO", "AUDITORIA DE ARMAZENAMENTO E CACHES", "Capacidade do SSD, lixo temporario e arquivos esquecidos");

    if (report->total_bytes > 0) {
        double ratio = (double)report->used_bytes / (double)report->total_bytes;
        char used_s[32], total_s[32], free_s[32], bar_s[256];
        ui_format_bytes(report->used_bytes, used_s, sizeof(used_s));
        ui_format_bytes(report->total_bytes, total_s, sizeof(total_s));
        ui_format_bytes(report->free_bytes, free_s, sizeof(free_s));
        ui_usage_bar(ratio, 28, bar_s, sizeof(bar_s));

        printf("  " ANSI_WHITE ANSI_BOLD "Unidade %-4s" ANSI_RESET " [%s] " ANSI_BOLD "%5.1f%%" ANSI_RESET
               "  " ANSI_GRAY "%s usados de %s (" ANSI_CYAN "%s livres" ANSI_GRAY ")" ANSI_RESET "\n",
               report->drive_root, bar_s, ratio * 100.0, used_s, total_s, free_s);

        if (ratio >= 0.85) {
            printf("               " ANSI_RED "╰─ Explicacao: SSDs com mais de 85%% de ocupacao sofrem queda de velocidade de escrita (Write Amplification)." ANSI_RESET "\n");
        } else {
            printf("               " ANSI_DIM "╰─ Explicacao: O disco possui margem livre saudavel para operacoes de TRIM e memoria virtual." ANSI_RESET "\n");
        }
        printf("\n");
    }

    printf("  " ANSI_CYAN ANSI_BOLD "%-26s %-13s %-10s %s" ANSI_RESET "\n",
           "LOCAL DE CACHE / TEMP", "TAMANHO", "ARQUIVOS", "STATUS DE LIMPEZA");
    ui_divider(84);

    for (int i = 0; i < report->cache_count; i++) {
        const cache_target_t *c = &report->caches[i];
        char sz[32];
        ui_format_bytes(c->size_bytes, sz, sizeof(sz));

        const char *size_col = (c->size_bytes > 250ULL * 1024 * 1024) ? ANSI_AMBER :
                               (c->size_bytes > 20ULL * 1024 * 1024)  ? ANSI_CYAN  : ANSI_GRAY;

        const char *tag = c->safe_to_clean
            ? (ANSI_GREEN "[LIMPEZA AUTOMATICA SEGURA]" ANSI_RESET)
            : (ANSI_DIM   "[CACHE DE FERRAMENTA/APP]" ANSI_RESET);

        printf("  " ANSI_WHITE ANSI_BOLD "%-26s" ANSI_RESET " %s%-13s" ANSI_RESET " " ANSI_GRAY "%-10u" ANSI_RESET " %s\n",
               c->name, size_col, sz, c->file_count, tag);
        printf("    " ANSI_DIM "├─ Caminho   : %s" ANSI_RESET "\n", c->path);
        printf("    " ANSI_DIM "╰─ O que e   : %s" ANSI_RESET "\n", c->explanation);
    }

    char total_cache_str[32], cleanable_str[32];
    ui_format_bytes(report->total_cache_bytes, total_cache_str, sizeof(total_cache_str));
    ui_format_bytes(report->safe_cleanable_bytes, cleanable_str, sizeof(cleanable_str));
    ui_divider(84);
    printf("  " ANSI_BOLD "Total em Caches Mapeados : " ANSI_CYAN "%-14s" ANSI_RESET
           "  |  " ANSI_GREEN ANSI_BOLD "Limpeza Imediata Segura: %s" ANSI_RESET "\n",
           total_cache_str, cleanable_str);

    printf("\n  " BG_AMBER " ARQUIVOS FRIOS " ANSI_RESET " " ANSI_AMBER ANSI_BOLD "ARQUIVOS GRANDES ESQUECIDOS (Sem uso recente nas suas pastas)" ANSI_RESET "\n");
    printf("  " ANSI_DIM "Explicacao: Arquivos pesados (>50 MB) que nao sao abertos ou modificados ha mais de 30 dias.\n"
           "  Eles nao sao apagados automaticamente para sua seguranca, mas revisa-los libera muito espaco." ANSI_RESET "\n");
    ui_divider(84);

    if (report->cold_file_count == 0) {
        printf("  " ANSI_GREEN "[OK]" ANSI_RESET " " ANSI_GRAY "Nenhum arquivo gigante esquecido foi encontrado nas pastas do usuario." ANSI_RESET "\n");
    } else {
        for (int i = 0; i < report->cold_file_count; i++) {
            char sz[32];
            ui_format_bytes(report->cold_files[i].size_bytes, sz, sizeof(sz));
            printf("  " ANSI_AMBER ANSI_BOLD "%-11s" ANSI_RESET " " ANSI_DIM "[%3d dias sem uso]" ANSI_RESET "  " ANSI_WHITE "%s" ANSI_RESET "\n",
                   sz, report->cold_files[i].days_unused, report->cold_files[i].path);
        }
    }
}

/* List sample files inside a directory so the user can see what will be deleted */
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

            printf("      " ANSI_RED "[-]" ANSI_RESET " " ANSI_GRAY "%-10s" ANSI_RESET " %s\n", sz, full_path);
            (*printed)++;
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
}

void storage_print_removal_preview(const storage_report_t *report) {
    ui_section_header("INSPECAO", "DETALHAMENTO EXATO DE ARQUIVOS PARA REMOCAO",
                      "Veja exatamente o que sera removido antes de confirmar");

    printf("  " ANSI_WHITE "1. CACHES TEMPORARIOS QUE SERAO LIMPOS AUTOMATICAMENTE:" ANSI_RESET "\n");
    printf("  " ANSI_DIM "   (Arquivos atualmente abertos/travados pelo Windows sao ignorados com seguranca)" ANSI_RESET "\n\n");

    for (int i = 0; i < report->cache_count; i++) {
        const cache_target_t *c = &report->caches[i];
        if (!c->safe_to_clean || c->file_count == 0) continue;

        char sz[32];
        ui_format_bytes(c->size_bytes, sz, sizeof(sz));

        printf("  " BG_CYAN " PASTA " ANSI_RESET " " ANSI_WHITE ANSI_BOLD "%s" ANSI_RESET
               "  (" ANSI_CYAN "%s" ANSI_RESET " em %u arquivos)\n",
               c->name, sz, c->file_count);
        printf("    " ANSI_DIM "Caminho : %s" ANSI_RESET "\n", c->path);
        printf("    " ANSI_DIM "Motivo  : %s" ANSI_RESET "\n", c->explanation);
        printf("    " ANSI_GRAY "Amostra de arquivos reais encontrados nesta pasta:" ANSI_RESET "\n");

        int printed = 0;
        preview_files_in_dir(c->path, &printed, 6);
        if ((int)c->file_count > printed) {
            printf("      " ANSI_DIM "... e mais %u arquivo(s) temporario(s) nesta pasta." ANSI_RESET "\n",
                   c->file_count - (uint32_t)printed);
        }
        printf("\n");
    }

    printf("  " ANSI_WHITE "2. ARQUIVOS GRANDES SEM USO RECENTE (REVISAO MANUAL RECOMENDADA):" ANSI_RESET "\n");
    ui_divider(84);
    if (report->cold_file_count == 0) {
        printf("    " ANSI_GRAY "Nenhum arquivo grande esquecido encontrado." ANSI_RESET "\n");
    } else {
        for (int i = 0; i < report->cold_file_count; i++) {
            char sz[32];
            ui_format_bytes(report->cold_files[i].size_bytes, sz, sizeof(sz));
            printf("    " ANSI_AMBER "[%2d]" ANSI_RESET " " ANSI_AMBER ANSI_BOLD "%-10s" ANSI_RESET
                   " (%3d dias sem abrir) -> " ANSI_WHITE "%s" ANSI_RESET "\n",
                   i + 1, sz, report->cold_files[i].days_unused, report->cold_files[i].path);
        }
    }
    ui_divider(84);
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

    const char *mode_label = dry_run ? "Simulando Limpeza (--dry-run)" : "Limpando Caches Temporarios";
    ui_progress_bar(1, 3, mode_label, "Analisando %TEMP%");

    if (temp_dir[0]) {
        clean_dir_contents(temp_dir, dry_run, out_bytes_cleaned, out_files_cleaned);
    }

    ui_progress_bar(2, 3, mode_label, "Analisando CrashDumps & D3DSCache");
    if (local_appdata[0]) {
        char crash_dir[MAX_PATH];
        snprintf(crash_dir, sizeof(crash_dir), "%s\\CrashDumps", local_appdata);
        DWORD attr = GetFileAttributesA(crash_dir);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            clean_dir_contents(crash_dir, dry_run, out_bytes_cleaned, out_files_cleaned);
        }
    }

    ui_progress_bar(3, 3, mode_label, "Concluido");
}
