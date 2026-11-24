# Windows Tweaker (`windows-tweaker`)

Assistente de terminal em **C11 puro** (zero dependencias externas) para diagnostico, limpeza de disco e otimizacao do Windows 10/11. Roda direto sobre as APIs nativas do sistema (`Win32 API`, Registro, `FindFirstFileA`, `GlobalMemoryStatusEx`) e instrucoes de hardware do processador (`__cpuid`).

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
windows-tweaker                     abre o menu interativo no terminal
windows-tweaker scan                diagnostico completo (Hardware, Disco, Windows e Nota explicada)
windows-tweaker limpar              mostra os caches e arquivos grandes e permite excluir de verdade
windows-tweaker otimizar            mostra e aplica ajustes de desempenho no Registro (MenuShowDelay, GameDVR)
windows-tweaker startup             lista todos os programas que iniciam sozinhos com o Windows
windows-tweaker hardware            mostra processador (__cpuid), memoria RAM, placa-mae e BIOS
```

Opcoes: `--dry-run` (apenas simula e mostra o que seria feito), `--apply` / `--yes` (executa a limpeza de caches ou aplica as otimizacoes direto), `--min-mb 50`, `--days 30`.

## O que cada modulo faz

- **Disco, Caches e Exclusao Verificada (`src/storage.c`):**
  - Mapeia `%TEMP%`, `C:\Windows\Temp`, `SoftwareDistribution\Download`, `CrashDumps`, `D3DSCache`, `npm-cache`, `pip` e cache do Chrome.
  - Encontra arquivos grandes esquecidos (*cold files*, `>= 50 MB` sem uso ha `+30 dias`) nas pastas do usuario (`Downloads`, `Documents`, `Videos`, `Desktop`).
  - Na tela `limpar`, permite digitar `c` para limpar todas as pastas de cache ou o numero do arquivo (`1`, `2`, `1,3`) para **excluir o arquivo diretamente da pasta de origem**, removendo travas `READONLY` e verificando no disco (`GetFileAttributesA`) se o arquivo realmente deixou de existir.
- **Otimizacoes do Windows e Jogos (`src/tweaks.c`):**
  - Verifica o Plano de Energia ativo, status do **TRIM do SSD** (`DisableDeleteNotify`), gravacao oculta em segundo plano do **Xbox GameDVR**, atraso de abertura de menus (`MenuShowDelay`) e lista todos os programas nas chaves `HKCU\...\Run` e `HKLM\...\Run`.
  - Explica para cada item o motivo (`Por que importa`) e a solucao (`Como resolver`).
- **Hardware, Memoria e BIOS (`src/hardware.c`):**
  - Executa a instrucao `__cpuid` direto no processador para ler o modelo da CPU, threads logicas e suporte a instrucoes vetoriais **AVX2 256-bit**.
  - Confere carga da memoria RAM fisica, tempo ligado (*uptime*), **Virtualizacao na BIOS (AMD-V / VT-x)** e **UEFI Secure Boot**.
- **Interface Visual de Terminal (`src/ui.c`):**
  - Baseada no design do `Turtle CLI`: arte em meio-bloco (`▄`, `▀`, `█`) com degrade diagonal TrueColor (`\x1b[38;2;r;g;bm`) por codepoint UTF-8, passos com `◆`, status com `✓`, `!`, `✗`, `·`, `│`, spinner de uma linha e caixa de resumo dinamica `╭─ Resumo do seu PC ───╮`.

## Compilar e Testar

```bat
.\build.bat
.\bin\test_suite.exe
.\bin\windows-tweaker.exe
```
