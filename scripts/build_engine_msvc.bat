@echo off
setlocal

if not exist build mkdir build

cl /nologo /W3 /O1 /MT /I src src\main_win32.c src\platform\win32\gdi_renderer.c /link /SUBSYSTEM:WINDOWS,4.00 /OUT:build\yaat_engine_msvc.exe user32.lib gdi32.lib
exit /b %ERRORLEVEL%
