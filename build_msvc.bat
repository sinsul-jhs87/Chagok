@echo off
rem Visual Studio 2019/2022 "x64 Native Tools Command Prompt" 에서 실행
cmake -S . -B build-msvc -A x64
cmake --build build-msvc --config Release
echo.
echo 결과: build-msvc\Release\Chagok.exe, ChagokShell.dll
