@echo off
setlocal enabledelayedexpansion
rem Build and run one example. usage: build.cmd [virtual.cpp|exceptions.cpp|containers.cpp]
set "ROOT=%~dp0.."
set "SOURCE=%~1"
if "%SOURCE%"=="" set "SOURCE=virtual.cpp"
if not exist "%ROOT%\examples\%SOURCE%" (
    echo unknown example: %SOURCE%
    exit /b 2
)
set "WORK=%ROOT%\examples\build"
if not exist "%WORK%" mkdir "%WORK%"

if exist "%ROOT%\bin\krc.exe" (
    set "KRC=%ROOT%\bin\krc.exe"
    set "KRLD=%ROOT%\bin\krld.exe"
    set "EMU=%ROOT%\bin\kr216.exe"
    set "RUNTIME=%ROOT%\runtime"
    set "SYSROOT=%ROOT%\sysroot"
) else (
    if not defined KRCOMPILER set "KRCOMPILER=%ROOT%\..\krcompiler"
    set "KRC=!KRCOMPILER!\build\windows-msvc-debug\host\bin\Debug\krc.exe"
    set "KRLD=!KRCOMPILER!\build\windows-msvc-debug\host\bin\Debug\krld.exe"
    set "EMU=%ROOT%\build\windows-msvc-debug\tools\kr216-emu\Debug\kr216.exe"
    set "RUNTIME=!KRCOMPILER!\targets\kr216\runtime"
    set "SYSROOT=!KRCOMPILER!\build\windows-msvc-debug\stage\sysroot"
)
if not exist "%KRC%" (
    echo krc not found at %KRC%
    echo Build krcompiler first, or set KRCOMPILER to its checkout.
    exit /b 1
)

"%KRC%" -c --target=kr216 "%RUNTIME%\crt0.s" -o "%WORK%\crt0.kro" || exit /b 1
"%KRC%" -c --target=kr216 "%RUNTIME%\syscalls.s" -o "%WORK%\syscalls.kro" || exit /b 1
"%KRC%" -c --target=kr216 "%RUNTIME%\eh.s" -o "%WORK%\eh.kro" || exit /b 1
"%KRC%" -c --target=kr216 "%RUNTIME%\libc.c" -o "%WORK%\libc.kro" || exit /b 1
"%KRC%" -c --target=kr216 "%ROOT%\examples\%SOURCE%" --sysroot "%SYSROOT%" -o "%WORK%\example.kro" || exit /b 1
"%KRLD%" --gc-sections "%WORK%\crt0.kro" "%WORK%\syscalls.kro" "%WORK%\eh.kro" "%WORK%\libc.kro" "%WORK%\example.kro" -o "%WORK%\example.k216" || exit /b 1
"%EMU%" "%WORK%\example.k216"
exit /b %ERRORLEVEL%
