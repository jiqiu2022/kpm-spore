@echo off
REM KPM Build Script for Windows
REM Copyright (c) 2024

setlocal enabledelayedexpansion

REM Script directory
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

REM Load .env file if exists
if exist ".env" (
    echo Loading configuration from .env...
    for /f "tokens=*" %%a in ('type .env ^| findstr /v "^#" ^| findstr /v "^$"') do (
        set "%%a"
    )
)

REM Print banner
echo ========================================
echo    KPM Build System (CMake)
echo    Build Anywhere - Any Platform
echo ========================================
echo.

REM Parse command line arguments
set CLEAN=0
set VERBOSE=
set BUILD_JOBS=

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="clean" (
    set CLEAN=1
    shift
    goto parse_args
)
if /i "%~1"=="-v" (
    set VERBOSE=--verbose
    shift
    goto parse_args
)
if /i "%~1"=="--verbose" (
    set VERBOSE=--verbose
    shift
    goto parse_args
)
shift
goto parse_args

:args_done

REM Clean if requested
if %CLEAN%==1 (
    echo Cleaning build directory...
    if exist build rmdir /s /q build 2>nul
    if exist third_party rmdir /s /q third_party 2>nul
    echo Clean complete
    exit /b 0
)

REM Show configuration
echo Configuration:
if defined NDK_PATH (
    echo   [OK] NDK Path: %NDK_PATH%
) else (
    echo   [*] NDK Path: Auto-detect
)

if defined KP_DIR (
    echo   [OK] KernelPatch: %KP_DIR%
) else (
    echo   [*] KernelPatch: Auto-download to third_party/
)

REM Detect build jobs
if not defined BUILD_JOBS (
    if defined NUMBER_OF_PROCESSORS (
        set BUILD_JOBS=-j%NUMBER_OF_PROCESSORS%
    ) else (
        set BUILD_JOBS=-j2
    )
)
echo   [OK] Parallel jobs: %BUILD_JOBS%
echo.

REM Configure CMake
echo Configuring CMake...
set CMAKE_ARGS=
if defined KP_DIR set CMAKE_ARGS=%CMAKE_ARGS% -DKP_DIR=%KP_DIR%
if defined KP_ZIP_URL set CMAKE_ARGS=%CMAKE_ARGS% -DKP_ZIP_URL=%KP_ZIP_URL%

cmake -B build %CMAKE_ARGS%
if errorlevel 1 (
    echo CMake configuration failed
    exit /b 1
)

REM Build
echo.
echo Building...
cmake --build build %BUILD_JOBS% %VERBOSE%
if errorlevel 1 (
    echo Build failed
    exit /b 1
)

REM Check if build was successful
dir /s /b build\*.kpm >nul 2>&1
if errorlevel 1 (
    echo Build failed - no output files found
    exit /b 1
)

echo.
echo ========================================
echo   Build Successful!
echo ========================================
echo.
echo Output files:
for /r build %%f in (*.kpm) do (
    echo   [OK] %%f
    dir "%%f" | findstr /v "Directory"
)
echo.
echo KPM modules ready in build directory

endlocal

