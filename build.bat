REM User Configuration
REM ==================
@echo off
set HereDir=%~d0%~p0
if not defined OutputDir set OutputDir=%HereDir%\output
if not defined ObjDir set ObjDir=%OutputDir%\obj
if not defined CLExe set CLExe=cl.exe
if not defined LinkExe set LinkExe=link.exe
setlocal

if not exist "%OutputDir%" mkdir "%OutputDir%"
if not exist "%ObjDir%" mkdir "%ObjDir%"
if %errorlevel% neq 0 exit /b 1

set CLCommonFlags=-nologo -Z7 -W1 -wd4244 -wd4267 -wd4204 -wd4201

REM Actual Build
REM ============
echo NONE

echo off
