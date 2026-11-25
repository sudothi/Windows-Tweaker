#include "ui.h"
#include "storage.h"
#include "tweaks.h"
#include "hardware.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define ASSERT_TRUE(cond, msg) do { \
    g_tests_run++; \
    if (!(cond)) { \
        ui_fail("%s (linha %d)", msg, __LINE__); \
        return 1; \
    } else { \
        g_tests_passed++; \
        ui_ok("%s", msg); \
    } \
} while (0)

static int test_byte_formatter(void) {
    char buf[64];
    ui_format_bytes(512, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "512 B") == 0, "Formatacao exata em bytes (512 B)");

    ui_format_bytes(2048, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "2.0 KB") == 0, "Formatacao em kilobytes (2.0 KB)");

    ui_format_bytes(10ULL * 1024ULL * 1024ULL, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "10.0 MB") == 0, "Formatacao em megabytes (10.0 MB)");

    ui_format_bytes(2ULL * 1024ULL * 1024ULL * 1024ULL, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "2.00 GB") == 0, "Formatacao em gigabytes (2.00 GB)");
    return 0;
}

static int test_verified_file_deletion(void) {
    char temp_dir[MAX_PATH];
    GetTempPathA(sizeof(temp_dir), temp_dir);

    char test_file[MAX_PATH];
    snprintf(test_file, sizeof(test_file), "%swt_delete_test_file.tmp", temp_dir);

    FILE *fp = fopen(test_file, "wb");
    ASSERT_TRUE(fp != NULL, "Criacao de arquivo temporario de teste no disco");
    const char *payload = "windows-tweaker verified deletion test payload";
    fwrite(payload, 1, strlen(payload), fp);
    fclose(fp);

    SetFileAttributesA(test_file, FILE_ATTRIBUTE_READONLY);

    uint64_t freed = 0;
    char err[128] = {0};
    bool ok = storage_delete_single_file(test_file, &freed, err, sizeof(err));
    ASSERT_TRUE(ok && freed == strlen(payload), "Exclusao real de arquivo com atributo READONLY");
    ASSERT_TRUE(GetFileAttributesA(test_file) == INVALID_FILE_ATTRIBUTES,
                "Confirmacao no disco: arquivo nao existe mais na pasta de origem");
    return 0;
}

static int test_hardware_probe(void) {
    hardware_report_t hw;
    hardware_run_audit(&hw);

    ASSERT_TRUE(hw.logical_cores > 0, "Contagem de threads logicas da CPU > 0");
    ASSERT_TRUE(strlen(hw.cpu_brand) > 0, "Leitura do nome do processador via __cpuid");
    ASSERT_TRUE(hw.ram_total_bytes > 1024ULL * 1024ULL * 128ULL, "Deteccao de memoria RAM fisica > 128 MB");
    return 0;
}

static int test_tweaks_audit(void) {
    tweaks_report_t tw;
    tweaks_run_audit(&tw);

    ASSERT_TRUE(tw.rule_count == MAX_TWEAK_RULES, "Catalogo completo de 12 regras de otimizacao carregado");
    ASSERT_TRUE(tw.optimized_tweaks_count + tw.pending_tweaks_count == MAX_TWEAK_RULES,
                "Soma exata de regras otimizadas + pendentes == 12");
    ASSERT_TRUE(tw.startup_enabled_count + tw.startup_disabled_count == tw.startup_count,
                "Sincronizacao StartupApproved\\Run (Habilitados + Desabilitados == Total)");

    /* Testa a aplicacao real de todas as 12 regras e verifica se todas as 12 ficam JA OTIMIZADO */
    for (int i = 0; i < MAX_TWEAK_RULES; i++) {
        bool ok = tweaks_apply_single_rule((tweak_id_t)i);
        char msg[128];
        snprintf(msg, sizeof(msg), "Execucao da Regra #%d (%s)", i + 1, tw.rules[i].title);
        ASSERT_TRUE(ok, msg);
    }

    ASSERT_TRUE(tweaks_flush_dns_cache(), "Execucao de limpeza de cache DNS (ipconfig /flushdns)");

    tweaks_report_t tw_after;
    tweaks_run_audit(&tw_after);
    ASSERT_TRUE(tw_after.optimized_tweaks_count == MAX_TWEAK_RULES && tw_after.pending_tweaks_count == 0,
                "Confirmacao no Registro: 12/12 regras detectadas como JA OTIMIZADO (0 pendentes)");

    return 0;
}

int main(void) {
    ui_init();
    ui_banner();

    ui_step(1, 4, "Formatadores de Bytes");
    int rc = 0;
    rc |= test_byte_formatter();

    ui_step(2, 4, "Exclusao Real de Arquivos no Disco");
    rc |= test_verified_file_deletion();

    ui_step(3, 4, "Telemetria de Hardware (__cpuid)");
    rc |= test_hardware_probe();

    ui_step(4, 4, "Auditoria das 12 Regras do Windows & StartupApproved\\Run");
    rc |= test_tweaks_audit();

    char summary[64];
    snprintf(summary, sizeof(summary), "%d/%d testes passaram com sucesso", g_tests_passed, g_tests_run);
    const char *lines[] = { summary };
    ui_box(lines, 1, rc == 0 ? "Testes Aprovados" : "Falha nos Testes");

    ui_cleanup();
    return rc;
}
