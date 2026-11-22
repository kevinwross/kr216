@echo off
rem Build the kr216 toolchain from the sibling checkouts and package it.
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\package-release.ps1" %*
exit /b %ERRORLEVEL%
