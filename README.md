<div align="center">
  <img src="assets/logo.png" width="112" alt="Windows Tweaker">

  <h1>Windows Tweaker</h1>

  <p><strong>Diagnóstico, limpeza profunda de disco e otimização do Windows 10/11.</strong></p>

  <p>
    <a href="https://github.com/sudothi/Windows-Tweaker/releases"><img src="https://img.shields.io/github/v/release/sudothi/Windows-Tweaker?style=flat-square&label=vers%C3%A3o&color=10b981" alt="versão"></a>
    <a href="LICENSE"><img src="https://img.shields.io/github/license/sudothi/Windows-Tweaker?style=flat-square&label=licen%C3%A7a" alt="licença"></a>
    <a href="#requisitos"><img src="https://img.shields.io/badge/plataforma-Windows%20x64-0078D6?style=flat-square" alt="plataforma"></a>
    <a href="#desenvolvimento"><img src="https://img.shields.io/badge/feito%20com-C11%20(Win32%20API)-00599C?style=flat-square" alt="feito com"></a>
  </p>

  [Baixar](https://github.com/sudothi/Windows-Tweaker/releases) · [Instalar](#instalar) · [Recursos](#recursos) · [Uso](#uso) · [Otimizações](#otimizações) · [Requisitos](#requisitos) · [Desenvolvimento](#desenvolvimento) · [Licença](#licença)
</div>

```text
        ▄████▄▄████▄
       ▀████████████▀          Windows Tweaker
        ▀██████████▀           o seu PC, limpo e otimizado
  ▄██████████████████████▄
  ████████████████████████     diagnostico, limpeza de disco e
   ▀████████████████████▀      ajustes de desempenho do Windows
    ████████▀██▀████████
     ▀▀█▀▀  ▄██   ▀▀█▀▀
           ███
```

## Instalar

Baixe o **`windows-tweaker.exe`** em [Releases](https://github.com/sudothi/Windows-Tweaker/releases).

O executável ainda não tem assinatura digital, então o SmartScreen pode avisar sobre editor desconhecido. Se o arquivo veio das Releases deste repositório, clique em **Mais informações → Executar assim mesmo**.

O Windows Tweaker é um binário portátil único (`~150 KB`), sem instalador e sem dependências externas. Ele roda direto no nível do seu usuário, sem instalar serviços em segundo plano e sem exigir permissão de administrador para as otimizações padrão.

Prefere baixar e conferir o hash SHA-256 pelo Windows PowerShell?

```powershell
Invoke-WebRequest -Uri "https://github.com/sudothi/Windows-Tweaker/releases/latest/download/windows-tweaker.exe" -OutFile "windows-tweaker.exe"
Get-FileHash .\windows-tweaker.exe -Algorithm SHA256
.\windows-tweaker.exe
```

## Recursos

- **Modo Seguro (Análise Primeiro):** Ao abrir qualquer função, o aplicativo apenas lê o estado atual do seu sistema. Nenhuma limpeza ou alteração no Registro é feita sem a sua permissão explícita na etapa seguinte.
- **12 Otimizações Reais em 4 Categorias:** Verifica automaticamente o que já está otimizado (`JA OTIMIZADO`) e aplica apenas o que estiver pendente (`PENDENTE`), em lote ou 1 por 1.
- **Limpeza Verificada de Disco:** Mapeia caches do sistema (`%TEMP%`, `Win\Temp`, `SoftwareDistribution`, `CrashDumps`, `D3DSCache`), caches de desenvolvimento (`NPM`, `Pip`, `Chrome`), pastas `node_modules`/`.venv` esquecidas e arquivos grandes (`> 50 MB`) sem uso há mais de 30 dias, confirmando no disco após a exclusão.
- **Gerenciador de Boot Sincronizado com o Task Manager:** Lê e grava diretamente na chave binária `StartupApproved\Run`, diferenciando os aplicativos que estão realmente habilitados daqueles que você já desabilitou no Gerenciador de Tarefas.
- **Telemetria Direta de Silício (`__cpuid`):** Extrai modelo do processador, suporte a instruções vetoriais AVX2 256-bit, carga de memória RAM física, tamanho do `C:\hiberfil.sys`, status de Virtualização na BIOS (AMD-V / VT-x) e UEFI Secure Boot.
- **100% Reversível:** Inclui opção integrada para restaurar todas as configurações aos valores originais de fábrica do Windows a qualquer momento.

## Uso

1. Abra o `windows-tweaker.exe` dando duplo clique ou rodando pelo terminal.
2. Navegue pelas opções usando apenas as setas do teclado (`↑` / `↓`) e confirme com `ENTER`.
3. Escolha **1. Diagnóstico Completo do PC** para ver a nota de 0 a 100 do seu computador e o motivo exato de cada ponto descontado, ou acesse diretamente as ferramentas de limpeza, otimização e inicialização.

Para uso direto via linha de comando ou scripts:

```powershell
windows-tweaker.exe scan                # diagnóstico completo com nota explicada (somente leitura)
windows-tweaker.exe limpar              # abre o inspetor de caches, node_modules e arquivos grandes
windows-tweaker.exe limpar --apply      # limpa todos os caches temporários do sistema direto
windows-tweaker.exe otimizar --dry-run  # lista o estado das 12 regras sem alterar nada
windows-tweaker.exe otimizar --apply    # aplica todas as regras que estiverem pendentes
windows-tweaker.exe startup             # lista os programas habilitados e desabilitados no boot
windows-tweaker.exe hardware            # exibe telemetria de CPU (__cpuid), RAM e BIOS
```

## Otimizações

| Categoria | Regra | O que faz |
| --- | --- | --- |
| **Energia & CPU** | 1. Plano de Energia da CPU | Ativa o esquema *Alto Desempenho* (`8c5e7fda-...`) para evitar quedas de clock |
| **Energia & CPU** | 2. Clock Mínimo na Tomada | Define `PROCTHROTTLEMIN = 100%` na tomada (mantendo `5%` na bateria) |
| **Jogos & Input** | 3. Gravação Xbox GameDVR | Desativa a captura contínua de vídeo em segundo plano na GPU (`AppCaptureEnabled = 0`) |
| **Jogos & Input** | 4. Modo de Jogo (Game Mode) | Ativa `AutoGameModeEnabled = 1` para priorizar threads do jogo aberto |
| **Jogos & Input** | 5. Precisão 1:1 do Mouse | Remove a curva de aceleração do ponteiro (`MouseSpeed = 0` via `SPI_SETMOUSE`) |
| **Jogos & Input** | 6. Prioridade MMCSS Games | Define `GPU Priority = 8`, `Priority = 6` e `Scheduling Category = High` |
| **Rede & Ping** | 7. Upload P2P do Windows Update | Bloqueia o envio oculto de atualizações via P2P (`DODownloadMode = 0`) |
| **Rede & Ping** | 8. Limpeza de Cache DNS | Limpa o cache de rotas DNS via chamada nativa `DnsFlushResolverCache()` (`dnsapi.dll`) |
| **RAM & Privacidade** | 9. Apps da Store em 2º Plano | Bloqueia apps UWP ociosos rodando escondidos na memória RAM (`GlobalUserDisabled = 1`) |
| **RAM & Privacidade** | 10. Busca Web do Bing no Iniciar | Desativa buscas web no Menu Iniciar (`DisableSearchBoxSuggestions = 1`), deixando a busca local instantânea |
| **RAM & Privacidade** | 11. Bloatware e ID de Anúncios | Impede instalação silenciosa de apps patrocinados (`SilentInstalledAppsEnabled = 0`) |
| **Interface & Sistema** | 12. Menus 20ms e Fechamento Rápido | Reduz atraso de menus para `20ms`, fecha apps travados em `2s` (`WaitToKillAppTimeout = 2000`) e remove anúncios do Explorer |

## Requisitos

- Windows 10 ou Windows 11 x64.
- Terminal com suporte a sequências ANSI / UTF-8 (Windows Terminal, PowerShell ou Prompt de Comando do Windows 10/11).
- Nenhuma biblioteca ou runtime externo (compilado estaticamente contra `kernel32.lib`, `user32.lib` e `advapi32.lib`).

## Desenvolvimento

Estrutura do projeto em C11:

| Arquivo | Responsabilidade |
| --- | --- |
| `src/main.c` | Fluxo interativo por setas, roteador de subcomandos CLI e cálculo explicado da nota (0 a 100) |
| `src/ui.c` | Renderizador ANSI TrueColor, banner em meio-bloco e seletor com coordenada fixa (`SetConsoleCursorPosition`) |
| `src/tweaks.c` | Catálogo das 12 regras de otimização, integração `StartupApproved\Run` e `DnsFlushResolverCache()` |
| `src/storage.c` | Varredura recursiva Win32 (`FindFirstFileA`), detector de `node_modules`/arquivos frios e exclusão verificada |
| `src/hardware.c` | Instruções de silício `__cpuid`, telemetria de RAM (`GlobalMemoryStatusEx`) e verificações de BIOS/UEFI |
| `tests/test_suite.c` | Suíte automatizada com 27 testes unitários e de integração |

Para compilar a partir do código-fonte (com Visual Studio Build Tools / MSVC ou MinGW-w64):

```bat
.\build.bat
.\bin\test_suite.exe
```

## Contribuir

Bugs e sugestões de novas regras de otimização podem ser abertos nas [Issues](https://github.com/sudothi/Windows-Tweaker/issues). Todas as regras propostas devem ser documentadas, reversíveis pelo botão de restauração de fábrica e aprovadas na suíte de testes do GitHub Actions.

## Licença

Distribuído sob a licença [MIT](LICENSE), sem garantia.
