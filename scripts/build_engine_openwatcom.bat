@echo off
setlocal

rem Build the YAAT Win32 engine shell with Open Watcom C/C++.
rem Run this from the repository root after OWSETENV.BAT has configured WATCOM,
rem INCLUDE, LIB, and PATH.

if not exist build mkdir build

wcl386 -q -bt=nt -i=src -os -w3 -l=win95 -fe=build\yaat_engine_openwatcom.exe src\main_win32.c src\platform\win32\gdi_renderer.c src\script_tokenizer.c src\runtime\asset_loader.c user32.lib gdi32.lib
exit /b %ERRORLEVEL%
