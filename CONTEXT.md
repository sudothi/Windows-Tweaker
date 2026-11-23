# WINDOWS TWEAKER - PROJECT CONTEXT & HANDOFF GUIDE

Este documento registra todo o contexto, decisoes de produto, regras visuais, arquitetura tecnica e proximos passos acordados durante a concepcao do **Windows Tweaker** (`windows-tweaker`). Use este arquivo ao clonar o repositorio em outra maquina (ou ao iniciar uma nova sessao com um assistente de IA) para retomar o desenvolvimento sem perder nenhum detalhe.

---

## 1. Objetivo do Projeto

Construir uma ferramenta de linha de comando (CLI) e interface interativa de terminal (TUI) em **C11 puro** para diagnostico, auditoria de armazenamento, limpeza inteligente de caches e otimizacao do sistema operacional Windows 10/11.

O foco principal do projeto e ser uma **peca de destaque de portfolio para vagas de C / Sistemas / Baixo Nivel**, demonstrando:
1. Uso direto das **APIs nativas do Windows (`Win32 API`)** sem depender de bibliotecas externas pesadas.
2. Uso de instrucoes diretas de processador (**`__cpuid` / `__cpuidex`**) para telemetria de silicio e verificacao de BIOS/UEFI.
3. Seguranca de engenharia: operacoes destrutivas ou que alteram estado (`clean`, `optimize`) possuem **modo de simulacao (`--dry-run`)** e inspecao previa transparente antes de modificar o disco ou o Registro.
4. Utilidade real no dia a dia para identificar gargalos de performance, programas que atrasam o boot e arquivos gigantes esquecidos (*cold files*).

---

## 2. Regras Estritas de Design e UX (NAO VIOLAR)

1. **ZERO EMOJIS:**
   - **Nunca utilize emojis** em nenhum arquivo de codigo (`.c`, `.h`), nos testes ou na documentacao (`README.md`, `CONTEXT.md`).
   - Utilize apenas caracteres ASCII/Unicode geometricos limpos (`╭─╮`, `│`, `╰─╯`, `├─`, `>>`, `::`, `█`, `░`) e badges de texto com fundo colorido (`[ OK ]`, `[ ATENCAO ]`, `[ CRITICO ]`, `[ INFO ]`, `[ AJUSTE ]`).
2. **Estetica de Terminal (Inspirada no Antigravity CLI):**
   - **Emblema Principal em Degrade RGB TrueColor:** O cabecalho renderiza o logo ASCII `WIN TWEAKER` com interpolacao de cores 24-bit ANSI (`\033[38;2;R;G;Bm`) do Ciano Eletrico ao Violeta/Magenta.
   - **Main Interface Interativa:** Ao executar `windows-tweaker.exe` sem argumentos (ou `windows-tweaker menu`), o programa abre um hub interativo navegavel pelas setas `CIMA`/`BAIXO` (ou teclas numericas `1-6`) e `ENTER`, usando `_getch()` e redesenho in-place sem piscar a tela.
   - **Spinners e Barras de Progresso:** Animacoes suaves em Braille (`⠋ ⠙ ⠹ ⠸`) durante varreduras de disco e leituras de Registro.
3. **Transparencia e Explicabilidade Total:**
   - Nunca diga apenas que o sistema esta "mal otimizado": todo diagnostico deve exibir explicitamente o **`Impacto`** (por que aquilo prejudica o PC/FPS/Boot) e a **`Recomendacao`** (como corrigir).
   - O **Health Score (0 a 100)** deve sempre listar o detalhamento exato de cada ponto descontado (`[-8 pts]`, `[-7 pts]`, `[-6 pts]`) e como recuperar esses pontos.
   - Antes de remover arquivos, o usuario deve ter a opcao de **inspecionar exatamente quais pastas e amostras de arquivos serao removidos** (`inspect` / botao no menu).

---

## 3. Arquitetura Atual do Codigo (`v1.1.0`)

```text
windows-tweaker/
├── include/
│   ├── ui.h          # Engine visual ANSI TrueColor, spinners, barras, cards e menu interativo
│   ├── storage.h     # Estruturas de varredura de disco, caches e arquivos frios (cold files)
│   ├── tweaks.h      # Auditoria do Registro (Run, PowerSchemes, TRIM, GameDVR, MenuShowDelay)
│   └── hardware.h    # Telemetria de CPU (__cpuid), RAM (GlobalMemoryStatusEx) e BIOS/UEFI
├── src/
│   ├── main.c        # Hub interativo TUI, roteador de subcomandos CLI e calculo explicado do Score
│   ├── ui.c          # Inicializacao UTF-8/VT100 no console Windows e componentes visuais
│   ├── storage.c     # Crawler recursivo Win32 (FindFirstFileA), inspetor de remocao e limpeza
│   ├── tweaks.c      # Leitura/Escrita segura no Registro do Windows (RegOpenKeyExA / RegSetValueExA)
│   └── hardware.c    # Instrucoes __cpuid, deteccao de virtualizacao na BIOS, Secure Boot e HAGS
├── tests/
│   └── test_suite.c  # Suite de testes unitarios automatizados (10 assercoes)
├── .github/workflows/
│   └── ci.yml        # Pipeline GitHub Actions (compila com MSVC /W4 /O2 e roda os testes)
├── build.bat         # Script de build para Windows MSVC (localiza vcvars64.bat automaticamente)
├── Makefile          # Script de build alternativo para GCC / MinGW-w64 / Clang
├── README.md         # Documentacao tecnica do repositorio (sem emojis)
└── CONTEXT.md        # Este documento de contexto e continuidade
```

---

## 4. Como Compilar e Rodar no Outro PC

### Pre-requisitos no Windows
- **Visual Studio Build Tools / Visual Studio Community** (com workload *Desktop development with C++*) **OU** **MinGW-w64 (`gcc`)**.
- **Git** instalado.

### Passo a Passo
```powershell
# 1. Clonar o repositorio
git clone https://github.com/sudothi/windows-tweaker.git
cd windows-tweaker

# 2. Compilar (com MSVC)
.\build.bat

# (Alternativa com MinGW/GCC)
mingw32-make

# 3. Rodar a suite de testes unitarios
.\bin\test_suite.exe

# 4. Abrir a Main Interface interativa
.\bin\windows-tweaker.exe
```

---

## 5. Roadmap de Evolucao (Proximas Features para Implementar)

Quando retomar o projeto no outro PC, estas sao as melhores evolucoes para tornar o projeto ainda maior e mais impressionante:

1. **Gerenciador Interativo de Startup (Ativar/Desativar pelo Terminal):**
   - Permitir selecionar um dos programas listados em `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` com as setas do teclado e desativa-lo (movendo para uma subchave de backup `RunDisabled` com possibilidade de restaurar a qualquer momento).
2. **Detector Ultrarrapido de Arquivos Duplicados (Fast Partial Hashing):**
   - Agrupar arquivos grandes pelo tamanho exato em bytes e calcular um hash rapido (ex: FNV-1a ou SHA-256) apenas dos primeiros e ultimos 64 KB para encontrar instaladores, ISOs e videos duplicados instantaneamente.
3. **Cacador de Pastas de Desenvolvimento Esquecidas (`node_modules`, `target`, `.venv`, `build`):**
   - Um modulo dedicado para desenvolvedores que varre pastas de projetos antigos e encontra `node_modules` ou pastas de build cujo projeto nao e modificado ha mais de 60 dias.
4. **Ativador Direto de Plano de Energia (High / Ultimate Performance):**
   - Integrar chamada nativa a API `PowerSetActiveScheme` (`powrprof.lib`) para permitir alternar entre *Equilibrado* e *Alto Desempenho* diretamente por um botao na interface.
5. **Exportacao de Relatorio de Auditoria (`.txt` / `.json`):**
   - Adicionar flag `--export relatorio.json` para salvar o diagnostico completo de hardware, BIOS, disco e sistema.
