@echo off
setlocal

if not exist build mkdir build

mingw32-gcc -I src -mwindows -march=i386 -Os -static-libgcc -o build\yaat_engine_mingw.exe src\main_win32.c src\platform\win32\gdi_renderer.c -luser32 -lgdi32
exit /b %ERRORLEVEL%
