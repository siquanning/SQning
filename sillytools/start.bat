@echo off
cd /d %~dp0
echo Starting SillyTavern Manager...
echo.
node server.cjs
pause
