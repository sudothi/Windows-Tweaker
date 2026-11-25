#include "ui.h"
#include "storage.h"
#include "tweaks.h"
#include "hardware.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define WT_VERSION "1.5.0"

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
                 "Plano de energia em '%s' (pode mudar para Alto Desempenho no menu abaixo)", tw->power_plan_name);
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

    if (tw->menu_show_delay_ms > 50) {
        penalties[count].points_lost = 6;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Velocidade dos menus em %d ms (atraso padrao lento do Windows)", tw->menu_show_delay_ms);
        score -= 6;
        count++;
    }

    if (tw->explorer_ads_enabled) {
        penalties[count].points_lost = 4;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Anuncios e sugestoes promocionais ativos no Windows Explorer");
        score -= 4;
        count++;
    }

    /* So desconta pontos se houver mais de 6 programas REALMENTE HABILITADOS no Gerenciador de Tarefas */
    if (tw->startup_enabled_count > 6) {
        penalties[count].points_lost = 8;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "%d programas habilitados abrindo sozinhos no boot (gasta RAM e atrasa o login)",
                 tw->startup_enabled_count);
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

    /* Usa o mesmo valor de safe_cleanable_bytes exibido no topo da caixa para nunca divergir! */
    if (st->safe_cleanable_bytes > 500ULL * 1024 * 1024) {
        char sz[32];
        ui_format_bytes(st->safe_cleanable_bytes, sz, sizeof(sz));
        penalties[count].points_lost = 7;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "%s acumulados em lixo temporario do sistema (%%TEMP%%, CrashDumps, DirectX)", sz);
        score -= 7;
        count++;
    } else if (st->safe_cleanable_bytes > 100ULL * 1024 * 1024) {
        char sz[32];
        ui_format_bytes(st->safe_cleanable_bytes, sz, sizeof(sz));
        penalties[count].points_lost = 4;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "%s acumulados em lixo temporario do sistema prontos para limpar", sz);
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
                 "UEFI Secure Boot desativado na BIOS (so pode ser alterado manualmente na BIOS da placa-mae)");
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

    char sys_cache_str[32], dev_cache_str[32], cold_str[32];
    uint64_t dev_cache_bytes = (st->total_cache_bytes > st->safe_cleanable_bytes)
                               ? (st->total_cache_bytes - st->safe_cleanable_bytes) : 0;
    ui_format_bytes(st->safe_cleanable_bytes, sys_cache_str, sizeof(sys_cache_str));
    ui_format_bytes(dev_cache_bytes, dev_cache_str, sizeof(dev_cache_str));
    ui_format_bytes(st->total_cold_bytes, cold_str, sizeof(cold_str));

    char line_storage_buf[24][256];
    const char *box_lines[24];
    int n = 0;

    snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
             "Nota do seu PC            %s" ANSI_BOLD "%d / 100" ANSI_RESET "  " ANSI_DIM "(%s)" ANSI_RESET,
             score_col, score, status_label);
    box_lines[n] = line_storage_buf[n];
    n++;

    snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
             "Lixo temporario sistema   " ANSI_BOLD "%s" ANSI_RESET "  ·  Caches de Apps (NPM/Pip/Chrome): " ANSI_BOLD "%s" ANSI_RESET,
             sys_cache_str, dev_cache_str);
    box_lines[n] = line_storage_buf[n];
    n++;

    snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
             "Arquivos grandes (+50MB)  " ANSI_BOLD "%s" ANSI_RESET " sem uso ha +30 dias (%d arquivos)",
             cold_str, st->cold_file_count);
    box_lines[n] = line_storage_buf[n];
    n++;

    snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
             "Programas no Boot         " ANSI_BOLD "%d Habilitados" ANSI_RESET "  " ANSI_DIM "(%d ja Desabilitados por voce no Task Manager)" ANSI_RESET,
             tw->startup_enabled_count, tw->startup_disabled_count);
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

    ui_box(box_lines, n, "Resultado da Analise (Nada foi alterado ainda)");
}

/* Tela de Limpeza e Exclusao: Permite limpar caches do sistema, caches de dev (NPM/Pip) e arquivos grandes */
static void interactive_file_removal_screen(storage_report_t *st, bool already_scanned) {
    if (!already_scanned) {
        ui_clear();
        ui_banner();
        ui_step(1, 1, "Analisando disco, pastas de cache e arquivos grandes (somente leitura)");
        storage_run_audit(st, 50, 30, NULL);
    }

    for (;;) {
        ui_clear();
        ui_banner();
        storage_print_removal_preview(st);

        printf("  " ANSI_GREEN "·" ANSI_RESET " " ANSI_DIM "Analise concluida. Escolha abaixo com as setas o que deseja apagar:" ANSI_RESET "\n");

        char labels_buf[MAX_CACHE_TARGETS + MAX_COLD_FILES + 4][120];
        const char *labels[MAX_CACHE_TARGETS + MAX_COLD_FILES + 4];
        int opt_type[MAX_CACHE_TARGETS + MAX_COLD_FILES + 4]; /* 0=all_sys_cache, 1=single_cache, 2=cold_file, 3=back */
        int opt_index[MAX_CACHE_TARGETS + MAX_COLD_FILES + 4];
        int total_opts = 0;

        char cache_sz[32];
        ui_format_bytes(st->safe_cleanable_bytes, cache_sz, sizeof(cache_sz));

        snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                 "[LIMPAR CACHES DO SISTEMA] Apagar %%TEMP%%, Win\\Temp, CrashDumps e DirectX (%s)", cache_sz);
        labels[total_opts]    = labels_buf[total_opts];
        opt_type[total_opts]  = 0;
        opt_index[total_opts] = -1;
        total_opts++;

        /* Tambem permite limpar individualmente os caches pesados de NPM / Pip se tiverem arquivos */
        for (int i = 0; i < st->cache_count; i++) {
            const cache_target_t *c = &st->caches[i];
            if (c->safe_to_clean || c->size_bytes == 0) continue;
            char csz[32];
            ui_format_bytes(c->size_bytes, csz, sizeof(csz));
            snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                     "[LIMPAR CACHE OPCIONAL]    %s (%s em %u arquivos)",
                     c->name, csz, c->file_count);
            labels[total_opts]    = labels_buf[total_opts];
            opt_type[total_opts]  = 1;
            opt_index[total_opts] = i;
            total_opts++;
        }

        for (int i = 0; i < st->cold_file_count; i++) {
            const cold_file_t *cf = &st->cold_files[i];
            char fsz[32];
            ui_format_bytes(cf->size_bytes, fsz, sizeof(fsz));

            const char *p = cf->path;
            char short_path[52];
            size_t plen = strlen(p);
            if (plen > 48) {
                snprintf(short_path, sizeof(short_path), "...%s", p + (plen - 45));
            } else {
                snprintf(short_path, sizeof(short_path), "%s", p);
            }

            if (cf->deleted) {
                snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                         "[JA EXCLUIDO]              %s (%s)", short_path, fsz);
            } else {
                snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                         "[EXCLUIR ARQUIVO GRANDE]   %-10s (%3d dias) -> %s", fsz, cf->days_unused, short_path);
            }
            labels[total_opts]    = labels_buf[total_opts];
            opt_type[total_opts]  = 2;
            opt_index[total_opts] = i;
            total_opts++;
        }

        snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                 "Sair e voltar ao Menu Principal");
        labels[total_opts]    = labels_buf[total_opts];
        opt_type[total_opts]  = 3;
        opt_index[total_opts] = -1;
        total_opts++;

        int sel = ui_select("ESCOLHA O QUE DESEJA LIMPAR (PEDIRA SUA PERMISSAO ANTES)", labels, total_opts);

        if (opt_type[sel] == 3) {
            break;
        }

        if (opt_type[sel] == 0) {
            char detail[200];
            snprintf(detail, sizeof(detail),
                     "Apagara os arquivos temporarios de %%TEMP%%, Win\\Temp, CrashDumps e DirectX (%s).",
                     cache_sz);
            if (ui_confirm_arrows("Autoriza a limpeza das pastas de cache temporario do sistema?", detail)) {
                printf("\n");
                uint64_t freed = 0;
                uint32_t files = 0, locked = 0;
                storage_clean_caches(st, false, &freed, &files, &locked);

                char freed_s[32];
                ui_format_bytes(freed, freed_s, sizeof(freed_s));
                printf("\n");
                ui_ok(ANSI_BOLD "Limpeza concluida:" ANSI_RESET " %u arquivos removidos (%s liberados no disco)", files, freed_s);
                if (locked > 0) {
                    ui_info("%u arquivos temporarios foram mantidos porque estao em uso pelo Windows agora.", locked);
                }
                ui_pause_enter("Continuar");
                storage_run_audit(st, 50, 30, NULL);
            }
        } else if (opt_type[sel] == 1) {
            int c_idx = opt_index[sel];
            const cache_target_t *c = &st->caches[c_idx];
            char csz[32];
            ui_format_bytes(c->size_bytes, csz, sizeof(csz));
            char q[200], detail[240];
            snprintf(q, sizeof(q), "Autoriza limpar a pasta '%s' (%s)?", c->name, csz);
            snprintf(detail, sizeof(detail), "Caminho: %s (%s)", c->path, c->explanation);
            if (ui_confirm_arrows(q, detail)) {
                printf("\n");
                uint64_t freed = 0;
                uint32_t files = 0, locked = 0;
                storage_clean_single_cache(c, false, &freed, &files, &locked);
                ui_pause_enter("Continuar");
                storage_run_audit(st, 50, 30, NULL);
            }
        } else if (opt_type[sel] == 2) {
            int file_idx = opt_index[sel];
            cold_file_t *cf = &st->cold_files[file_idx];
            if (cf->deleted) {
                ui_info("Este arquivo ja foi excluido do seu disco.");
                ui_pause_enter("Continuar");
                continue;
            }

            char fsz[32];
            ui_format_bytes(cf->size_bytes, fsz, sizeof(fsz));
            char q[320], detail[360];
            snprintf(q, sizeof(q), "Autoriza excluir permanentemente o arquivo '%s'?", cf->path);
            snprintf(detail, sizeof(detail),
                     "O arquivo sera apagado definitivamente da pasta de origem e liberara %s no disco.", fsz);

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

/* Tela Interativa de Programas na Inicializacao (Boot): Permite Habilitar / Desabilitar igual ao Task Manager */
static void interactive_startup_screen(tweaks_report_t *tw, bool already_scanned) {
    if (!already_scanned) {
        ui_clear();
        ui_banner();
        tweaks_run_audit(tw);
    }

    for (;;) {
        ui_clear();
        ui_banner();
        tweaks_print_startup(tw);

        printf("  " ANSI_GREEN "·" ANSI_RESET " " ANSI_DIM "Selecione qualquer programa abaixo e aperte ENTER para Desabilitar ou Habilitar no Boot:" ANSI_RESET "\n");

        char labels_buf[MAX_STARTUP_ENTRIES + 2][120];
        const char *labels[MAX_STARTUP_ENTRIES + 2];
        int total_opts = 0;

        for (int i = 0; i < tw->startup_count; i++) {
            const startup_entry_t *e = &tw->startup_apps[i];
            if (e->enabled) {
                snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                         "[HABILITADO   -> Clique para Desabilitar]  %s (%s)",
                         e->name, e->is_hkcu ? "Usuario" : "Sistema");
            } else {
                snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                         "[DESABILITADO -> Clique para Habilitar  ]  %s (%s)",
                         e->name, e->is_hkcu ? "Usuario" : "Sistema");
            }
            labels[total_opts] = labels_buf[total_opts];
            total_opts++;
        }

        snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                 "Sair e voltar ao Menu Principal");
        labels[total_opts] = labels_buf[total_opts];
        int back_idx = total_opts;
        total_opts++;

        int sel = ui_select("GERENCIAR PROGRAMAS DO BOOT (SINCRONIZADO COM TASK MANAGER)", labels, total_opts);
        if (sel == back_idx) {
            break;
        }

        if (sel >= 0 && sel < tw->startup_count) {
            startup_entry_t *e = &tw->startup_apps[sel];
            bool target_state = !e->enabled;
            char q[200], detail[260];
            snprintf(q, sizeof(q), "Deseja %s '%s' na inicializacao do Windows?",
                     target_state ? "HABILITAR" : "DESABILITAR", e->name);
            snprintf(detail, sizeof(detail), "Executavel: %s", e->command);

            if (ui_confirm_arrows(q, detail)) {
                printf("\n");
                if (tweaks_toggle_startup_entry(e, target_state)) {
                    if (target_state) {
                        tw->startup_enabled_count++;
                        tw->startup_disabled_count--;
                        ui_ok("'%s' agora esta " ANSI_BOLD "HABILITADO" ANSI_RESET " na inicializacao.", e->name);
                    } else {
                        tw->startup_enabled_count--;
                        tw->startup_disabled_count++;
                        ui_ok("'%s' agora esta " ANSI_BOLD "DESABILITADO" ANSI_RESET " (nao abrira mais junto com o PC).", e->name);
                    }
                } else {
                    ui_fail("Permissao negada para alterar '%s' (itens do Sistema HKLM exigem abrir como Administrador).", e->name);
                }
                ui_pause_enter("Continuar");
            }
        }
    }
}

/* Tela de Otimizacao do Windows: So oferece aplicar o que estiver realmente PENDENTE */
static void interactive_optimize_screen(void) {
    for (;;) {
        ui_clear();
        ui_banner();

        tweaks_report_t current;
        int pending = tweaks_inspect_optimizations(&current);

        if (pending == 0) {
            ui_ok(ANSI_BOLD "Tudo otimizado!" ANSI_RESET " Todos os 4 ajustes acima ja estao ativos no seu Windows (nada pendente).");

            const char *done_labels[] = {
                "Voltar ao Menu Principal",
                "Restaurar padroes originais de fabrica do Windows (desfazer otimizacoes)"
            };
            int sel = ui_select("NENHUMA OTIMIZACAO PENDENTE", done_labels, 2);
            if (sel == 1) {
                if (ui_confirm_arrows("Tem certeza que deseja voltar as configuracoes para o padrao lento de fabrica?",
                                      "Volta o plano para Equilibrado, MenuShowDelay para 400ms e reativa o GameDVR.")) {
                    printf("\n");
                    tweaks_restore_windows_defaults();
                    ui_pause_enter("Continuar");
                    continue;
                }
            }
            return;
        }

        printf("  " ANSI_GREEN "·" ANSI_RESET " " ANSI_DIM "Encontrados %d ajuste(s) pendente(s). Os itens que ja estao 'JA OTIMIZADO' nao serao tocados." ANSI_RESET "\n",
               pending);

        char apply_all_btn[128];
        snprintf(apply_all_btn, sizeof(apply_all_btn),
                 "Autorizar e aplicar todos os %d ajuste(s) pendente(s) agora", pending);

        char opt_buf[6][128];
        const char *opt_labels[6];
        int opt_code[6]; /* 0=apply_all, 1=power_only, 2=back, 3=restore */
        int count = 0;

        snprintf(opt_buf[count], sizeof(opt_buf[count]), "%s", apply_all_btn);
        opt_labels[count] = opt_buf[count];
        opt_code[count] = 0;
        count++;

        if (!current.is_high_performance) {
            snprintf(opt_buf[count], sizeof(opt_buf[count]),
                     "Mudar apenas o Plano de Energia para 'Alto Desempenho' agora");
            opt_labels[count] = opt_buf[count];
            opt_code[count] = 1;
            count++;
        }

        snprintf(opt_buf[count], sizeof(opt_buf[count]),
                 "Sair e voltar ao Menu Principal sem alterar nada");
        opt_labels[count] = opt_buf[count];
        opt_code[count] = 2;
        count++;

        snprintf(opt_buf[count], sizeof(opt_buf[count]),
                 "Restaurar padroes originais de fabrica do Windows (desfazer otimizacoes)");
        opt_labels[count] = opt_buf[count];
        opt_code[count] = 3;
        count++;

        int sel = ui_select("DESEJA APLICAR OS AJUSTES PENDENTES?", opt_labels, count);
        if (opt_code[sel] == 2) {
            break;
        } else if (opt_code[sel] == 0) {
            printf("\n");
            int applied = tweaks_apply_pending_optimizations(&current);
            printf("\n");
            ui_ok(ANSI_BOLD "%d ajuste(s) pendente(s) aplicado(s) com sucesso!" ANSI_RESET, applied);
            ui_pause_enter("Ver resultado atualizado");
        } else if (opt_code[sel] == 1) {
            printf("\n");
            if (tweaks_activate_high_performance_power()) {
                ui_ok("Plano de Energia alterado com sucesso para " ANSI_BOLD "Alto Desempenho" ANSI_RESET "!");
            } else {
                ui_warn("Nao foi possivel ativar o plano Alto Desempenho.");
            }
            ui_pause_enter("Ver resultado atualizado");
        } else if (opt_code[sel] == 3) {
            if (ui_confirm_arrows("Tem certeza que deseja voltar as configuracoes para o padrao de fabrica?",
                                  "Volta o plano para Equilibrado, MenuShowDelay para 400ms e reativa o GameDVR.")) {
                printf("\n");
                tweaks_restore_windows_defaults();
                ui_pause_enter("Ver resultado atualizado");
            }
        }
    }
}

static void print_help(void) {
    printf(ANSI_BOLD "Uso:" ANSI_RESET " windows-tweaker [comando] [opcoes]\n\n");
    printf(ANSI_BOLD "Como usar" ANSI_RESET "\n");
    printf("  windows-tweaker             abre o menu interativo controlado pelas setas (↑/↓) e Enter\n");
    printf("  windows-tweaker scan        executa a analise completa (somente leitura)\n");
    printf("  windows-tweaker limpar      analisa os caches e arquivos grandes e pede permissao para limpar\n");
    printf("  windows-tweaker otimizar    analisa ou aplica os ajustes do Windows e Jogos\n");
    printf("  windows-tweaker startup     lista e gerencia os programas que iniciam com o Windows\n");
    printf("  windows-tweaker hardware    mostra processador, memoria RAM, placa-mae e BIOS\n\n");
    printf(ANSI_BOLD "Opcoes para uso via script" ANSI_RESET "\n");
    printf("  --dry-run                   apenas analisa sem apagar nem alterar nada\n");
    printf("  --apply, --yes              executa a limpeza de caches ou aplica as otimizacoes direto\n");
    printf("  --min-mb 50                 tamanho minimo em MB para listar arquivos grandes (padrao: 50)\n");
    printf("  --days 30                   dias sem uso para listar arquivos grandes esquecidos (padrao: 30)\n");
}

static int interactive_menu(void) {
    static const char *MAIN_LABELS[] = {
        "1. Diagnostico Completo do PC (Nota de 0 a 100 explicada)",
        "2. Limpar Caches Temporarios e Excluir Arquivos Pesados",
        "3. Otimizar Windows e Jogos (Plano de Energia, Menus e GameDVR)",
        "4. Programas na Inicializacao (Habilitar / Desabilitar no Boot)",
        "5. Hardware, Memoria RAM e BIOS (CPU, Virtualizacao e Secure Boot)",
        "0. Sair do Windows Tweaker"
    };

    for (;;) {
        ui_clear();
        ui_banner();

        printf("  " ANSI_GREEN "·" ANSI_RESET " " ANSI_DIM "Modo Seguro: Ao abrir qualquer opcao, inicialmente e feita apenas a analise do seu PC." ANSI_RESET "\n");
        printf("  " ANSI_GREEN "·" ANSI_RESET " " ANSI_DIM "Qualquer limpeza ou alteracao so e executada depois, com a sua permissao." ANSI_RESET "\n");

        int choice = ui_select("MENU PRINCIPAL - ESCOLHA UMA OPCAO", MAIN_LABELS, 6);

        if (choice == 5) {
            printf("\n  " ANSI_GREEN "✓" ANSI_RESET " Ate mais!\n\n");
            return 0;
        }

        if (choice == 0) {
            if (!ui_open_tool_screen(
                    "1. DIAGNOSTICO COMPLETO DO PC",
                    "Vai ler as informacoes do seu Processador, Memoria RAM, BIOS, medir o espaco ocupado por lixo temporario no disco e checar o Plano de Energia e programas habilitados no Boot.",
                    "Voce vera um relatorio explicando cada item e o motivo da sua nota de 0 a 100. Logo abaixo do resultado apareceram os botoes diretos para resolver cada item pendente."
                )) {
                continue;
            }

            ui_clear();
            ui_banner();

            hardware_report_t hw;
            storage_report_t  st;
            tweaks_report_t   tw;

            ui_step(1, 3, "HARDWARE, MEMORIA RAM E BIOS (SOMENTE LEITURA)");
            hardware_run_audit(&hw);
            hardware_print_report(&hw);

            ui_step(2, 3, "DISCO, CACHES TEMPORARIOS E ARQUIVOS ESQUECIDOS (SOMENTE LEITURA)");
            storage_run_audit(&st, 50, 30, NULL);
            storage_print_report(&st);

            ui_step(3, 3, "CONFIGURACOES DO WINDOWS E JOGOS (SOMENTE LEITURA)");
            tweaks_run_audit(&tw);
            tweaks_print_report(&tw);

            render_summary_box(&hw, &st, &tw);

            /* Monta botoes de acao pos-diagnostico conectados diretamente aos problemas encontrados! */
            char post_buf[6][128];
            const char *post_labels[6];
            int post_code[6]; /* 0=power_fix, 1=optimize_screen, 2=clean_screen, 3=startup_screen, 4=back */
            int pcount = 0;

            if (!tw.is_high_performance) {
                snprintf(post_buf[pcount], sizeof(post_buf[pcount]),
                         "Corrigir Plano de Energia: Ativar 'Alto Desempenho' agora (+8 pts na nota)");
                post_labels[pcount] = post_buf[pcount];
                post_code[pcount] = 0;
                pcount++;
            }

            if (tw.pending_tweaks_count > 0) {
                snprintf(post_buf[pcount], sizeof(post_buf[pcount]),
                         "Ir para Otimizacao do Windows e Jogos (%d ajuste(s) pendente(s))",
                         tw.pending_tweaks_count);
                post_labels[pcount] = post_buf[pcount];
                post_code[pcount] = 1;
                pcount++;
            }

            char clean_sz[32];
            ui_format_bytes(st.safe_cleanable_bytes, clean_sz, sizeof(clean_sz));
            snprintf(post_buf[pcount], sizeof(post_buf[pcount]),
                     "Ir para Limpeza de Caches (%s) e Arquivos Grandes (%d encontrados)",
                     clean_sz, st.cold_file_count);
            post_labels[pcount] = post_buf[pcount];
            post_code[pcount] = 2;
            pcount++;

            snprintf(post_buf[pcount], sizeof(post_buf[pcount]),
                     "Gerenciar Programas do Boot (%d habilitados, %d ja desabilitados)",
                     tw.startup_enabled_count, tw.startup_disabled_count);
            post_labels[pcount] = post_buf[pcount];
            post_code[pcount] = 3;
            pcount++;

            snprintf(post_buf[pcount], sizeof(post_buf[pcount]),
                     "Voltar ao Menu Principal");
            post_labels[pcount] = post_buf[pcount];
            post_code[pcount] = 4;
            pcount++;

            int next_sel = ui_select("ANALISE CONCLUIDA - O QUE DESEJA FAZER AGORA?", post_labels, pcount);
            int action = post_code[next_sel];

            if (action == 0) {
                if (ui_confirm_arrows("Autoriza mudar o Plano de Energia do Windows para 'Alto Desempenho' agora?",
                                      "Executa powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c para evitar quedas de clock da CPU.")) {
                    printf("\n");
                    if (tweaks_activate_high_performance_power()) {
                        ui_ok("Plano de Energia alterado com sucesso para " ANSI_BOLD "Alto Desempenho" ANSI_RESET " (+8 pts recuperados)!");
                    } else {
                        ui_warn("Nao foi possivel ativar o plano Alto Desempenho.");
                    }
                    ui_pause_enter("Voltar ao Menu Principal");
                }
            } else if (action == 1) {
                interactive_optimize_screen();
            } else if (action == 2) {
                interactive_file_removal_screen(&st, true);
            } else if (action == 3) {
                interactive_startup_screen(&tw, true);
            }
        } else if (choice == 1) {
            if (!ui_open_tool_screen(
                    "2. LIMPAR CACHES TEMPORARIOS E EXCLUIR ARQUIVOS PESADOS",
                    "Vai escanear as pastas de lixo temporario do Windows (%TEMP%, Win\\Temp, CrashDumps, DirectX), caches de desenvolvimento (NPM/Pip) e arquivos grandes (>50 MB) sem uso ha +30 dias.",
                    "Mostrara a lista completa do que foi encontrado. Nada sera apagado durante a analise: voce escolhida na tela seguinte exatamente o que autoriza remover."
                )) {
                continue;
            }
            storage_report_t st;
            interactive_file_removal_screen(&st, false);
        } else if (choice == 2) {
            if (!ui_open_tool_screen(
                    "3. OTIMIZAR WINDOWS E JOGOS",
                    "Vai verificar 4 configuracoes: Plano de Energia (Alto Desempenho), Velocidade dos Menus (MenuShowDelay), Xbox GameDVR e Anuncios no Windows Explorer.",
                    "Os itens que ja estiverem otimizados aparecerao como 'JA OTIMIZADO' e nao serao tocados. Voce podera aplicar apenas o que estiver pendente."
                )) {
                continue;
            }
            interactive_optimize_screen();
        } else if (choice == 3) {
            if (!ui_open_tool_screen(
                    "4. PROGRAMAS NA INICIALIZACAO (BOOT)",
                    "Vai ler as chaves Run e StartupApproved\\Run do Registro (a mesma tabela do Gerenciador de Tarefas) para identificar quais programas estao realmente HABILITADOS e quais voce ja desabilitou.",
                    "Voce podera selecionar qualquer programa da lista com as setas e apertar ENTER para Desabilitar ou Habilitar no Boot."
                )) {
                continue;
            }
            tweaks_report_t tw;
            interactive_startup_screen(&tw, false);
        } else if (choice == 4) {
            if (!ui_open_tool_screen(
                    "5. HARDWARE, MEMORIA RAM E BIOS",
                    "Vai consultar diretamente os registradores do seu processador (__cpuid), medir o uso atual da memoria RAM fisica e verificar na BIOS se a Virtualizacao e o Secure Boot estao ativos.",
                    "Exibira o relatorio detalhado do seu hardware com recomendacoes claras (apenas leitura)."
                )) {
                continue;
            }
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
        tweaks_report_t current;
        int pending = tweaks_inspect_optimizations(&current);
        if (pending == 0) {
            ui_ok("Todos os ajustes ja estao otimizados no seu PC (0 pendentes).");
        } else if (dry) {
            printf("\n");
            ui_info("(dry-run: %d ajuste(s) pendente(s). Rode 'windows-tweaker otimizar --apply' para aplicar)", pending);
        } else {
            printf("\n");
            tweaks_apply_pending_optimizations(&current);
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
