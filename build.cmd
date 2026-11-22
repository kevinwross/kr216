@echo off
rem Build entry point for kr216 on Windows.
rem
rem The toolchain is resolved, never assumed: vswhere names the Visual Studio installation,
rem vcvars64 puts cl.exe on PATH, and CMake and ctest come from the Visual Studio copy
rem because neither is on PATH in a plain shell (AGENTS.local.md records the discovery).
rem The doctor runs first, then configure, build and test for the named preset.
setlocal EnableExtensions

set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=windows-msvc-debug"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo build: vswhere.exe not found at "%VSWHERE%"
    exit /b 2
)

set "VS="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS (
    echo build: no Visual Studio installation with the C++ build tools
    exit /b 2
)

set "VCVARS=%VS%\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CTEST=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"

if not exist "%CMAKE%" (
    echo build: cmake not found at "%CMAKE%"
    exit /b 2
)
if not exist "%CTEST%" (
    echo build: ctest not found at "%CTEST%"
    exit /b 2
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0.agents\doctor.msvc.ps1"
if errorlevel 1 exit /b %errorlevel%

call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
    echo build: vcvars64.bat failed
    exit /b 2
)

echo build: preset %PRESET%
"%CMAKE%" --preset "%PRESET%"
if errorlevel 1 exit /b 1
"%CMAKE%" --build --preset "%PRESET%"
if errorlevel 1 exit /b 1
"%CTEST%" --preset "%PRESET%"
if errorlevel 1 exit /b 1
echo build: %PRESET% passed
exit /b 0
