@echo off
setlocal
set DEVCPP_BIN=C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin
set RAYLIB_ROOT=%~dp0raylib\raylib-6.0_win64_mingw-w64

"%DEVCPP_BIN%\g++.exe" "%~dp0main.cpp" "%~dp0src\simulation.cpp" "%~dp0src\risk.cpp" "%~dp0src\mission.cpp" "%~dp0src\ui.cpp" -o "%~dp0OrbitGuard.exe" -std=c++17 -I "%~dp0include" -I "%RAYLIB_ROOT%\include" -L "%RAYLIB_ROOT%\lib" -lraylibdll -lopengl32 -lgdi32 -lwinmm
if errorlevel 1 (
    echo.
    echo Compile failed. Check the error messages above.
    pause
    exit /b 1
)

copy "%RAYLIB_ROOT%\lib\raylib.dll" "%~dp0raylib.dll" >nul
echo.
echo Compile succeeded: OrbitGuard.exe
pause
