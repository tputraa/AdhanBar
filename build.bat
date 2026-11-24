@echo off
setlocal

where zig >nul 2>nul
if errorlevel 1 (
    echo Zig is required to build this project. Install Zig and reopen the terminal.
    exit /b 1
)

if not exist native-build mkdir native-build
zig rc /fo native-build\app.res app.rc
if errorlevel 1 exit /b %errorlevel%

zig c++ -std=c++17 -O2 -Wall -Wextra -municode AdhanBar.cpp native-build\app.res -lwinhttp -lshell32 -lgdi32 -luser32 -lcomdlg32 -o native-build\AdhanBar.exe -Wl,--subsystem,windows
exit /b %errorlevel%
