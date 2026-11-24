# WINDOWS TWEAKER - PROJECT CONTEXT & HANDOFF GUIDE

Este documento registra o contexto, regras visuais, arquitetura tecnica e proximos passos do **Windows Tweaker** (`windows-tweaker`). Use este arquivo ao clonar o repositorio em outro PC para continuar o desenvolvimento.

---

## 1. Estetica e Sistema Visual (Padrao `sudothi/Turtle`)

A interface de terminal do `windows-tweaker` segue **exatamente a mesma arquitetura visual do CLI do projeto `sudothi/Turtle` (`apps/cli/src/ui.mjs` e `apps/cli/bin/turtle-setup.mjs`)**:

1. **Zero Emojis e Zero Blocos de Fundo Colorido:**
   - Nao utilize emojis e nao utilize blocos com background colorido (`\x1b[48;...m`).
   - Use apenas as cores de frente limpas do terminal: `bold` (`\x1b[1m`), `dim` (`\x1b[2m`), `red` (`\x1b[31m`), `green` (`\x1b[32m`), `yellow` (`\x1b[33m`), `cyan` (`\x1b[36m`) e `magenta` (`\x1b[35m`).
2. **Banner Half-Block em Degrade Diagonal (`ui_banner`):**
   - Arte em meio-bloco (`▄`, `▀`, `█`) de 35 colunas a esquerda com degrade diagonal TrueColor (`FIRE`: vermelho -> laranja -> amarelo) aplicado por codepoint UTF-8 completo, e o texto alinhado a direita (`Windows Tweaker / o seu PC, limpo e otimizado`).
3. **Primitivas de Saida (`include/ui.h` / `src/ui.c`):**
   - `ui_step(n, total, title)` -> `◆ 1/3 Titulo`
   - `ui_ok(...)`   -> `  ✓ texto` (verde)
   - `ui_warn(...)` -> `  ! texto` (amarelo)
   - `ui_fail(...)` -> `  ✗ texto` (vermelho)
   - `ui_info(...)` -> `  · texto` (dim)
   - `ui_line(...)` -> `  │ texto` (linha vertical suave para explicacoes de "Por que importa" e "Como resolver")
   - `ui_box(lines, count, title)` -> Caixa `╭─ Titulo ───╮` com largura calculada dinamicamente pelo comprimento visual UTF-8 das linhas.
4. **Menu Interativo Simples e Direto:**
   - Ao rodar `windows-tweaker.exe` sem argumentos, exibe o menu numerado (`1)` a `6)`, `0) Sair`, prompt `Numero:`), exatamente como em `turtle-setup.mjs`.

---

## 2. Motor de Exclusao Real de Arquivos (`src/storage.c`)

- **`storage_delete_single_file`:**
  - Antes de chamar `DeleteFileA(path)`, limpa atributos `FILE_ATTRIBUTE_READONLY`, `FILE_ATTRIBUTE_HIDDEN` e `FILE_ATTRIBUTE_SYSTEM` via `SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL)`.
  - Apos o `DeleteFileA`, **verifica no disco** com `GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES` que o arquivo realmente sumiu da pasta de origem.
  - Se o arquivo estiver travado por um programa aberto (`ERROR_SHARING_VIOLATION`) ou exigir Administrador (`ERROR_ACCESS_DENIED`), informa o motivo exato ao usuario.
- **Tela Interativa de Exclusao (`windows-tweaker limpar` ou opcao `2` do menu):**
  - Mostra as pastas de cache temporario (com amostras reais dos arquivos la dentro) e a lista numerada (`1)`, `2)`...) dos arquivos grandes sem uso recente nas pastas de origem (`Downloads`, `Documents`, `Videos`, `Desktop`).
  - O usuario pode digitar:
    - `c` -> limpa todas as pastas de cache temporario (`%TEMP%`, `Win\Temp`, `SoftwareDistribution\Download`, `CrashDumps`, `D3DSCache`).
    - `1` ou `1,3` -> exclui permanentemente o(s) arquivo(s) grande(s) escolhido(s) direto da pasta de origem.
    - `t` -> limpa todos os caches e todos os arquivos grandes listados.

---

## 3. Como Compilar e Testar no Outro PC

```powershell
git clone https://github.com/sudothi/windows-tweaker.git
cd windows-tweaker
.\build.bat
.\bin\test_suite.exe
.\bin\windows-tweaker.exe
```
