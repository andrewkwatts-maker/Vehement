@echo off
REM Generate a single hero icon using GI pipeline
REM Usage: generate_single_icon.bat <hero_name> [size]

setlocal

if "%~1"=="" (
    echo Usage: generate_single_icon.bat ^<hero_name^> [size]
    echo.
    echo Examples:
    echo   generate_single_icon.bat alien_commander
    echo   generate_single_icon.bat alien_commander 2048
    echo.
    echo Available heroes:
    dir /b ..\game\assets\heroes\*.json
    exit /b 1
)

set HERO=%~1
set SIZE=%~2
if "%SIZE%"=="" set SIZE=1024

set BUILD_DIR=..\build\bin\Debug
set RENDERER=%BUILD_DIR%\asset_icon_renderer_gi.exe
set ASSET=..\game\assets\heroes\%HERO%.json
set OUTPUT=..\game\assets\heroes\icons\%HERO%_icon.png

echo ========================================
echo Single Hero Icon Generator (GI)
echo ========================================
echo Hero:     %HERO%
echo Size:     %SIZE%x%SIZE%
echo Input:    %ASSET%
echo Output:   %OUTPUT%
echo ========================================
echo.

REM Check if renderer exists
if not exist "%RENDERER%" (
    echo ERROR: Renderer not found at %RENDERER%
    echo Please build the asset_icon_renderer_gi target first:
    echo   cd build
    echo   cmake --build . --config Debug --target asset_icon_renderer_gi
    exit /b 1
)

REM Check if asset exists
if not exist "%ASSET%" (
    echo ERROR: Asset not found: %ASSET%
    echo.
    echo Available heroes:
    dir /b ..\game\assets\heroes\*.json
    exit /b 1
)

REM Create output directory
set OUTPUT_DIR=%OUTPUT:~0,-4%
for %%f in ("%OUTPUT%") do set OUTPUT_DIR=%%~dpf
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM Run renderer
"%RENDERER%" "%ASSET%" "%OUTPUT%" %SIZE% %SIZE%

if %ERRORLEVEL% equ 0 (
    echo.
    echo ========================================
    echo SUCCESS! Icon saved to:
    echo %OUTPUT%
    echo ========================================
) else (
    echo.
    echo ========================================
    echo FAILED! See error output above.
    echo ========================================
    exit /b 1
)

endlocal
