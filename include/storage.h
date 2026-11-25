#ifndef WT_STORAGE_H
#define WT_STORAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_COLD_FILES    15
#define MAX_CACHE_TARGETS 12
#define MAX_DEV_ARTIFACT_DIRS 10

typedef struct {
    char     name[64];
    char     path[260];
    char     explanation[128];
    uint64_t size_bytes;
    uint32_t file_count;
    bool     exists;
    bool     safe_to_clean;
} cache_target_t;

typedef struct {
    char     path[260];
    uint64_t size_bytes;
    int      days_unused;
    bool     deleted;
} cold_file_t;

typedef struct {
    char     kind[24];      /* "node_modules", ".venv", "target" */
    char     path[260];
    uint64_t size_bytes;
    uint32_t file_count;
    int      days_unused;
    bool     deleted;
} dev_artifact_t;

typedef struct {
    char           drive_root[8];
    uint64_t       total_bytes;
    uint64_t       free_bytes;
    uint64_t       used_bytes;

    /* Arquivos de sistema ocultos na raiz do SSD */
    uint64_t       hiberfil_bytes;
    uint64_t       pagefile_bytes;

    cache_target_t caches[MAX_CACHE_TARGETS];
    int            cache_count;
    uint64_t       total_cache_bytes;
    uint64_t       safe_cleanable_bytes;
    uint32_t       total_cache_files;

    cold_file_t    cold_files[MAX_COLD_FILES];
    int            cold_file_count;
    uint64_t       total_cold_bytes;

    dev_artifact_t dev_dirs[MAX_DEV_ARTIFACT_DIRS];
    int            dev_dir_count;
    uint64_t       total_dev_bytes;

    double         scan_elapsed_ms;
} storage_report_t;

/* Executa auditoria completa de disco, hiberfil.sys, caches, pastas node_modules e arquivos frios */
void storage_run_audit(storage_report_t *report, uint64_t min_cold_size_mb, int min_cold_days, const char *custom_dir);

/* Imprime resumo de armazenamento no Diagnostico Completo */
void storage_print_report(const storage_report_t *report);

/* Mostra a lista de pastas de cache, pastas node_modules/.venv e arquivos grandes */
void storage_print_removal_preview(const storage_report_t *report);

/* Exclui um arquivo especifico da pasta de origem e confirma no disco se ele sumiu */
bool storage_delete_single_file(const char *path, uint64_t *out_freed_bytes, char *err_buf, size_t err_size);

/* Exclui uma pasta inteira (ex: node_modules antiga) recursivamente */
bool storage_delete_directory_tree(const char *dir_path, uint64_t *out_freed_bytes, uint32_t *out_files_deleted);

/* Limpa uma pasta de cache especifica */
void storage_clean_single_cache(
    const cache_target_t *cache,
    bool dry_run,
    uint64_t *out_bytes_cleaned,
    uint32_t *out_files_cleaned,
    uint32_t *out_files_locked
);

/* Limpa todas as pastas de cache marcadas como seguras */
void storage_clean_caches(
    const storage_report_t *report,
    bool dry_run,
    uint64_t *out_bytes_cleaned,
    uint32_t *out_files_cleaned,
    uint32_t *out_files_locked
);

#endif /* WT_STORAGE_H */
