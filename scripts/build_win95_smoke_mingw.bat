@echo off
setlocal

if not exist build mkdir build

mingw32-gcc -mwindows -march=i386 -Os -static-libgcc -o build\win95_smoke_mingw.exe tools\win95_smoke\main.c -luser32 -lgdi32
exit /b %ERRORLEVEL%
