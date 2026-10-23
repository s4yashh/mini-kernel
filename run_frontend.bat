@echo off
REM Launch the Mini Kernel Simulator frontend with a local static server.
REM The page must be served over HTTP so it can read the committed sample files.
cd /d "%~dp0"

where node >nul 2>nul
if errorlevel 1 (
  echo Node.js was not found on PATH.
  echo Install Node.js from https://nodejs.org/ and run this file again.
  pause
  exit /b 1
)

echo Starting the Mini Kernel Simulator frontend...
node tools\serve.js
pause
