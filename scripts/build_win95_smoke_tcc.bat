@echo off
setlocal

if not exist build mkdir build

tcc -m32 -Wl,-subsystem=windows -o build\win95_smoke_tcc.exe tools\win95_smoke\main.c -luser32 -lgdi32
exit /b %ERRORLEVEL%
