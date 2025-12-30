@echo off
REM Build script for socket_client.dll
REM Run this from MSYS2 UCRT64 terminal or with MinGW in PATH

echo Building socket_client.dll...

g++ -shared -o socket_client.dll socket_client.cpp -lws2_32 -static -O2

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS: socket_client.dll created!
    echo.
    copy socket_client.dll ..\socket_client.dll >nul
    echo Copied to python folder.
) else (
    echo.
    echo ERROR: Build failed!
)

pause
