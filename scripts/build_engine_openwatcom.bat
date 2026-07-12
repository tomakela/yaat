@echo off
setlocal EnableDelayedExpansion

rem Build the YAAT Win32 engine shell with Open Watcom C/C++.
rem Run this from the repository root after OWSETENV.BAT has configured WATCOM,
rem INCLUDE, LIB, and PATH.
rem Engine sources are read from scripts/engine_sources.txt.

if not exist build mkdir build

set "ENGINE_SOURCES="
for /f "usebackq eol=# tokens=* delims=" %%S in ("scripts\engine_sources.txt") do (
    set "SRC=%%S"
    if not "!SRC!"=="" (
        set "SRC=!SRC:/=\!"
        set "ENGINE_SOURCES=!ENGINE_SOURCES! !SRC!"
    )
)

wcl386 -q -bt=nt -i=src -os -w3 -l=win95 -fe=build\yaat_engine_openwatcom.exe !ENGINE_SOURCES! user32.lib gdi32.lib winmm.lib
exit /b %ERRORLEVEL%
