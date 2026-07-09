@echo off
setlocal

rem Build the Win95 GUI smoke test with Open Watcom C/C++.
rem Run this from the repository root after OWSETENV.BAT has configured WATCOM,
rem INCLUDE, LIB, and PATH.

if not exist build mkdir build

wcl386 -q -bt=nt -os -w3 -l=win95 -fe=build\win95_smoke_openwatcom.exe tools\win95_smoke\main.c user32.lib gdi32.lib
exit /b %ERRORLEVEL%
