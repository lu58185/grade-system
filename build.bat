@echo off
setlocal
cd /d "%~dp0"

echo ========================================
echo   Building...
echo ========================================

set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VCVARS%" goto novs

call "%VCVARS%" >nul
cl /nologo /EHsc /utf-8 /std:c++17 main.cpp /Fe:grade.exe
if errorlevel 1 goto buildfail

echo.
echo ========================================
echo   BUILD OK  --  grade.exe created
echo   Now double-click run.bat
echo ========================================
pause
exit /b 0

:novs
echo.
echo [ERROR] Visual Studio compiler not found.
echo Expected path:
echo   %VCVARS%
pause
exit /b 1

:buildfail
echo.
echo [ERROR] Build failed. Read the messages above.
pause
exit /b 1
