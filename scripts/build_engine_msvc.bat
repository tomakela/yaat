@echo off
setlocal

if not exist build mkdir build

cl /nologo /W3 /O1 /MT /I src src\main_win32.c src\platform\win32\gdi_renderer.c src\platform\win32\audio_winmm.c src\script_tokenizer.c src\script_parser.c src\script_package.c src\script_bytecode.c src\runtime\asset_loader.c src\runtime\zip_archive.c src\third_party\miniz\miniz.c src\third_party\miniz\miniz_zip.c src\third_party\miniz\miniz_tinfl.c /link /SUBSYSTEM:WINDOWS,4.00 /OUT:build\yaat_engine_msvc.exe user32.lib gdi32.lib winmm.lib
exit /b %ERRORLEVEL%
