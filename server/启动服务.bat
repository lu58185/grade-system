@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ============================================
echo   Student Grade System - Backend
echo   Open in browser: http://localhost:8000
echo   Stop: press Ctrl + C
echo ============================================
echo.

rem ---- find a working python interpreter ----
set "PY="

if exist "C:\Users\russ\.workbuddy\binaries\python\versions\3.13.12\python.exe" (
    set "PY=C:\Users\russ\.workbuddy\binaries\python\versions\3.13.12\python.exe"
    goto found
)

where py >nul 2>nul
if not errorlevel 1 (
    set "PY=py -3"
    goto found
)

where python >nul 2>nul
if not errorlevel 1 (
    rem the WindowsApps python is only a store stub, so verify it really runs
    python -c "print(1)" >nul 2>nul
    if not errorlevel 1 (
        set "PY=python"
        goto found
    )
)

goto nopy

:found
echo [info] using: !PY!
echo [info] starting server, keep this window open...
echo.
!PY! "%~dp0server.py"
echo.
echo [info] server stopped
pause
exit /b 0

:nopy
echo [ERROR] No usable Python found on this computer.
echo.
echo Option 1: install Python from https://www.python.org/downloads/
echo           remember to tick "Add python.exe to PATH"
echo.
echo Option 2: run from the VS Code terminal with a full path:
echo   "C:\Users\russ\.workbuddy\binaries\python\versions\3.13.12\python.exe" server\server.py
echo.
pause
exit /b 1
