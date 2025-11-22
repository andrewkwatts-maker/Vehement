@echo off
REM Nova3D Engine - Windows Setup Script
REM This script automates the build process and dependency fetching

setlocal enabledelayedexpansion

echo.
echo ============================================
echo   Nova3D Engine - Build Setup
echo ============================================
echo.

REM Check for CMake
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake not found in PATH
    echo Please install CMake from https://cmake.org/download/
    pause
    exit /b 1
)

REM Check for Git
where git >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Git not found in PATH
    echo Please install Git from https://git-scm.com/
    pause
    exit /b 1
)

REM Parse arguments
set BUILD_TYPE=Release
set GENERATOR=
set CLEAN_BUILD=0

:parse_args
if "%~1"=="" goto end_parse
if /i "%~1"=="--debug" set BUILD_TYPE=Debug
if /i "%~1"=="--release" set BUILD_TYPE=Release
if /i "%~1"=="--clean" set CLEAN_BUILD=1
if /i "%~1"=="--vs2022" set GENERATOR=-G "Visual Studio 17 2022"
if /i "%~1"=="--vs2019" set GENERATOR=-G "Visual Studio 16 2019"
if /i "%~1"=="--ninja" set GENERATOR=-G Ninja
if /i "%~1"=="--help" goto show_help
shift
goto parse_args
:end_parse

REM Create build directory
set BUILD_DIR=build

if %CLEAN_BUILD%==1 (
    echo [INFO] Cleaning build directory...
    if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
)

if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo [INFO] Build Type: %BUILD_TYPE%
echo [INFO] Build Directory: %BUILD_DIR%
echo.

REM Configure with CMake
echo [INFO] Configuring project with CMake...
echo [INFO] This will automatically fetch all dependencies...
echo.

cd %BUILD_DIR%

cmake .. %GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DNOVA_BUILD_EXAMPLES=ON

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo [INFO] Configuration complete!
echo.

REM Build the project
echo [INFO] Building project...
cmake --build . --config %BUILD_TYPE% --parallel

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Build failed!
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo ============================================
echo   Build Complete!
echo ============================================
echo.
echo Executable location: %BUILD_DIR%\bin\%BUILD_TYPE%\nova_demo.exe
echo.
echo To run: %BUILD_DIR%\bin\%BUILD_TYPE%\nova_demo.exe
echo.
pause
exit /b 0

:show_help
echo Usage: setup.bat [options]
echo.
echo Options:
echo   --debug     Build in Debug mode
echo   --release   Build in Release mode (default)
echo   --clean     Clean build directory before building
echo   --vs2022    Use Visual Studio 2022 generator
echo   --vs2019    Use Visual Studio 2019 generator
echo   --ninja     Use Ninja generator (faster builds)
echo   --help      Show this help message
echo.
exit /b 0
