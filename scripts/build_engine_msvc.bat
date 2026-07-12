@echo off
setlocal EnableDelayedExpansion

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

cl /nologo /W3 /O1 /MT /I src !ENGINE_SOURCES! /link /SUBSYSTEM:WINDOWS,4.00 /OUT:build\yaat_engine_msvc.exe user32.lib gdi32.lib winmm.lib
exit /b %ERRORLEVEL%
