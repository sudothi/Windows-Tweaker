#include "ui.h"
#include "storage.h"
#include "tweaks.h"
#include "hardware.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define WT_VERSION "2.0.0"

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

    /* 1. Avalia cada uma das 12 regras de otimizacao do Windows */
    for (int i = 0; i < tw->rule_count && count < 18; i++) {
        const tweak_rule_t *r = &tw->rules[i];
        if (!r->is_optimized) {
            penalties[count].points_lost = r->score_weight;
            snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                     "%s -> %s", r->title, r->current_state);
            score -= r->score_weight;
            count++;
        }
    }

    /* 2. Avalia sobrecarga real do Boot (apenas os HABILITADOS em StartupApproved\Run) */
    if (tw->startup_enabled_count > 6 && count < 18) {
        penalties[count].points_lost = 8;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "%d programas habilitados abrindo sozinhos no boot (gasta RAM e atrasa o login)",
                 tw->startup_enabled_count);
        score -= 8;
        count++;
    }

    if (!tw->ntfs_trim_enabled && count < 18) {
        penalties[count].points_lost = 10;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Comando TRIM do SSD desativado (deixa o SSD lento com o tempo)");
        score -= 10;
        count++;
    }

    /* 3. Avalia lixo temporario do sistema */
    if (st->safe_cleanable_bytes > 500ULL * 1024 * 1024 && count < 18) {
        char sz[32];
        ui_format_bytes(st->safe_cleanable_bytes, sz, sizeof(sz));
        penalties[count].points_lost = 6;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "%s acumulados em lixo temporario do sistema (%%TEMP%%, CrashDumps, DirectX)", sz);
        score -= 6;
        count++;
    } else if (st->safe_cleanable_bytes > 100ULL * 1024 * 1024 && count < 18) {
        char sz[32];
        ui_format_bytes(st->safe_cleanable_bytes, sz, sizeof(sz));
        penalties[count].points_lost = 3;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "%s acumulados em lixo temporario do sistema prontos para limpar", sz);
        score -= 3;
        count++;
    }

    /* 4. Avalia Hardware e BIOS */
    if (!hw->virt_firmware_enabled && count < 18) {
        penalties[count].points_lost = 6;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Virtualizacao (VT-x / AMD-V) desativada na BIOS");
        score -= 6;
        count++;
    }

    if (!hw->secure_boot_enabled && count < 18) {
        penalties[count].points_lost = 4;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "UEFI Secure Boot desativado na BIOS (alteravel apenas no menu da BIOS da placa-mae)");
        score -= 4;
        count++;
    }

    if (hw->ram_load_pct > 85 && count < 18) {
        penalties[count].points_lost = 5;
        snprintf(penalties[count].reason, sizeof(penalties[count].reason),
                 "Memoria RAM muito cheia (%u%% em uso agora)", hw->ram_load_pct);
        score -= 5;
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
    score_penalty_t penalties[20];
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
    ui_format_bytes(dev_cache_bytes + st->total_dev_bytes, dev_cache_str, sizeof(dev_cache_str));
    ui_format_bytes(st->total_cold_bytes, cold_str, sizeof(cold_str));

    char line_storage_buf[28][256];
    const char *box_lines[28];
    int n = 0;

    snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
             "Nota do seu PC            %s" ANSI_BOLD "%d / 100" ANSI_RESET "  " ANSI_DIM "(%s)" ANSI_RESET,
             score_col, score, status_label);
    box_lines[n] = line_storage_buf[n];
    n++;

    snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
             "Otimizacoes do Windows    " ANSI_GREEN ANSI_BOLD "%d/%d ativas" ANSI_RESET "  ·  " ANSI_YELLOW ANSI_BOLD "%d pendentes para aplicar" ANSI_RESET,
             tw->optimized_tweaks_count, tw->rule_count, tw->pending_tweaks_count);
    box_lines[n] = line_storage_buf[n];
    n++;

    snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
             "Lixo temporario sistema   " ANSI_BOLD "%s" ANSI_RESET "  ·  Caches Dev/Apps (NPM/Pip/Chrome): " ANSI_BOLD "%s" ANSI_RESET,
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

        for (int i = 0; i < penalty_count && n < 27; i++) {
            snprintf(line_storage_buf[n], sizeof(line_storage_buf[n]),
                     "  " ANSI_RED "-%2d pts" ANSI_RESET "  %s",
                     penalties[i].points_lost, penalties[i].reason);
            box_lines[n] = line_storage_buf[n];
            n++;
        }
    }

    ui_box(box_lines, n, "Resultado da Analise (Nada foi alterado ainda)");
}

/* Tela de Limpeza e Exclusao: Caches do sistema, Caches opcionais, Pastas node_modules/.venv e Arquivos grandes */
static void interactive_file_removal_screen(storage_report_t *st, bool already_scanned) {
    if (!already_scanned) {
        ui_clear();
        ui_banner();
        ui_step(1, 1, "Analisando disco, pastas de cache, node_modules e arquivos grandes (somente leitura)");
        storage_run_audit(st, 50, 30, NULL);
    }

    for (;;) {
        ui_clear();
        ui_banner();
        storage_print_removal_preview(st);

        printf("  " ANSI_GREEN "·" ANSI_RESET " " ANSI_DIM "Analise concluida. Escolha abaixo com as setas o que deseja apagar:" ANSI_RESET "\n");

        #define MAX_CLEAN_MENU_OPTS (MAX_CACHE_TARGETS + MAX_DEV_ARTIFACT_DIRS + MAX_COLD_FILES + 4)
        char labels_buf[MAX_CLEAN_MENU_OPTS][120];
        const char *labels[MAX_CLEAN_MENU_OPTS];
        int opt_type[MAX_CLEAN_MENU_OPTS]; /* 0=all_sys_cache, 1=single_cache, 2=dev_dir, 3=cold_file, 4=back */
        int opt_index[MAX_CLEAN_MENU_OPTS];
        int total_opts = 0;

        char cache_sz[32];
        ui_format_bytes(st->safe_cleanable_bytes, cache_sz, sizeof(cache_sz));

        snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                 "[LIMPAR CACHES DO SISTEMA] Apagar %%TEMP%%, Win\\Temp, CrashDumps e DirectX (%s)", cache_sz);
        labels[total_opts]    = labels_buf[total_opts];
        opt_type[total_opts]  = 0;
        opt_index[total_opts] = -1;
        total_opts++;

        /* Caches de ferramentas / navegadores (NPM, Pip, Chrome) */
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

        /* Pastas pesadas de desenvolvimento (node_modules, .venv) */
        for (int i = 0; i < st->dev_dir_count; i++) {
            const dev_artifact_t *d = &st->dev_dirs[i];
            char dsz[32];
            ui_format_bytes(d->size_bytes, dsz, sizeof(dsz));

            const char *p = d->path;
            char short_path[48];
            size_t plen = strlen(p);
            if (plen > 44) {
                snprintf(short_path, sizeof(short_path), "...%s", p + (plen - 41));
            } else {
                snprintf(short_path, sizeof(short_path), "%s", p);
            }

            if (d->deleted) {
                snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                         "[PASTA JA EXCLUIDA]        %s (%s)", short_path, dsz);
            } else {
                snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                         "[EXCLUIR PASTA PROJETO]    %-10s (%s) -> %s", dsz, d->kind, short_path);
            }
            labels[total_opts]    = labels_buf[total_opts];
            opt_type[total_opts]  = 2;
            opt_index[total_opts] = i;
            total_opts++;
        }

        /* Arquivos grandes esquecidos (>50 MB) */
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
                         "[ARQUIVO JA EXCLUIDO]      %s (%s)", short_path, fsz);
            } else {
                snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                         "[EXCLUIR ARQUIVO GRANDE]   %-10s (%3d dias) -> %s", fsz, cf->days_unused, short_path);
            }
            labels[total_opts]    = labels_buf[total_opts];
            opt_type[total_opts]  = 3;
            opt_index[total_opts] = i;
            total_opts++;
        }

        snprintf(labels_buf[total_opts], sizeof(labels_buf[total_opts]),
                 "Sair e voltar ao Menu Principal");
        labels[total_opts]    = labels_buf[total_opts];
        opt_type[total_opts]  = 4;
        opt_index[total_opts] = -1;
        total_opts++;

        int sel = ui_select("ESCOLHA O QUE DESEJA LIMPAR (PEDIRA SUA PERMISSAO ANTES)", labels, total_opts);

        if (opt_type[sel] == 4) {
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
            int d_idx = opt_index[sel];
            dev_artifact_t *d = &st->dev_dirs[d_idx];
            if (d->deleted) {
                ui_info("Esta pasta ja foi excluida.");
                ui_pause_enter("Continuar");
                continue;
            }
            char dsz[32];
            ui_format_bytes(d->size_bytes, dsz, sizeof(dsz));
            char q[320], detail[360];
            snprintf(q, sizeof(q), "Autoriza excluir a pasta de dependencias '%s'?", d->path);
            snprintf(detail, sizeof(detail),
                     "Apagara %u arquivos da pasta %s liberando %s (voce pode recriar depois com npm/pip install).",
                     d->file_count, d->kind, dsz);
            if (ui_confirm_arrows(q, detail)) {
                uint64_t freed_b = 0;
                uint32_t del_f = 0;
                printf("\n");
                if (storage_delete_directory_tree(d->path, &freed_b, &del_f)) {
                    char f_str[32];
                    ui_format_bytes(freed_b, f_str, sizeof(f_str));
                    ui_ok("Pasta excluida com sucesso: " ANSI_BOLD "%s" ANSI_RESET " (%s liberados em %u arquivos)",
                          d->path, f_str, del_f);
                    d->deleted = true;
                } else {
                    ui_fail("Nao foi possivel excluir '%s'.", d->path);
                }
                ui_pause_enter("Continuar");
            }
        } else if (opt_type[sel] == 3) {
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

/* Tela Interativa de Programas na Inicializacao (Boot) */
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

/* Tela Completa de Otimizacao do Windows (12 Regras + Selecao em Lote ou Individual + Flush DNS) */
static void interactive_optimize_screen(void) {
    for (;;) {
        ui_clear();
        ui_banner();

        tweaks_report_t current;
        int pending = tweaks_inspect_optimizations(&current);

        char opt_buf[MAX_TWEAK_RULES + 5][128];
        const char *opt_labels[MAX_TWEAK_RULES + 5];
        int opt_code[MAX_TWEAK_RULES + 5]; /* 0=apply_all, 1=single_rule, 2=flush_dns, 3=restore, 4=back */
        int opt_rule_idx[MAX_TWEAK_RULES + 5];
        int count = 0;

        if (pending == 0) {
            ui_ok(ANSI_BOLD "Tudo otimizado!" ANSI_RESET " Todas as %d regras de desempenho acima ja estao ativas no seu Windows.",
                  current.rule_count);
        } else {
            printf("  " ANSI_GREEN "·" ANSI_RESET " " ANSI_DIM "Status: %d/%d ja otimizadas · %d pendente(s) para aplicar. Escolha abaixo:" ANSI_RESET "\n",
                   current.optimized_tweaks_count, current.rule_count, pending);

            snprintf(opt_buf[count], sizeof(opt_buf[count]),
                     "[APLICAR TODOS OS PENDENTES] Autorizar e aplicar os %d ajuste(s) pendente(s) de uma vez",
                     pending);
            opt_labels[count]   = opt_buf[count];
            opt_code[count]     = 0;
            opt_rule_idx[count] = -1;
            count++;

            /* Tambem lista cada ajuste PENDENTE individualmente para o usuario poder escolher 1 por 1 */
            for (int i = 0; i < current.rule_count; i++) {
                const tweak_rule_t *r = &current.rules[i];
                if (r->is_optimized) continue;
                snprintf(opt_buf[count], sizeof(opt_buf[count]),
                         "[APLICAR INDIVIDUAL]         %s (%s)", r->title, r->target_state);
                opt_labels[count]   = opt_buf[count];
                opt_code[count]     = 1;
                opt_rule_idx[count] = i;
                count++;
            }
        }

        snprintf(opt_buf[count], sizeof(opt_buf[count]),
                 "[REDE & PING]                Limpar Cache de Rotas DNS agora (ipconfig /flushdns)");
        opt_labels[count]   = opt_buf[count];
        opt_code[count]     = 2;
        opt_rule_idx[count] = -1;
        count++;

        snprintf(opt_buf[count], sizeof(opt_buf[count]),
                 "Sair e voltar ao Menu Principal");
        opt_labels[count]   = opt_buf[count];
        opt_code[count]     = 4;
        opt_rule_idx[count] = -1;
        count++;

        snprintf(opt_buf[count], sizeof(opt_buf[count]),
                 "[RESTAURAR PADROES]          Voltar todas as configuracoes para o padrao de fabrica do Windows");
        opt_labels[count]   = opt_buf[count];
        opt_code[count]     = 3;
        opt_rule_idx[count] = -1;
        count++;

        int sel = ui_select("PAINEL DE OTIMIZACAO - ESCOLHA UMA ACAO", opt_labels, count);
        int action = opt_code[sel];

        if (action == 4) {
            break;
        } else if (action == 0) {
            printf("\n");
            int applied = tweaks_apply_pending_optimizations(&current);
            printf("\n");
            ui_ok(ANSI_BOLD "%d ajuste(s) pendente(s) aplicado(s) com sucesso!" ANSI_RESET, applied);
            ui_pause_enter("Ver painel atualizado");
        } else if (action == 1) {
            int r_idx = opt_rule_idx[sel];
            const tweak_rule_t *r = &current.rules[r_idx];
            char q[180];
            snprintf(q, sizeof(q), "Autoriza aplicar '%s' (%s)?", r->title, r->target_state);
            if (ui_confirm_arrows(q, r->explanation)) {
                printf("\n");
                tweaks_apply_single_rule(r->id);
                ui_pause_enter("Ver painel atualizado");
            }
        } else if (action == 2) {
            printf("\n");
            if (tweaks_flush_dns_cache()) {
                ui_ok("Cache de resolucao DNS limpo com sucesso (" ANSI_BOLD "ipconfig /flushdns" ANSI_RESET ")!");
            } else {
                ui_warn("Nao foi possivel limpar o cache DNS.");
            }
            ui_pause_enter("Continuar");
        } else if (action == 3) {
            if (ui_confirm_arrows("Tem certeza que deseja restaurar todas as 12 regras para o padrao original do Windows?",
                                  "Volta o plano para Equilibrado, Menus para 400ms, reativa GameDVR, Bing e apps em 2o plano.")) {
                printf("\n");
                tweaks_restore_windows_defaults();
                ui_pause_enter("Ver painel atualizado");
            }
        }
    }
}

static void print_help(void) {
    printf(ANSI_BOLD "Uso:" ANSI_RESET " windows-tweaker [comando] [opcoes]\n\n");
    printf(ANSI_BOLD "Como usar" ANSI_RESET "\n");
    printf("  windows-tweaker             abre o menu interativo controlado pelas setas (↑/↓) e Enter\n");
    printf("  windows-tweaker scan        executa a analise completa das 12 otimizacoes, disco e hardware\n");
    printf("  windows-tweaker limpar      analisa os caches, node_modules e arquivos grandes para limpeza\n");
    printf("  windows-tweaker otimizar    inspeciona ou aplica as 12 regras de desempenho do Windows\n");
    printf("  windows-tweaker startup     lista e gerencia os programas que iniciam com o Windows\n");
    printf("  windows-tweaker hardware    mostra processador, memoria RAM, placa-mae e BIOS\n\n");
    printf(ANSI_BOLD "Opcoes para uso via script" ANSI_RESET "\n");
    printf("  --dry-run                   apenas analisa sem apagar nem alterar nada\n");
    printf("  --apply, --yes              executa a limpeza de caches ou aplica as otimizacoes pendentes direto\n");
    printf("  --min-mb 50                 tamanho minimo em MB para listar arquivos grandes (padrao: 50)\n");
    printf("  --days 30                   dias sem uso para listar arquivos grandes esquecidos (padrao: 30)\n");
}

static int interactive_menu(void) {
    static const char *MAIN_LABELS[] = {
        "1. Diagnostico Completo do PC (Nota de 0 a 100 + 12 Regras Auditadas)",
        "2. Limpar Caches, Pastas de Projetos (node_modules) e Arquivos Pesados",
        "3. Otimizar Windows, Rede, RAM e Jogos (Painel de 12 Otimizacoes)",
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
                    "1. DIAGNOSTICO COMPLETO DO PC (12 REGRAS + DISCO + HARDWARE)",
                    "Vai analisar Processador, RAM, BIOS, medir os caches temporarios e verificar todas as 12 regras de Energia, Jogos, Mouse 1:1, Rede P2P, Apps em 2o plano, Bing e Interface.",
                    "Voce vera o motivo exato da sua nota de 0 a 100 e tera botoes diretos logo abaixo para aplicar apenas o que estiver pendente."
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

            ui_step(2, 3, "DISCO, HIBERFIL.SYS, CACHES E ARQUIVOS ESQUECIDOS (SOMENTE LEITURA)");
            storage_run_audit(&st, 50, 30, NULL);
            storage_print_report(&st);

            ui_step(3, 3, "AUDITORIA DAS 12 OTIMIZACOES DO WINDOWS, REDE, RAM E JOGOS");
            tweaks_run_audit(&tw);
            tweaks_print_report(&tw);

            render_summary_box(&hw, &st, &tw);

            char post_buf[6][128];
            const char *post_labels[6];
            int post_code[6]; /* 0=apply_all_pending, 1=optimize_screen, 2=clean_screen, 3=startup_screen, 4=back */
            int pcount = 0;

            if (tw.pending_tweaks_count > 0) {
                snprintf(post_buf[pcount], sizeof(post_buf[pcount]),
                         "Aplicar todos os %d ajuste(s) pendente(s) do Windows agora",
                         tw.pending_tweaks_count);
                post_labels[pcount] = post_buf[pcount];
                post_code[pcount] = 0;
                pcount++;

                snprintf(post_buf[pcount], sizeof(post_buf[pcount]),
                         "Abrir Painel de 12 Otimizacoes (escolher ajustes 1 por 1)");
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
                char q[160];
                snprintf(q, sizeof(q), "Autoriza aplicar todos os %d ajuste(s) pendente(s) encontrados na analise?",
                         tw.pending_tweaks_count);
                if (ui_confirm_arrows(q, "Os itens que ja estao otimizados serao mantidos intactos.")) {
                    printf("\n");
                    int applied = tweaks_apply_pending_optimizations(&tw);
                    printf("\n");
                    ui_ok(ANSI_BOLD "%d ajuste(s) aplicado(s) com sucesso!" ANSI_RESET, applied);
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
                    "2. LIMPAR CACHES, NODE_MODULES E EXCLUIR ARQUIVOS PESADOS",
                    "Vai escanear as pastas de lixo temporario do Windows (%TEMP%, Win\\Temp, CrashDumps, DirectX), caches de desenvolvimento (NPM/Pip), pastas node_modules/.venv e arquivos grandes (>50 MB) sem uso ha +30 dias.",
                    "Mostrara a lista completa do que foi encontrado. Nada sera apagado durante a analise: voce escolhera na tela seguinte exatamente o que autoriza remover."
                )) {
                continue;
            }
            storage_report_t st;
            interactive_file_removal_screen(&st, false);
        } else if (choice == 2) {
            if (!ui_open_tool_screen(
                    "3. PAINEL DE 12 OTIMIZACOES DO WINDOWS, REDE, RAM E JOGOS",
                    "Vai analisar 12 configuracoes reais em 4 categorias: Energia & Hibernacao, Jogos & Mouse 1:1 Raw Input, Rede P2P & DNS, e Economia de RAM (Apps da Store em 2o plano, Bing no Menu Iniciar e Bloatware).",
                    "Os itens que ja estiverem otimizados aparecerao como 'JA OTIMIZADO'. Voce podera aplicar todos os pendentes de uma vez ou escolher 1 por 1 pelas setas."
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

        ui_step(2, 3, "DISCO, HIBERFIL.SYS, CACHES E ARQUIVOS ESQUECIDOS");
        storage_run_audit(&st, min_cold_mb, min_cold_days, custom_dir);
        storage_print_report(&st);

        ui_step(3, 3, "AUDITORIA DAS 12 OTIMIZACOES DO WINDOWS");
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
            ui_ok("Todas as 12 otimizacoes ja estao ativas no seu PC (0 pendentes).");
        } else if (dry) {
            printf("\n");
            ui_info("(dry-run: %d de %d ajuste(s) pendente(s). Rode 'windows-tweaker otimizar --apply' para aplicar)",
                    pending, current.rule_count);
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
