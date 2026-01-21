@echo off
REM ==========================================================================
REM Nova3D AI Tools - Quick Launcher
REM ==========================================================================
REM
REM Launch the AI content generation tools menu.
REM Run this from anywhere to access all AI-powered tools.
REM
REM First-time users: This will check for API key setup.
REM
REM ==========================================================================

setlocal

REM Navigate to tools directory and run the launcher
cd /d "%~dp0tools"
call nova3d_ai_tools.bat

endlocal
