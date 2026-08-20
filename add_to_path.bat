@echo off
setlocal

set "BIN_PATH=%CD%\bin\release"

if not exist "%BIN_PATH%\ume.exe" (
    echo [!] ume.exe not found at: %BIN_PATH%
    echo [*] Please run build.bat first to compile the binaries!
    echo.
    pause
    exit /b 1
)

echo Adding %BIN_PATH% to your User PATH...
for /f "tokens=2*" %%A in ('reg query "HKCU\Environment" /v Path 2^>nul') do set "USER_PATH=%%B"

echo %USER_PATH% | find /i "%BIN_PATH%" >nul
if %errorlevel% equ 0 (
    echo [OK] %BIN_PATH% is already in your PATH!
) else (
    if defined USER_PATH (
        setx Path "%USER_PATH%;%BIN_PATH%"
    ) else (
        setx Path "%BIN_PATH%"
    )
    echo [SUCCESS] Added %BIN_PATH% to your PATH.
    echo [*] Restart your terminal for the changes to take effect.
)

echo.
pause
