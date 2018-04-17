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

set CLCommonFlags=-nologo -W4 -WX -Zs -wd4204

REM Actual Check
REM ============
%CLExe% %HereDir%/libs/xxxx_buf.c ^
  %CLCommonFlags%

%CLExe% %HereDir%/libs/xxxx_map.c ^
  %CLCommonFlags%

%CLExe% %HereDir%/libs/xxxx_iobuffer.c ^
  %CLCommonFlags% -D_CRT_SECURE_NO_WARNINGS

echo off
