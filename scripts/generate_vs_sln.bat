@echo off
setlocal enabledelayedexpansion

rem Generate a Visual Studio solution from the MSVC debug preset.
rem Usage:
rem   generate_vs_sln.bat
rem   generate_vs_sln.bat vs2026
rem   generate_vs_sln.bat vs2022

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "REPO_ROOT=%%~fI"
set "CMAKE_PRESET=msvc-debug"
set "REQUESTED_VS=%~1"

if not defined REQUESTED_VS set "REQUESTED_VS=vs2026"

if /I "%REQUESTED_VS%"=="vs2022" goto :use_vs2022
if /I "%REQUESTED_VS%"=="vs2026" goto :use_vs2026

echo [ERROR] Unsupported argument: "%REQUESTED_VS%"
echo [INFO] Usage: %~nx0 [vs2022^|vs2026]
exit /b 1

:use_vs2022
set "VS_DISPLAY_NAME=Visual Studio 2022"
set "VS_VERSION_RANGE=[17.0,18.0)"
set "BUILD_DIR=build/vs2022"
set "CMAKE_GENERATOR=Visual Studio 17 2022"
goto :after_vs_selection

:use_vs2026
set "VS_DISPLAY_NAME=Visual Studio 2026"
set "VS_VERSION_RANGE=[18.0,19.0)"
set "BUILD_DIR=build/vs2026"
set "CMAKE_GENERATOR=Visual Studio 18 2026"
goto :after_vs_selection

:after_vs_selection

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_DEV_CMD="

if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe not found: "%VSWHERE%"
    exit /b 1
)
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"

call :find_vs_dev_cmd "%VS_VERSION_RANGE%" "%VS_DISPLAY_NAME%"
if errorlevel 1 exit /b 1

if not defined VS_DEV_CMD (
    echo [ERROR] %VS_DISPLAY_NAME% not found. Please install MSVC x64 toolset.
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
echo [INFO] Generating %VS_DISPLAY_NAME% solution from preset "%CMAKE_PRESET%" ...
cmake --preset "%CMAKE_PRESET%" -G "%CMAKE_GENERATOR%" -A x64 -B "%BUILD_DIR%" --fresh
if errorlevel 1 goto :command_failed

popd
echo.
echo [INFO] %VS_DISPLAY_NAME% solution generated: "%REPO_ROOT%\%BUILD_DIR:/=\%"
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
