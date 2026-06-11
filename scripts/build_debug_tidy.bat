@echo off
setlocal enabledelayedexpansion

rem Configure and build the MSVC debug preset with clang-tidy enabled.

set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%.."
set "CMAKE_PRESET=msvc-debug-tidy"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_DEV_CMD="

if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe not found: "%VSWHERE%"
    exit /b 1
)

call :find_vs_dev_cmd "[18.0,19.0)" "Visual Studio 2026"
if errorlevel 1 exit /b 1

if not defined VS_DEV_CMD (
    call :find_vs_dev_cmd "[17.0,18.0)" "Visual Studio 2022"
    if errorlevel 1 exit /b 1
)

if not defined VS_DEV_CMD (
    echo [ERROR] Visual Studio 2026 or 2022 not found. Please install MSVC x64 toolset.
    exit /b 1
)

echo [INFO] Using dev environment: "%VS_DEV_CMD%"
call "%VS_DEV_CMD%" -arch=x64 -host_arch=x64
if errorlevel 1 (
    echo [ERROR] Failed to enter Visual Studio dev environment.
    exit /b !errorlevel!
)

pushd "%REPO_ROOT%"
if errorlevel 1 (
    echo [ERROR] Cannot enter repo root: "%REPO_ROOT%"
    exit /b !errorlevel!
)

echo.
echo [INFO] Configuring preset "%CMAKE_PRESET%" ...
cmake --preset "%CMAKE_PRESET%" --fresh
if errorlevel 1 goto :command_failed

echo.
echo [INFO] Building preset "%CMAKE_PRESET%" ...
cmake --build --preset "%CMAKE_PRESET%"
if errorlevel 1 goto :command_failed

popd
echo.
echo [INFO] Build passed.
exit /b 0

:command_failed
set "COMMAND_EXIT_CODE=!errorlevel!"
popd
echo [ERROR] Command failed with exit code !COMMAND_EXIT_CODE!.
exit /b !COMMAND_EXIT_CODE!

:find_vs_dev_cmd
set "VS_DEV_CMD="
for /f "usebackq delims=" %%I in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'; & $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -version '%~1' -property installationPath | Select-Object -First 1"`) do (
    if exist "%%~I\Common7\Tools\VsDevCmd.bat" (
        set "VS_DEV_CMD=%%~I\Common7\Tools\VsDevCmd.bat"
    )
)
if defined VS_DEV_CMD echo [INFO] Found %~2.
exit /b 0
