@echo off
cd /d H:\Github\Old3DEngine\build\bin\Debug
asset_icon_renderer_gi.exe "..\..\..\game\assets\heroes\test_gi_hero.json" "..\..\..\game\assets\heroes\icons\test_gi_hero_icon.png" 512 512 > "..\..\..\icon_renderer_output.log" 2>&1
exit /b %ERRORLEVEL%
