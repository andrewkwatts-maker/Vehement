@echo off
REM Generate icons for all hero assets using GI pipeline
REM Usage: generate_hero_icons.bat

setlocal enabledelayedexpansion

echo ========================================
echo Hero Asset Icon Generator (GI Pipeline)
echo ========================================
echo.

REM Set paths
set BUILD_DIR=..\build\bin\Debug
set RENDERER=%BUILD_DIR%\asset_icon_renderer_gi.exe
set ASSET_DIR=..\game\assets\heroes
set OUTPUT_DIR=..\game\assets\heroes\icons
set SIZE=1024

REM Check if renderer exists
if not exist "%RENDERER%" (
    echo ERROR: Renderer not found at %RENDERER%
    echo Please build the asset_icon_renderer_gi target first:
    echo   cd build
    echo   cmake --build . --config Debug --target asset_icon_renderer_gi
    exit /b 1
)

REM Create output directory
if not exist "%OUTPUT_DIR%" (
    mkdir "%OUTPUT_DIR%"
)

echo Renderer: %RENDERER%
echo Assets:   %ASSET_DIR%
echo Output:   %OUTPUT_DIR%
echo Size:     %SIZE%x%SIZE%
echo.

REM Counter
set COUNT=0
set SUCCESS=0
set FAILED=0

REM Process each hero asset
for %%f in ("%ASSET_DIR%\*.json") do (
    set /a COUNT+=1
    set "ASSET=%%~nf"
    set "INPUT=%%f"
    set "OUTPUT=%OUTPUT_DIR%\!ASSET!_icon.png"

    echo [!COUNT!] Rendering: !ASSET!
    echo     Input:  !INPUT!
    echo     Output: !OUTPUT!

    REM Run renderer
    "%RENDERER%" "!INPUT!" "!OUTPUT!" %SIZE% %SIZE%

    if !ERRORLEVEL! equ 0 (
        echo     ✓ SUCCESS
        set /a SUCCESS+=1
    ) else (
        echo     ✗ FAILED
        set /a FAILED+=1
    )
    echo.
)

echo ========================================
echo Icon Generation Complete
echo ========================================
echo Total:    %COUNT%
echo Success:  %SUCCESS%
echo Failed:   %FAILED%
echo ========================================

endlocal
pause
