#include "ui.h"
#include "storage.h"
#include "tweaks.h"
#include "hardware.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define WT_VERSION "1.3.0"

typedef struct {
    int  points_lost;
    char reason[160];
} score_penalty_t;

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
                 "Plano de energia em '%s' (reduz a velocidade do processador)", tw->power_plan_name);
        score -= 8;
        count++;
    }

    if (tw->game_dvr_enabled) {
        penalties[count].points_lost = 8;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Xbox GameDVR ativo em segundo plano (gasta placa de video e reduz FPS)");
        score -= 8;
        count++;
    }

    if (tw->menu_show_delay_ms > 100) {
        penalties[count].points_lost = 6;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Velocidade dos menus em %d ms (atraso padrao lento do Windows)", tw->menu_show_delay_ms);
        score -= 6;
        count++;
    }

    if (tw->startup_count > 6) {
        penalties[count].points_lost = 8;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "%d programas abrindo sozinhos ao ligar o PC (gasta RAM e atrasa o boot)", tw->startup_count);
        score -= 8;
        count++;
    }

    if (!tw->ntfs_trim_enabled) {
        penalties[count].points_lost = 12;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Comando TRIM do SSD desativado (deixa o SSD lento com o tempo)");
        score -= 12;
        count++;
    }

    if (st->total_cache_bytes > 1024ULL * 1024 * 1024) {
        char sz[32];
        ui_format_bytes(st->total_cache_bytes, sz, sizeof(sz));
        penalties[count].points_lost = 7;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "%s acumulados em lixo e pastas de cache temporario", sz);
        score -= 7;
        count++;
    } else if (st->total_cache_bytes > 200ULL * 1024 * 1024) {
        char sz[32];
        ui_format_bytes(st->total_cache_bytes, sz, sizeof(sz));
        penalties[count].points_lost = 4;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "%s acumulados em pastas de cache temporario", sz);
        score -= 4;
        count++;
    }

    if (!hw->virt_firmware_enabled) {
        penalties[count].points_lost = 8;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Virtualizacao (VT-x / AMD-V) desativada na BIOS");
        score -= 8;
        count++;
    }

    if (!hw->secure_boot_enabled) {
        penalties[count].points_lost = 5;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "UEFI Secure Boot desativado na BIOS");
        score -= 5;
        count++;
    }

    if (hw->ram_load_pct > 85) {
        penalties[count].points_lost = 6;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Memoria RAM muito cheia (%u%% em uso agora)", hw->ram_load_pct);
        score -= 6;
        count++;
    }

    if (score < 15) score = 15;
    if (score > 100) score = 100;
    *out_penalty_count = count;
    return score;
}

static void render_summary_box(
    const hardware_report_t *hw,
    const storage_report_t  *st,
    const tweaks_report_t   *tw
) {
    score_penalty_t penalties[12];
    int penalty_count = 0;
    int score = evaluate_health_score(hw, st, tw, penalties, &penalty_count);

    const char *score_col = (score >= 85) ? ANSI_GREEN :
                            (score >= 65) ? ANSI_YELLOW : ANSI_RED;
    const char *status_label = (score >= 85) ? "Bem otimizado" :
                               (score >= 65) ? "Pode melhorar" : "Precisa de limpeza e ajustes";

    char cache_str[32], cold_str[32];
    ui_format_bytes(st->safe_cleanable_bytes, cache_str, sizeof(cache_str));
    ui_format_bytes(st->total_cold_bytes, cold_str, sizeof(cold_str));

    char line_storage_buf[24][256];
    const char *box_lines[24];
    int n = 0;

    snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
             "Nota do seu PC           %s" ANSI_BOLD "%d / 100" ANSI_RESET "  " ANSI_DIM "(%s)" ANSI_RESET,
             score_col, score, status_label);
    box_lines[n] = line_storage_buf[n];
    n++;

    snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
             "Lixo temporario (Cache)  " ANSI_BOLD "%s" ANSI_RESET " prontos para limpar agora",
             cache_str);
    box_lines[n] = line_storage_buf[n];
    n++;

    snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
             "Arquivos grandes (+50MB) " ANSI_BOLD "%s" ANSI_RESET " sem uso ha mais de 30 dias (%d arquivos)",
             cold_str, st->cold_file_count);
    box_lines[n] = line_storage_buf[n];
    n++;

    if (penalty_count > 0) {
        snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]), "%s", "");
        box_lines[n] = line_storage_buf[n];
        n++;

        snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
                 ANSI_BOLD "Por que sua nota foi %d/100 (motivo de cada ponto perdido):" ANSI_RESET, score);
        box_lines[n] = line_storage_buf[n];
        n++;

        for (int i = 0; i < penalty_count && n < 23; i++) {
            snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
                     "  " ANSI_RED "-%2d pts" ANSI_RESET "  %s",
                     penalties[i].points_lost, penalties[i].reason);
            box_lines[n] = line_storage_buf[n];
            n++;
        }
    }

    ui_box(box_lines, n, "Resumo do Diagnostico");
}

/* Tela de Limpeza e Exclusao controlada 100% por Setas (CIMA/BAIXO) e ENTER */
static void interactive_file_removal_screen(storage_report_t *st, bool already_scanned) {
    if (!already_scanned) {
        ui_clear();
        ui_banner();
        ui_step(1, 1, "Procurando lixo temporario e arquivos grandes esquecidos no disco");
        storage_run_audit(st, 50, 30, NULL);
    }

    for (;;) {
        ui_clear();
        ui_banner();
        storage_print_removal_preview(st);

        /* Monta a lista de opcoes navegavel por setas:
         * [0]   -> Limpar todas as pastas de Cache Temporario
         * [1..N]-> Excluir cada arquivo grande individualmente
         * [fim] -> Voltar ao Menu Principal
         */
        char labels_buf[MAX_COLD_FILES + 3][220];
        char descs_buf[MAX_COLD_FILES + 3][256];
        const char *labels[MAX_COLD_FILES + 3];
        const char *descs[MAX_COLD_FILES + 3];
        int total_opts = 0;

        char cache_sz[32];
        ui_format_bytes(st->safe_cleanable_bytes, cache_sz, sizeof(cache_sz));

        snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                 "[LIMPAR CACHES] Apagar todo o lixo temporario das pastas acima (%s)", cache_sz);
        snprintf(descs_buf[total_opts], sizeof(descs_buf[total_opts]),
                 "Apaga arquivos temporarios em %%TEMP%%, Win\\Temp, CrashDumps e DirectX (nao apaga documentos pessoais).");
        labels[total_opts] = labels_buf[total_opts];
        descs[total_opts]  = descs_buf[total_opts];
        total_opts++;

        for (int i = 0; i < st->cold_file_count; i++) {
            const cold_file_t *cf = &st->cold_files[i];
            char fsz[32];
            ui_format_bytes(cf->size_bytes, fsz, sizeof(fsz));

            if (cf->deleted) {
                snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                         "[JA EXCLUIDO]   %s (%s)", cf->path, fsz);
                snprintf(descs_buf[total_opts], sizeof(descs_buf[total_opts]),
                         "Este arquivo ja foi excluido permanentemente da pasta de origem.");
            } else {
                snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                         "[EXCLUIR ARQUIVO] %-10s (%3d dias sem abrir) -> %s",
                         fsz, cf->days_unused, cf->path);
                snprintf(descs_buf[total_opts], sizeof(descs_buf[total_opts]),
                         "Exclui permanentemente este arquivo de %s direto da pasta %s.", fsz, cf->path);
            }
            labels[total_opts] = labels_buf[total_opts];
            descs[total_opts]  = descs_buf[total_opts];
            total_opts++;
        }

        snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                 "Voltar ao Menu Principal");
        snprintf(descs_buf[total_opts], sizeof(descs_buf[total_opts]),
                 "Sai da tela de limpeza e volta para a tela inicial do Windows Tweaker.");
        labels[total_opts] = labels_buf[total_opts];
        descs[total_opts]  = descs_buf[total_opts];
        int back_idx = total_opts;
        total_opts++;

        int sel = ui_select("ESCOLHA COM AS SETAS O QUE VOCE QUER APAGAR", labels, descs, total_opts);

        if (sel == back_idx) {
            break;
        }

        if (sel == 0) {
            char detail[200];
            snprintf(detail, sizeof(detail),
                     "Vai apagar os arquivos temporarios desbloqueados de %%TEMP%%, Win\\Temp, CrashDumps e DirectX (%s).",
                     cache_sz);
            if (ui_confirm_arrows("Tem certeza que deseja limpar todas as pastas de cache temporario?", detail)) {
                printf("\n");
                uint64_t freed = 0;
                uint32_t files = 0, locked = 0;
                storage_clean_caches(st, false, &freed, &files, &locked);

                char freed_s[32];
                ui_format_bytes(freed, freed_s, sizeof(freed_s));
                printf("\n");
                ui_ok(ANSI_BOLD "Limpeza concluida:" ANSI_RESET " %u arquivos removidos (%s liberados no disco)", files, freed_s);
                if (locked > 0) {
                    ui_info("%u arquivos temporarios foram ignorados porque estao abertos/em uso pelo Windows agora.", locked);
                }
                ui_pause_enter("Continuar");
                storage_run_audit(st, 50, 30, NULL);
            }
        } else {
            int file_idx = sel - 1;
            if (file_idx >= 0 && file_idx < st->cold_file_count) {
                cold_file_t *cf = &st->cold_files[file_idx];
                if (cf->deleted) {
                    ui_info("Este arquivo ja foi excluido do seu disco.");
                    ui_pause_enter("Continuar");
                    continue;
                }

                char fsz[32];
                ui_format_bytes(cf->size_bytes, fsz, sizeof(fsz));
                char q[320];
                char detail[360];
                snprintf(q, sizeof(q), "Excluir permanentemente o arquivo '%s'?", cf->path);
                snprintf(detail, sizeof(detail),
                         "O arquivo sera apagado definitivamente da pasta de origem e voce vai liberar %s no disco.", fsz);

                if (ui_confirm_arrows(q, detail)) {
                    uint64_t freed_b = 0;
                    char err[128];
                    printf("\n");
                    if (storage_delete_single_file(cf->path, &freed_b, err, sizeof(err))) {
                        ui_ok("Arquivo excluido com sucesso da pasta de origem: " ANSI_BOLD "%s" ANSI_RESET " (%s liberados)",
                              cf->path, fsz);
                        cf->deleted = true;
                        if (st->total_cold_bytes >= freed_b) {
                            st->total_cold_bytes -= freed_b;
                        }
                    } else {
                        ui_fail("Nao foi possivel excluir '%s': %s", cf->path, err);
                    }
                    ui_pause_enter("Continuar");
                }
            }
        }
    }
}

/* Tela de Otimizacao do Windows controlada 100% por Setas e ENTER */
static void interactive_optimize_screen(void) {
    ui_clear();
    ui_banner();
    tweaks_apply_safe_optimizations(true);

    const char *opt_labels[] = {
        "Aplicar as 2 otimizacoes recomendadas agora (Menus 20ms + Desligar GameDVR)",
        "Voltar ao Menu Principal sem alterar nada"
    };
    const char *opt_descs[] = {
        "Grava MenuShowDelay=20 e AppCaptureEnabled=0 no Registro do seu usuario (HKEY_CURRENT_USER).",
        "Mantem as configuracoes atuais do Windows e volta para o inicio."
    };

    int sel = ui_select("ESCOLHA COM AS SETAS O QUE DESEJA FAZER", opt_labels, opt_descs, 2);
    if (sel == 0) {
        printf("\n");
        tweaks_apply_safe_optimizations(false);
        ui_pause_enter("Voltar ao Menu Principal");
    }
}

static void print_help(void) {
    printf(ANSI_BOLD "Uso:" ANSI_RESET " windows-tweaker [comando] [opcoes]\n\n");
    printf(ANSI_BOLD "Como usar" ANSI_RESET "\n");
    printf("  windows-tweaker             abre o menu interativo controlado pelas setas (↑/↓) e Enter\n");
    printf("  windows-tweaker scan        executa o diagnostico completo de uma vez\n");
    printf("  windows-tweaker limpar      abre a tela de limpeza de caches e exclusao de arquivos grandes\n");
    printf("  windows-tweaker otimizar    mostra ou aplica os ajustes do Windows e Jogos\n");
    printf("  windows-tweaker startup     lista todos os programas que iniciam sozinhos no boot\n");
    printf("  windows-tweaker hardware    mostra processador, memoria RAM, placa-mae e BIOS\n\n");
    printf(ANSI_BOLD "Opcoes para uso via script" ANSI_RESET "\n");
    printf("  --dry-run                   apenas simula sem apagar nem alterar nada\n");
    printf("  --apply, --yes              executa a limpeza de caches ou aplica as otimizacoes direto\n");
    printf("  --min-mb 50                 tamanho minimo em MB para listar arquivos grandes (padrao: 50)\n");
    printf("  --days 30                   dias sem uso para listar arquivos grandes esquecidos (padrao: 30)\n");
}

/* Menu Principal 100% controlado pelas setas (CIMA/BAIXO) e ENTER */
static int interactive_menu(void) {
    static const char *MAIN_LABELS[] = {
        "1. Diagnostico Completo do PC (Nota de 0 a 100 explicada)",
        "2. Limpar Caches Temporarios e Excluir Arquivos Pesados",
        "3. Otimizar Windows e Jogos (Menus Instantaneos e FPS)",
        "4. Programas na Inicializacao (Ver o que deixa o Boot lento)",
        "5. Hardware, Memoria RAM e BIOS (CPU, Virtualizacao e Secure Boot)",
        "0. Sair do Windows Tweaker"
    };
    static const char *MAIN_DESCS[] = {
        "Analisa Hardware, Disco e Windows e explica exatamente o motivo de cada ponto perdido na nota.",
        "Mostra as pastas de lixo temporario e cada arquivo grande (>50MB) para voce escolher o que apagar com Enter.",
        "Mostra o que esta lento no Registro do Windows (MenuShowDelay 400ms e Xbox GameDVR) e aplica a correcao.",
        "Lista todos os programas configurados para abrir sozinhos assim que voce liga o computador.",
        "Mostra o modelo exato do seu processador (__cpuid), quanto de RAM esta em uso e configuracoes da BIOS.",
        "Fecha o programa e volta para o terminal."
    };

    for (;;) {
        ui_clear();
        ui_banner();

        int choice = ui_select("MENU PRINCIPAL - O QUE VOCE QUER FAZER?", MAIN_LABELS, MAIN_DESCS, 6);

        if (choice == 5) {
            printf("\n  " ANSI_GREEN "✓" ANSI_RESET " Ate mais!\n\n");
            return 0;
        }

        if (choice == 0) {
            ui_clear();
            ui_banner();

            hardware_report_t hw;
            storage_report_t  st;
            tweaks_report_t   tw;

            ui_step(1, 3, "HARDWARE, MEMORIA RAM E BIOS");
            hardware_run_audit(&hw);
            hardware_print_report(&hw);

            ui_step(2, 3, "DISCO, CACHES TEMPORARIOS E ARQUIVOS ESQUECIDOS");
            storage_run_audit(&st, 50, 30, NULL);
            storage_print_report(&st);

            ui_step(3, 3, "CONFIGURACOES DO WINDOWS E JOGOS");
            tweaks_run_audit(&tw);
            tweaks_print_report(&tw);

            render_summary_box(&hw, &st, &tw);

            const char *post_labels[] = {
                "Abrir Limpeza de Caches e Arquivos Pesados agora",
                "Abrir Otimizacao do Windows e Jogos agora",
                "Ver Lista dos Programas que Iniciam no Boot",
                "Voltar ao Menu Principal"
            };
            const char *post_descs[] = {
                "Permite escolher com as setas quais caches ou arquivos grandes voce quer apagar.",
                "Permite aplicar o ajuste de Menus 20ms e desligar a captura oculta do Xbox GameDVR.",
                "Mostra os programas em HKCU\\Run e HKLM\\Run que estao abrindo sozinhos com o Windows.",
                "Volta para a tela inicial do Windows Tweaker."
            };

            int next_act = ui_select("PROXIMO PASSO RECOMENDADO", post_labels, post_descs, 4);
            if (next_act == 0) {
                interactive_file_removal_screen(&st, true);
            } else if (next_act == 1) {
                interactive_optimize_screen();
            } else if (next_act == 2) {
                ui_clear();
                ui_banner();
                tweaks_print_startup(&tw);
                ui_pause_enter("Voltar ao Menu Principal");
            }
        } else if (choice == 1) {
            storage_report_t st;
            interactive_file_removal_screen(&st, false);
        } else if (choice == 2) {
            interactive_optimize_screen();
        } else if (choice == 3) {
            ui_clear();
            ui_banner();
            tweaks_report_t tw;
            tweaks_run_audit(&tw);
            tweaks_print_startup(&tw);
            ui_pause_enter("Voltar ao Menu Principal");
        } else if (choice == 4) {
            ui_clear();
            ui_banner();
            hardware_report_t hw;
            ui_header_box("TELEMETRIA DE HARDWARE, MEMORIA RAM E BIOS",
                          "Informacoes lidas diretamente do processador (__cpuid) e da placa-mae.");
            hardware_run_audit(&hw);
            hardware_print_report(&hw);
            ui_pause_enter("Voltar ao Menu Principal");
        }
    }
}

static int dispatch_command(int argc, char *argv[]) {
    const char *cmd = argv[1];
    bool apply_mode = false;
    bool dry_run_flag = false;
    uint64_t min_cold_mb = 50;
    int min_cold_days = 30;
    const char *custom_dir = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--apply") == 0 || strcmp(argv[i], "--yes") == 0 || strcmp(argv[i], "-y") == 0) {
            apply_mode = true;
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            dry_run_flag = true;
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

    if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0) {
        printf("windows-tweaker %s\n", WT_VERSION);
        return 0;
    }

    ui_banner();

    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0 || strcmp(cmd, "help") == 0 || strcmp(cmd, "ajuda") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(cmd, "scan") == 0 || strcmp(cmd, "status") == 0) {
        hardware_report_t hw;
        storage_report_t  st;
        tweaks_report_t   tw;

        ui_step(1, 3, "HARDWARE, MEMORIA RAM E BIOS");
        hardware_run_audit(&hw);
        hardware_print_report(&hw);

        ui_step(2, 3, "DISCO, CACHES TEMPORARIOS E ARQUIVOS ESQUECIDOS");
        storage_run_audit(&st, min_cold_mb, min_cold_days, custom_dir);
        storage_print_report(&st);

        ui_step(3, 3, "CONFIGURACOES DO WINDOWS E JOGOS");
        tweaks_run_audit(&tw);
        tweaks_print_report(&tw);

        render_summary_box(&hw, &st, &tw);
        return 0;
    }

    if (strcmp(cmd, "limpar") == 0 || strcmp(cmd, "clean") == 0 || strcmp(cmd, "inspect") == 0 || strcmp(cmd, "storage") == 0) {
        storage_report_t st;
        ui_step(1, 1, "DISCO, CACHES TEMPORARIOS E ARQUIVOS GRANDES");
        storage_run_audit(&st, min_cold_mb, min_cold_days, custom_dir);

        if (apply_mode || dry_run_flag) {
            bool dry = dry_run_flag && !apply_mode;
            uint64_t freed = 0;
            uint32_t files = 0, locked = 0;
            storage_clean_caches(&st, dry, &freed, &files, &locked);
            char sz[32];
            ui_format_bytes(freed, sz, sizeof(sz));
            if (dry) {
                ui_info("(dry-run: %s em %u arquivos prontos para limpar; nada foi apagado)", sz, files);
            } else {
                ui_ok("Total liberado: " ANSI_BOLD "%s" ANSI_RESET " (%u arquivos excluidos)", sz, files);
            }
            return 0;
        }

        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        DWORD in_mode = 0;
        if (GetConsoleMode(hIn, &in_mode)) {
            interactive_file_removal_screen(&st, true);
        } else {
            storage_print_removal_preview(&st);
        }
        return 0;
    }

    if (strcmp(cmd, "otimizar") == 0 || strcmp(cmd, "optimize") == 0) {
        bool dry = !apply_mode;
        tweaks_apply_safe_optimizations(dry);
        if (dry) {
            printf("\n");
            ui_info("(dry-run: rode 'windows-tweaker otimizar --apply' ou use o menu interativo para aplicar)");
        }
        return 0;
    }

    if (strcmp(cmd, "startup") == 0 || strcmp(cmd, "boot") == 0) {
        tweaks_report_t tw;
        tweaks_run_audit(&tw);
        tweaks_print_startup(&tw);
        return 0;
    }

    if (strcmp(cmd, "hardware") == 0 || strcmp(cmd, "hw") == 0) {
        hardware_report_t hw;
        ui_header_box("TELEMETRIA DE HARDWARE, MEMORIA RAM E BIOS",
                      "Informacoes lidas diretamente do processador (__cpuid) e da placa-mae.");
        hardware_run_audit(&hw);
        hardware_print_report(&hw);
        return 0;
    }

    printf("  " ANSI_RED "Comando desconhecido: %s" ANSI_RESET "\n\n", cmd);
    print_help();
    return 2;
}

int main(int argc, char *argv[]) {
    ui_init();

    int code = 0;
    if (argc < 2 || strcmp(argv[1], "menu") == 0) {
        code = interactive_menu();
    } else {
        code = dispatch_command(argc, argv);
    }

    ui_cleanup();
    return code;
}
