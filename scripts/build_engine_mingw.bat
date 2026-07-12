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

mingw32-gcc -I src -mwindows -march=i386 -Os -static-libgcc -o build\yaat_engine_mingw.exe !ENGINE_SOURCES! -luser32 -lgdi32 -lwinmm
exit /b %ERRORLEVEL%
