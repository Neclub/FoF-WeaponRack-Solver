@echo off
cd /d "%~dp0"
call build.bat
if errorlevel 1 (
  pause
  exit /b 1
)
echo.
echo Starting FoF Weapon Rack Solver...
start "" "build\FoF_WeaponRack_Solver.exe"
