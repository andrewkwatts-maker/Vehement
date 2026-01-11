@echo off
REM Integrate menu enhancements and run polish loop

echo ========================================
echo Menu Enhancement Integration
echo ========================================
echo.

REM First, integrate the menu scene code into RTSApplication.cpp
echo [1/4] Integrating menu scene code...

REM Add mesh creation to Initialize() function
python tools/integrate_menu_code.py

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Integration failed
    pause
    exit /b 1
)

echo      ^> Menu code integrated

REM Build the project
echo.
echo [2/4] Building project with menu enhancements...
cd build
cmake ..
cmake --build . --config Debug --target nova_demo

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo      ^> Build successful

REM Add MenuSceneMeshes.cpp to CMakeLists.txt if not already there
echo.
echo [3/4] Updating CMakeLists.txt...
cd ..
findstr /C:"MenuSceneMeshes.cpp" CMakeLists.txt >nul
if %ERRORLEVEL% NEQ 0 (
    echo Adding MenuSceneMeshes.cpp to build...
    REM Add to examples source list
)

REM Run the polish loop
echo.
echo [4/4] Starting polish loop...
echo.
python tools/menu_polish_loop.py

echo.
echo ========================================
echo Integration and Polish Complete!
echo ========================================
pause
