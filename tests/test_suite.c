#include "ui.h"
#include "storage.h"
#include "tweaks.h"
#include "hardware.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define ASSERT_TRUE(cond, msg) do { \
    g_tests_run++; \
    if (!(cond)) { \
        printf("  [FALHA] %s (Linha %d)\n", msg, __LINE__); \
        return 1; \
    } else { \
        g_tests_passed++; \
        printf("  " ANSI_GREEN "[PASS]" ANSI_RESET " %s\n", msg); \
    } \
} while (0)

static int test_byte_formatter(void) {
    char buf[64];
    ui_format_bytes(512, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "512 B") == 0, "Format exact bytes (512 B)");

    ui_format_bytes(2048, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "2.0 KB") == 0, "Format kilobytes (2.0 KB)");

    ui_format_bytes(10ULL * 1024ULL * 1024ULL, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "10.0 MB") == 0, "Format megabytes (10.0 MB)");

    ui_format_bytes(2ULL * 1024ULL * 1024ULL * 1024ULL, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "2.00 GB") == 0, "Format gigabytes (2.00 GB)");
    return 0;
}

static int test_hardware_probe(void) {
    hardware_report_t hw;
    hardware_run_audit(&hw);

    ASSERT_TRUE(hw.logical_cores > 0, "Detected positive logical CPU core count");
    ASSERT_TRUE(strlen(hw.cpu_brand) > 0, "Populated CPU brand string via __cpuid");
    ASSERT_TRUE(hw.ram_total_bytes > 1024ULL * 1024ULL * 128ULL, "Detected physical RAM > 128 MB");
    ASSERT_TRUE(hw.ram_load_pct <= 100, "RAM load percentage within [0, 100]");
    return 0;
}

static int test_tweaks_audit(void) {
    tweaks_report_t tw;
    tweaks_run_audit(&tw);

    ASSERT_TRUE(strlen(tw.power_plan_name) > 0, "Resolved active Windows power scheme name");
    ASSERT_TRUE(tw.startup_count >= 0 && tw.startup_count <= MAX_STARTUP_ENTRIES,
                "Startup registry enumeration within bounds");
    return 0;
}

int main(void) {
    ui_init();
    ui_print_banner("TEST-SUITE");
    ui_section_header("TESTES", "SUITE AUTOMATIZADA DE TESTES UNITARIOS EM C11",
                      "Validando formatadores, registradores __cpuid e auditoria Win32");

    int rc = 0;
    rc |= test_byte_formatter();
    rc |= test_hardware_probe();
    rc |= test_tweaks_audit();

    ui_divider(84);
    if (rc == 0) {
        ui_print_status(UI_STATUS_OK, "Todos os Testes Aprovados", "Zero erros detectados");
    } else {
        ui_print_status(UI_STATUS_CRIT, "Falha nos Testes Unitarios", "Verifique os logs acima");
    }
    printf("  " ANSI_DIM "Executadas %d/%d assercoes com sucesso." ANSI_RESET "\n\n", g_tests_passed, g_tests_run);

    ui_cleanup();
    return rc;
}
