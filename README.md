# Windows Tweaker (`windows-tweaker`)

![C11](https://img.shields.io/badge/Language-C11-00599C?style=for-the-badge&logo=c&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Win32%20x86__64-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![Zero Dependencies](https://img.shields.io/badge/Dependencies-Zero%20(Native%20Win32%20API)-2ea44f?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT-purple?style=for-the-badge)

**Windows Tweaker** is a fast, zero-dependency, pure **C11** command-line and interactive TUI system diagnostic, storage auditing, and operating system tuning engine for Windows 10/11.

Built directly on top of low-level **Win32 APIs** (`windows.h`, Registry APIs, `FindFirstFileA`, `GlobalMemoryStatusEx`) and hardware **x86_64 CPU intrinsics (`__cpuid`)**, it delivers an interactive terminal interface featuring a **TrueColor RGB gradient emblem**, arrow-key menu navigation, detailed explanations for every diagnostic finding, and an exact file removal inspector.

---

## Key Engineering Highlights

* **TrueColor RGB Gradient & Interactive Terminal UI (`src/ui.c`)**
  * Enables Windows Virtual Terminal Sequences (`ENABLE_VIRTUAL_TERMINAL_PROCESSING`) and `CP_UTF8` codepage dynamically while restoring console state on exit.
  * TrueColor 24-bit ANSI gradient renderer (`\033[38;2;R;G;Bm`) for the main emblem.
  * Arrow-key (`UP`/`DOWN`, `1-6`, `ENTER`) interactive menu hub with zero external TUI dependencies (`_getch()` + in-place cursor redraws).
* **High-Speed Storage & Removal Preview Inspector (`src/storage.c`)**
  * Recursive Win32 directory walker with symlink/reparse-point cycle protection (`FILE_ATTRIBUTE_REPARSE_POINT`).
  * Audits known cache directories (`%TEMP%`, `Win\Temp`, `Prefetch`, `SoftwareDistribution`, `CrashDumps`, `D3DSCache`, `npm-cache`, `pip`, Chrome cache) and explains what each directory stores.
  * **Exact File Removal Inspector (`inspect`):** Lists the target directories, sample files, byte sizes, and cold files (`>= 50 MB` unused for `30+ days`) before any deletion occurs.
* **Windows OS, SSD & Gaming Tweaks (`src/tweaks.c`)**
  * Enumerates boot startup items across `HKCU\...\Run` and `HKLM\...\Run`.
  * Detects active CPU Power Scheme, **SSD NTFS TRIM** status (`DisableDeleteNotify`), **Xbox GameDVR** background capture overhead, **MMCSS** CPU reservation, and desktop **MenuShowDelay** latency.
  * Explains the **Impact** and **Recommendation** of every setting.
* **Hardware & BIOS/UEFI Telemetry (`src/hardware.c`)**
  * Executes raw `__cpuid` and `__cpuidex` silicon instructions to extract processor brand strings, vendor IDs, and **AVX2 256-bit SIMD** support.
  * Queries **BIOS Virtualization (VT-x / AMD-V)** firmware status, **UEFI Secure Boot**, **Hardware-Accelerated GPU Scheduling (HAGS)**, and kernel uptime.
* **Transparent Health Score Breakdown (`src/main.c`)**
  * Explains every single point deducted from the 0-100 system health score (`[-8 pts]`, `[-7 pts]`, etc.) along with how to fix it.

---

## Terminal Preview

```text
  ██╗    ██╗██╗███╗   ██╗    ████████╗██╗    ██╗███████╗ █████╗ ██╗  ██╗███████╗██████╗
  ██║    ██║██║████╗  ██║    ╚══██╔══╝██║    ██║██╔════╝██╔══██╗██║ ██╔╝██╔════╝██╔══██╗
  ██║ █╗ ██║██║██╔██╗ ██║       ██║   ██║ █╗ ██║█████╗  ███████║█████╔╝ █████╗  ██████╔╝
  ██║███╗██║██║██║╚██╗██║       ██║   ██║███╗██║██╔══╝  ██╔══██║██╔═██╗ ██╔══╝  ██╔══██╗
  ╚███╔███╔╝██║██║ ╚████║       ██║   ╚███╔███╔╝███████╗██║  ██║██║  ██╗███████╗██║  ██║
   ╚══╝╚══╝ ╚═╝╚═╝  ╚═══╝       ╚═╝    ╚══╝╚══╝ ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝
  ╭────────────────────────────────────────────────────────────────────────────────────╮
  │   CORE ENGINE   Sistema de Diagnostico, Limpeza de Disco e Otimizacao Windows      │
  │  Arquitetura C11 Nativa (Win32 API + __cpuid)  |  Versao: 1.1.0     |  Modo Seguro │
  ╰────────────────────────────────────────────────────────────────────────────────────╯

  :: MENU PRINCIPAL - SELECIONE UMA OPERACAO (Use Setas CIMA/BAIXO, Numeros 1-6 ou ENTER)
  ────────────────────────────────────────────────────────────────────────────────────
  >>  [1] Diagnostico Completo do Sistema (Score Explicado + Tudo)
        ╰─ Analisa Hardware, BIOS, Disco e Registro explicando o motivo de cada ponto perdido
      [2] Inspecionar Arquivos para Remover (Ver Lista Exata e Limpar)
           Mostra exatamente quais pastas, amostras de arquivos e tamanhos podem ser removidos
      [3] Inspecionar e Aplicar Otimizacoes do Windows (Registro e Jogos)
           Explica o que cada chave do Registro faz (MenuShowDelay, GameDVR, TRIM) e permite aplicar
      [4] Gerenciador de Inicializacao (Programas que Pesam no Boot)
           Lista todos os programas em HKCU\Run e HKLM\Run que atrasam a inicializacao do PC
      [5] Telemetria de Hardware e BIOS (CPU, RAM, Virtualizacao e HAGS)
           Le registradores __cpuid do processador, carga de RAM, Secure Boot e virtualizacao na BIOS
      [6] Sair do Windows Tweaker
           Encerra o aplicativo e restaura o terminal
```

---

## Project Architecture

```text
windows-tweaker/
├── include/
│   ├── ui.h          # TrueColor RGB gradients, interactive arrow-key menu & cards
│   ├── storage.h     # Disk capacity, cache scanner & file removal preview structs
│   ├── tweaks.h      # Registry inspection, power scheme, SSD TRIM & gaming tweaks
│   └── hardware.h    # __cpuid CPU probe, RAM telemetry & BIOS/UEFI state
├── src/
│   ├── main.c        # Interactive TUI Hub, CLI router & explained Health Score engine
│   ├── ui.c          # Win32 console initialization & zero-emoji terminal renderer
│   ├── storage.c     # Recursive Win32 FindFirstFileA traversal & file inspector
│   ├── tweaks.c      # RegOpenKeyExA / RegEnumValueA / RegSetValueExA engine
│   └── hardware.c    # Silicon intrinsics (__cpuid) & kernel telemetry queries
├── tests/
│   └── test_suite.c  # Automated unit test suite for formatting & system probes
├── .github/workflows/
│   └── ci.yml        # GitHub Actions CI compiling with MSVC /W4 & running tests
├── build.bat         # Zero-config MSVC build script (auto-detects vcvars64.bat)
└── Makefile          # GCC / MinGW-w64 / Clang compatible Makefile
```

---

## Building from Source

### Option 1: MSVC (`cl.exe` / Visual Studio Build Tools)
```bat
.\build.bat
```

### Option 2: MinGW-w64 / GCC (`Makefile`)
```bash
mingw32-make
```

---

## Usage

```bash
# 1. Open the Interactive Main Interface (arrow-key navigation)
.\bin\windows-tweaker.exe

# 2. Run complete diagnostic audit directly with explained score deductions
.\bin\windows-tweaker.exe scan

# 3. Inspect exact folders, sample files, and cold files that can be removed
.\bin\windows-tweaker.exe inspect

# 4. List all Windows Registry boot startup applications (HKCU & HKLM)
.\bin\windows-tweaker.exe startup

# 5. Preview or execute temporary cache cleanup
.\bin\windows-tweaker.exe clean --dry-run
.\bin\windows-tweaker.exe clean --apply

# 6. Preview or apply safe gaming & UI responsiveness tweaks
.\bin\windows-tweaker.exe optimize --dry-run
.\bin\windows-tweaker.exe optimize --apply
```
