@echo off
"%~dp0load_overlay.exe" %*
if errorlevel 1 pause
