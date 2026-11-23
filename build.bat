@echo off
setlocal

:: Auto-locate MSVC vcvars64.bat if cl.exe is not already in PATH
where cl.exe >nul 2>nul
if %ERRORLEVEL% neq 0 (
    if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
    ) else (
        echo [ERROR] MSVC cl.exe or vcvars64.bat not found.
        exit /b 1
    )
)

if not exist bin mkdir bin

echo [BUILD] Compiling windows-tweaker.exe (MSVC C11 /W4 /O2)...
cl.exe /nologo /W4 /O2 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Iinclude ^
    src\main.c src\ui.c src\storage.c src\tweaks.c src\hardware.c ^
    /Fe:bin\windows-tweaker.exe /Fo:bin\ ^
    advapi32.lib user32.lib kernel32.lib

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed!
    exit /b %ERRORLEVEL%
)

echo [BUILD] Compiling unit test suite (bin\test_suite.exe)...
cl.exe /nologo /W4 /O2 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Iinclude ^
    tests\test_suite.c src\ui.c src\storage.c src\tweaks.c src\hardware.c ^
    /Fe:bin\test_suite.exe /Fo:bin\ ^
    advapi32.lib user32.lib kernel32.lib

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Test suite build failed!
    exit /b %ERRORLEVEL%
)

del /q bin\*.obj 2>nul
echo [OK] Build complete: bin\windows-tweaker.exe
