@echo off
setlocal

if not exist build mkdir build

cl /nologo /W3 /O1 /MT tools\win95_smoke\main.c /link /SUBSYSTEM:WINDOWS,4.00 /OUT:build\win95_smoke_msvc.exe user32.lib gdi32.lib
exit /b %ERRORLEVEL%
