@echo off
echo ========================================
echo Searching for UI/HTML/Editor Systems
echo ========================================
echo.

echo [1] Searching for HTML files...
echo ----------------------------------------
dir /s /b *.html 2>nul
echo.

echo [2] Searching for UI-related headers...
echo ----------------------------------------
dir /s /b *UI*.hpp 2>nul
dir /s /b *Menu*.hpp 2>nul
dir /s /b *Panel*.hpp 2>nul
dir /s /b *Browser*.hpp 2>nul
dir /s /b *Editor*.hpp 2>nul
echo.

echo [3] Searching for Shader/Material systems...
echo ----------------------------------------
dir /s /b *Shader*.hpp 2>nul
dir /s /b *Material*.hpp 2>nul
echo.

echo [4] Searching for ImGui usage in source files...
echo ----------------------------------------
findstr /s /i /m "ImGui::" *.cpp *.hpp 2>nul | find /c ":"
echo files contain ImGui code
echo.

echo [5] Searching for web/HTML rendering...
echo ----------------------------------------
findstr /s /i /m "webkit\|webview\|html\|css" *.cpp *.hpp 2>nul
echo.

echo Search complete!
pause
