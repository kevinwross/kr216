@echo off
rem Build entry point for kr216 on Windows.
rem
rem One job: put cl.exe, cmake and ctest on PATH by resolving the Visual Studio
rem installation through vswhere, then hand the rest to the preset. Configure,
rem build, test, row counts and the ARM64 row all live in CMakePresets.json and
rem the gate scripts; this file knows nothing about them, which is why it no
rem longer carries a second copy of the build.
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

call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo build: vcvars64.bat failed
    exit /b 2
)

cmake --preset "%PRESET%" || exit /b 1
cmake --build --preset "%PRESET%" || exit /b 1
ctest --preset "%PRESET%" || exit /b 1
