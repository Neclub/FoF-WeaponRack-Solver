@echo off
setlocal EnableExtensions
cd /d "%~dp0"

if not exist build mkdir build

set OUT=build\wrc.exe
set TEST_OUT=build\test_solver.exe
set TEST_LM=build\test_logmon.exe

where cl >nul 2>&1
if %ERRORLEVEL%==0 goto :compile

set "VCVARS="
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
  set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
  set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
  set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)

if not defined VCVARS (
  echo.
  echo No Visual Studio C++ compiler found.
  echo Install the Desktop development with C++ workload.
  echo.
  exit /b 1
)

echo Loading Visual Studio build tools...
call "%VCVARS%" >nul

:compile
echo Building icon...
powershell -NoProfile -ExecutionPolicy Bypass -File src\make_icon.ps1 -Source "Icon\Medieval Weapon Rack Trio.png" -Dest "build\wrc.ico"
if errorlevel 1 goto :fail

echo Building with cl...
rc /nologo /fo build\wrc.res src\wrc.rc
if errorlevel 1 goto :fail
cl /nologo /O2 /W3 /std:c17 /D_CRT_SECURE_NO_WARNINGS /c /Fo:build\solver.obj src\solver.c
if errorlevel 1 goto :fail
cl /nologo /O2 /W3 /std:c17 /D_CRT_SECURE_NO_WARNINGS /c /Fo:build\logmon.obj src\logmon.c
if errorlevel 1 goto :fail
cl /nologo /O2 /W3 /std:c17 /D_CRT_SECURE_NO_WARNINGS /c /Fo:build\app.obj src\app.c
if errorlevel 1 goto :fail
cl /nologo /Fe:%OUT% build\app.obj build\solver.obj build\logmon.obj build\wrc.res /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comdlg32.lib comctl32.lib dwmapi.lib uxtheme.lib
if errorlevel 1 goto :fail

cl /nologo /O2 /W3 /std:c17 /D_CRT_SECURE_NO_WARNINGS /Fe:%TEST_OUT% src\test_solver.c src\solver.c
if errorlevel 1 goto :fail
cl /nologo /O2 /W3 /std:c17 /D_CRT_SECURE_NO_WARNINGS /Fe:%TEST_LM% src\test_logmon.c src\logmon.c
if errorlevel 1 goto :fail
del /q *.obj 2>nul
if exist src\*.obj del /q src\*.obj 2>nul
if exist build\*.obj del /q build\*.obj 2>nul

echo Running solver tests...
%TEST_OUT%
if errorlevel 1 goto :fail
echo Running logmon tests...
%TEST_LM%
if errorlevel 1 goto :fail
echo.
echo Build OK: %OUT%
exit /b 0

:fail
echo Build failed.
exit /b 1
