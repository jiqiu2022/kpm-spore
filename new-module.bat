@echo off
REM Script to create a new KPM module from template

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "MODULES_DIR=%SCRIPT_DIR%modules"
set "TEMPLATE_DIR=%MODULES_DIR%\template"

REM Check if module name is provided
if "%~1"=="" (
    echo Error: Module name is required
    echo.
    echo Usage: %0 ^<module-name^> [author] [description]
    echo.
    echo Examples:
    echo   %0 my-module
    echo   %0 my-module "Your Name" "My awesome module"
    exit /b 1
)

set "MODULE_NAME=%~1"
set "AUTHOR=%~2"
set "DESCRIPTION=%~3"

if "%AUTHOR%"=="" set "AUTHOR=Your Name"
if "%DESCRIPTION%"=="" set "DESCRIPTION=KPM Module - %MODULE_NAME%"

set "MODULE_DIR=%MODULES_DIR%\%MODULE_NAME%"

REM Check if module already exists
if exist "%MODULE_DIR%" (
    echo Error: Module '%MODULE_NAME%' already exists
    exit /b 1
)

REM Check if template exists
if not exist "%TEMPLATE_DIR%" (
    echo Error: Template directory not found: %TEMPLATE_DIR%
    exit /b 1
)

echo Creating new KPM module: %MODULE_NAME%
echo.

REM Create module directory
mkdir "%MODULE_DIR%"

REM Copy template files
copy "%TEMPLATE_DIR%\module.c" "%MODULE_DIR%\module.c" >nul
copy "%TEMPLATE_DIR%\module.lds" "%MODULE_DIR%\module.lds" >nul

REM Replace placeholders in module.c (using PowerShell for text replacement)
powershell -Command "(Get-Content '%MODULE_DIR%\module.c') -replace 'kpm-template', 'kpm-%MODULE_NAME%' -replace 'Your Name', '%AUTHOR%' -replace 'KPM Template Module', '%DESCRIPTION%' -replace 'template_', '%MODULE_NAME%_' -replace 'kpm template', 'kpm %MODULE_NAME%' | Set-Content '%MODULE_DIR%\module.c'"

echo Module created successfully!
echo.
echo Module location: %MODULE_DIR%
echo.
echo Next steps:
echo   1. Edit %MODULE_DIR%\module.c to implement your module
echo   2. Run: build.bat
echo   3. Or build specific module: cmake --build build --target %MODULE_NAME%
echo.
echo Files created:
dir /b "%MODULE_DIR%"

endlocal

