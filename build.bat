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

set CLCommonFlags=-nologo -Z7 -W3 -wd4244 -wd4267 -wd4204 -wd4201 -D_CRT_SECURE_NO_WARNINGS -Fo:"%ObjDir%"\

REM Actual Build
REM ============

set O="%ObjDir%\md2_cpp_unit.obj
"%CLExe%" "-I%HereDir%\libs" "%HereDir%\md2_cpp_unit.cpp" -c ^
  %CLCommonFlags%
if %errorlevel% neq 0 exit /b 1

set O="%OutputDir%"\md2.exe
"%CLExe%" -Fe:"%O%" %CLCommonFlags% "%HereDir%\md2_unit.c" %ObjDir%\md2_cpp_unit.obj ^
  "-I%HereDir%\deps\glad\include"
if %errorlevel% neq 0 exit /b 1
echo PROGRAM    %O%

xcopy /y /s "%HereDir%\assets" "%OutputDir%"\assets\

echo off
