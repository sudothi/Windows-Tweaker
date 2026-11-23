# ⚡ Windows Tweaker (`windows-tweaker`)

![C11](https://img.shields.io/badge/Language-C11-00599C?style=for-the-badge&logo=c&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Win32%20x86__64-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![Zero Dependencies](https://img.shields.io/badge/Dependencies-Zero%20(Native%20Win32%20API)-2ea44f?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT-purple?style=for-the-badge)

**Windows Tweaker** is a fast, zero-dependency, pure **C11** command-line diagnostic, storage auditing, and operating system tuning engine for Windows 10/11.

Built directly on top of low-level **Win32 APIs** (`windows.h`, Registry APIs, `FindFirstFile`, `GlobalMemoryStatusEx`) and hardware **x86_64 CPU intrinsics (`__cpuid`)**, it delivers a modern terminal experience inspired by modern developer CLIs — featuring animated Braille spinners, UTF-8 box cards, color-coded health badges, and safe `--dry-run` execution by default.

---

## ✨ Key Engineering Highlights

* **🎨 Custom ANSI & UTF-8 Terminal UI Engine (`src/ui.c`)**
  * Enables Windows Virtual Terminal Sequences (`ENABLE_VIRTUAL_TERMINAL_PROCESSING`) and `CP_UTF8` codepage dynamically while restoring console state on exit.
  * Smooth inline redraws (`\033[2K\r`), Braille activity spinners (`⠋ ⠙ ⠹ ⠸`), dynamic usage bars, and rounded Unicode data cards.
* **💾 High-Speed Storage & Cold-File Scanner (`src/storage.c`)**
  * Recursive Win32 directory walker with symlink/reparse-point cycle protection (`FILE_ATTRIBUTE_REPARSE_POINT`).
  * Audits known cache black holes (`%TEMP%`, `Win\Temp`, `Prefetch`, `SoftwareDistribution`, `CrashDumps`, `D3DSCache`, `npm-cache`, `pip`, Chrome cache).
  * **Cold Data Hunter:** Uses 64-bit `FILETIME` arithmetic to identify forgotten oversized files (`>= 50 MB` unused for `30+ days`).
* **⚙️ Windows OS, SSD & Gaming Tweaks (`src/tweaks.c`)**
  * Enumerates boot startup items across `HKCU\...\Run` and `HKLM\...\Run`.
  * Detects active CPU Power Scheme (`High Performance`, `Ultimate Performance`, `Balanced`, `Power Saver`).
  * Verifies **SSD NTFS TRIM** status (`DisableDeleteNotify`), **Xbox GameDVR** background capture overhead, **MMCSS** CPU reservation, and desktop **MenuShowDelay** latency.
* **🖥️ Hardware & BIOS/UEFI Telemetry (`src/hardware.c`)**
  * Executes raw `__cpuid` and `__cpuidex` silicon instructions to extract processor brand strings, vendor IDs, and **AVX2 256-bit SIMD** support.
  * Queries **BIOS Virtualization (VT-x / AMD-V)** firmware status, **UEFI Secure Boot**, **Hardware-Accelerated GPU Scheduling (HAGS)**, and kernel uptime.
* **🛡️ Safety-First Architecture (`--dry-run` by default)**
  * Destructive or state-changing commands (`clean` and `optimize`) run in **simulation mode (`--dry-run`)** by default unless `--apply` is explicitly provided.

---

## 🖥️ Terminal Preview

```text
  ╭──────────────────────────────────────────────────────────────────────╮
  │   ⚡ WINDOWS TWEAKER   System Diagnostic, Storage & OS Tuning Engine  │
  │  Pure C11 Win32 Native CLI  •  v1.0.0    •  Zero-Bloat Telemetry    │
  ╰──────────────────────────────────────────────────────────────────────╯

  OK    Hardware & BIOS Probe        AMD/Intel x86_64 Processor (16 threads) (1.2 ms)
  OK    Storage Engine Audit         Found 4.82 GB across 14,210 cached files (340.5 ms)
  WARN  Windows OS & Registry Audit  8 startup apps, 2 tuning opportunity(s) found (0.8 ms)

  🖥️ HARDWARE & BIOS/UEFI TELEMETRY ─ Silicon capabilities, Memory load & Firmware settings
  ────────────────────────────────────────────────────────────────────────
  ╭─ SYSTEM & SILICON PROFILE ───────────────────────────────────────────╮
  │  Processor (CPU)          AMD Ryzen / Intel Core (16 Logical Threads)
  │  Motherboard / BIOS       System Board (BIOS: UEFI Firmware)
  │  Physical Memory (RAM)    [██████████░░░░░░░░] 56% (17.9 GB / 32.0 GB)
  │  Kernel Uptime            1d 04h 22m (Healthy uptime)
  ╰──────────────────────────────────────────────────────────────────────╯

  OK    BIOS Virtualization          VT-x / AMD-V Active in BIOS (Optimal for WSL2/Docker/VMs)
  OK    Hardware GPU Scheduling      HAGS Enabled (HwSchMode = 2, lowers GPU scheduling latency)
  OK    UEFI Secure Boot             Active (Required by modern anti-cheats & kernel protection)
  OK    SIMD Vector Instruction Set  AVX2 256-bit SIMD Supported
```

---

## 🚀 Project Architecture

```text
windows-tweaker/
├── include/
│   ├── ui.h          # ANSI escape codes, spinners, progress bars & UTF-8 cards
│   ├── storage.h     # Disk capacity, cache scanner & cold-file audit structs
│   ├── tweaks.h      # Registry inspection, power scheme, SSD TRIM & gaming tweaks
│   └── hardware.h    # __cpuid CPU probe, RAM telemetry & BIOS/UEFI state
├── src/
│   ├── main.c        # CLI argument parser, subcommand router & health score engine
│   ├── ui.c          # Win32 console initialization & terminal rendering engine
│   ├── storage.c     # Recursive Win32 FindFirstFileA traversal & safe cleaner
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

## 🛠️ Building from Source

### Option 1: MSVC (`cl.exe` / Visual Studio Build Tools)
Run the included `build.bat` script (automatically locates `vcvars64.bat`):

```bat
.\build.bat
```

### Option 2: MinGW-w64 / GCC (`Makefile`)
```bash
mingw32-make
```

---

## 📖 CLI Usage

```bash
# 1. Run complete diagnostic audit (Hardware + BIOS + Storage + OS Tweaks + Health Score)
.\bin\windows-tweaker.exe scan

# 2. Deep storage audit with custom thresholds (e.g. files >= 100 MB unused for >= 60 days)
.\bin\windows-tweaker.exe storage --min-mb 100 --days 60

# 3. Hardware & BIOS virtualization probe only
.\bin\windows-tweaker.exe hardware

# 4. List all Windows Registry boot startup applications (HKCU & HKLM)
.\bin\windows-tweaker.exe startup

# 5. Preview temporary cache cleanup safely (--dry-run is default)
.\bin\windows-tweaker.exe clean --dry-run

# 6. Execute temporary cache cleanup for real
.\bin\windows-tweaker.exe clean --apply

# 7. Preview or apply safe gaming & UI responsiveness tweaks
.\bin\windows-tweaker.exe optimize --dry-run
.\bin\windows-tweaker.exe optimize --apply
```

---

## 🧪 Running Unit Tests

```bat
.\bin\test_suite.exe
```
