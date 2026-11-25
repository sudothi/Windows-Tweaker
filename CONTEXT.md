# WINDOWS TWEAKER v2.0 - PROJECT CONTEXT & HANDOFF GUIDE

Este documento registra o contexto completo, regras visuais e arquitetura da versao **v2.0.0** do **Windows Tweaker** (`windows-tweaker`).

---

## 1. Regras Visuais e de UX (Obrigatorias)

1. **Zero Emojis e Zero Blocos de Fundo Colorido:**
   - Nao utilize emojis em lugar nenhum do codigo ou da documentacao.
   - Use o banner do trevo de 3 folhas (`24 colunas x 9 linhas`) em meio-bloco (`▄`, `▀`, `█`) com degrade monocromatico em 3 tons de **Verde Esmeralda** (`#059669` -> `#10B981` -> `#6EE7B7`).
2. **Navegacao 100% por Setas (`↑`/`↓`) e `ENTER` com Cursor Fixo (`SetConsoleCursorPosition`):**
   - `ui_select` exibe estritamente **1 linha curta por item** e trava a coordenada `(X, Y)` do console via API do Windows (`GetConsoleScreenBufferInfo` / `SetConsoleCursorPosition`) para nunca duplicar linhas ao subir ou descer.
3. **Fluxo Seguro (Analise Primeiro -> Permissao Depois):**
   - Ao abrir qualquer ferramenta no Menu Principal, `ui_open_tool_screen` explica o que sera analisado, exibe a caixa `Aviso de Seguranca` (*"Inicialmente e feita apenas a analise do seu PC. Nenhum arquivo e apagado e nenhuma configuracao e alterada sem a sua permissao"*) e oferece `Prosseguir e iniciar analise` ou `Sair e voltar ao Menu Principal`.

---

## 2. O que o Windows Tweaker v2.0 Faz

1. **Catalogo de 12 Regras de Otimizacao Reais (`include/tweaks.h` / `src/tweaks.c`):**
   - **Energia & CPU:** (1) Plano Alto Desempenho (`powercfg`), (2) Compactacao do `C:\hiberfil.sys` (`powercfg /h /type reduced`).
   - **Jogos & Input:** (3) Desativar Captura Xbox GameDVR, (4) Ativar Windows Game Mode, (5) Mouse 1:1 Raw Input (`MouseSpeed=0` + `SystemParametersInfoA(SPI_SETMOUSE)`), (6) Prioridade Maxima de GPU/CPU (`MMCSS Games`).
   - **Rede & Ping:** (7) Bloquear Upload P2P do Windows Update (`DeliveryOptimization=0`), plus acao direta de `ipconfig /flushdns`.
   - **RAM, Privacidade & UI:** (8) Bloquear Apps da Microsoft Store em 2o plano (`GlobalUserDisabled=1`), (9) Desativar Busca Web do Bing no Menu Iniciar (`DisableSearchBoxSuggestions=1`), (10) Bloquear Bloatware automatico e ID de Anuncios, (11) Menus Instantaneos (`20ms`) e Explorer sem anuncios, (12) Fechar programas travados em `2s` (`HungAppTimeout=2000`, `WaitToKillAppTimeout=2000`, `AutoEndTasks=1`).
   - Cada regra e auditada individualmente (`JA OTIMIZADO` vs `PENDENTE`) e pode ser aplicada em lote ou 1 por 1 pelas setas.
2. **Gerenciador de Boot Sincronizado com o Gerenciador de Tarefas:**
   - Le e grava na chave binaria `StartupApproved\Run` (`0x02` = Habilitado, `0x03` + `FILETIME` = Desabilitado).
3. **Motor de Armazenamento (`include/storage.h` / `src/storage.c`):**
   - Mede `C:\hiberfil.sys` e `C:\pagefile.sys`.
   - Limpa caches do sistema, caches opcionais (`NPM`, `Pip`, `Chrome`), pastas `node_modules`/`.venv` pesadas (>25 MB) e arquivos grandes esquecidos (>50 MB sem uso ha +30 dias) com verificacao no disco apos `DeleteFileA`.

---

## 3. Como Compilar e Testar

```powershell
git clone https://github.com/sudothi/windows-tweaker.git
cd windows-tweaker
.\build.bat
.\bin\test_suite.exe
.\bin\windows-tweaker.exe
```
