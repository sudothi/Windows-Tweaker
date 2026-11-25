<div align="center">
  <img src="assets/logo.png" width="112" alt="Windows Tweaker">

  <h1>Windows Tweaker</h1>

  <p><strong>DiagnÃ³stico, limpeza profunda de disco e otimizaÃ§Ã£o do Windows 10/11.</strong></p>

  <p>
    <a href="https://github.com/sudothi/Windows-Tweaker/releases"><img src="https://img.shields.io/github/v/release/sudothi/Windows-Tweaker?style=flat-square&label=vers%C3%A3o&color=10b981" alt="versÃ£o"></a>
    <a href="LICENSE"><img src="https://img.shields.io/github/license/sudothi/Windows-Tweaker?style=flat-square&label=licen%C3%A7a" alt="licenÃ§a"></a>
    <a href="#requisitos"><img src="https://img.shields.io/badge/plataforma-Windows%20x64-0078D6?style=flat-square" alt="plataforma"></a>
    <a href="#desenvolvimento"><img src="https://img.shields.io/badge/feito%20com-C11%20(Win32%20API)-00599C?style=flat-square" alt="feito com"></a>
  </p>

  [Baixar](https://github.com/sudothi/Windows-Tweaker/releases) Â· [Instalar](#instalar) Â· [Recursos](#recursos) Â· [Uso](#uso) Â· [OtimizaÃ§Ãµes](#otimizaÃ§Ãµes) Â· [Requisitos](#requisitos) Â· [Desenvolvimento](#desenvolvimento) Â· [LicenÃ§a](#licenÃ§a)
</div>

```text
        â–„â–ˆâ–ˆâ–ˆâ–ˆâ–„â–„â–ˆâ–ˆâ–ˆâ–ˆâ–„
       â–€â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–€          Windows Tweaker
        â–€â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–€           o seu PC, limpo e otimizado
  â–„â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–„
  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆ     diagnostico, limpeza de disco e
   â–€â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–€      ajustes de desempenho do Windows
    â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–€â–ˆâ–ˆâ–€â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆ
     â–€â–€â–ˆâ–€â–€  â–„â–ˆâ–ˆ   â–€â–€â–ˆâ–€â–€
           â–ˆâ–ˆâ–ˆ
```

## Instalar

Baixe o **`windows-tweaker.exe`** em [Releases](https://github.com/sudothi/Windows-Tweaker/releases).

O executÃ¡vel ainda nÃ£o tem assinatura digital, entÃ£o o SmartScreen pode avisar sobre editor desconhecido. Se o arquivo veio das Releases deste repositÃ³rio, clique em **Mais informaÃ§Ãµes â†’ Executar assim mesmo**.

O Windows Tweaker Ã© um binÃ¡rio portÃ¡til Ãºnico (`~150 KB`), sem instalador e sem dependÃªncias externas. Ele roda direto no nÃ­vel do seu usuÃ¡rio, sem instalar serviÃ§os em segundo plano e sem exigir permissÃ£o de administrador para as otimizaÃ§Ãµes padrÃ£o.

Prefere baixar e conferir o hash SHA-256 pelo Windows PowerShell?

```powershell
Invoke-WebRequest -Uri "https://github.com/sudothi/Windows-Tweaker/releases/latest/download/windows-tweaker.exe" -OutFile "windows-tweaker.exe"
Get-FileHash .\windows-tweaker.exe -Algorithm SHA256
.\windows-tweaker.exe
```

## Recursos

- **Modo Seguro (AnÃ¡lise Primeiro):** Ao abrir qualquer funÃ§Ã£o, o aplicativo apenas lÃª o estado atual do seu sistema. Nenhuma limpeza ou alteraÃ§Ã£o no Registro Ã© feita sem a sua permissÃ£o explÃ­cita na etapa seguinte.
- **12 OtimizaÃ§Ãµes Reais em 4 Categorias:** Verifica automaticamente o que jÃ¡ estÃ¡ otimizado (`JA OTIMIZADO`) e aplica apenas o que estiver pendente (`PENDENTE`), em lote ou 1 por 1.
- **Limpeza Verificada de Disco:** Mapeia caches do sistema (`%TEMP%`, `Win\Temp`, `SoftwareDistribution`, `CrashDumps`, `D3DSCache`), caches de desenvolvimento (`NPM`, `Pip`, `Chrome`), pastas `node_modules`/`.venv` esquecidas e arquivos grandes (`> 50 MB`) sem uso hÃ¡ mais de 30 dias, confirmando no disco apÃ³s a exclusÃ£o.
- **Gerenciador de Boot Sincronizado com o Task Manager:** LÃª e grava diretamente na chave binÃ¡ria `StartupApproved\Run`, diferenciando os aplicativos que estÃ£o realmente habilitados daqueles que vocÃª jÃ¡ desabilitou no Gerenciador de Tarefas.
- **Telemetria Direta de SilÃ­cio (`__cpuid`):** Extrai modelo do processador, suporte a instruÃ§Ãµes vetoriais AVX2 256-bit, carga de memÃ³ria RAM fÃ­sica, tamanho do `C:\hiberfil.sys`, status de VirtualizaÃ§Ã£o na BIOS (AMD-V / VT-x) e UEFI Secure Boot.
- **100% ReversÃ­vel:** Inclui opÃ§Ã£o integrada para restaurar todas as configuraÃ§Ãµes aos valores originais de fÃ¡brica do Windows a qualquer momento.

## Uso

1. Abra o `windows-tweaker.exe` dando duplo clique ou rodando pelo terminal.
2. Navegue pelas opÃ§Ãµes usando apenas as setas do teclado (`â†‘` / `â†“`) e confirme com `ENTER`.
3. Escolha **1. DiagnÃ³stico Completo do PC** para ver a nota de 0 a 100 do seu computador e o motivo exato de cada ponto descontado, ou acesse diretamente as ferramentas de limpeza, otimizaÃ§Ã£o e inicializaÃ§Ã£o.

Para uso direto via linha de comando ou scripts:

```powershell
windows-tweaker.exe scan                # diagnÃ³stico completo com nota explicada (somente leitura)
windows-tweaker.exe limpar              # abre o inspetor de caches, node_modules e arquivos grandes
windows-tweaker.exe limpar --apply      # limpa todos os caches temporÃ¡rios do sistema direto
windows-tweaker.exe otimizar --dry-run  # lista o estado das 12 regras sem alterar nada
windows-tweaker.exe otimizar --apply    # aplica todas as regras que estiverem pendentes
windows-tweaker.exe startup             # lista os programas habilitados e desabilitados no boot
windows-tweaker.exe hardware            # exibe telemetria de CPU (__cpuid), RAM e BIOS
```

## OtimizaÃ§Ãµes

| Categoria | Regra | O que faz |
| --- | --- | --- |
| **Energia & CPU** | 1. Plano de Energia da CPU | Ativa o esquema *Alto Desempenho* (`8c5e7fda-...`) para evitar quedas de clock |
| **Energia & CPU** | 2. Clock MÃ­nimo na Tomada | Define `PROCTHROTTLEMIN = 100%` na tomada (mantendo `5%` na bateria) |
| **Jogos & Input** | 3. GravaÃ§Ã£o Xbox GameDVR | Desativa a captura contÃ­nua de vÃ­deo em segundo plano na GPU (`AppCaptureEnabled = 0`) |
| **Jogos & Input** | 4. Modo de Jogo (Game Mode) | Ativa `AutoGameModeEnabled = 1` para priorizar threads do jogo aberto |
| **Jogos & Input** | 5. PrecisÃ£o 1:1 do Mouse | Remove a curva de aceleraÃ§Ã£o do ponteiro (`MouseSpeed = 0` via `SPI_SETMOUSE`) |
| **Jogos & Input** | 6. Prioridade MMCSS Games | Define `GPU Priority = 8`, `Priority = 6` e `Scheduling Category = High` |
| **Rede & Ping** | 7. Upload P2P do Windows Update | Bloqueia o envio oculto de atualizaÃ§Ãµes via P2P (`DODownloadMode = 0`) |
| **Rede & Ping** | 8. Limpeza de Cache DNS | Limpa o cache de rotas DNS via chamada nativa `DnsFlushResolverCache()` (`dnsapi.dll`) |
| **RAM & Privacidade** | 9. Apps da Store em 2Âº Plano | Bloqueia apps UWP ociosos rodando escondidos na memÃ³ria RAM (`GlobalUserDisabled = 1`) |
| **RAM & Privacidade** | 10. Busca Web do Bing no Iniciar | Desativa buscas web no Menu Iniciar (`DisableSearchBoxSuggestions = 1`), deixando a busca local instantÃ¢nea |
| **RAM & Privacidade** | 11. Bloatware e ID de AnÃºncios | Impede instalaÃ§Ã£o silenciosa de apps patrocinados (`SilentInstalledAppsEnabled = 0`) |
| **Interface & Sistema** | 12. Menus 20ms e Fechamento RÃ¡pido | Reduz atraso de menus para `20ms`, fecha apps travados em `2s` (`WaitToKillAppTimeout = 2000`) e remove anÃºncios do Explorer |

## Requisitos

- Windows 10 ou Windows 11 x64.
- Terminal com suporte a sequÃªncias ANSI / UTF-8 (Windows Terminal, PowerShell ou Prompt de Comando do Windows 10/11).
- Nenhuma biblioteca ou runtime externo (compilado estaticamente contra `kernel32.lib`, `user32.lib` e `advapi32.lib`).

## Desenvolvimento

Estrutura do projeto em C11:

| Arquivo | Responsabilidade |
| --- | --- |
| `src/main.c` | Fluxo interativo por setas, roteador de subcomandos CLI e cÃ¡lculo explicado da nota (0 a 100) |
| `src/ui.c` | Renderizador ANSI TrueColor, banner em meio-bloco e seletor com coordenada fixa (`SetConsoleCursorPosition`) |
| `src/tweaks.c` | CatÃ¡logo das 12 regras de otimizaÃ§Ã£o, integraÃ§Ã£o `StartupApproved\Run` e `DnsFlushResolverCache()` |
| `src/storage.c` | Varredura recursiva Win32 (`FindFirstFileA`), detector de `node_modules`/arquivos frios e exclusÃ£o verificada |
| `src/hardware.c` | InstruÃ§Ãµes de silÃ­cio `__cpuid`, telemetria de RAM (`GlobalMemoryStatusEx`) e verificaÃ§Ãµes de BIOS/UEFI |
| `tests/test_suite.c` | SuÃ­te automatizada com 27 testes unitÃ¡rios e de integraÃ§Ã£o |

Para compilar a partir do cÃ³digo-fonte (com Visual Studio Build Tools / MSVC ou MinGW-w64):

```bat
.\build.bat
.\bin\test_suite.exe
```

## Contribuir

Bugs e sugestÃµes de novas regras de otimizaÃ§Ã£o podem ser abertos nas [Issues](https://github.com/sudothi/Windows-Tweaker/issues). Todas as regras propostas devem ser documentadas, reversÃ­veis pelo botÃ£o de restauraÃ§Ã£o de fÃ¡brica e aprovadas na suÃ­te de testes do GitHub Actions.

## LicenÃ§a

DistribuÃ­do sob a licenÃ§a [MIT](LICENSE), sem garantia.
