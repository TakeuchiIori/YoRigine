@echo off
chcp 65001 > nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0package.ps1" %*
pause
