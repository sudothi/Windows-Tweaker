#ifndef WT_STORAGE_H
#define WT_STORAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_COLD_FILES    15
#define MAX_CACHE_TARGETS 10

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
    char           drive_root[8];
    uint64_t       total_bytes;
    uint64_t       free_bytes;
    uint64_t       used_bytes;

    cache_target_t caches[MAX_CACHE_TARGETS];
    int            cache_count;
    uint64_t       total_cache_bytes;
    uint64_t       safe_cleanable_bytes;
    uint32_t       total_cache_files;

    cold_file_t    cold_files[MAX_COLD_FILES];
    int            cold_file_count;
    uint64_t       total_cold_bytes;

    double         scan_elapsed_ms;
} storage_report_t;

/* Executa auditoria completa de disco, caches e arquivos frios */
void storage_run_audit(storage_report_t *report, uint64_t min_cold_size_mb, int min_cold_days, const char *custom_dir);

/* Imprime resumo de armazenamento no estilo Turtle */
void storage_print_report(const storage_report_t *report);

/* Mostra a lista numerada de pastas de cache, amostras reais e arquivos grandes nas pastas de origem */
void storage_print_removal_preview(const storage_report_t *report);

/* Exclui um arquivo especifico da pasta de origem e confirma no disco se ele sumiu */
bool storage_delete_single_file(const char *path, uint64_t *out_freed_bytes, char *err_buf, size_t err_size);

/* Limpa uma pasta de cache especifica removendo atributos READONLY e contabilizando arquivos em uso */
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
