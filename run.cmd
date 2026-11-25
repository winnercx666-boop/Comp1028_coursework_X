@echo off
cd /d "%~dp0"
if exist "build\coursework.exe" (
    "build\coursework.exe"
) else if exist "coursework.exe" (
    "coursework.exe"
) else if exist "build\coursework_standard.exe" (
    "build\coursework_standard.exe"
) else if exist "coursework_standard.exe" (
    "coursework_standard.exe"
) else (
    echo No compiled program found. Please compile the project using README.md first.
)
pause
