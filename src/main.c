#include "ui.h"
#include "storage.h"
#include "tweaks.h"
#include "hardware.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WT_VERSION "1.1.0"

typedef struct {
    int  points_lost;
    char reason[160];
    char fix_hint[160];
} score_penalty_t;

/* Compute system health score and populate human-readable explanations for every point lost */
static int evaluate_health_score(
    const hardware_report_t *hw,
    const storage_report_t  *st,
    const tweaks_report_t   *tw,
    score_penalty_t         *penalties,
    int                     *out_penalty_count
) {
    int score = 100;
    int count = 0;

    if (!tw->is_high_performance) {
        penalties[count].points_lost = 8;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Plano de energia em '%s' (limita picos de clock da CPU)", tw->power_plan_name);
        snprintf(penalties[count].fix_hint, sizeof(penalties[count].fix_hint),
                 "Mude para 'Alto Desempenho' nas Opcoes de Energia do Windows");
        score -= 8;
        count++;
    }

    if (tw->game_dvr_enabled) {
        penalties[count].points_lost = 8;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Xbox GameDVR ativo em segundo plano (consome GPU e reduz estabilidade de FPS)");
        snprintf(penalties[count].fix_hint, sizeof(penalties[count].fix_hint),
                 "Desative usando o botao 'Aplicar Otimizacoes do Windows'");
        score -= 8;
        count++;
    }

    if (tw->menu_show_delay_ms > 100) {
        penalties[count].points_lost = 6;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Atraso visual de menus em %d ms (padrao lento do Windows Explorer)", tw->menu_show_delay_ms);
        snprintf(penalties[count].fix_hint, sizeof(penalties[count].fix_hint),
                 "Reduza para 20 ms usando o botao 'Aplicar Otimizacoes do Windows'");
        score -= 6;
        count++;
    }

    if (tw->startup_count > 6) {
        penalties[count].points_lost = 8;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "%d programas iniciando junto com o Windows (sobrecarga no boot e RAM ociosa)", tw->startup_count);
        snprintf(penalties[count].fix_hint, sizeof(penalties[count].fix_hint),
                 "Inspecione a lista no botao 'Ver Programas na Inicializacao'");
        score -= 8;
        count++;
    }

    if (!tw->ntfs_trim_enabled) {
        penalties[count].points_lost = 12;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Comando TRIM do SSD desativado (degrada velocidade de escrita e vida util)");
        snprintf(penalties[count].fix_hint, sizeof(penalties[count].fix_hint),
                 "Ative via 'fsutil behavior set DisableDeleteNotify 0'");
        score -= 12;
        count++;
    }

    if (st->total_cache_bytes > 1024ULL * 1024 * 1024) {
        char sz[32];
        ui_format_bytes(st->total_cache_bytes, sz, sizeof(sz));
        penalties[count].points_lost = 7;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "%s acumulados em pastas de cache e arquivos temporarios", sz);
        snprintf(penalties[count].fix_hint, sizeof(penalties[count].fix_hint),
                 "Use o botao 'Ver o que sera removido' e execute a limpeza segura");
        score -= 7;
        count++;
    } else if (st->total_cache_bytes > 200ULL * 1024 * 1024) {
        char sz[32];
        ui_format_bytes(st->total_cache_bytes, sz, sizeof(sz));
        penalties[count].points_lost = 4;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "%s acumulados em caches temporarios", sz);
        snprintf(penalties[count].fix_hint, sizeof(penalties[count].fix_hint),
                 "Execute a limpeza de temporarios para liberar espaco");
        score -= 4;
        count++;
    }

    if (!hw->virt_firmware_enabled) {
        penalties[count].points_lost = 8;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Virtualizacao de hardware (VT-x / AMD-V) desativada na BIOS");
        snprintf(penalties[count].fix_hint, sizeof(penalties[count].fix_hint),
                 "Ative SVM/VT-x na BIOS UEFI da placa-mae");
        score -= 8;
        count++;
    }

    if (!hw->secure_boot_enabled) {
        penalties[count].points_lost = 5;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "UEFI Secure Boot inativo na BIOS (exigido por anti-cheats modernos e seguranca de boot)");
        snprintf(penalties[count].fix_hint, sizeof(penalties[count].fix_hint),
                 "Pode ser ativado na aba Security/Boot da BIOS UEFI");
        score -= 5;
        count++;
    }

    if (hw->ram_load_pct > 85) {
        penalties[count].points_lost = 6;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Uso elevado de memoria RAM fisica (%u%% ocupada no momento)", hw->ram_load_pct);
        snprintf(penalties[count].fix_hint, sizeof(penalties[count].fix_hint),
                 "Feche processos em segundo plano ou reduza apps que iniciam no boot");
        score -= 6;
        count++;
    }

    if (score < 15) score = 15;
    if (score > 100) score = 100;
    *out_penalty_count = count;
    return score;
}

static void render_explained_health_summary(
    const hardware_report_t *hw,
    const storage_report_t  *st,
    const tweaks_report_t   *tw
) {
    score_penalty_t penalties[12];
    int penalty_count = 0;
    int score = evaluate_health_score(hw, st, tw, penalties, &penalty_count);

    const char *score_color = ANSI_GREEN;
    const char *grade_text  = "EXCELENTE (SISTEMA BEM OTIMIZADO)";
    if (score < 65) {
        score_color = ANSI_RED;
        grade_text  = "MAL OTIMIZADO (REQUER AJUSTES E LIMPEZA)";
    } else if (score < 85) {
        score_color = ANSI_AMBER;
        grade_text  = "MODERADO (PODE GANHAR MAIS DESEMPENHO)";
    }

    char total_reclaim_str[64], safe_clean_str[64];
    ui_format_bytes(st->total_cache_bytes + st->total_cold_bytes, total_reclaim_str, sizeof(total_reclaim_str));
    ui_format_bytes(st->safe_cleanable_bytes, safe_clean_str, sizeof(safe_clean_str));

    char bar[256];
    ui_usage_bar((double)score / 100.0, 26, bar, sizeof(bar));

    printf("\n");
    printf("  " ANSI_DIM "╭─ " ANSI_RESET ANSI_MAGENTA ANSI_BOLD "DIAGNOSTICO GERAL E NOTA DE OTIMIZACAO DO SISTEMA" ANSI_RESET ANSI_DIM " ────────────────────────────────╮" ANSI_RESET "\n");
    printf("  " ANSI_DIM "│" ANSI_RESET "  Nota de Saude      : %s" ANSI_BOLD "%3d / 100" ANSI_RESET "  [%s]  %s" ANSI_BOLD "%s" ANSI_RESET "\n",
           score_color, score, bar, score_color, grade_text);
    printf("  " ANSI_DIM "│" ANSI_RESET "  Espaco Mapeado     : " ANSI_CYAN ANSI_BOLD "%-12s" ANSI_RESET ANSI_GRAY "(Caches + Arquivos Frios)  |  Limpeza Imediata: " ANSI_GREEN ANSI_BOLD "%s" ANSI_RESET "\n",
           total_reclaim_str, safe_clean_str);
    printf("  " ANSI_DIM "├────────────────────────────────────────────────────────────────────────────────────┤" ANSI_RESET "\n");

    if (penalty_count == 0) {
        printf("  " ANSI_DIM "│" ANSI_RESET "  " ANSI_GREEN "[OK] Parabens! Nenhum ponto foi descontado. Seu PC esta na configuracao maxima." ANSI_RESET "\n");
    } else {
        printf("  " ANSI_DIM "│" ANSI_RESET "  " ANSI_AMBER ANSI_BOLD "POR QUE SUA NOTA FOI %d/100? (MOTIVOS EXATOS DOS PONTOS DESCONTADOS):" ANSI_RESET "\n", score);
        for (int i = 0; i < penalty_count; i++) {
            printf("  " ANSI_DIM "│" ANSI_RESET "   " ANSI_RED ANSI_BOLD "[-%2d pts]" ANSI_RESET " " ANSI_WHITE "%s" ANSI_RESET "\n",
                   penalties[i].points_lost, penalties[i].reason);
            printf("  " ANSI_DIM "│" ANSI_RESET "             " ANSI_CYAN "╰─ Como resolver: %s" ANSI_RESET "\n",
                   penalties[i].fix_hint);
        }
    }
    printf("  " ANSI_DIM "╰────────────────────────────────────────────────────────────────────────────────────╯" ANSI_RESET "\n");
}

static void run_interactive_cleanup_flow(storage_report_t *st, bool already_scanned) {
    if (!already_scanned) {
        printf("\n");
        storage_run_audit(st, 50, 30, NULL);
    }

    storage_print_removal_preview(st);

    const char *sub_items[] = {
        "Executar Limpeza Segura Agora (Apagar arquivos temporarios listados acima)",
        "Rodar Apenas Simulacao (--dry-run, sem apagar nenhum arquivo)",
        "Voltar ao Menu Principal"
    };
    const char *sub_descs[] = {
        "Remove arquivos temporarios desbloqueados em %TEMP%, Win\\Temp e CrashDumps",
        "Calcula exatamente quantos bytes e arquivos serao liberados sem tocar no disco",
        "Retorna para a tela inicial sem fazer alteracoes"
    };

    int choice = ui_menu_select("O QUE VOCE DESEJA FAZER COM OS ARQUIVOS ACIMA?", sub_items, sub_descs, 3);
    if (choice == 0) {
        if (ui_confirm_prompt("Confirma a remocao dos arquivos temporarios de cache listados?")) {
            uint64_t freed = 0;
            uint32_t files = 0;
            printf("\n");
            storage_clean_caches(false, &freed, &files);
            char sz[32];
            ui_format_bytes(freed, sz, sizeof(sz));
            char msg[128];
            snprintf(msg, sizeof(msg), "Liberados %s em %u arquivos temporarios removidos!", sz, files);
            printf("\n");
            ui_print_status(UI_STATUS_OK, "Limpeza Concluida", msg);
        }
    } else if (choice == 1) {
        uint64_t freed = 0;
        uint32_t files = 0;
        printf("\n");
        storage_clean_caches(true, &freed, &files);
        char sz[32];
        ui_format_bytes(freed, sz, sizeof(sz));
        char msg[128];
        snprintf(msg, sizeof(msg), "A limpeza liberara %s em %u arquivos desbloqueados", sz, files);
        printf("\n");
        ui_print_status(UI_STATUS_INFO, "Simulacao Concluida", msg);
    }
}

static void run_interactive_optimize_flow(void) {
    /* First show exact preview of what each tweak changes */
    tweaks_apply_safe_optimizations(true);

    const char *opt_items[] = {
        "Aplicar Estas Otimizacoes no Registro Agora (--apply)",
        "Voltar ao Menu Principal sem Alterar o Registro"
    };
    const char *opt_descs[] = {
        "Grava MenuShowDelay=20ms e desativa Xbox GameDVR em HKEY_CURRENT_USER",
        "Mantem as configuracoes atuais do Windows intactas"
    };

    int choice = ui_menu_select("DESEJA APLICAR ESTAS OTIMIZACOES NO SEU WINDOWS?", opt_items, opt_descs, 2);
    if (choice == 0) {
        tweaks_apply_safe_optimizations(false);
        printf("\n");
        ui_print_status(UI_STATUS_OK, "Otimizacoes Aplicadas", "Chaves gravadas com sucesso no Registro do Usuario!");
    }
}

static void run_interactive_main_hub(void) {
    while (1) {
        ui_clear_screen();
        ui_print_banner(WT_VERSION);

        const char *main_items[] = {
            "Diagnostico Completo do Sistema (Score Explicado + Tudo)",
            "Inspecionar Arquivos para Remover (Ver Lista Exata e Limpar)",
            "Inspecionar e Aplicar Otimizacoes do Windows (Registro e Jogos)",
            "Gerenciador de Inicializacao (Programas que Pesam no Boot)",
            "Telemetria de Hardware e BIOS (CPU, RAM, Virtualizacao e HAGS)",
            "Sair do Windows Tweaker"
        };
        const char *main_descs[] = {
            "Analisa Hardware, BIOS, Disco e Registro explicando o motivo de cada ponto perdido",
            "Mostra exatamente quais pastas, amostras de arquivos e tamanhos podem ser removidos",
            "Explica o que cada chave do Registro faz (MenuShowDelay, GameDVR, TRIM) e permite aplicar",
            "Lista todos os programas em HKCU\\Run e HKLM\\Run que atrasam a inicializacao do PC",
            "Le registradores __cpuid do processador, carga de RAM, Secure Boot e virtualizacao na BIOS",
            "Encerra o aplicativo e restaura o terminal"
        };

        int sel = ui_menu_select("MENU PRINCIPAL - SELECIONE UMA OPERACAO", main_items, main_descs, 6);

        if (sel == 5) {
            printf("\n  " ANSI_CYAN ">> Encerrando Windows Tweaker. Ate logo!" ANSI_RESET "\n\n");
            break;
        }

        if (sel == 0) {
            ui_clear_screen();
            ui_print_banner(WT_VERSION);
            printf("\n");

            hardware_report_t hw;
            storage_report_t  st;
            tweaks_report_t   tw;

            hardware_run_audit(&hw);
            storage_run_audit(&st, 50, 30, NULL);
            tweaks_run_audit(&tw);

            hardware_print_report(&hw);
            storage_print_report(&st);
            tweaks_print_report(&tw);
            render_explained_health_summary(&hw, &st, &tw);

            /* Post-scan interactive action buttons */
            const char *post_items[] = {
                "BOTAO: Ver Exatamente Quais Arquivos Serao Removidos (E Limpar Cache)",
                "BOTAO: Ver e Aplicar Otimizacoes de Registro (Corrigir Alertas do Windows)",
                "BOTAO: Ver Lista Completa dos Programas que Iniciam no Boot",
                "Voltar ao Menu Principal"
            };
            const char *post_descs[] = {
                "Abre o inspetor detalhado mostrando as pastas, amostras de arquivos e permite limpar",
                "Mostra as chaves exatas para resolver o atraso de 400ms e desligar o Xbox GameDVR",
                "Exibe a tabela completa dos programas encontrados nas chaves Run do Registro",
                "Retorna para o menu inicial do Windows Tweaker"
            };

            int post_choice = ui_menu_select("ACOES RECOMENDADAS PARA MELHORAR SUA NOTA", post_items, post_descs, 4);
            if (post_choice == 0) {
                run_interactive_cleanup_flow(&st, true);
                ui_wait_for_key(NULL);
            } else if (post_choice == 1) {
                run_interactive_optimize_flow();
                ui_wait_for_key(NULL);
            } else if (post_choice == 2) {
                tweaks_print_startup(&tw);
                ui_wait_for_key(NULL);
            }
        } else if (sel == 1) {
            ui_clear_screen();
            ui_print_banner(WT_VERSION);
            storage_report_t st;
            run_interactive_cleanup_flow(&st, false);
            ui_wait_for_key(NULL);
        } else if (sel == 2) {
            ui_clear_screen();
            ui_print_banner(WT_VERSION);
            run_interactive_optimize_flow();
            ui_wait_for_key(NULL);
        } else if (sel == 3) {
            ui_clear_screen();
            ui_print_banner(WT_VERSION);
            printf("\n");
            tweaks_report_t tw;
            tweaks_run_audit(&tw);
            tweaks_print_startup(&tw);
            ui_wait_for_key(NULL);
        } else if (sel == 4) {
            ui_clear_screen();
            ui_print_banner(WT_VERSION);
            printf("\n");
            hardware_report_t hw;
            hardware_run_audit(&hw);
            hardware_print_report(&hw);
            ui_wait_for_key(NULL);
        }
    }
}

static void print_help(void) {
    ui_print_banner(WT_VERSION);
    ui_section_header("AJUDA", "REFERENCIA DE COMANDOS DA CLI", "Uso: windows-tweaker [comando] [opcoes]");

    printf("  " ANSI_CYAN ANSI_BOLD "MODO INTERATIVO (PADRAO):" ANSI_RESET "\n");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "windows-tweaker", "Abre a Main Interface interativa navegavel pelas setas do teclado");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "menu",            "Abre a Main Interface interativa no terminal");

    printf("\n  " ANSI_CYAN ANSI_BOLD "COMANDOS DIRETOS (PARA SCRIPTS / AUTOMACAO):" ANSI_RESET "\n");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "scan",            "Executa diagnostico completo com explicacao detalhada do Score");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "inspect",         "Mostra a lista exata de pastas e amostras de arquivos para remover");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "storage",         "Auditoria profunda de disco, caches e arquivos frios");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "hardware",        "Telemetria de CPU (__cpuid), RAM, virtualizacao na BIOS e HAGS");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "startup",         "Lista todos os programas que iniciam junto com o Windows");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "clean",           "Limpa caches temporarios (--dry-run por padrao, ou --apply)");
    printf("    " ANSI_WHITE ANSI_BOLD "%-22s" ANSI_RESET " %s\n", "optimize",        "Inspeciona ou aplica ajustes no Registro (--dry-run ou --apply)");

    printf("\n  " ANSI_CYAN ANSI_BOLD "OPCOES E FLAGS:" ANSI_RESET "\n");
    printf("    " ANSI_AMBER "%-22s" ANSI_RESET " %s\n", "--dry-run",       "Modo simulacao: mostra o que faria sem alterar nada (padrao)");
    printf("    " ANSI_GREEN "%-22s" ANSI_RESET " %s\n", "--apply",         "Executa a limpeza ou aplica as otimizacoes de verdade");
    printf("    " ANSI_CYAN  "%-22s" ANSI_RESET " %s\n", "--min-mb <MB>",   "Tamanho minimo em MB para detectar arquivos frios (padrao: 50)");
    printf("    " ANSI_CYAN  "%-22s" ANSI_RESET " %s\n", "--days <DIAS>",   "Dias sem uso para detectar arquivos frios (padrao: 30)");
    printf("    " ANSI_GRAY  "%-22s" ANSI_RESET " %s\n", "--help, -h",      "Exibe esta tela de ajuda");
    printf("\n");
}

int main(int argc, char *argv[]) {
    ui_init();

    /* Launch Interactive Main Interface when run with no arguments or 'menu' */
    if (argc < 2 || strcmp(argv[1], "menu") == 0 || strcmp(argv[1], "ui") == 0) {
        run_interactive_main_hub();
        ui_cleanup();
        return 0;
    }

    const char *cmd = argv[1];
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
        render_explained_health_summary(&hw, &st, &tw);
        printf("\n");
    } else if (strcmp(cmd, "inspect") == 0) {
        printf("\n");
        storage_report_t st;
        storage_run_audit(&st, min_cold_mb, min_cold_days, custom_dir);
        storage_print_removal_preview(&st);
        printf("\n");
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
        ui_section_header("LIMPEZA", dry_run ? "SIMULACAO DE LIMPEZA DE CACHE (--dry-run)" : "EXECUTANDO LIMPEZA DE CACHES TEMPORARIOS",
                          dry_run ? "Use --apply para remover de verdade os arquivos temporarios" : "Removendo arquivos temporarios desbloqueados");

        uint64_t bytes_cleaned = 0;
        uint32_t files_cleaned = 0;
        storage_clean_caches(dry_run, &bytes_cleaned, &files_cleaned);

        char sz[32];
        ui_format_bytes(bytes_cleaned, sz, sizeof(sz));
        char msg[128];
        snprintf(msg, sizeof(msg), "%s %s em %u arquivos temporarios",
                 dry_run ? "Podem ser liberados" : "Foram liberados", sz, files_cleaned);
        printf("\n");
        ui_print_status(UI_STATUS_OK, dry_run ? "Simulacao Concluida" : "Limpeza Concluida", msg);
        printf("\n");
    } else if (strcmp(cmd, "optimize") == 0) {
        bool dry_run = !apply_mode;
        int count = tweaks_apply_safe_optimizations(dry_run);
        printf("\n  " ANSI_GREEN "[OK]" ANSI_RESET " %d regra(s) de otimizacao %s.\n\n",
               count, dry_run ? "inspecionadas em modo simulacao (--dry-run)" : "aplicadas no Registro (HKCU)");
    } else {
        printf("\n");
        ui_print_status(UI_STATUS_CRIT, "Comando Desconhecido", cmd);
        printf("  " ANSI_GRAY "Execute " ANSI_WHITE "windows-tweaker" ANSI_GRAY " (sem argumentos) para abrir a interface interativa ou " ANSI_WHITE "windows-tweaker --help" ANSI_RESET "\n\n");
        ui_cleanup();
        return 1;
    }

    ui_cleanup();
    return 0;
}
