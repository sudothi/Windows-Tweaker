#include "ui.h"
#include "storage.h"
#include "tweaks.h"
#include "hardware.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>

#define WT_VERSION "1.2.0"

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
                 "Plano de energia em '%s' (reduz clock da CPU)", tw->power_plan_name);
        score -= 8;
        count++;
    }

    if (tw->game_dvr_enabled) {
        penalties[count].points_lost = 8;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Xbox GameDVR ativo em segundo plano (consome GPU e reduz FPS)");
        score -= 8;
        count++;
    }

    if (tw->menu_show_delay_ms > 100) {
        penalties[count].points_lost = 6;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Atraso de abertura de menus em %d ms (padrao lento do Windows)", tw->menu_show_delay_ms);
        score -= 6;
        count++;
    }

    if (tw->startup_count > 6) {
        penalties[count].points_lost = 8;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "%d programas iniciando sozinhos no boot (consome RAM e atrasa login)", tw->startup_count);
        score -= 8;
        count++;
    }

    if (!tw->ntfs_trim_enabled) {
        penalties[count].points_lost = 12;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Comando TRIM do SSD desativado (degrada escrita e vida util)");
        score -= 12;
        count++;
    }

    if (st->total_cache_bytes > 1024ULL * 1024 * 1024) {
        char sz[32];
        ui_format_bytes(st->total_cache_bytes, sz, sizeof(sz));
        penalties[count].points_lost = 7;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "%s acumulados em pastas de cache temporario", sz);
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
                 "UEFI Secure Boot inativo na BIOS");
        score -= 5;
        count++;
    }

    if (hw->ram_load_pct > 85) {
        penalties[count].points_lost = 6;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Memoria RAM acima de 85%% de uso (%u%% ocupada agora)", hw->ram_load_pct);
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
             "Nota do sistema   %s" ANSI_BOLD "%d / 100" ANSI_RESET "  " ANSI_DIM "(%s)" ANSI_RESET,
             score_col, score, status_label);
    box_lines[n] = line_storage_buf[n];
    n++;

    snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
             "Caches limpaveis  " ANSI_BOLD "%s" ANSI_RESET "  ·  Arquivos grandes sem uso: " ANSI_BOLD "%s" ANSI_RESET,
             cache_str, cold_str);
    box_lines[n] = line_storage_buf[n];
    n++;

    if (penalty_count > 0) {
        snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]), "%s", "");
        box_lines[n] = line_storage_buf[n];
        n++;

        snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
                 ANSI_BOLD "Por que sua nota foi %d/100:" ANSI_RESET, score);
        box_lines[n] = line_storage_buf[n];
        n++;

        for (int i = 0; i < penalty_count && n < 22; i++) {
            snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
                     "  " ANSI_RED "-%d pts" ANSI_RESET "  %s",
                     penalties[i].points_lost, penalties[i].reason);
            box_lines[n] = line_storage_buf[n];
            n++;
        }
    }

    snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]), "%s", "");
    box_lines[n] = line_storage_buf[n];
    n++;

    snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
             "Comandos:  " ANSI_DIM "windows-tweaker limpar · otimizar · startup · hardware" ANSI_RESET);
    box_lines[n] = line_storage_buf[n];
    n++;

    ui_box(box_lines, n, "Resumo do seu PC");
}

/* Fluxo interativo de inspecao e exclusao real de caches e arquivos grandes */
static void interactive_file_removal_screen(storage_report_t *st, bool already_scanned) {
    if (!already_scanned) {
        ui_step(1, 1, "Analisando disco, caches temporarios e arquivos grandes");
        storage_run_audit(st, 50, 30, NULL);
    }

    for (;;) {
        storage_print_removal_preview(st);

        printf("  " ANSI_BOLD "O que voce quer excluir?" ANSI_RESET "\n");
        printf("   " ANSI_CYAN "c" ANSI_RESET "        limpa todas as pastas de cache temporario (%%TEMP%%, Win\\Temp, CrashDumps, DirectX)\n");
        if (st->cold_file_count > 0) {
            printf("   " ANSI_CYAN "1..%d" ANSI_RESET "     exclui o(s) arquivo(s) grande(s) da pasta de origem (ex.: " ANSI_BOLD "1" ANSI_RESET " ou " ANSI_BOLD "1,3" ANSI_RESET ")\n",
                   st->cold_file_count);
            printf("   " ANSI_CYAN "t" ANSI_RESET "        limpa todos os caches + exclui todos os arquivos grandes listados\n");
        }
        printf("   " ANSI_DIM "Enter" ANSI_RESET "    voltar ao menu\n\n");

        char choice[128] = {0};
        ui_ask("Escolha:", "", choice, sizeof(choice));

        if (choice[0] == '\0' || strcmp(choice, "0") == 0 || _stricmp(choice, "voltar") == 0) {
            break;
        }

        if (_stricmp(choice, "c") == 0 || _stricmp(choice, "caches") == 0) {
            char clean_sz[32];
            ui_format_bytes(st->safe_cleanable_bytes, clean_sz, sizeof(clean_sz));
            char q[160];
            snprintf(q, sizeof(q), "Limpar todas as pastas de cache temporario (%s)?", clean_sz);
            if (ui_confirm(q, true)) {
                printf("\n");
                uint64_t freed = 0;
                uint32_t files = 0, locked = 0;
                storage_clean_caches(st, false, &freed, &files, &locked);

                char freed_s[32];
                ui_format_bytes(freed, freed_s, sizeof(freed_s));
                printf("\n");
                ui_ok(ANSI_BOLD "Limpeza concluida:" ANSI_RESET " %u arquivos removidos (%s liberados no disco)", files, freed_s);
                if (locked > 0) {
                    ui_info("%u arquivos temporarios foram mantidos porque estao em uso por programas abertos agora.", locked);
                }
                /* Re-scan caches to update sizes on screen */
                storage_run_audit(st, 50, 30, NULL);
            }
            continue;
        }

        if (_stricmp(choice, "t") == 0 || _stricmp(choice, "tudo") == 0) {
            if (ui_confirm("ATENCAO: Limpar todos os caches E excluir permanentemente todos os arquivos grandes listados?", false)) {
                printf("\n");
                uint64_t freed = 0;
                uint32_t files = 0, locked = 0;
                storage_clean_caches(st, false, &freed, &files, &locked);

                for (int i = 0; i < st->cold_file_count; i++) {
                    if (st->cold_files[i].deleted) continue;
                    uint64_t f_bytes = 0;
                    char err[128];
                    if (storage_delete_single_file(st->cold_files[i].path, &f_bytes, err, sizeof(err))) {
                        char fsz[32];
                        ui_format_bytes(f_bytes, fsz, sizeof(fsz));
                        ui_ok("Excluido da pasta de origem: %s (%s)", st->cold_files[i].path, fsz);
                        st->cold_files[i].deleted = true;
                        freed += f_bytes;
                        files++;
                    } else {
                        ui_fail("Nao foi possivel excluir %s: %s", st->cold_files[i].path, err);
                    }
                }

                char freed_s[32];
                ui_format_bytes(freed, freed_s, sizeof(freed_s));
                printf("\n");
                ui_ok(ANSI_BOLD "Total liberado:" ANSI_RESET " %s (%u arquivos removidos)", freed_s, files);
            }
            continue;
        }

        /* Parse comma or space separated file numbers (e.g. "1" or "1, 3" or "2 4") */
        bool any_parsed = false;
        const char *p = choice;
        while (*p) {
            while (*p && (isspace((unsigned char)*p) || *p == ',' || *p == ';')) p++;
            if (!*p) break;

            if (isdigit((unsigned char)*p)) {
                int idx = (int)strtol(p, (char **)&p, 10) - 1;
                any_parsed = true;

                if (idx < 0 || idx >= st->cold_file_count) {
                    ui_warn("Numero %d invalido (escolha entre 1 e %d).", idx + 1, st->cold_file_count);
                    continue;
                }

                cold_file_t *cf = &st->cold_files[idx];
                if (cf->deleted) {
                    ui_info("O arquivo #%d ja foi excluido.", idx + 1);
                    continue;
                }

                char fsz[32];
                ui_format_bytes(cf->size_bytes, fsz, sizeof(fsz));
                char q[380];
                snprintf(q, sizeof(q), "Excluir permanentemente '%s' (%s)?", cf->path, fsz);
                if (ui_confirm(q, false)) {
                    uint64_t freed_b = 0;
                    char err[128];
                    if (storage_delete_single_file(cf->path, &freed_b, err, sizeof(err))) {
                        ui_ok("Arquivo excluido da pasta de origem: " ANSI_BOLD "%s" ANSI_RESET " (%s liberados)",
                              cf->path, fsz);
                        cf->deleted = true;
                    } else {
                        ui_fail("Falha ao excluir '%s': %s", cf->path, err);
                    }
                } else {
                    ui_info("Arquivo #%d mantido.", idx + 1);
                }
            } else {
                ui_warn("Opcao desconhecida: '%s'. Digite 'c', o numero do arquivo (ex.: 1) ou Enter.", choice);
                break;
            }
        }
        (void)any_parsed;
    }
}

static void print_help(void) {
    printf(ANSI_BOLD "Uso:" ANSI_RESET " windows-tweaker <comando> [opcoes]\n\n");
    printf(ANSI_BOLD "Comandos" ANSI_RESET "\n");
    printf("  scan                diagnostico completo (Hardware, Disco, Windows e Nota explicada)\n");
    printf("  limpar              mostra os caches e arquivos grandes e permite excluir de verdade\n");
    printf("  otimizar            mostra e aplica ajustes de desempenho no Registro (MenuShowDelay, GameDVR)\n");
    printf("  startup             lista todos os programas que iniciam sozinhos com o Windows\n");
    printf("  hardware            mostra processador (__cpuid), memoria RAM, placa-mae e BIOS\n\n");
    printf(ANSI_BOLD "Opcoes" ANSI_RESET "\n");
    printf("  --dry-run           apenas simula e mostra o que seria feito sem apagar nem alterar nada\n");
    printf("  --apply, --yes      executa a limpeza de caches ou aplica as otimizacoes direto\n");
    printf("  --min-mb 50         tamanho minimo em MB para listar arquivos grandes sem uso (padrao: 50)\n");
    printf("  --days 30           dias sem uso para listar arquivos grandes esquecidos (padrao: 30)\n");
}

static int dispatch_command(int argc, char *argv[]);

/* Menu interativo identico ao interactive() de apps/cli/bin/turtle-setup.mjs */
static int interactive_menu(void) {
    for (;;) {
        ui_banner();
        printf("  " ANSI_BOLD "O que voce quer fazer?" ANSI_RESET "\n\n");
        printf("   1) Diagnostico completo do PC (Score explicado)\n");
        printf("   2) Ver e excluir arquivos pesados / caches temporarios\n");
        printf("   3) Aplicar otimizacoes do Windows e Jogos\n");
        printf("   4) Ver programas que iniciam com o Windows (Boot)\n");
        printf("   5) Ver Hardware, Memoria RAM e BIOS\n");
        printf("   6) Ajuda\n");
        printf("   0) Sair\n\n");

        char ans[32] = {0};
        ui_ask("Numero:", "", ans, sizeof(ans));

        if (ans[0] == '\0' || strcmp(ans, "0") == 0) {
            return 0;
        }

        int choice = atoi(ans);
        if (choice == 1) {
            hardware_report_t hw;
            storage_report_t  st;
            tweaks_report_t   tw;

            ui_step(1, 3, "Hardware, Memoria e BIOS");
            hardware_run_audit(&hw);
            hardware_print_report(&hw);

            ui_step(2, 3, "Disco, Caches e Arquivos Esquecidos");
            storage_run_audit(&st, 50, 30, NULL);
            storage_print_report(&st);

            ui_step(3, 3, "Configuracoes do Windows e Jogos");
            tweaks_run_audit(&tw);
            tweaks_print_report(&tw);

            render_summary_box(&hw, &st, &tw);

            printf("\n");
            if (ui_confirm("Quer abrir a lista de arquivos para escolher o que excluir agora?", true)) {
                interactive_file_removal_screen(&st, true);
            }
            ui_pause();
        } else if (choice == 2) {
            storage_report_t st;
            interactive_file_removal_screen(&st, false);
            ui_pause();
        } else if (choice == 3) {
            ui_step(1, 1, "Otimizacoes do Windows e Jogos (HKEY_CURRENT_USER)");
            tweaks_apply_safe_optimizations(true);
            printf("\n");
            if (ui_confirm("Aplicar essas 2 otimizacoes no Registro do seu Windows agora?", true)) {
                printf("\n");
                tweaks_apply_safe_optimizations(false);
            }
            ui_pause();
        } else if (choice == 4) {
            tweaks_report_t tw;
            ui_step(1, 1, "Programas na Inicializacao do Windows (Boot)");
            tweaks_run_audit(&tw);
            tweaks_print_startup(&tw);
            ui_pause();
        } else if (choice == 5) {
            hardware_report_t hw;
            ui_step(1, 1, "Hardware, Memoria RAM e BIOS");
            hardware_run_audit(&hw);
            hardware_print_report(&hw);
            ui_pause();
        } else if (choice == 6) {
            printf("\n");
            print_help();
            ui_pause();
        } else {
            printf("  " ANSI_RED "Opcao invalida." ANSI_RESET "\n");
            ui_pause();
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

        ui_step(1, 3, "Hardware, Memoria e BIOS");
        hardware_run_audit(&hw);
        hardware_print_report(&hw);

        ui_step(2, 3, "Disco, Caches e Arquivos Esquecidos");
        storage_run_audit(&st, min_cold_mb, min_cold_days, custom_dir);
        storage_print_report(&st);

        ui_step(3, 3, "Configuracoes do Windows e Jogos");
        tweaks_run_audit(&tw);
        tweaks_print_report(&tw);

        render_summary_box(&hw, &st, &tw);
        return 0;
    }

    if (strcmp(cmd, "limpar") == 0 || strcmp(cmd, "clean") == 0 || strcmp(cmd, "inspect") == 0 || strcmp(cmd, "storage") == 0) {
        storage_report_t st;
        ui_step(1, 1, "Disco, Caches Temporarios e Arquivos Grandes");
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

        /* Se estiver num terminal interativo, abre o seletor para escolher o que excluir */
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
        ui_step(1, 1, dry ? "Otimizacoes do Windows (simulacao --dry-run)" : "Aplicando Otimizacoes do Windows");
        tweaks_apply_safe_optimizations(dry);
        if (dry) {
            printf("\n");
            ui_info("(dry-run: rode 'windows-tweaker otimizar --apply' para gravar no Registro)");
        }
        return 0;
    }

    if (strcmp(cmd, "startup") == 0 || strcmp(cmd, "boot") == 0) {
        tweaks_report_t tw;
        ui_step(1, 1, "Programas na Inicializacao do Windows");
        tweaks_run_audit(&tw);
        tweaks_print_startup(&tw);
        return 0;
    }

    if (strcmp(cmd, "hardware") == 0 || strcmp(cmd, "hw") == 0) {
        hardware_report_t hw;
        ui_step(1, 1, "Hardware, Memoria RAM e BIOS");
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
