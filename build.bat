@echo off
setlocal

echo =================================================================
echo           Ume Language - Automated Build ^& Setup System
echo =================================================================
echo.

set "CMAKE_BIN=cmake"
where cmake >nul 2>nul
if %errorlevel% equ 0 goto found_cmake

if exist "C:\Program Files\CMake\bin\cmake.exe" (
    set "CMAKE_BIN=C:\Program Files\CMake\bin\cmake.exe"
    goto found_cmake
)
if exist "C:\Program Files (x86)\CMake\bin\cmake.exe" (
    set "CMAKE_BIN=C:\Program Files (x86)\CMake\bin\cmake.exe"
    goto found_cmake
)
if exist "%LOCALAPPDATA%\Programs\CMake\bin\cmake.exe" (
    set "CMAKE_BIN=%LOCALAPPDATA%\Programs\CMake\bin\cmake.exe"
    goto found_cmake
)

echo [*] 'cmake' not found. Attempting to install CMake via winget...
where winget >nul 2>nul
if %errorlevel% equ 0 (
    winget install --id Kitware.CMake -e --accept-source-agreements --accept-package-agreements
    if exist "C:\Program Files\CMake\bin\cmake.exe" (
        set "CMAKE_BIN=C:\Program Files\CMake\bin\cmake.exe"
        goto found_cmake
    )
)

echo [X] CMake not found. Please install CMake from: https://cmake.org/download/
pause
exit /b 1

:found_cmake
echo [OK] Using CMake: %CMAKE_BIN%

where cl >nul 2>nul
if %errorlevel% equ 0 goto msvc_ready

echo [*] MSVC compiler environment not active. Searching for Visual Studio...
set "VS_PATH="

set "VSWHERE_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE_PATH%" goto run_vswhere
set "VSWHERE_PATH=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE_PATH%" goto run_vswhere
goto skip_vswhere

:run_vswhere
for /f "usebackq delims=" %%i in (`"%VSWHERE_PATH%" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)

:skip_vswhere
if not defined VS_PATH (
    echo [!] Visual Studio path not found via vswhere. CMake will attempt direct generator detection.
    goto msvc_ready
)

if exist "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo [OK] Initializing MSVC environment from: %VS_PATH%
    call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
    goto msvc_ready
)
if exist "%VS_PATH%\Common7\Tools\VsDevCmd.bat" (
    echo [OK] Initializing DevCmd from: %VS_PATH%
    call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" >nul
    goto msvc_ready
)

:msvc_ready

echo.
echo [*] Configuring build with CMake in Release mode...
"%CMAKE_BIN%" -S . -B build -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo.
    echo [X] CMake configuration failed.
    echo [*] Please ensure Visual Studio "Desktop development with C++" workload is installed.
    pause
    exit /b 1
)

echo.
echo [*] Compiling Ume Language binaries and runtime...
"%CMAKE_BIN%" --build build --config Release --parallel
if %errorlevel% neq 0 (
    echo.
    echo [X] Compilation failed.
    pause
    exit /b 1
)

if not exist "bin\release" mkdir bin\release
if not exist "bin\release\include" mkdir bin\release\include

echo.
echo [*] Deploying executables and libraries to bin\release...
copy /Y build\bin\Release\* bin\release\ >nul 2>nul
copy /Y build\lib\Release\* bin\release\ >nul 2>nul

xcopy /Y /S /Q graphics\include\* bin\release\include\ >nul 2>nul
xcopy /Y /S /Q compiler\include\* bin\release\include\ >nul 2>nul

echo.
echo =================================================================
echo [SUCCESS] Ume Language Compiler build ^& deployment completed!
echo =================================================================
echo.
echo Executable location:
echo   %CD%\bin\release\ume.exe
echo.
echo Quick verification test:
bin\release\ume.exe version
echo.
echo Quick start commands:
echo   bin\release\ume.exe -run examples\01_basics\variables_and_types.ume
echo   bin\release\ume.exe new MyGame
echo   bin\release\ume.exe repl
echo.
echo [TIP] Run 'add_to_path.bat' to use 'ume' anywhere from any terminal.
echo.
