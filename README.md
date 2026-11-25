# Windows Tweaker (`windows-tweaker` v2.0)

Assistente interativo de terminal em **C11 puro** (zero dependencias externas) para diagnostico, limpeza profunda de disco e otimizacao do Windows 10/11. Construido direto sobre as APIs nativas do sistema (`Win32 API`, Registro, `FindFirstFileA`, `GlobalMemoryStatusEx`, `SystemParametersInfoA`) e instrucoes de hardware do processador (`__cpuid`).

```text
        ▄████▄▄████▄
       ▀████████████▀          Windows Tweaker
        ▀██████████▀           o seu PC, limpo e otimizado
  ▄██████████████████████▄
  ████████████████████████     diagnostico, limpeza de disco e
   ▀████████████████████▀      ajustes do Windows, direto em C
    ████████▀██▀████████
     ▀▀█▀▀  ▄██   ▀▀█▀▀
           ███
```

## Comandos

```text
windows-tweaker                     abre o menu interativo controlado pelas setas (↑/↓) e Enter
windows-tweaker scan                executa a analise completa das 12 otimizacoes, disco e hardware
windows-tweaker limpar              analisa caches, pastas node_modules/.venv e arquivos grandes para limpeza
windows-tweaker otimizar            inspeciona ou aplica as 12 regras de desempenho do Windows
windows-tweaker startup             lista e gerencia (Habilita/Desabilita) os programas do Boot
windows-tweaker hardware            mostra processador (__cpuid), memoria RAM, placa-mae e BIOS
```

Opcoes via CLI: `--dry-run` (apenas analisa), `--apply` / `--yes` (aplica todas as regras pendentes ou limpa caches direto), `--min-mb 50`, `--days 30`.

---

## As 12 Regras de Otimizacao Reais (`src/tweaks.c`)

O motor verifica o estado atual de cada regra no Registro/Sistema e separa automaticamente o que ja esta **`JA OTIMIZADO`** do que esta **`PENDENTE`** (permitindo aplicar todas as pendentes de uma vez ou escolher 1 por 1 pelas setas):

### 1. Energia & CPU
1. **Plano de Energia Alto Desempenho (`powercfg`):** Evita quedas de clock (*core parking/throttling*) do processador.
2. **Compactacao do `C:\hiberfil.sys` (`powercfg /h /type reduced`):** Reduz pela metade o arquivo oculto de hibernacao no SSD (liberando ~3 GB a 6 GB) sem perder o Inicio Rapido.

### 2. Jogos & Input Lag
3. **Desativar Gravacao Oculta Xbox GameDVR (`AppCaptureEnabled = 0`):** Libera o encoder da placa de video em segundo plano.
4. **Modo de Jogo Automatico (`AutoGameModeEnabled = 1`):** Prioriza recursos para o jogo aberto e suspende indexacao em segundo plano.
5. **Precisao 1:1 do Mouse — Raw Input (`MouseSpeed = 0` + `SPI_SETMOUSE`):** Desativa a aceleracao artificial do ponteiro em tempo real para mira linear 1:1.
6. **Prioridade Maxima de Escalonamento para Jogos (`MMCSS Games`):** Configura `GPU Priority = 8`, `Priority = 6` e `Scheduling Category = High`.

### 3. Rede & Ping
7. **Bloquear Upload P2P Oculto do Windows Update (`DeliveryOptimization = 0`):** Impede o Windows de usar seu upload de internet para enviar atualizacoes a outros PCs durante partidas.
8. **Limpeza de Cache de Rotas DNS (`ipconfig /flushdns`):** Acao direta no painel para limpar rotas DNS travadas.

### 4. Memoria RAM, Privacidade & Interface
9. **Bloquear Apps da Microsoft Store em 2o Plano (`GlobalUserDisabled = 1`):** Impede que dezenas de apps UWP (Clima, Noticias, Fotos, Xbox, Cortana) fiquem abertos escondidos consumindo RAM.
10. **Desativar Pesquisa Web do Bing no Menu Iniciar (`DisableSearchBoxSuggestions = 1`):** Deixa a busca do Menu Iniciar instantanea e 100% local.
11. **Bloquear Bloatware Automatico e ID de Anuncios (`SilentInstalledAppsEnabled = 0`):** Impede o Windows de baixar jogos patrocinados sozinho.
12. **Menus Instantaneos (20ms), Explorer sem Anuncios e Fechamento Rapido de Apps Travados (2s):** Reduz `MenuShowDelay` para `20ms` e `WaitToKillAppTimeout` de `20000ms` (20s) para `2000ms` (2s) com `AutoEndTasks = 1`.

---

## Gerenciador de Inicializacao Sincronizado com o Task Manager

- Cruza as chaves `HKCU\...\Run` e `HKLM\...\Run` com a chave binaria `StartupApproved\Run` e `StartupApproved\Run32`.
- Mostra com 100% de fidelidade quais programas estao **`[HABILITADO]`** e quais ja foram **`[DESABILITADO]`** no Gerenciador de Tarefas, permitindo Habilitar ou Desabilitar qualquer item pelas setas e `ENTER`.

---

## Motor de Armazenamento e Exclusao Verificada (`src/storage.c`)

- Mede o tamanho real dos arquivos ocultos `C:\hiberfil.sys` e `C:\pagefile.sys`.
- Mapeia caches do sistema (`%TEMP%`, `Win\Temp`, `SoftwareDistribution\Download`, `CrashDumps`, `D3DSCache`) e caches opcionais (`NPM`, `Pip`, `Chrome`).
- **Cacador de Pastas `node_modules` / `.venv`:** Localiza pastas pesadas de dependencias (>25 MB) e permite exclui-las individualmente.
- **Arquivos Grandes Esquecidos (`>= 50 MB` sem uso ha `+30 dias`):** Permite selecionar e excluir permanentemente da pasta de origem, removendo travas `READONLY` e confirmando no disco (`GetFileAttributesA`) que o arquivo deixou de existir.

---

## Compilar e Testar

```bat
.\build.bat
.\bin\test_suite.exe
.\bin\windows-tweaker.exe
```
